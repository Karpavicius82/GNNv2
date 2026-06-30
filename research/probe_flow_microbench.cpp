// -----------------------------------------------------------------------------
// Matrix-free flow microbenchmark.
//
// This isolates the nonlinear physics operator from the streaming graph,
// unordered_map memory, project/unproject, sense, bridges, and random control.
// -----------------------------------------------------------------------------
#define NOMINMAX
#include "../tools/graph_wave_substrate.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

using gw::cd;
using gw::Vec;

static int argInt(char** argv, int argc, int i, int fallback) {
  return i < argc ? std::atoi(argv[i]) : fallback;
}

static std::vector<gw::SparseBond> makeBonds(int n) {
  std::vector<gw::SparseBond> bonds;
  bonds.reserve((size_t)n * 3);
  for (int i = 0; i < n; ++i) {
    bonds.push_back({i, (i + 1) % n, 1.0, 0.006 * (i % 7)});
    bonds.push_back({i, (i + 3) % n, 0.45, -0.004 * (i % 5)});
    if ((i & 1) == 0) bonds.push_back({i, (i + 7) % n, 0.20, 0.003 * (i % 11)});
  }
  return bonds;
}

static Vec makeField(int n, double shift) {
  Vec v(n, cd(0.0, 0.0));
  for (int i = 0; i < n; ++i) {
    const double x = (double)(i - n / 2);
    const double amp = 1.0 / (1.0 + 0.015 * (x - shift) * (x - shift));
    v[i] = cd(amp, 0.01 * ((i % 9) - 4));
  }
  gw::normalizeInPlace(v);
  return v;
}

template <class Fn>
static double bench(const char* name, int iters, int n, const std::vector<gw::SparseBond>& bonds, Fn fn) {
  Vec lin = makeField(n, 0.0), ker = makeField(n, 2.0);
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < iters; ++i) fn(lin, ker);
  const auto t1 = std::chrono::steady_clock::now();
  const double sec = std::chrono::duration<double>(t1 - t0).count();
  const double updates = (double)iters / (sec + 1e-300);
  const double bondSteps = updates * (double)bonds.size() * 4.0;
  const double comp = gw::participationRatio(lin) / (gw::participationRatio(ker) + 1e-300);
  std::printf("%-22s sec=%.3f updates/s=%.0f bond_steps/s=%.0f final_pr_ratio=%.3f\n",
              name, sec, updates, bondSteps, comp);
  return sec;
}

int main(int argc, char** argv) {
  const int iters = argInt(argv, argc, 1, 200000);
  const int n = argInt(argv, argc, 2, 48);
  const double dt = 0.3, g = 7.0;
  const int steps = 2;
  const std::vector<gw::SparseBond> bonds = makeBonds(n);

  std::printf("=== FLOW MICROBENCH: n=%d bonds=%d iters=%d ===\n", n, (int)bonds.size(), iters);

  bench("rational/no-stats", iters, n, bonds, [&](Vec& lin, Vec& ker) {
    gw::edgeLocalRationalKerrFlowPair(lin, ker, bonds, dt, g, steps);
  });

  bench("rational/stats", iters, n, bonds, [&](Vec& lin, Vec& ker) {
    gw::LocalFlowStats stats;
    gw::edgeLocalRationalKerrFlowPair(lin, ker, bonds, dt, g, steps, &stats);
  });

  bench("exp/no-stats", iters, n, bonds, [&](Vec& lin, Vec& ker) {
    gw::edgeLocalKerrFlowPair(lin, ker, bonds, dt, g, steps);
  });

  bench("exp/stats", iters, n, bonds, [&](Vec& lin, Vec& ker) {
    gw::LocalFlowStats stats;
    gw::edgeLocalKerrFlowPair(lin, ker, bonds, dt, g, steps, &stats);
  });

  return 0;
}
