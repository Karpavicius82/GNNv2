// probe_streaming_compression
// ----------------------------------------------------------------------------
// Streaming compression smoke probe using the GLOBAL substrate nonlinear physics.
// Compression is not a separate operation: every token event evolves
// the local field with matrix-free edge current + Kerr phase pressure.
// A horizon is only a detector condition over the already-evolved field.
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

#ifndef GW_USE_RATIONAL_KERR_STREAM
#define GW_USE_RATIONAL_KERR_STREAM 1
#endif
#ifndef GW_STREAM_COLLECT_FLOW_STATS
#define GW_STREAM_COLLECT_FLOW_STATS 0
#endif
#ifndef GW_STREAM_RENORMALIZE_AFTER_FLOW
#define GW_STREAM_RENORMALIZE_AFTER_FLOW 1
#endif
#ifndef GW_STREAM_PROFILE
#define GW_STREAM_PROFILE 0
#endif
#ifndef GW_STREAM_PACKET_MEM
#define GW_STREAM_PACKET_MEM 1
#endif
#ifndef GW_STREAM_PREPARED_CAYLEY
#define GW_STREAM_PREPARED_CAYLEY 1
#endif

#if GW_STREAM_PROFILE
struct StreamProfile {
  long long process_ns = 0, graph_ns = 0, evolve_ns = 0, bridge_ns = 0;
  long long hood_ns = 0, index_ns = 0, bonds_ns = 0, project_ns = 0, flow_ns = 0;
  long long postnorm_ns = 0, unproject_ns = 0, sense_ns = 0, pr_ns = 0, stable_ns = 0;
  long long eval_ns = 0;
  long long samples = 0, nodes_sum = 0, bonds_sum = 0;
  long long lin_support_in_sum = 0, ker_support_in_sum = 0;
  long long lin_support_out_sum = 0, ker_support_out_sum = 0, sense_support_sum = 0;
};
static StreamProfile gProf;
static long long profileNowNs(){
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
#define GW_PROFILE_START(name) const long long name = profileNowNs()
#define GW_PROFILE_ADD(field, start) gProf.field += profileNowNs() - (start)
#else
#define GW_PROFILE_START(name)
#define GW_PROFILE_ADD(field, start)
#endif

static double peakRamMb(){
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) return (double)pmc.PeakWorkingSetSize / (1024.0*1024.0);
#endif
  return -1.0;
}

constexpr int TOPICS = 3, PER = 24, WIN = 4, TOPK = 6;
constexpr double ETA = 1.0, DECAY = 0.99, EDGE_FLOOR = 1e-3, WMIN = 8.0, INJECT = 0.35;
constexpr int SMIN = 4, STEPS = 2;
constexpr double DT = 0.3, GK = 7.0;
constexpr int BASE_NODES = TOPICS * PER, BRIDGE_TOPK = 1;
constexpr double BRIDGE_MIN_COH = 0.20;

