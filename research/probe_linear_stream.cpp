// probe_linear_stream
// ----------------------------------------------------------------------------
// Production linear token-stream control (g = 0, no Kerr).
//
// This is the linear half of the same streaming substrate used by
// research/probe_streaming_compression.cpp:
//   token -> plastic graph -> 2-hop light cone -> packet field -> prepared Cayley
//   flow -> packet field.
//
// There is no Kerr pressure, no horizon gate and no bridge materialization here.
// The point is to measure the honest linear field carrier with the same low-level
// memory/flow discipline as the nonlinear engine, not the older unordered_map +
// trig LocalFlowCarrier path.
// ----------------------------------------------------------------------------
#define NOMINMAX
#include "../tools/graph_wave_substrate.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <unordered_map>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

#ifndef GW_LINEAR_COLLECT_FLOW_STATS
#define GW_LINEAR_COLLECT_FLOW_STATS 0
#endif

using gw::cd;
using gw::Vec;

constexpr int TOPICS = 3, PER = 24, WIN = 4, TOPK = 6;
constexpr int BASE_NODES = TOPICS * PER;
constexpr double ETA = 1.0, DECAY = 0.99, EDGE_FLOOR = 1e-3, WMIN = 8.0, INJECT = 0.35;
constexpr int SMIN = 4, STEPS = 2;
constexpr double DT = 0.3;

static double peakRamMb() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
#endif
  return -1.0;
}

struct Edge { double w = 0; double phase = 0; int last = 0; };

struct PacketField {
  std::vector<int> ids;
  std::vector<cd> vals;
  size_t size() const { return ids.size(); }
  void clear(){ ids.clear(); vals.clear(); }
  void reserve(size_t n){ ids.reserve(n); vals.reserve(n); }
  int find(int id) const { for (int i = 0; i < (int)ids.size(); ++i) if (ids[i] == id) return i; return -1; }
  void set(int id, cd v){
    int i = find(id);
    if (i >= 0) vals[i] = v;
    else { ids.push_back(id); vals.push_back(v); }
  }
  void pushKnownUnique(int id, cd v){ ids.push_back(id); vals.push_back(v); }
  void add(int id, cd v){
    int i = find(id);
    if (i >= 0) vals[i] += v;
    else { ids.push_back(id); vals.push_back(v); }
  }
};

struct Mem { PacketField lin; double prLin = 1.0; };

struct Graph {
  std::vector<std::unordered_map<int, Edge>> adj;
  std::vector<int> seen;
  std::deque<int> win;
  int t = 0;

  int add(){ adj.push_back({}); seen.push_back(0); return (int)adj.size() - 1; }
  double incident(int a) const { double s = 0; for (auto& kv : adj[a]) s += kv.second.w; return s; }
  void decay(int a, int b){ auto it = adj[a].find(b); if (it == adj[a].end()) return; int dt = t - it->second.last; if (dt > 0) { it->second.w *= std::pow(DECAY, dt); it->second.last = t; } }
  void eraseEdge(int a, int b){ adj[a].erase(b); adj[b].erase(a); }
  static double wrapPhase(double x){ while (x > gw::kPi) x -= 2.0 * gw::kPi; while (x < -gw::kPi) x += 2.0 * gw::kPi; return x; }
  bool hasEdge(int a, int b) const { return adj[a].find(b) != adj[a].end(); }
  double orientedPhase(int u, int v) const { auto it = adj[u].find(v); if (it == adj[u].end()) return 0.0; return u < v ? it->second.phase : -it->second.phase; }
  void addSignedPhase(int u, int v, double dphi){ auto it = adj[u].find(v); if (it == adj[u].end()) return; Edge e = it->second; double sign = u < v ? 1.0 : -1.0; e.phase = std::clamp(wrapPhase(e.phase + sign * dphi), -1.4, 1.4); adj[u][v] = e; adj[v][u] = e; }
  void addPlaquetteFlux(int a, int b, int c){ constexpr double dphi = 0.018; addSignedPhase(a, b, dphi / 3.0); addSignedPhase(b, c, dphi / 3.0); addSignedPhase(c, a, dphi / 3.0); }
  void closePlaquettes(int node){ for (int j = 1; j < (int)win.size(); ++j) for (int i = 0; i < j; ++i) { int a = win[i], b = win[j]; if (hasEdge(a, b) && hasEdge(a, node) && hasEdge(b, node)) addPlaquetteFlux(a, b, node); } }
  void relaxNode(int a){ std::vector<int> dead; for (auto& kv : adj[a]) { int b = kv.first; int dt = t - kv.second.last; if (dt > 0) { kv.second.w *= std::pow(DECAY, dt); kv.second.last = t; auto back = adj[b].find(a); if (back != adj[b].end()) back->second = kv.second; } if (kv.second.w < EDGE_FLOOR) dead.push_back(b); } for (int b : dead) eraseEdge(a, b); }
  void prune(int a){ while ((int)adj[a].size() > TOPK) { int wk = -1; double mn = 1e100; for (auto& kv : adj[a]) if (kv.second.w < mn) { mn = kv.second.w; wk = kv.first; } eraseEdge(a, wk); } }
  void touch(int a, int b){ if (a == b) return; decay(a, b); decay(b, a); adj[a][b].w += ETA; adj[a][b].last = t; adj[b][a] = adj[a][b]; if ((int)adj[a].size() > TOPK) prune(a); if ((int)adj[b].size() > TOPK) prune(b); }
};

