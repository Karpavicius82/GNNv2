// graph_wave_qubit_readout_contract_test  (PHYSICS-ONLY)
// ----------------------------------------------------------------------------
// THE BACKWARD READOUT, done as an experiment would do it. No qubit observable is
// ever pulled out of the amplitude array by hand: the ONLY readout is the Born
// rule (node population |psi|^2), and relative phase / Bloch / entropy come from
// arranging physical interference (a hypercube-edge beam-splitter + a gauge-flux
// phase shifter, both gw::Stepper) so the answer appears in populations -- see
// qubit_physics.hpp. States are PREPARED physically (a single-node injection grown
// by physical rotations / flux), never typed in as amplitudes.
//
// One question: does the physical readout recover the hidden qubits?
//   [1] an injected |0..0> reads as definite & separable (<Z>=+1, entropy 0)
//   [2] physical Rx(pi/2) on each qubit -> equatorial but still PURE/product (S=0)
//   [3] a PHYSICALLY-prepared entangled pair (rotations + flux-CZ) -> S = ln2
//   [4] a global phase leaves every physical readout unchanged
// Substrate-only, deterministic. No weights, no trainer.
// ----------------------------------------------------------------------------
#include "qubit_physics.hpp"

#include <cmath>
#include <complex>
#include <cstdio>

namespace {
using gw::cd;
using gw::Vec;
const double kLn2 = std::log(2.0);
bool report(const char* w, bool ok) { std::printf("   => %s\n", ok ? "PASS" : "FAIL"); if (!ok) std::printf("   !! %s\n", w); return ok; }
} // namespace