struct Edge { double w = 0; double phase = 0; int last = 0; };
struct BridgeBond { int a = 0, b = 0; double w = 0; double phase = 0; };
#if GW_STREAM_PACKET_MEM
struct PacketField {
  std::vector<int> ids;
  std::vector<cd> vals;
  size_t size() const { return ids.size(); }
  void clear(){ ids.clear(); vals.clear(); }
  void reserve(size_t n){ ids.reserve(n); vals.reserve(n); }
  int find(int id) const { for (int i = 0; i < (int)ids.size(); ++i) if (ids[i] == id) return i; return -1; }
  cd get(int id) const { int i = find(id); return i >= 0 ? vals[i] : cd(0,0); }
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
struct SenseField {
  std::vector<int> ids;
  std::vector<double> vals;
  size_t size() const { return ids.size(); }
  void clear(){ ids.clear(); vals.clear(); }
  void reserve(size_t n){ ids.reserve(n); vals.reserve(n); }
  int find(int id) const { for (int i = 0; i < (int)ids.size(); ++i) if (ids[i] == id) return i; return -1; }
  double get(int id) const { int i = find(id); return i >= 0 ? vals[i] : 0.0; }
  void set(int id, double v){
    int i = find(id);
    if (i >= 0) vals[i] = v;
    else { ids.push_back(id); vals.push_back(v); }
  }
  void pushKnownUnique(int id, double v){ ids.push_back(id); vals.push_back(v); }
};
struct Mem { PacketField lin, ker; SenseField sense; double prLin = 1, prKer = 1; };
#else
struct Mem { std::unordered_map<int, cd> lin, ker; std::unordered_map<int, double> sense; double prLin = 1, prKer = 1; };
#endif
struct BridgeStats {
  long long materialized = 0, trueBridge = 0, falseBridge = 0;
  double sameOverlap = 0, crossOverlap = 0, maxSame = 0, maxCross = 0;
  int samePairs = 0, crossPairs = 0;
};
struct Graph {
  std::vector<std::unordered_map<int, Edge>> adj; std::vector<BridgeBond> bridges; std::vector<int> seen; std::deque<int> win; int t = 0;
  int add(){ adj.push_back({}); seen.push_back(0); return (int)adj.size() - 1; }
  double incident(int a) const { double s = 0; for (auto& kv : adj[a]) s += kv.second.w; return s; }
  double meanBond(int a) const { return adj[a].empty() ? 0.0 : incident(a) / (double)adj[a].size(); }
  void decay(int a, int b){ auto it = adj[a].find(b); if (it == adj[a].end()) return; int dt = t - it->second.last; if (dt > 0) { it->second.w *= std::pow(DECAY, dt); it->second.last = t; } }
  void eraseEdge(int a, int b){ adj[a].erase(b); adj[b].erase(a); }
  static double wrapPhase(double x){ while (x > gw::kPi) x -= 2.0 * gw::kPi; while (x < -gw::kPi) x += 2.0 * gw::kPi; return x; }
  bool hasEdge(int a, int b) const { return adj[a].find(b) != adj[a].end(); }
  bool hasBridge(int a, int b) const { for (const auto& e : bridges) if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) return true; return false; }
  int bridgeDegree(int a) const { int d = 0; for (const auto& e : bridges) if (e.a == a || e.b == a) d++; return d; }
  double orientedPhase(int u, int v) const { auto it = adj[u].find(v); if (it == adj[u].end()) return 0.0; return u < v ? it->second.phase : -it->second.phase; }
  double orientedBridgePhase(int u, int v) const { for (const auto& e : bridges) { if (e.a == u && e.b == v) return e.phase; if (e.a == v && e.b == u) return -e.phase; } return 0.0; }
  void addSignedPhase(int u, int v, double dphi){ auto it = adj[u].find(v); if (it == adj[u].end()) return; Edge e = it->second; double sign = u < v ? 1.0 : -1.0; e.phase = std::clamp(wrapPhase(e.phase + sign * dphi), -1.4, 1.4); adj[u][v] = e; adj[v][u] = e; }
  void addPlaquetteFlux(int a, int b, int c){ constexpr double dphi = 0.018; addSignedPhase(a, b, dphi / 3.0); addSignedPhase(b, c, dphi / 3.0); addSignedPhase(c, a, dphi / 3.0); }
  void closePlaquettes(int node){ for (int j = 1; j < (int)win.size(); ++j) for (int i = 0; i < j; ++i) { int a = win[i], b = win[j]; if (hasEdge(a, b) && hasEdge(a, node) && hasEdge(b, node)) addPlaquetteFlux(a, b, node); } }
  void relaxNode(int a){ std::vector<int> dead; for (auto& kv : adj[a]) { int b = kv.first; int dt = t - kv.second.last; if (dt > 0) { kv.second.w *= std::pow(DECAY, dt); kv.second.last = t; auto back = adj[b].find(a); if (back != adj[b].end()) back->second = kv.second; } if (kv.second.w < EDGE_FLOOR) dead.push_back(b); } for (int b : dead) eraseEdge(a, b); }
  void prune(int a){ while ((int)adj[a].size() > TOPK) { int wk = -1; double mn = 1e100; for (auto& kv : adj[a]) if (kv.second.w < mn) { mn = kv.second.w; wk = kv.first; } eraseEdge(a, wk); } }
  void touch(int a, int b){ if (a == b) return; decay(a, b); decay(b, a); adj[a][b].w += ETA; adj[a][b].last = t; adj[b][a] = adj[a][b]; if ((int)adj[a].size() > TOPK) prune(a); if ((int)adj[b].size() > TOPK) prune(b); }
  void setBridge(int a, int b, double w, double phaseAB){
    if (a == b || hasBridge(a, b)) return;
    if (a > b) { std::swap(a, b); phaseAB = -phaseAB; }
    bridges.push_back({a, b, w, wrapPhase(phaseAB)});
  }
};

static unsigned hash3(int a, int b, int c){ unsigned x = 2166136261u; x = (x ^ (unsigned)a) * 16777619u; x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u; x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u; return x; }

static std::vector<int> hood(const Graph& g, int src, bool useBridges = false){
  std::vector<int> nodes = {src}; std::unordered_map<int,int> seen; seen[src] = 0;
  for (int hop = 0; hop < 2; ++hop) {
    int sz = (int)nodes.size();
    for (int h = 0; h < sz; ++h) {
      int u = nodes[h];
      for (auto& kv : g.adj[u]) if (!seen.count(kv.first)) { seen[kv.first] = (int)nodes.size(); nodes.push_back(kv.first); }
    }
  }
  if (useBridges) {
    for (const auto& e : g.bridges) {
      int v = e.a == src ? e.b : (e.b == src ? e.a : -1);
      if (v >= 0 && !seen.count(v)) { seen[v] = (int)nodes.size(); nodes.push_back(v); }
    }
  }
  return nodes;
}