struct StreamScratch {
  std::vector<int> mark, idx, nodes;
  std::vector<gw::LocalCayleyFlowBond> cayleyBonds;
  int stamp = 1;
  void begin(int n){
    if ((int)mark.size() < n) { mark.resize(n, 0); idx.resize(n, -1); }
    if (++stamp == 0x3fffffff) { std::fill(mark.begin(), mark.end(), 0); stamp = 1; }
    nodes.clear();
    cayleyBonds.clear();
  }
  bool add(int v){
    if (mark[v] == stamp) return false;
    mark[v] = stamp;
    idx[v] = (int)nodes.size();
    nodes.push_back(v);
    return true;
  }
  int local(int v) const { return mark[v] == stamp ? idx[v] : -1; }
};

static unsigned hash3(int a, int b, int c){ unsigned x = 2166136261u; x = (x ^ (unsigned)a) * 16777619u; x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u; x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u; return x; }

static void setFieldSingle(PacketField& f, int id, cd v){ f.clear(); f.set(id, v); }

static void hoodScratch(const Graph& g, int src, StreamScratch& scratch){
  scratch.begin((int)g.adj.size());
  scratch.add(src);
  for (int hop = 0; hop < 2; ++hop) {
    int sz = (int)scratch.nodes.size();
    for (int h = 0; h < sz; ++h) {
      int u = scratch.nodes[h];
      for (auto& kv : g.adj[u]) scratch.add(kv.first);
    }
  }
}

static void bondsInLightConeScratch(const Graph& g, StreamScratch& scratch){
  double maxW = 0.0;
  scratch.cayleyBonds.clear();
  scratch.cayleyBonds.reserve((size_t)scratch.nodes.size() * TOPK);
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) {
    const int u = scratch.nodes[i];
    for (auto& kv : g.adj[u]) {
      const int j = scratch.local(kv.first);
      if (j >= 0 && i < j) {
        maxW = std::max(maxW, std::abs(kv.second.w));
        scratch.cayleyBonds.push_back({i, j, kv.second.w, 1.0, 0.0, gw::bondPhase(g.orientedPhase(u, kv.first))});
      }
    }
  }
  const double inv = maxW > 1e-300 ? 1.0 / maxW : 0.0;
  for (auto& e : scratch.cayleyBonds) e = gw::makeLocalCayleyFlowBond(e.a, e.b, e.w, e.phase_u, DT, inv);
}