int main() {
 std::printf("=====================================================================\n");
 std::printf(" GRAPH-WAVE QUBIT READOUT CONTRACT (physics-only: populations + interference)\n");
 std::printf(" Read hidden qubits with the Born rule alone -- no array bookkeeping.\n");
 std::printf("=====================================================================\n");
 int pass = 0, total = 0;
 // an exact pi-flux CZ in 32 Cayley steps: per-step phase = pi/32 -> total pi.
 const double czDt = 2.0 * std::tan(qp::kPi / 64.0); const int czSteps = 32;

 std::printf("\n[1] INJECTED |0..0> READS AS DEFINITE & SEPARABLE (<Z>=+1, S=0)\n");
 {
  const int n = 3; Vec f = qp::inject0(n); bool ok = true;
  for (int q = 0; q < n; ++q) { const double z = qp::expectZ(f, n, q), s = qp::entropyOf(f, n, q);
   std::printf("   qubit %d: <Z>=%.12f  entropy=%.3e\n", q, z, s);
   ok = ok && std::abs(z - 1.0) < 1e-12 && s < 1e-9; }
  ++total; pass += report("injected basis state not read as definite/separable", ok) ? 1 : 0;
 }

 std::printf("\n[2] PHYSICAL Rx(pi/2) ON EACH QUBIT -> EQUATORIAL BUT STILL PURE/PRODUCT (S=0)\n");
 {
  const int n = 3; Vec f = qp::inject0(n);
  for (int q = 0; q < n; ++q) f = qp::edgeRotX(f, n, q, qp::dtHalfPi());
  double maxZ = 0, maxS = 0;
  for (int q = 0; q < n; ++q) { maxZ = std::max(maxZ, std::abs(qp::expectZ(f, n, q))); maxS = std::max(maxS, qp::entropyOf(f, n, q)); }
  std::printf("   after Rx(pi/2) on all 3: max|<Z>|=%.3e (equator)  max entropy=%.3e (still product)\n", maxZ, maxS);
  ++total; pass += report("rotation moved off equator or created mixing", maxZ < 1e-12 && maxS < 1e-9) ? 1 : 0;
 }

 std::printf("\n[3] PHYSICALLY-PREPARED ENTANGLED PAIR (rotations + flux-CZ) -> S = ln2\n");
 {
  const int n = 2; Vec f = qp::inject0(n);
  f = qp::edgeRotX(f, n, 0, qp::dtHalfPi());          // superpose q0
  f = qp::edgeRotX(f, n, 1, qp::dtHalfPi());          // superpose q1  (product so far)
  const double before = qp::maxEntropy(f, n);
  f = qp::fluxCZ(f, n, 0, 1, czDt, czSteps);          // physical CZ grows entanglement
  const double s0 = qp::entropyOf(f, n, 0), s1 = qp::entropyOf(f, n, 1);
  std::printf("   entropy before flux-CZ = %.3e\n", before);
  std::printf("   qubit 0 entropy = %.12f   qubit 1 entropy = %.12f  (ln2=%.12f)\n", s0, s1, kLn2);
  const bool ok = before < 1e-9 && std::abs(s0 - kLn2) < 1e-9 && std::abs(s1 - kLn2) < 1e-9;
  ++total; pass += report("physical entangler did not reach maximal entanglement", ok) ? 1 : 0;
 }

 std::printf("\n[4] GLOBAL PHASE LEAVES EVERY PHYSICAL READOUT UNCHANGED\n");
 {
  const int n = 2; Vec f = qp::inject0(n);
  f = qp::edgeRotX(f, n, 0, qp::dtHalfPi());
  f = qp::fluxCZ(f, n, 0, 1, czDt, czSteps);
  const double s0 = qp::entropyOf(f, n, 0), z0 = qp::expectZ(f, n, 0);
  Vec g = f; for (auto& v : g) v *= std::exp(cd(0, 1.2345));   // global phase (unobservable)
  const double s0g = qp::entropyOf(g, n, 0), z0g = qp::expectZ(g, n, 0);
  std::printf("   entropy diff = %.3e   <Z> diff = %.3e\n", std::abs(s0 - s0g), std::abs(z0 - z0g));
  ++total; pass += report("global phase changed a physical readout", std::abs(s0 - s0g) < 1e-12 && std::abs(z0 - z0g) < 1e-12) ? 1 : 0;
 }

 std::printf("\n[5] ENTANGLED-PAIR PHASE captured as a POLAR ANGLE (single-qubit readout is blind to it)\n");
 {
  const int n = 2; Vec f = qp::inject0(n);
  f = qp::edgeRotX(f, n, 0, qp::dtHalfPi());
  f = qp::edgeRotX(f, n, 1, qp::dtHalfPi());
  f = qp::fluxCZ(f, n, 0, 1, czDt, czSteps);          // entangled pair
  Vec f2 = qp::rzAngle(f, n, 0, qp::kPi / 2.0);        // shift the RELATIVE phase by pi/2
  const double sA = qp::entropyOf(f, n, 0), sB = qp::entropyOf(f2, n, 0);   // identical -> blind
  const double phiA = qp::pairPhase(f, n, 0, 1), phiB = qp::pairPhase(f2, n, 0, 1);
  double dphi = phiB - phiA; while (dphi <= -qp::kPi) dphi += 2 * qp::kPi; while (dphi > qp::kPi) dphi -= 2 * qp::kPi;
  std::printf("   single-qubit entropy: %.6f vs %.6f (identical -> single-qubit readout is blind)\n", sA, sB);
  std::printf("   POLAR pair-phase:     %.4f vs %.4f rad  (signed shift=%+.4f, |shift|=pi/2=%.4f)\n", phiA, phiB, dphi, qp::kPi / 2.0);
  const bool ok = std::abs(sA - sB) < 1e-9 && std::abs(std::abs(dphi) - qp::kPi / 2.0) < 0.02;
  ++total; pass += report("polar pair-phase did not capture the relative phase", ok) ? 1 : 0;
 }

 std::printf("\n=====================================================================\n");
 std::printf(" RESULT : %d / %d verified\n", pass, total);
 std::printf(" %s\n", pass == total
             ? "hidden qubits read out by physics alone (Born rule + interference)"
             : "physical readout failed; inspect the failing gate");
 std::printf("=====================================================================\n");
 return pass == total ? 0 : 1;
}