#if !GW_STREAM_PACKET_MEM
static cd feltOverlap(const Mem& a, const Mem& b){
  cd z(0,0);
  for (auto& kv : a.ker) {
    auto ib = b.ker.find(kv.first); if (ib == b.ker.end()) continue;
    auto sa = a.sense.find(kv.first), sb = b.sense.find(kv.first);
    if (sa == a.sense.end() || sb == b.sense.end()) continue;
    z += std::conj(kv.second) * ib->second * std::sqrt(sa->second * sb->second);
  }
  return z;
}

static double feltPower(const Mem& m){
  double s = 0;
  for (auto& kv : m.ker) { auto it = m.sense.find(kv.first); if (it != m.sense.end()) s += std::norm(kv.second) * it->second; }
  return s;
}

static double feltCoherence(const Mem& a, const Mem& b, cd* raw = nullptr){
  cd z = feltOverlap(a, b); if (raw) *raw = z;
  return std::abs(z) / (std::sqrt(feltPower(a) * feltPower(b)) + 1e-300);
}

#endif

#if GW_STREAM_PACKET_MEM
static void setFieldSingle(PacketField& f, int id, cd v){ f.clear(); f.set(id, v); }
static void addFieldToPsi(Vec& psi, const PacketField& f, const std::unordered_map<int,int>& idx){
  for (int i = 0; i < (int)f.ids.size(); ++i) { auto it = idx.find(f.ids[i]); if (it != idx.end()) psi[it->second] += f.vals[i]; }
}
static cd packetOverlap(const PacketField& a, const PacketField& b){
  cd z(0,0);
  for (int i = 0; i < (int)a.ids.size(); ++i) {
    int j = b.find(a.ids[i]);
    if (j >= 0) z += std::conj(a.vals[i]) * b.vals[j];
  }
  return z;
}
static cd feltOverlap(const Mem& a, const Mem& b){
  cd z(0,0);
  for (int i = 0; i < (int)a.ker.ids.size(); ++i) {
    const int id = a.ker.ids[i];
    const int jb = b.ker.find(id);
    if (jb < 0) continue;
    const double sa = a.sense.get(id), sb = b.sense.get(id);
    if (sa <= 0.0 || sb <= 0.0) continue;
    z += std::conj(a.ker.vals[i]) * b.ker.vals[jb] * std::sqrt(sa * sb);
  }
  return z;
}
static double feltPower(const Mem& m){
  double s = 0.0;
  for (int i = 0; i < (int)m.ker.ids.size(); ++i) {
    const double sense = m.sense.get(m.ker.ids[i]);
    if (sense > 0.0) s += std::norm(m.ker.vals[i]) * sense;
  }
  return s;
}
static double feltCoherence(const Mem& a, const Mem& b, cd* raw = nullptr){
  cd z = feltOverlap(a, b); if (raw) *raw = z;
  return std::abs(z) / (std::sqrt(feltPower(a) * feltPower(b)) + 1e-300);
}
#else
static void setFieldSingle(std::unordered_map<int, cd>& f, int id, cd v){ f.clear(); f[id] = v; }
static void addFieldToPsi(Vec& psi, const std::unordered_map<int, cd>& f, const std::unordered_map<int,int>& idx){
  for (auto& kv : f) { auto it = idx.find(kv.first); if (it != idx.end()) psi[it->second] += kv.second; }
}
#endif

struct Evo { bool horizon = false, stable = false; double prLin = 1, prKer = 1, phaseSpeed = 0; long long ops = 0; };

struct StreamScratch {
  std::vector<int> mark, idx, nodes;
  std::vector<gw::SparseBond> bonds;
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
  std::vector<gw::LocalCayleyFlowBond> cayleyBonds;
#endif
  int stamp = 1;
  void begin(int n){
    if ((int)mark.size() < n) { mark.resize(n, 0); idx.resize(n, -1); }
    if (++stamp == 0x3fffffff) { std::fill(mark.begin(), mark.end(), 0); stamp = 1; }
    nodes.clear(); bonds.clear();
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
    cayleyBonds.clear();
#endif
  }
  bool add(int v){
    if (mark[v] == stamp) return false;
    mark[v] = stamp; idx[v] = (int)nodes.size(); nodes.push_back(v); return true;
  }
  int local(int v) const { return mark[v] == stamp ? idx[v] : -1; }
  size_t flowBondCount() const {
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
    return cayleyBonds.size();
#else
    return bonds.size();
#endif
  }
};

static std::vector<gw::SparseBond> bondsInLightCone(const Graph& g, const std::vector<int>& nodes, const std::unordered_map<int,int>& idx, bool useBridges = false){
  std::vector<gw::SparseBond> bonds;
  for (int i = 0; i < (int)nodes.size(); ++i) for (auto& kv : g.adj[nodes[i]]) {
    int u = nodes[i];
    auto it = idx.find(kv.first);
    if (it != idx.end() && i < it->second) bonds.push_back({i, it->second, kv.second.w, g.orientedPhase(u, kv.first)});
  }
  if (useBridges) {
    for (const auto& e : g.bridges) {
      auto ia = idx.find(e.a), ib = idx.find(e.b);
      if (ia != idx.end() && ib != idx.end()) {
        if (ia->second < ib->second) bonds.push_back({ia->second, ib->second, e.w, g.orientedBridgePhase(e.a, e.b)});
        else if (ib->second < ia->second) bonds.push_back({ib->second, ia->second, e.w, g.orientedBridgePhase(e.b, e.a)});
      }
    }
  }
  return bonds;
}

