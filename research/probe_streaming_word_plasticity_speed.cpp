// probe_streaming_word_plasticity_speed
// ----------------------------------------------------------------------------
// 1M-token smoke benchmark for physics-native streaming word plasticity.
// Measures local online learning only: dynamic nodes, local context edges,
// decay/pruning, and sparse light-cone propagation. No vocab cap, no VxV matrix,
// no full dictionary readout.
// ----------------------------------------------------------------------------
#define NOMINMAX
#include "graph_wave_substrate.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

namespace {
using gw::cd;

constexpr int kWindow = 4;
constexpr int kTopK = 5;
constexpr double kEta = 1.0;
constexpr double kDecay = 0.985;
constexpr double kDrop = 0.035;
constexpr double kTau = 0.42;

struct Edge {
  double w = 0.0;
  int last = 0;
};

struct Field {
  std::unordered_map<int, cd> a;
};

static double power(const Field& f) {
  double s = 0.0;
  for (const auto& kv : f.a) s += std::norm(kv.second);
  return s;
}

static void normalize(Field& f) {
  double n = std::sqrt(power(f)) + 1e-15;
  for (auto& kv : f.a) kv.second /= n;
}

struct StreamGraph {
  int t = 0;
  long long op_total = 0;
  long long op_max = 0;
  int max_degree_seen = 0;
  std::unordered_map<std::string, int> id;
  std::vector<std::string> words;
  std::vector<int> last_seen;
  std::vector<std::unordered_map<int, Edge>> adj;
  std::deque<int> window;

  int findOrCreate(const std::string& token) {
    auto it = id.find(token);
    if (it != id.end()) return it->second;
    int n = (int)words.size();
    id[token] = n;
    words.push_back(token);
    last_seen.push_back(-1);
    adj.push_back({});
    return n;
  }

  void reserve(int expected_nodes) {
    id.reserve(expected_nodes);
    words.reserve(expected_nodes);
    last_seen.reserve(expected_nodes);
    adj.reserve(expected_nodes);
  }

  void decayEdge(int a, int b, long long& ops) {
    auto it = adj[a].find(b);
    if (it == adj[a].end()) return;
    int dt = t - it->second.last;
    if (dt > 0) {
      it->second.w *= std::pow(kDecay, dt);
      it->second.last = t;
    }
    ops++;
  }

  void eraseEdge(int a, int b) {
    adj[a].erase(b);
    adj[b].erase(a);
  }

  void pruneNode(int a, long long& ops) {
    std::vector<int> drop;
    drop.reserve(adj[a].size());
    for (auto& kv : adj[a]) {
      int b = kv.first;
      decayEdge(a, b, ops);
      if (kv.second.w < kDrop) drop.push_back(b);
      ops++;
    }
    for (int b : drop) eraseEdge(a, b);

    while ((int)adj[a].size() > kTopK) {
      int weakest = -1;
      double ww = 1e100;
      for (const auto& kv : adj[a]) {
        if (kv.second.w < ww) {
          ww = kv.second.w;
          weakest = kv.first;
        }
        ops++;
      }
      eraseEdge(a, weakest);
    }
    max_degree_seen = std::max(max_degree_seen, (int)adj[a].size());
  }

  void touchEdge(int a, int b, long long& ops) {
    if (a == b) return;
    decayEdge(a, b, ops);
    decayEdge(b, a, ops);
    adj[a][b].w += kEta;
    adj[a][b].last = t;
    adj[b][a].w = adj[a][b].w;
    adj[b][a].last = t;
    ops += 2;
    pruneNode(a, ops);
    pruneNode(b, ops);
  }

  int process(const std::string& token) {
    t++;
    long long ops = 0;
    int a = findOrCreate(token);
    last_seen[a] = t;
    ops += 2;
    pruneNode(a, ops);
    for (int c : window) touchEdge(a, c, ops);
    window.push_back(a);
    if ((int)window.size() > kWindow) window.pop_front();
    op_total += ops;
    op_max = std::max(op_max, ops);
    return a;
  }

  int nodeCount() const { return (int)words.size(); }

