// probe_linear_stream
// ----------------------------------------------------------------------------
// REALISTIC linear token-stream control (g = 0, no Kerr) -- the speed/quality
// baseline the nonlinear streaming engine is measured against. Same stream
// generator and plastic-graph parameters as research/probe_streaming_compression.cpp
// (TOPICS/PER/WIN/TOPK, seed 11, bursts of 6, uniqueEvery 7), but a single field
// evolved with the LINEAR edge-flow only (edgeLocalKerrFlow at g = 0): no Kerr, no
// horizon, no bridges. Unit is TOKENS (this is the streaming engine, not the
// node-scaling engine).
//
// It deliberately includes the FULL realistic cost: stream generation, node
// creation, edge touch/decay/prune, plaquette phase, the 2-hop light cone, the
// project -> edge-flow -> unproject round trip (unordered_map), PR, and the
// classification check. That round trip -- NOT the graph update -- is the dominant
// cost.
//
// Three token-stream speed regimes (so the numbers are never confused):
//   ~1.2-1.4M tok/s : lightweight graph-stream only (no field evolution).
//   ~150-185k tok/s : THIS -- realistic linear field (project/edge-flow/unproject).
//   ~85-90k  tok/s  : nonlinear Kerr streaming (probe_streaming_compression).
// (Absolute tok/s is container-dependent; the ratios are the point. The 1,000,000
//  *nodes/s* figure is a different engine entirely -- the node-scaling
//  probe_sparse_scale.cpp -- never compare nodes/s to tokens/s.)
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

using gw::cd;
using gw::Vec;

constexpr int TOPICS = 3, PER = 24, WIN = 4, TOPK = 6;
constexpr double ETA = 1.0, DECAY = 0.99, EDGE_FLOOR = 1e-3, WMIN = 8.0, INJECT = 0.35;
constexpr int SMIN = 4, STEPS = 2;
constexpr double DT = 0.3;   // g = 0 -> linear edge-flow only (no Kerr)

static double peakRamMb() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
#endif
  return -1.0;
}

struct Edge { double w = 0; double phase = 0; int last = 0; };
struct Mem { std::unordered_map<int, cd> lin; double prLin = 1.0; };