static void hoodScratch(const Graph& g, int src, StreamScratch& scratch, bool useBridges = false){
  scratch.begin((int)g.adj.size());
  scratch.add(src);
  for (int hop = 0; hop < 2; ++hop) {
    int sz = (int)scratch.nodes.size();
    for (int h = 0; h < sz; ++h) {
      int u = scratch.nodes[h];
      for (auto& kv : g.adj[u]) scratch.add(kv.first);
    }
  }
  if (useBridges) {
    for (const auto& e : g.bridges) {
      int v = e.a == src ? e.b : (e.b == src ? e.a : -1);
      if (v >= 0) scratch.add(v);
    }
  }
}

static void bondsInLightConeScratch(const Graph& g, StreamScratch& scratch, bool useBridges = false){
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
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
  if (useBridges) {
    for (const auto& e : g.bridges) {
      const int ia = scratch.local(e.a), ib = scratch.local(e.b);
      if (ia >= 0 && ib >= 0) {
        if (ia < ib) {
          maxW = std::max(maxW, std::abs(e.w));
          scratch.cayleyBonds.push_back({ia, ib, e.w, 1.0, 0.0, gw::bondPhase(g.orientedBridgePhase(e.a, e.b))});
        } else if (ib < ia) {
          maxW = std::max(maxW, std::abs(e.w));
          scratch.cayleyBonds.push_back({ib, ia, e.w, 1.0, 0.0, gw::bondPhase(g.orientedBridgePhase(e.b, e.a))});
        }
      }
    }
  }
  const double inv = maxW > 1e-300 ? 1.0 / maxW : 0.0;
  for (auto& e : scratch.cayleyBonds) e = gw::makeLocalCayleyFlowBond(e.a, e.b, e.w, e.phase_u, DT, inv);
#else
  scratch.bonds.clear();
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) {
    const int u = scratch.nodes[i];
    for (auto& kv : g.adj[u]) {
      const int j = scratch.local(kv.first);
      if (j >= 0 && i < j) scratch.bonds.push_back({i, j, kv.second.w, g.orientedPhase(u, kv.first)});
    }
  }
  if (useBridges) {
    for (const auto& e : g.bridges) {
      const int ia = scratch.local(e.a), ib = scratch.local(e.b);
      if (ia >= 0 && ib >= 0) {
        if (ia < ib) scratch.bonds.push_back({ia, ib, e.w, g.orientedBridgePhase(e.a, e.b)});
        else if (ib < ia) scratch.bonds.push_back({ib, ia, e.w, g.orientedBridgePhase(e.b, e.a)});
      }
    }
  }
#endif
}

static Vec projectScratch(
#if GW_STREAM_PACKET_MEM
    const PacketField& f,
#else
    const std::unordered_map<int, cd>& f,
#endif
    const StreamScratch& scratch, int src){
  Vec psi((int)scratch.nodes.size(), cd(0,0));
#if GW_STREAM_PACKET_MEM
  for (int i = 0; i < (int)f.ids.size(); ++i) {
    const int j = f.ids[i] < (int)scratch.mark.size() ? scratch.local(f.ids[i]) : -1;
    if (j >= 0) psi[j] += f.vals[i];
  }
#else
  for (auto& kv : f) {
    const int j = kv.first < (int)scratch.mark.size() ? scratch.local(kv.first) : -1;
    if (j >= 0) psi[j] += kv.second;
  }
#endif
  psi[scratch.local(src)] += cd(INJECT, 0);
  gw::normalizeInPlace(psi);
  return psi;
}

#if GW_STREAM_PACKET_MEM
static PacketField unprojectScratch(const StreamScratch& scratch, const Vec& psi){
  PacketField out;
  out.reserve(scratch.nodes.size());
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) if (std::norm(psi[i]) > 1e-14) out.pushKnownUnique(scratch.nodes[i], psi[i]);
  return out;
}
#else
static std::unordered_map<int, cd> unprojectScratch(const StreamScratch& scratch, const Vec& psi){
  std::unordered_map<int, cd> out;
  out.reserve(scratch.nodes.size());
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) if (std::norm(psi[i]) > 1e-14) out[scratch.nodes[i]] = psi[i];
  return out;
}
#endif