static Vec projectScratch(const PacketField& f, const StreamScratch& scratch, int src){
  Vec psi((int)scratch.nodes.size(), cd(0,0));
  for (int i = 0; i < (int)f.ids.size(); ++i) {
    const int j = f.ids[i] < (int)scratch.mark.size() ? scratch.local(f.ids[i]) : -1;
    if (j >= 0) psi[j] += f.vals[i];
  }
  psi[scratch.local(src)] += cd(INJECT, 0);
  gw::normalizeInPlace(psi);
  return psi;
}

static PacketField unprojectScratch(const StreamScratch& scratch, const Vec& psi){
  PacketField out;
  out.reserve(scratch.nodes.size());
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) if (std::norm(psi[i]) > 1e-14) out.pushKnownUnique(scratch.nodes[i], psi[i]);
  return out;
}

static cd packetOverlap(const PacketField& a, const PacketField& b){
  cd z(0,0);
  for (int i = 0; i < (int)a.ids.size(); ++i) {
    int j = b.find(a.ids[i]);
    if (j >= 0) z += std::conj(a.vals[i]) * b.vals[j];
  }
  return z;
}

struct Metrics {
  long long updates = 0, stable = 0, totalLocalNodes = 0, totalBonds = 0, totalBondVisits = 0;
  int maxLocalNodes = 0, maxBonds = 0;
  long long maxBondVisits = 0;
  double sumPR = 0, minPR = 1e100, maxPR = 0, maxPhaseSpeed = 0;
};

struct System {
  Graph g;
  std::vector<Mem> mem;
  StreamScratch scratch;
  bool randomize = false;
  Metrics m;

  explicit System(bool r) : randomize(r) { for (int i = 0; i < BASE_NODES; ++i) add(); }
  int add(){ int id = g.add(); mem.push_back({}); return id; }
  int randomEndpoint(int a, int b) const { int n = (int)g.adj.size(); int r = (int)(hash3(a,b,g.t) % (unsigned)n); return r == a ? (r + 1) % n : r; }

  void evolve(int src){
    hoodScratch(g, src, scratch);
    int n = (int)scratch.nodes.size();
    if (n < 3) { setFieldSingle(mem[src].lin, src, cd(1,0)); return; }
    bondsInLightConeScratch(g, scratch);
    Vec lin = projectScratch(mem[src].lin, scratch, src);
#if GW_LINEAR_COLLECT_FLOW_STATS
    gw::LocalFlowStats st;
    gw::edgeLocalCayleyFlowPrepared(lin, scratch.cayleyBonds, STEPS, &st);
    const long long bondVisits = st.bond_visits;
    m.maxPhaseSpeed = std::max(m.maxPhaseSpeed, st.max_bond_speed);
#else
    gw::edgeLocalCayleyFlowPrepared(lin, scratch.cayleyBonds, STEPS);
    const long long bondVisits = (long long)scratch.cayleyBonds.size() * 2LL * STEPS;
#endif
    gw::normalizeInPlace(lin);
    mem[src].lin = unprojectScratch(scratch, lin);

    double pr = gw::participationRatio(lin);
    mem[src].prLin = pr;
    m.sumPR += pr;
    m.minPR = std::min(m.minPR, pr);
    m.maxPR = std::max(m.maxPR, pr);
    m.totalLocalNodes += n;
    m.totalBonds += (long long)scratch.cayleyBonds.size();
    m.totalBondVisits += bondVisits;
    m.maxLocalNodes = std::max(m.maxLocalNodes, n);
    m.maxBonds = std::max(m.maxBonds, (int)scratch.cayleyBonds.size());
    m.maxBondVisits = std::max(m.maxBondVisits, bondVisits);
    if (((int)g.adj[src].size() >= TOPK) && g.incident(src) >= WMIN && g.seen[src] >= SMIN) m.stable++;
  }

  void process(int node){
    g.t++; g.seen[node]++; g.relaxNode(node);
    for (int c : g.win) { g.relaxNode(c); g.touch(node, randomize ? randomEndpoint(node,c) : c); }
    g.closePlaquettes(node);
    g.win.push_back(node); if ((int)g.win.size() > WIN) g.win.pop_front();
    evolve(node); m.updates++;
  }
};

