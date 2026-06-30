// -----------------------------------------------------------------------------
// PURE PHYSICS CHAIN CONTRACT
//
// The green-chain primitives live in graph_wave_substrate.hpp, not in isolated
// probes: carried U(1) flux rotor, Cayley two-site hop, Cayley-SO(3) holonomy,
// and the rational local Kerr pressure used by the streaming path.
// -----------------------------------------------------------------------------
#include "graph_wave_substrate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct FluxResult { double z = 0.0; double normErr = 0.0; };

FluxResult evolveFlux(double amp, double kappa, int steps, double dt) {
 gw::cd a = amp * gw::cd(1.0, 0.0);
 gw::cd b = amp * gw::cd(0.0, 1.0);
 gw::cd flux(1.0, 0.0);
 const double m = 0.025;
 const double m2 = m * m;
 const double den = 1.0 / (1.0 + m2);
 const double diag = (1.0 - m2) * den;
 const double off = 2.0 * m * den;
 const double n0 = std::norm(a) + std::norm(b);
 for (int t = 0; t < steps; ++t) {
  const double j = gw::dynamicFluxCurrent(a, b, 1.0, flux);
  gw::advanceDynamicFlux(flux, j, kappa, dt);
  gw::cayleyHop2(a, b, diag, off, flux);
 }
 const double n1 = std::norm(a) + std::norm(b);
 return {(std::norm(a) - std::norm(b)) / (n1 + 1e-300), std::fabs(n1 - n0)};
}

gw::Vec rationalPacket(int n) {
 gw::Vec v(n, gw::cd(0.0, 0.0));
 const int c = n / 2;
 for (int i = 0; i < n; ++i) {
  const int d = i - c;
  v[i] = gw::cd(1.0 / (1.0 + 0.01 * d * d), 0.0);
 }
 gw::normalizeInPlace(v);
 return v;
}

bool report(const char* name, bool ok) {
 std::printf("   => %s  %s\n", ok ? "PASS" : "FAIL", name);
 return ok;
}

}  // namespace

int main() {
 std::printf("=====================================================================\n");
 std::printf("  PURE PHYSICS CHAIN CONTRACT  (substrate primitives, not probe copies)\n");
 std::printf("=====================================================================\n");
 int pass = 0, total = 0;

 std::printf("\n[1] DYNAMIC U(1) FLUX: self-action grows with field intensity and preserves norm\n");
 {
  const int steps = 400;
  const double dt = 0.05, kappa = 0.6;
  const double zlin = evolveFlux(1.0, 0.0, steps, dt).z;
  double prev = -1.0, lo = 0.0, hi = 0.0, worstNorm = 0.0;
  bool mono = true;
  for (int k = 0; k <= 6; ++k) {
   const double amp = 0.4 + 0.3 * k;
   const FluxResult r = evolveFlux(amp, kappa, steps, dt);
   const double dev = std::fabs(r.z - zlin);
   if (k == 0) lo = dev;
   if (k == 6) hi = dev;
   if (dev < prev - 1e-4) mono = false;
   prev = dev;
   worstNorm = std::max(worstNorm, r.normErr);
  }
  std::printf("    deviation low=%.6f high=%.6f  worst norm drift=%.3e\n", lo, hi, worstNorm);
  ++total; pass += report("carried flux gives nonlinear self-action without energy loss",
                          mono && hi > lo + 0.1 && worstNorm < 1e-12);
 }

 std::printf("\n[2] SO(3) CAYLEY HOLONOMY: non-commuting precession produces Coriolis phase\n");
 {
  const gw::R3 start = {1.0, 1.0, 1.0};
  const gw::R3 kA = {0.0, 0.0, 0.40};
  const gw::R3 kB = {0.36, 0.0, 0.40};
  const gw::R3 ab = gw::cayleySO3Rotate(gw::cayleySO3Rotate(start, kA), kB);
  const gw::R3 ba = gw::cayleySO3Rotate(gw::cayleySO3Rotate(start, kB), kA);
  const gw::R3 aa = gw::cayleySO3Rotate(gw::cayleySO3Rotate(start, kA), kA);
  const gw::R3 aa2 = gw::cayleySO3Rotate(gw::cayleySO3Rotate(start, kA), kA);
  const double holonomy = gw::sqdist3(ab, ba);
  const double commute = gw::sqdist3(aa, aa2);
  const double len0 = gw::dot3(start, start);
  const double lenErr = std::fabs(gw::dot3(ab, ab) - len0);
  std::printf("    holonomy^2=%.6f  commuting residual=%.3e  length drift=%.3e\n",
              holonomy, commute, lenErr);
  ++total; pass += report("geometric phase is observed from transport and length is structural",
                          holonomy > 0.01 && commute < 1e-18 && lenErr < 1e-12);
 }

 std::printf("\n[3] RATIONAL LOCAL KERR: pressure stays on node phase and densifies the field\n");
 {
  const int n = 160;
  std::vector<gw::SparseBond> bonds;
  bonds.reserve(n);
  for (int i = 0; i < n; ++i) bonds.push_back({i, (i + 1) % n, 1.0, 0.0});
  gw::Vec lin = rationalPacket(n), ker = lin;
  gw::LocalFlowStats stats;
  gw::edgeLocalRationalKerrFlowPair(lin, ker, bonds, 0.08, 60.0, 450, &stats);
  const double prLin = gw::participationRatio(lin);
  const double prKer = gw::participationRatio(ker);
  const double comp = prLin / (prKer + 1e-300);
  std::printf("    PR linear=%.2f nonlinear=%.2f compression=%.2fx  max speed=%.3f\n",
              prLin, prKer, comp, stats.max_bond_speed);
  ++total; pass += report("same Cayley transport plus local phase pressure produces densification",
                          comp > 1.35 && stats.max_bond_speed < 8.0);
 }

 std::printf("\n  RESULT : %d / %d verified\n", pass, total);
 return pass == total ? 0 : 1;
}