#if GW_STREAM_PACKET_MEM
static SenseField feelMotionScratch(const StreamScratch& scratch, const Vec& before, const Vec& after){
  SenseField out;
  out.reserve(scratch.nodes.size());
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) {
    double move = std::abs(after[i] - before[i]);
    double phase = std::abs(std::arg(std::conj(before[i]) * after[i])) * std::sqrt(std::norm(before[i]) * std::norm(after[i]));
    double sense = move + phase;
    if (sense > 1e-14) out.pushKnownUnique(scratch.nodes[i], sense);
  }
  return out;
}
#else
static std::unordered_map<int, double> feelMotionScratch(const StreamScratch& scratch, const Vec& before, const Vec& after){
  std::unordered_map<int, double> out;
  out.reserve(scratch.nodes.size());
  for (int i = 0; i < (int)scratch.nodes.size(); ++i) {
    double move = std::abs(after[i] - before[i]);
    double phase = std::abs(std::arg(std::conj(before[i]) * after[i])) * std::sqrt(std::norm(before[i]) * std::norm(after[i]));
    double sense = move + phase;
    if (sense > 1e-14) out[scratch.nodes[i]] = sense;
  }
  return out;
}
#endif

static Evo evolve(Graph& g, int src, Mem& m, StreamScratch& scratch){
  GW_PROFILE_START(tEvolve);
  Evo e;
  GW_PROFILE_START(tHood);
  hoodScratch(g, src, scratch);
  GW_PROFILE_ADD(hood_ns, tHood);
  int n = (int)scratch.nodes.size();
  #if GW_STREAM_PROFILE
  gProf.samples++;
  gProf.nodes_sum += n;
  gProf.lin_support_in_sum += (long long)m.lin.size();
  gProf.ker_support_in_sum += (long long)m.ker.size();
  #endif
  if (n < 3) { setFieldSingle(m.lin, src, cd(1,0)); setFieldSingle(m.ker, src, cd(1,0)); GW_PROFILE_ADD(evolve_ns, tEvolve); return e; }
  GW_PROFILE_START(tIndex);
  // Indexing is stamped in StreamScratch while the light cone is discovered.
  GW_PROFILE_ADD(index_ns, tIndex);
  GW_PROFILE_START(tBonds);
  bondsInLightConeScratch(g, scratch, false);
  #if GW_STREAM_PROFILE
  gProf.bonds_sum += (long long)scratch.flowBondCount();
  #endif
  GW_PROFILE_ADD(bonds_ns, tBonds);
  GW_PROFILE_START(tProject);
  Vec lin = projectScratch(m.lin, scratch, src), ker = projectScratch(m.ker, scratch, src);
  Vec kerBefore;
  if (src < BASE_NODES) kerBefore = ker;
  GW_PROFILE_ADD(project_ns, tProject);
#if GW_STREAM_COLLECT_FLOW_STATS
  gw::LocalFlowStats kerStats;
#endif
  GW_PROFILE_START(tFlow);
#if GW_USE_RATIONAL_KERR_STREAM
#if GW_STREAM_COLLECT_FLOW_STATS
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
  gw::edgeLocalRationalKerrFlowPairPrepared(lin, ker, scratch.cayleyBonds, DT, GK, STEPS, &kerStats);
#else
  gw::edgeLocalRationalKerrFlowPair(lin, ker, scratch.bonds, DT, GK, STEPS, &kerStats);
#endif
#else
#if GW_STREAM_PREPARED_CAYLEY && GW_USE_RATIONAL_KERR_STREAM
  gw::edgeLocalRationalKerrFlowPairPrepared(lin, ker, scratch.cayleyBonds, DT, GK, STEPS);
#else
  gw::edgeLocalRationalKerrFlowPair(lin, ker, scratch.bonds, DT, GK, STEPS);
#endif
#endif
#else
#if GW_STREAM_COLLECT_FLOW_STATS
  gw::edgeLocalKerrFlowPair(lin, ker, scratch.bonds, DT, GK, STEPS, &kerStats);
#else
  gw::edgeLocalKerrFlowPair(lin, ker, scratch.bonds, DT, GK, STEPS);
#endif
#endif
  GW_PROFILE_ADD(flow_ns, tFlow);
#if GW_STREAM_RENORMALIZE_AFTER_FLOW
  GW_PROFILE_START(tPostnorm);
  gw::normalizeInPlace(lin); gw::normalizeInPlace(ker);
  GW_PROFILE_ADD(postnorm_ns, tPostnorm);
#endif
  GW_PROFILE_START(tUnproject);
  m.lin = unprojectScratch(scratch, lin); m.ker = unprojectScratch(scratch, ker);
  #if GW_STREAM_PROFILE
  gProf.lin_support_out_sum += (long long)m.lin.size();
  gProf.ker_support_out_sum += (long long)m.ker.size();
  #endif
  GW_PROFILE_ADD(unproject_ns, tUnproject);
  GW_PROFILE_START(tSense);
  if (src < BASE_NODES) m.sense = feelMotionScratch(scratch, kerBefore, ker);
  else m.sense.clear();
  #if GW_STREAM_PROFILE
  gProf.sense_support_sum += (long long)m.sense.size();
  #endif
  GW_PROFILE_ADD(sense_ns, tSense);
  GW_PROFILE_START(tPr);
  m.prLin = e.prLin = gw::participationRatio(lin); m.prKer = e.prKer = gw::participationRatio(ker);
  GW_PROFILE_ADD(pr_ns, tPr);
#if GW_STREAM_COLLECT_FLOW_STATS
  e.ops = (long long)scratch.flowBondCount() * STEPS * 2 + kerStats.bond_visits;
  e.phaseSpeed = kerStats.max_bond_speed;
#else
  e.ops = (long long)scratch.flowBondCount() * STEPS * 4;
  e.phaseSpeed = 0.0;
#endif
  GW_PROFILE_START(tStable);
  e.stable = ((int)g.adj[src].size() >= TOPK) && g.incident(src) >= WMIN && g.seen[src] >= SMIN;
  // horizon = collapse beats dispersion: the nonlinear field is concentrated to less
  // than half the PR of its own linear (g=0) control. Scale-free, no magic cutoff.
  e.horizon = e.stable && e.prKer < 0.5 * e.prLin;
  GW_PROFILE_ADD(stable_ns, tStable);
  GW_PROFILE_ADD(evolve_ns, tEvolve);
  return e;
}