struct Graph {
  std::vector<std::unordered_map<int, Edge>> adj; std::vector<int> seen; std::deque<int> win; int t = 0;
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

static unsigned hash3(int a, int b, int c){ unsigned x = 2166136261u; x = (x ^ (unsigned)a) * 16777619u; x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u; x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u; return x; }

static std::vector<int> hood(const Graph& g, int src){
  std::vector<int> nodes = {src}; std::unordered_map<int,int> seen; seen[src] = 0;
  for (int hop = 0; hop < 2; ++hop) { int sz = (int)nodes.size(); for (int h = 0; h < sz; ++h) for (auto& kv : g.adj[nodes[h]]) if (!seen.count(kv.first)) { seen[kv.first] = (int)nodes.size(); nodes.push_back(kv.first); } }
  return nodes;
}
static Vec project(const std::unordered_map<int, cd>& f, const std::unordered_map<int,int>& idx, int n, int src){
  Vec psi(n, cd(0,0)); for (auto& kv : f) { auto it = idx.find(kv.first); if (it != idx.end()) psi[it->second] += kv.second; }
  psi[idx.at(src)] += cd(INJECT, 0); gw::normalizeInPlace(psi); return psi;
}
static std::unordered_map<int, cd> unproject(const std::vector<int>& nodes, const Vec& psi){
  std::unordered_map<int, cd> out; for (int i = 0; i < (int)nodes.size(); ++i) if (std::norm(psi[i]) > 1e-14) out[nodes[i]] = psi[i]; return out;
}
static std::vector<gw::SparseBond> bondsInLightCone(const Graph& g, const std::vector<int>& nodes, const std::unordered_map<int,int>& idx){
  std::vector<gw::SparseBond> bonds;
  for (int i = 0; i < (int)nodes.size(); ++i) for (auto& kv : g.adj[nodes[i]]) {
    int u = nodes[i]; auto it = idx.find(kv.first);
    if (it != idx.end() && i < it->second) bonds.push_back({i, it->second, kv.second.w, g.orientedPhase(u, kv.first)});
  }
  return bonds;
}
static cd overlap(const std::unordered_map<int, cd>& a, const std::unordered_map<int, cd>& b){ cd s(0,0); for (auto& kv : a) { auto it = b.find(kv.first); if (it != b.end()) s += std::conj(kv.second) * it->second; } return s; }

// metrics averaged over ALL updates (the realistic per-token cost), matching the
// reference realistic-linear test.
struct Metrics {
  long long updates = 0, stable = 0, totalLocalNodes = 0, totalBonds = 0, totalBondVisits = 0;
  int maxLocalNodes = 0, maxBonds = 0; long long maxBondVisits = 0;
  double sumPR = 0, minPR = 1e100, maxPR = 0, maxPhaseSpeed = 0;
};

struct System {
  Graph g; std::vector<Mem> mem; bool randomize = false; Metrics m;
  explicit System(bool r) : randomize(r) { for (int i = 0; i < TOPICS * PER; ++i) add(); }
  int add(){ int id = g.add(); mem.push_back({}); return id; }
  int randomEndpoint(int a, int b) const { int n = (int)g.adj.size(); int r = (int)(hash3(a,b,g.t) % (unsigned)n); return r == a ? (r + 1) % n : r; }
  void evolve(int src){
    auto nodes = hood(g, src); int n = (int)nodes.size();
    if (n < 3) { mem[src].lin[src] = cd(1,0); return; }
    std::unordered_map<int,int> idx; for (int i = 0; i < n; ++i) idx[nodes[i]] = i;
    auto bonds = bondsInLightCone(g, nodes, idx);
    Vec lin = project(mem[src].lin, idx, n, src);
    gw::LocalFlowStats st;
    lin = gw::edgeLocalKerrFlow(lin, bonds, DT, 0.0, STEPS, &st);   // g = 0 -> linear edge flow (symmetric split)
    gw::normalizeInPlace(lin);
    mem[src].lin = unproject(nodes, lin);
    double pr = gw::participationRatio(lin); mem[src].prLin = pr;
    m.sumPR += pr; m.minPR = std::min(m.minPR, pr); m.maxPR = std::max(m.maxPR, pr);
    m.totalLocalNodes += n; m.totalBonds += (long long)bonds.size(); m.totalBondVisits += st.bond_visits;
    m.maxLocalNodes = std::max(m.maxLocalNodes, n); m.maxBonds = std::max(m.maxBonds, (int)bonds.size()); m.maxBondVisits = std::max(m.maxBondVisits, st.bond_visits);
    m.maxPhaseSpeed = std::max(m.maxPhaseSpeed, st.max_bond_speed);
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
  std::mt19937 r(11); std::uniform_int_distribution<int> tt(0, TOPICS-1), ww(0, PER-1);
  for (int i = 0; i < stream;) { int tp = tt(r); s.g.win.clear();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) { int node = (uniqueEvery > 0 && i % uniqueEvery == 0) ? s.add() : tp * PER + ww(r); s.process(node); } }
}
static double eval(System& s){
  std::vector<std::unordered_map<int,cd>> proto(TOPICS);
  for (int tp = 0; tp < TOPICS; ++tp) for (int w = 0; w < PER/2; ++w) for (auto& kv : s.mem[tp*PER+w].lin) proto[tp][kv.first] += kv.second;
  int ok = 0, total = 0; for (int tp = 0; tp < TOPICS; ++tp) for (int w = PER/2; w < PER; ++w) { int node = tp*PER+w, best = -1; double bs = -1; for (int c = 0; c < TOPICS; ++c) { double m = std::abs(overlap(s.mem[node].lin, proto[c])); if (m > bs) { bs = m; best = c; } } ok += (best == tp); total++; }
  return 100.0 * ok / total;
}

static int argInt(char** argv, int argc, int i, int fallback){ return i < argc ? std::atoi(argv[i]) : fallback; }

int main(int argc, char** argv){
  const int stream = argInt(argv, argc, 1, 1000000), uniqueEvery = argInt(argv, argc, 2, 7);
  auto t0 = std::chrono::steady_clock::now();
  System real(false); run(real, stream, uniqueEvery);
  auto t1 = std::chrono::steady_clock::now();
  double sec = std::chrono::duration<double>(t1 - t0).count();
  double acc = eval(real);
  Metrics& m = real.m;
  long edges = 0; for (auto& a : real.g.adj) edges += (long)a.size(); edges /= 2;

  std::printf("=== GNNv2 realistic LINEAR token stream (g=0, no Kerr) ===\n");
  std::printf("params topics=%d per_topic=%d base_nodes=%d win=%d topk=%d\n", TOPICS, PER, TOPICS*PER, WIN, TOPK);
  std::printf("eta=%.3f decay=%.3f edge_floor=%.1e wmin=%.1f inject=%.2f smin=%d steps=%d dt=%.3f\n", ETA, DECAY, EDGE_FLOOR, WMIN, INJECT, SMIN, STEPS, DT);
  std::printf("stream=%d uniqueEvery=%d seed=11 burst=6 mode=unordered_map+2hop+phase+decay+prune\n\n", stream, uniqueEvery);
  std::printf("updates=%lld nodes=%d edges=%ld stable_events=%lld\n", m.updates, (int)real.g.adj.size(), edges, m.stable);
  std::printf("PR avg=%.3f min=%.3f max=%.3f value_LINEAR=%.1f%%\n", m.sumPR/std::max(1LL,m.updates), (m.minPR==1e100?0.0:m.minPR), m.maxPR, acc);
  std::printf("local_nodes avg=%.2f max=%d   bonds avg=%.2f max=%d\n", (double)m.totalLocalNodes/std::max(1LL,m.updates), m.maxLocalNodes, (double)m.totalBonds/std::max(1LL,m.updates), m.maxBonds);
  std::printf("bond_visits total=%lld avg=%.2f max=%lld max_phase_speed=%.3f\n", m.totalBondVisits, (double)m.totalBondVisits/std::max(1LL,m.updates), m.maxBondVisits, m.maxPhaseSpeed);
  std::printf("train_sec=%.3f tokens_per_sec=%.0f peak_rss_mb=%.0f\n", sec, stream/std::max(sec,1e-12), peakRamMb());
  bool ok = m.updates == stream && acc >= 90.0 && m.maxPhaseSpeed < 8.0;
  std::printf("RESULT : %d / 1  (%s)\n", ok ? 1 : 0, ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
