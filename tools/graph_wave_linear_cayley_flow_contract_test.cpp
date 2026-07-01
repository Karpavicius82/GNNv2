// Contract for the production linear streaming carrier.
//
// The linear path must use the same prepared Cayley bond carrier as the nonlinear
// packet/prepared stream, with Kerr pressure absent rather than replaced by a
// different trig carrier.
#include "graph_wave_substrate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using gw::cd;
using gw::Vec;

static double norm2(const Vec& v){
  double s = 0.0;
  for (const auto& z : v) s += std::norm(z);
  return s;
}

static double maxDiff(const Vec& a, const Vec& b){
  double m = 0.0;
  for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::abs(a[i] - b[i]));
  return m;
}

static int report(const char* name, bool ok){
  std::printf("  %-58s %s\n", name, ok ? "PASS" : "FAIL");
  return ok ? 1 : 0;
}

int main(){
  std::printf("=== LINEAR CAYLEY FLOW CONTRACT ===\n");

  std::vector<gw::SparseBond> sparse = {
    {0, 1, 3.0,  0.20},
    {1, 2, 1.5, -0.35},
    {2, 3, 2.0,  0.60},
    {0, 3, 0.7, -0.10},
  };
  gw::LocalCayleyFlowCarrier carrier(sparse, 0.3);

  Vec psi = {cd(1.0, 0.0), cd(0.2, -0.1), cd(-0.1, 0.3), cd(0.05, 0.4)};
  gw::normalizeInPlace(psi);

  Vec single = psi;
  gw::LocalFlowStats stats;
  gw::edgeLocalCayleyFlowPrepared(single, carrier.bonds, 5, &stats);

  Vec pairLin = psi, pairKer = psi;
  gw::edgeLocalRationalKerrFlowPairPrepared(pairLin, pairKer, carrier.bonds, 0.3, 0.0, 5);

  Vec phaseShifted = psi;
  std::vector<gw::SparseBond> shifted = sparse;
  shifted[0].phase_u = 0.77;
  gw::LocalCayleyFlowCarrier shiftedCarrier(shifted, 0.3);
  gw::edgeLocalCayleyFlowPrepared(phaseShifted, shiftedCarrier.bonds, 5);

  int pass = 0, total = 0;
  ++total; pass += report("prepared linear Cayley preserves norm", std::abs(norm2(single) - 1.0) < 1e-12);
  ++total; pass += report("single-field helper matches pair helper at g=0", maxDiff(single, pairLin) < 1e-15 && maxDiff(single, pairKer) < 1e-15);
  ++total; pass += report("phase carrier is active, not ignored", maxDiff(single, phaseShifted) > 1e-4);
  ++total; pass += report("stats observe both palindrome halves", stats.bond_visits == (long long)carrier.bonds.size() * 2LL * 5LL);

  std::printf("RESULT : %d / %d verified.\n", pass, total);
  return pass == total ? 0 : 1;
}