static int argInt(char** argv, int argc, int i, int fallback){ return i < argc ? std::atoi(argv[i]) : fallback; }
static int topicOf(int node){ return node >= 0 && node < BASE_NODES ? node / PER : -1; }

struct System {
  Graph g; std::vector<Mem> mem; std::vector<bool> novel, horizonHub;
  std::vector<unsigned char> measuredPair, bridgePair;
  StreamScratch scratch;
  BridgeStats bridgeStats;
  bool randomize = false;
  long long updates = 0, stableHorizons = 0, novelHorizons = 0, horizonSamples = 0, maxOps = 0;
  double maxPhaseSpeed = 0;
  double sumHLin = 0, sumHKer = 0;
  explicit System(bool r) : randomize(r) { for (int i = 0; i < TOPICS * PER; ++i) add(false); measuredPair.assign(BASE_NODES * BASE_NODES, 0); bridgePair.assign(BASE_NODES * BASE_NODES, 0); }
  int add(bool n){ int id = g.add(); mem.push_back({}); novel.push_back(n); horizonHub.push_back(false); return id; }
  int randomEndpoint(int a, int b) const { int n = (int)g.adj.size(); int r = (int)(hash3(a,b,g.t) % (unsigned)n); return r == a ? (r + 1) % n : r; }
  int pairKey(int a, int b) const { if (a > b) std::swap(a, b); return a * BASE_NODES + b; }
  void observeCandidate(int a, int b, double coh){
    int key = pairKey(a, b); if (measuredPair[key]) return; measuredPair[key] = 1;
    if (topicOf(a) == topicOf(b)) { bridgeStats.sameOverlap += coh; bridgeStats.samePairs++; bridgeStats.maxSame = std::max(bridgeStats.maxSame, coh); }
    else { bridgeStats.crossOverlap += coh; bridgeStats.crossPairs++; bridgeStats.maxCross = std::max(bridgeStats.maxCross, coh); }
  }
  double bridgeImpedance(int a, int b, double coh) const { return std::sqrt(g.meanBond(a) * g.meanBond(b)) * coh * coh; }
  bool pressureCompatible(int a, int b, double w, double phaseAB) const {
    std::vector<int> nodes = hood(g, a, false); std::unordered_map<int,int> idx;
    for (int i = 0; i < (int)nodes.size(); ++i) idx[nodes[i]] = i;
    std::vector<int> hb = hood(g, b, false);
    for (int x : hb) if (!idx.count(x)) { idx[x] = (int)nodes.size(); nodes.push_back(x); }
    if (!idx.count(a) || !idx.count(b)) return false;
    std::vector<gw::SparseBond> bonds = bondsInLightCone(g, nodes, idx, false), bridged = bonds;
    int ia = idx[a], ib = idx[b];
    bridged.push_back({ia, ib, w, ia < ib ? phaseAB : -phaseAB});
    Vec psi(nodes.size(), cd(0,0));
    addFieldToPsi(psi, mem[a].ker, idx);
    addFieldToPsi(psi, mem[b].ker, idx);
    if (gw::power(psi) < 1e-300) psi[ia] = cd(1,0);
    gw::normalizeInPlace(psi);
    Vec noBridge = gw::edgeLocalKerrFlow(psi, bonds, DT, GK, STEPS);
    Vec withBridge = gw::edgeLocalKerrFlow(psi, bridged, DT, GK, STEPS);
    return gw::participationRatio(withBridge) <= gw::participationRatio(noBridge) + 1e-9;
  }
  int bestPartner(int a, cd* bestOverlap, double* bestCoh, bool observe){
    int best = -1; cd zBest(0,0); double cBest = BRIDGE_MIN_COH;
    for (int b = 0; b < BASE_NODES; ++b) {
      if (b == a || !horizonHub[b] || g.bridgeDegree(b) >= BRIDGE_TOPK) continue;
      int key = pairKey(a, b); if (bridgePair[key] || g.hasBridge(a, b)) continue;
      cd z(0,0); double coh = feltCoherence(mem[a], mem[b], &z);
      if (observe) observeCandidate(a, b, coh);
      if (coh > cBest) { best = b; cBest = coh; zBest = z; }
    }
    if (bestOverlap) *bestOverlap = zBest; if (bestCoh) *bestCoh = cBest; return best;
  }
  void tryMaterializeBridge(int a, bool horizon){
    if (!horizon || a >= BASE_NODES || novel[a]) return;
    horizonHub[a] = true; if (g.bridgeDegree(a) >= BRIDGE_TOPK) return;
    cd z(0,0); double coh = 0; int b = bestPartner(a, &z, &coh, true); if (b < 0) return;
    cd rz(0,0); double rcoh = 0; if (bestPartner(b, &rz, &rcoh, false) != a) return;
    double w = bridgeImpedance(a, b, coh); if (w <= 1e-12) return;
    double phaseAB = -std::arg(z); if (!pressureCompatible(a, b, w, phaseAB)) return;
    g.setBridge(a, b, w, phaseAB); bridgePair[pairKey(a, b)] = 1; bridgeStats.materialized++;
    if (topicOf(a) >= 0 && topicOf(a) == topicOf(b)) bridgeStats.trueBridge++; else bridgeStats.falseBridge++;
  }
  void process(int node){
    GW_PROFILE_START(tProcess);
    GW_PROFILE_START(tGraph);
    g.t++; g.seen[node]++; g.relaxNode(node); for (int c : g.win) { g.relaxNode(c); g.touch(node, randomize ? randomEndpoint(node,c) : c); }
    g.closePlaquettes(node);
    g.win.push_back(node); if ((int)g.win.size() > WIN) g.win.pop_front();
    GW_PROFILE_ADD(graph_ns, tGraph);
    Evo e = evolve(g, node, mem[node], scratch); updates++; maxOps = std::max(maxOps, e.ops);
    maxPhaseSpeed = std::max(maxPhaseSpeed, e.phaseSpeed);
    if (e.horizon) { horizonSamples++; sumHLin += e.prLin; sumHKer += e.prKer; if (novel[node]) novelHorizons++; else stableHorizons++; }
    GW_PROFILE_START(tBridge);
    tryMaterializeBridge(node, e.horizon);
    GW_PROFILE_ADD(bridge_ns, tBridge);
    GW_PROFILE_ADD(process_ns, tProcess);
  }
};

