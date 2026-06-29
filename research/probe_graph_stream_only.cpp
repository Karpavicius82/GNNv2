// probe_graph_stream_only
// ----------------------------------------------------------------------------
// GRAPH-STREAM-ONLY regime -- the top of the token speed ladder. Per token it does
// ONLY the plastic-graph bookkeeping: node creation, edge touch/decay/prune,
// window, plaquette gauge phase. The wave FIELD is never touched -- no project, no
// edge-flow, no unproject. It computes nothing on the substrate.
//
// This is the diagnostic ceiling "how fast if you maintained the graph but computed
// nothing" (~1.2-1.4M tok/s on a fast host). It exists so all three token-stream
// regimes are reproduced the same way; the difference between this and
// probe_linear_stream is exactly the cost of the per-token field round trip:
//
//   probe_graph_stream_only.exe 1000000   graph only            (no field)
//   probe_linear_stream.exe     1000000   + linear field (g=0)
//   probe_streaming_compression.exe 1000000   + Kerr nonlinearity (g=7)
//
// Unit is TOKENS. See docs/PERFORMANCE.md for the full ladder. (The 1,000,000
// nodes/s figure is a different engine entirely -- probe_sparse_scale.cpp.)
// ----------------------------------------------------------------------------
#define NOMINMAX
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

constexpr int TOPICS = 3, PER = 24, WIN = 4, TOPK = 6;
constexpr double ETA = 1.0, DECAY = 0.99, EDGE_FLOOR = 1e-3, WMIN = 8.0;
constexpr int SMIN = 4;
constexpr double kPi = 3.141592653589793238462643383279502884;

static double peakRamMb() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
#endif
  return -1.0;
}

struct Edge { double w = 0; double phase = 0; int last = 0; };

struct Graph {
  std::vector<std::unordered_map<int, Edge>> adj; std::vector<int> seen; std::deque<int> win; int t = 0;
  int add(){ adj.push_back({}); seen.push_back(0); return (int)adj.size() - 1; }
  double incident(int a) const { double s = 0; for (auto& kv : adj[a]) s += kv.second.w; return s; }
  void decay(int a, int b){ auto it = adj[a].find(b); if (it == adj[a].end()) return; int dt = t - it->second.last; if (dt > 0) { it->second.w *= std::pow(DECAY, dt); it->second.last = t; } }
  void eraseEdge(int a, int b){ adj[a].erase(b); adj[b].erase(a); }
  static double wrapPhase(double x){ while (x > kPi) x -= 2.0 * kPi; while (x < -kPi) x += 2.0 * kPi; return x; }
  bool hasEdge(int a, int b) const { return adj[a].find(b) != adj[a].end(); }
  void addSignedPhase(int u, int v, double dphi){ auto it = adj[u].find(v); if (it == adj[u].end()) return; Edge e = it->second; double sign = u < v ? 1.0 : -1.0; e.phase = std::clamp(wrapPhase(e.phase + sign * dphi), -1.4, 1.4); adj[u][v] = e; adj[v][u] = e; }
  void addPlaquetteFlux(int a, int b, int c){ constexpr double dphi = 0.018; addSignedPhase(a, b, dphi / 3.0); addSignedPhase(b, c, dphi / 3.0); addSignedPhase(c, a, dphi / 3.0); }
  void closePlaquettes(int node){ for (int j = 1; j < (int)win.size(); ++j) for (int i = 0; i < j; ++i) { int a = win[i], b = win[j]; if (hasEdge(a, b) && hasEdge(a, node) && hasEdge(b, node)) addPlaquetteFlux(a, b, node); } }
  void relaxNode(int a){ std::vector<int> dead; for (auto& kv : adj[a]) { int b = kv.first; int dt = t - kv.second.last; if (dt > 0) { kv.second.w *= std::pow(DECAY, dt); kv.second.last = t; auto back = adj[b].find(a); if (back != adj[b].end()) back->second = kv.second; } if (kv.second.w < EDGE_FLOOR) dead.push_back(b); } for (int b : dead) eraseEdge(a, b); }
  void prune(int a){ while ((int)adj[a].size() > TOPK) { int wk = -1; double mn = 1e100; for (auto& kv : adj[a]) if (kv.second.w < mn) { mn = kv.second.w; wk = kv.first; } eraseEdge(a, wk); } }
  void touch(int a, int b){ if (a == b) return; decay(a, b); decay(b, a); adj[a][b].w += ETA; adj[a][b].last = t; adj[b][a] = adj[a][b]; if ((int)adj[a].size() > TOPK) prune(a); if ((int)adj[b].size() > TOPK) prune(b); }
};

struct System {
  Graph g; long long updates = 0, stableEvents = 0;
  System(){ for (int i = 0; i < TOPICS * PER; ++i) g.add(); }
  void process(int node){
    g.t++; g.seen[node]++; g.relaxNode(node);
    for (int c : g.win) { g.relaxNode(c); g.touch(node, c); }
    g.closePlaquettes(node);
    g.win.push_back(node); if ((int)g.win.size() > WIN) g.win.pop_front();
    if (((int)g.adj[node].size() >= TOPK) && g.incident(node) >= WMIN && g.seen[node] >= SMIN) stableEvents++;
    updates++;
    // NO field evolution -- this is the graph-only ceiling.
  }
};

static void run(System& s, int stream, int uniqueEvery){
  std::mt19937 r(11); std::uniform_int_distribution<int> tt(0, TOPICS-1), ww(0, PER-1);
  for (int i = 0; i < stream;) { int tp = tt(r); s.g.win.clear();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) { int node = (uniqueEvery > 0 && i % uniqueEvery == 0) ? s.g.add() : tp * PER + ww(r); s.process(node); } }
}
static int argInt(char** argv, int argc, int i, int fallback){ return i < argc ? std::atoi(argv[i]) : fallback; }

int main(int argc, char** argv){
  const int stream = argInt(argv, argc, 1, 1000000), uniqueEvery = argInt(argv, argc, 2, 7);
  auto t0 = std::chrono::steady_clock::now();
  System s; run(s, stream, uniqueEvery);
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  long edges = 0; for (auto& a : s.g.adj) edges += (long)a.size(); edges /= 2;

  std::printf("=== GNNv2 GRAPH-STREAM-ONLY (no field evolution) ===\n");
  std::printf("params topics=%d per_topic=%d base_nodes=%d win=%d topk=%d\n", TOPICS, PER, TOPICS*PER, WIN, TOPK);
  std::printf("eta=%.3f decay=%.3f edge_floor=%.1e wmin=%.1f smin=%d\n", ETA, DECAY, EDGE_FLOOR, WMIN, SMIN);
  std::printf("stream=%d uniqueEvery=%d seed=11 burst=6 mode=graph-update-only (no project/edge-flow/unproject)\n\n", stream, uniqueEvery);
  std::printf("updates=%lld nodes=%d edges=%ld stable_events=%lld\n", s.updates, (int)s.g.adj.size(), edges, s.stableEvents);
  std::printf("train_sec=%.3f tokens_per_sec=%.0f peak_rss_mb=%.0f\n", sec, stream/std::max(sec,1e-12), peakRamMb());
  bool ok = s.updates == stream && (int)s.g.adj.size() > 0;
  std::printf("RESULT : %d / 1  (%s)\n", ok ? 1 : 0, ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