static void run(System& s, int stream, int uniqueEvery){
  std::mt19937 r(11);
  std::uniform_int_distribution<int> tt(0, TOPICS-1), ww(0, PER-1);
  for (int i = 0; i < stream;) {
    int tp = tt(r);
    s.g.win.clear();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) {
      int node = (uniqueEvery > 0 && i % uniqueEvery == 0) ? s.add() : tp * PER + ww(r);
      s.process(node);
    }
  }
}

static double eval(System& s){
  std::vector<PacketField> proto(TOPICS);
  for (int tp = 0; tp < TOPICS; ++tp) {
    for (int w = 0; w < PER/2; ++w) {
      const PacketField& f = s.mem[tp*PER+w].lin;
      for (int i = 0; i < (int)f.ids.size(); ++i) proto[tp].add(f.ids[i], f.vals[i]);
    }
  }
  int ok = 0, total = 0;
  for (int tp = 0; tp < TOPICS; ++tp) {
    for (int w = PER/2; w < PER; ++w) {
      int node = tp*PER+w, best = -1;
      double bs = -1;
      for (int c = 0; c < TOPICS; ++c) {
        double m = std::abs(packetOverlap(s.mem[node].lin, proto[c]));
        if (m > bs) { bs = m; best = c; }
      }
      ok += (best == tp);
      total++;
    }
  }
  return 100.0 * ok / total;
}

static int argInt(char** argv, int argc, int i, int fallback){ return i < argc ? std::atoi(argv[i]) : fallback; }

int main(int argc, char** argv){
  const int stream = argInt(argv, argc, 1, 1000000), uniqueEvery = argInt(argv, argc, 2, 7);
  auto t0 = std::chrono::steady_clock::now();
  System real(false);
  run(real, stream, uniqueEvery);
  auto t1 = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(t1 - t0).count();
  double acc = eval(real);
  Metrics& m = real.m;
  long edges = 0;
  for (auto& a : real.g.adj) edges += (long)a.size();
  edges /= 2;

  std::printf("=== GNNv2 production LINEAR token stream (g=0, no Kerr) ===\n");
  std::printf("params topics=%d per_topic=%d base_nodes=%d win=%d topk=%d\n", TOPICS, PER, BASE_NODES, WIN, TOPK);
  std::printf("eta=%.3f decay=%.3f edge_floor=%.1e wmin=%.1f inject=%.2f smin=%d steps=%d dt=%.3f stats=%s\n",
              ETA, DECAY, EDGE_FLOOR, WMIN, INJECT, SMIN, STEPS, DT,
              GW_LINEAR_COLLECT_FLOW_STATS ? "on" : "off");
  std::printf("stream=%d uniqueEvery=%d seed=11 burst=6 mode=packet+2hop+prepared_cayley+phase+decay+prune\n\n", stream, uniqueEvery);
  std::printf("updates=%lld nodes=%d edges=%ld stable_events=%lld\n", m.updates, (int)real.g.adj.size(), edges, m.stable);
  std::printf("PR avg=%.3f min=%.3f max=%.3f value_LINEAR=%.1f%%\n", m.sumPR/std::max(1LL,m.updates), (m.minPR==1e100?0.0:m.minPR), m.maxPR, acc);
  std::printf("local_nodes avg=%.2f max=%d   bonds avg=%.2f max=%d\n", (double)m.totalLocalNodes/std::max(1LL,m.updates), m.maxLocalNodes, (double)m.totalBonds/std::max(1LL,m.updates), m.maxBonds);
  std::printf("bond_visits total=%lld avg=%.2f max=%lld max_phase_speed=%.3f\n", m.totalBondVisits, (double)m.totalBondVisits/std::max(1LL,m.updates), m.maxBondVisits, m.maxPhaseSpeed);
  std::printf("train_sec=%.3f tokens_per_sec=%.0f peak_rss_mb=%.0f\n", sec, stream/std::max(sec,1e-12), peakRamMb());
  bool ok = m.updates == stream && acc >= 90.0 && m.maxLocalNodes >= 3;
  std::printf("RESULT : %d / 1  (%s)\n", ok ? 1 : 0, ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