static void run(System& s, int stream, int uniqueEvery){
  std::mt19937 r(11); std::uniform_int_distribution<int> tt(0, TOPICS-1), ww(0, PER-1);
  for (int i = 0; i < stream;) { int tp = tt(r); s.g.win.clear(); for (int b = 0; b < 6 && i < stream; ++b, ++i) { int node = (uniqueEvery > 0 && i % uniqueEvery == 0) ? s.add(true) : tp * PER + ww(r); s.process(node); } }
}

static double eval(System& s){
  GW_PROFILE_START(tEval);
#if GW_STREAM_PACKET_MEM
  std::vector<PacketField> proto(TOPICS);
  for (int tp = 0; tp < TOPICS; ++tp) {
    for (int w = 0; w < PER/2; ++w) {
      const PacketField& ker = s.mem[tp*PER+w].ker;
      for (int i = 0; i < (int)ker.ids.size(); ++i) proto[tp].add(ker.ids[i], ker.vals[i]);
    }
  }
  int ok = 0, total = 0;
  for (int tp = 0; tp < TOPICS; ++tp) for (int w = PER/2; w < PER; ++w) {
    int node = tp*PER+w, best = -1; double bs = -1;
    for (int c = 0; c < TOPICS; ++c) {
      double m = std::abs(packetOverlap(s.mem[node].ker, proto[c]));
      if (m > bs) { bs = m; best = c; }
    }
    ok += (best == tp); total++;
  }
#else
  std::vector<std::unordered_map<int,cd>> proto(TOPICS);
  for (int tp = 0; tp < TOPICS; ++tp) for (int w = 0; w < PER/2; ++w) for (auto& kv : s.mem[tp*PER+w].ker) proto[tp][kv.first] += kv.second;
  int ok = 0, total = 0; for (int tp = 0; tp < TOPICS; ++tp) for (int w = PER/2; w < PER; ++w) { int node = tp*PER+w, best = -1; double bs = -1; for (int c = 0; c < TOPICS; ++c) { double m = std::abs(gw::fieldOverlap(s.mem[node].ker, proto[c])); if (m > bs) { bs = m; best = c; } } ok += (best == tp); total++; }
#endif
  GW_PROFILE_ADD(eval_ns, tEval);
  return 100.0 * ok / total;
}