  long long directedEdges() const {
    long long e = 0;
    for (const auto& m : adj) e += (long long)m.size();
    return e;
  }
};

static Field seed(int node) {
  Field f;
  f.a[node] = cd(1, 0);
  return f;
}

static Field propagate(const StreamGraph& g, Field f, int steps, long long& ops) {
  for (int step = 0; step < steps; step++) {
    Field next;
    for (const auto& kv : f.a) {
      int u = kv.first;
      cd amp = kv.second;
      for (const auto& e : g.adj[u]) {
        next.a[e.first] += cd(0, -kTau * e.second.w) * amp;
        ops++;
      }
    }
    f = std::move(next);
    normalize(f);
  }
  return f;
}

static std::vector<std::string> makeStablePool() {
  std::vector<std::string> pool;
  pool.reserve(96);
  for (int topic = 0; topic < 3; topic++) {
    char prefix = (topic == 0 ? 'a' : topic == 1 ? 'b' : 'c');
    for (int i = 0; i < 32; i++) {
      pool.push_back(std::string(1, prefix) + std::to_string(i));
    }
  }
  return pool;
}

static std::string tokenAt(int i, int uniqueEvery, const std::vector<std::string>& stable) {
  if (uniqueEvery > 0 && i % uniqueEvery == 0) {
    return "novel_" + std::to_string(i / uniqueEvery);
  }
  int topic = i % 3;
  int local = (i / 3) % 32;
  return stable[topic * 32 + local];
}

static double peakWorkingSetMb() {
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
    return (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
  }
#endif
  return -1.0;
}

static int argInt(char** argv, int argc, int index, int fallback) {
  if (index >= argc) return fallback;
  return std::atoi(argv[index]);
}
}  // namespace

int main(int argc, char** argv) {
  const int tokens = argInt(argv, argc, 1, 1000000);
  const int uniqueEvery = argInt(argv, argc, 2, 5);
  const int propagateEvery = argInt(argv, argc, 3, 1);
  const int reserveNodes = uniqueEvery == 1 ? tokens : tokens / std::max(1, uniqueEvery) + 128;

  std::printf("=====================================================================\n");
  std::printf(" STREAMING WORD PLASTICITY SPEED PROBE\n");
  std::printf(" tokens=%d  uniqueEvery=%d  propagateEvery=%d\n", tokens, uniqueEvery, propagateEvery);
  std::printf(" window=%d  topK=%d  no vocab cap / no VxV / no dictionary scan\n", kWindow, kTopK);
  std::printf("=====================================================================\n");

  auto stable = makeStablePool();
  StreamGraph g;
  g.reserve(reserveNodes);

  long long prop_ops = 0;
  long long max_prop_ops = 0;
  double checksum = 0.0;

  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < tokens; i++) {
    std::string tok = tokenAt(i, uniqueEvery, stable);
    int node = g.process(tok);
    if (propagateEvery > 0 && i % propagateEvery == 0) {
      long long before = prop_ops;
      Field q = propagate(g, seed(node), 2, prop_ops);
      max_prop_ops = std::max(max_prop_ops, prop_ops - before);
      checksum += (double)q.a.size() + power(q);
    }
  }
  auto t1 = std::chrono::steady_clock::now();

  double sec = std::chrono::duration<double>(t1 - t0).count();
  double tokPerSec = tokens / std::max(sec, 1e-12);
  long long directed = g.directedEdges();
  double avgOps = (double)g.op_total / std::max(tokens, 1);
  long long opBound = 16LL * (kWindow + kTopK) + 32;
  bool localOps = g.op_max <= opBound && g.nodeCount() > opBound;
  bool localProp = max_prop_ops <= kTopK * kTopK + kTopK;
  bool degreeOk = g.max_degree_seen <= kTopK;

  std::printf("\nRESULT\n");
  std::printf(" elapsed_sec=%.3f\n", sec);
  std::printf(" tokens_per_sec=%.0f\n", tokPerSec);
  std::printf(" nodes=%d\n", g.nodeCount());
  std::printf(" undirected_edges=%lld\n", directed / 2);
  std::printf(" max_degree=%d\n", g.max_degree_seen);
  std::printf(" avg_update_ops/token=%.2f\n", avgOps);
  std::printf(" max_update_ops/token=%lld  bound=%lld\n", g.op_max, opBound);
  std::printf(" max_propagation_edge_ops/sample=%lld\n", max_prop_ops);
  std::printf(" peak_working_set_mb=%.1f\n", peakWorkingSetMb());
  std::printf(" checksum=%.3f\n", checksum);

  std::printf("\nGUARDS\n");
  std::printf(" local_update_ops=%s\n", localOps ? "PASS" : "FAIL");
  std::printf(" local_propagation=%s\n", localProp ? "PASS" : "FAIL");
  std::printf(" degree_cap=%s\n", degreeOk ? "PASS" : "FAIL");
  return (localOps && localProp && degreeOk) ? 0 : 1;
}