#if GW_STREAM_PROFILE
static double nsMs(long long ns){ return (double)ns / 1.0e6; }
static void printProfile(const char* label, const StreamProfile& p){
  const double total = std::max(1.0, nsMs(p.process_ns));
  auto line = [&](const char* name, long long ns){
    const double ms = nsMs(ns);
    std::printf("  %-10s %9.2f ms  %5.1f%%\n", name, ms, 100.0 * ms / total);
  };
  std::printf("\nPROFILE %s (process total %.2f ms)\n", label, total);
  line("graph", p.graph_ns);
  line("evolve", p.evolve_ns);
  line("bridge", p.bridge_ns);
  std::printf("  evolve breakdown:\n");
  line("hood", p.hood_ns);
  line("index", p.index_ns);
  line("bonds", p.bonds_ns);
  line("project", p.project_ns);
  line("flow", p.flow_ns);
  line("postnorm", p.postnorm_ns);
  line("unproject", p.unproject_ns);
  line("sense", p.sense_ns);
  line("pr", p.pr_ns);
  line("stable", p.stable_ns);
  if (p.samples > 0) {
    const double s = (double)p.samples;
    std::printf("  averages: samples=%lld nodes=%.2f bonds=%.2f lin_in=%.2f ker_in=%.2f lin_out=%.2f ker_out=%.2f sense=%.2f\n",
                p.samples, p.nodes_sum / s, p.bonds_sum / s,
                p.lin_support_in_sum / s, p.ker_support_in_sum / s,
                p.lin_support_out_sum / s, p.ker_support_out_sum / s,
                p.sense_support_sum / s);
  }
}
#endif

int main(int argc, char** argv){
  const int stream = argInt(argv, argc, 1, 60000), uniqueEvery = argInt(argv, argc, 2, 7);
  // real system trains on the full stream (timed); random control is capped (only a
  // value baseline) so 10M runs don't hold two giant systems in memory at once.
  auto t0 = std::chrono::steady_clock::now();
  System real(false); run(real, stream, uniqueEvery);
  auto tReal = std::chrono::steady_clock::now();
#if GW_STREAM_PROFILE
  StreamProfile realTrainProfile = gProf;
  gProf = {};
#endif
  System random(true); run(random, std::min(stream, 200000), uniqueEvery);
#if GW_STREAM_PROFILE
  StreamProfile randomTrainProfile = gProf;
  gProf = {};
#endif
  double comp = real.sumHKer > 0 ? real.sumHLin / real.sumHKer : 0;
  double realAcc = eval(real), randomAcc = eval(random);
#if GW_STREAM_PROFILE
  StreamProfile evalProfile = gProf;
#endif
  double realSec = std::chrono::duration<double>(tReal - t0).count();
  double sameBridgeOv = real.bridgeStats.samePairs ? real.bridgeStats.sameOverlap / real.bridgeStats.samePairs : 0.0;
  double crossBridgeOv = real.bridgeStats.crossPairs ? real.bridgeStats.crossOverlap / real.bridgeStats.crossPairs : 0.0;
  std::printf("=== STREAMING COMPRESSION: nonlinear physics always on (%s, stats %s, postnorm %s, mem %s, flow %s) ===\n",
              GW_USE_RATIONAL_KERR_STREAM ? "Cayley hop + rational Kerr" : "exp Kerr reference",
              GW_STREAM_COLLECT_FLOW_STATS ? "on" : "off",
              GW_STREAM_RENORMALIZE_AFTER_FLOW ? "on" : "off",
              GW_STREAM_PACKET_MEM ? "packet" : "map",
              GW_STREAM_PREPARED_CAYLEY ? "prepared" : "sparse");
  std::printf("stream=%d uniqueEvery=%d field_updates=%lld nodes=%d horizons=%lld novel_horizons=%lld\n", stream, uniqueEvery, real.updates, (int)real.g.adj.size(), real.stableHorizons, real.novelHorizons);
  std::printf("horizon PR linear=%.2f nonlinear=%.2f compression=%.2fx\n", real.horizonSamples ? real.sumHLin/real.horizonSamples : 0, real.horizonSamples ? real.sumHKer/real.horizonSamples : 0, comp);
  std::printf("bridges total=%lld true=%lld false=%lld overlap same=%.3f cross=%.3f max_same=%.3f max_cross=%.3f\n",
              real.bridgeStats.materialized, real.bridgeStats.trueBridge, real.bridgeStats.falseBridge,
              sameBridgeOv, crossBridgeOv, real.bridgeStats.maxSame, real.bridgeStats.maxCross);
  std::printf("value REAL=%.1f%% RANDOM=%.1f%% max_bond_visits=%lld max_phase_speed=%.3f\n", realAcc, randomAcc, real.maxOps, real.maxPhaseSpeed);
  std::printf("train_sec=%.2f tokens_per_sec=%.0f peak_ram_mb=%.0f\n", realSec, stream/std::max(realSec,1e-9), peakRamMb());
#if GW_STREAM_PROFILE
  printProfile("real-train", realTrainProfile);
  printProfile("random-control", randomTrainProfile);
  std::printf("\nPROFILE eval %.2f ms\n", nsMs(evalProfile.eval_ns));
#endif
  bool ok = real.updates == stream && real.stableHorizons > 0 && real.novelHorizons == 0 && comp > 1.35 && realAcc > randomAcc + 20.0 && real.maxOps <= 64LL * TOPK * 4 && real.maxPhaseSpeed < 8.0;
  return ok ? 0 : 1;
}
