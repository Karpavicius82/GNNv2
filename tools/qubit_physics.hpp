// qubit_physics.hpp
// ----------------------------------------------------------------------------
// PHYSICS-ONLY qubit instrumentation for the hidden-qubit substrate. No qubit
// observable is ever pulled out of the amplitude array by hand (no indexed
// Re/Im, no sum_i psi_i conj psi_j). The ONLY readout is the Born rule -- node
// population |psi|^2 -- and every other quantity (relative phase, Bloch vector,
// entanglement entropy) is obtained the way an experiment obtains it: arrange a
// physical interference (a hypercube-edge beam-splitter and a gauge-flux phase
// shifter, both realised by gw::Stepper) so the answer appears in populations.
//
// All rotations are EXACT single Cayley steps: for H = sigma_x (a hypercube edge)
// or H = Z (a diagonal flux), one step with dt = 2 tan(pi/8) is exactly a pi/2
// rotation, because the full-space Cayley factorises into 2x2 blocks on the qubit.
// ----------------------------------------------------------------------------
#pragma once
#include "graph_wave_substrate.hpp"
#include <cmath>
#include <vector>

namespace qp {
using gw::cd;
using gw::Vec;
using gw::CMat;
constexpr double kPi = gw::kPi;

// dt giving an EXACT pi/2 rotation in one Cayley step (2*arctan(tan(pi/8)) = pi/4)
inline double dtHalfPi() { return 2.0 * std::tan(kPi / 8.0); }

// ---- the ONLY measurement primitive: node population (Born rule) ----
inline double population(const Vec& f, std::size_t node) { return std::norm(f[node]); }

// <Z_q> = P(bit q = 0) - P(bit q = 1), read straight from populations
inline double expectZ(const Vec& f, int n, int q) {
 const int sh = n - 1 - q; double p0 = 0, p1 = 0;
 for (std::size_t i = 0; i < f.size(); ++i) { if ((i >> sh) & 1) p1 += std::norm(f[i]); else p0 += std::norm(f[i]); }
 return p0 - p1;
}

// ---- physical single-qubit operations via gw::Stepper (one exact step) ----
// hypercube edge along qubit q  ==  sigma_x  ==  Rx(2*theta); dt -> pi/2 rotation
inline Vec edgeRotX(const Vec& f, int n, int q, double dt) {
 const std::size_t N = f.size(); CMat H(N, Vec(N, cd(0, 0)));
 const std::size_t e = std::size_t(1) << (n - 1 - q);
 for (std::size_t i = 0; i < N; ++i) H[i][i ^ e] = cd(1, 0);
 gw::Stepper U; U.build(H, dt); return U.step(f);
}
// diagonal gauge flux on qubit q  ==  Z  ==  a phase shifter (Rz); dt -> pi/2
inline Vec fluxRotZ(const Vec& f, int n, int q, double dt) {
 const std::size_t N = f.size(); CMat H(N, Vec(N, cd(0, 0)));
 const int sh = n - 1 - q;
 for (std::size_t i = 0; i < N; ++i) H[i][i] = cd(((i >> sh) & 1) ? -1.0 : 1.0, 0);
 gw::Stepper U; U.build(H, dt); return U.step(f);
}

// <X_q>, <Y_q>: rotate the wanted axis into Z with physical pi/2 rotations, then
// read populations. Signs are calibrated (see probe_qubit_physics_calibration) so
// the triple matches the standard Bloch convention exactly.
//   Rx(pi/2): a Z-measurement returns +<Y>  =>  <Y> = expectZ(edgeRotX(.))
//   Rz(pi/2) then Rx(pi/2): returns +<X>    =>  <X> = expectZ(edgeRotX(fluxRotZ(.)))
// (signs fixed by probe_qp_calib: physical triple matches arithmetic to ~1e-16.)
inline double expectY(const Vec& f, int n, int q) { return expectZ(edgeRotX(f, n, q, dtHalfPi()), n, q); }
inline double expectX(const Vec& f, int n, int q) { return expectZ(edgeRotX(fluxRotZ(f, n, q, dtHalfPi()), n, q, dtHalfPi()), n, q); }

// ---- entanglement entropy of qubit q from its PHYSICALLY MEASURED Bloch vector ----
// (the reduced state is (I + r.sigma)/2; r is three population measurements)
inline double entropyOf(const Vec& f, int n, int q) {
 const double x = expectX(f, n, q), y = expectY(f, n, q), z = expectZ(f, n, q);
 double r = std::sqrt(x * x + y * y + z * z); if (r > 1.0) r = 1.0;
 const double l1 = 0.5 * (1 + r), l2 = 0.5 * (1 - r);
 auto h = [](double l) { return l > 1e-12 ? -l * std::log(l) : 0.0; };
 return h(l1) + h(l2);
}
inline double maxEntropy(const Vec& f, int n) { double m = 0; for (int q = 0; q < n; ++q) m = std::max(m, entropyOf(f, n, q)); return m; }

// ---- JOINT (two-qubit) correlators -- the entangled-pair relative phase lives
// here, not in any single-qubit marginal (partial trace erases it). Same physics:
// rotate each qubit's measurement axis into Z, then read the joint populations.
inline Vec axisToZ(Vec f, int n, int q, char axis) {
 if (axis == 'Y') return edgeRotX(f, n, q, dtHalfPi());                       // <Y> = <Z> after Rx
 if (axis == 'X') return edgeRotX(fluxRotZ(f, n, q, dtHalfPi()), n, q, dtHalfPi()); // <X> = <Z> after Rz;Rx
 return f;                                                                    // 'Z'
}
inline double expectZZ(const Vec& f, int n, int a, int b) {
 const int sa = n - 1 - a, sb = n - 1 - b; double s = 0;
 for (std::size_t i = 0; i < f.size(); ++i) {
  const double za = ((i >> sa) & 1) ? -1.0 : 1.0, zb = ((i >> sb) & 1) ? -1.0 : 1.0;
  s += za * zb * std::norm(f[i]);
 }
 return s;
}
// <P_a Q_b> for P,Q in {'X','Y','Z'}: rotate both axes to Z, then joint population
inline double expectPair(const Vec& f, int n, int a, char pa, int b, char qb) {
 Vec g = axisToZ(f, n, a, pa); g = axisToZ(g, n, b, qb); return expectZZ(g, n, a, b);
}

// general-angle phase shifter Rz(alpha) on qubit q (composed exact Cayley steps)
inline Vec rzAngle(Vec f, int n, int q, double alpha) {
 const int steps = 1 + int(std::abs(alpha) / (kPi / 4));
 const double dt = 2.0 * std::tan((alpha / steps) / 4.0);
 const std::size_t N = f.size(); CMat H(N, Vec(N, cd(0, 0))); const int sh = n - 1 - q;
 for (std::size_t i = 0; i < N; ++i) H[i][i] = cd(((i >> sh) & 1) ? -1.0 : 1.0, 0);
 gw::Stepper U; U.build(H, dt); for (int s = 0; s < steps; ++s) f = U.step(f); return f;
}
// POLAR readout of the entangled-pair relative phase: the single ANGLE where the
// joint interference fringe focuses. Scan a phase-flux on qubit a; the fringe
// <X_a X_b> = cos(phi - theta) peaks at theta = phi. One polar quantity, physical.
inline double pairPhase(const Vec& f, int n, int a, int b) {
 int best = 0; double bestS = -2; const int M = 720;
 for (int k = 0; k < M; ++k) {
  const double th = 2 * kPi * k / M;
  const double s = expectPair(rzAngle(f, n, a, th), n, a, 'X', b, 'X');
  if (s > bestS) { bestS = s; best = k; }
 }
 return 2 * kPi * best / M;
}

// ---- physical two-qubit entangler: a diagonal pi-flux on the (bit a = bit b = 1)
// node, grown by gw::Stepper. Like the XY-coupling test, entanglement is GENERATED
// by evolving a real Hamiltonian term, not posited by a hand-written sign flip.
inline Vec fluxCZ(Vec f, int n, int a, int b, double dt, int steps) {
 const std::size_t N = f.size(); CMat H(N, Vec(N, cd(0, 0)));
 const std::size_t ea = std::size_t(1) << (n - 1 - a), eb = std::size_t(1) << (n - 1 - b);
 for (std::size_t i = 0; i < N; ++i) if ((i & ea) && (i & eb)) H[i][i] = cd(1.0, 0);
 gw::Stepper U; U.build(H, dt); for (int s = 0; s < steps; ++s) f = U.step(f); return f;
}

// ---- the substrate's local nonlinear (Kerr) step: a polar phase from the field's
// own intensity. The one genuinely native operation; no array bookkeeping.
inline Vec kerrStep(Vec f, double g, double dt) { for (auto& v : f) v *= std::exp(cd(0, -g * std::norm(v) * dt)); return f; }

// ---- physical state preparation: only a single-node injection + physical rotations.
// |0...0> is one excitation at node 0 (the input packet). Everything else is grown.
inline Vec inject0(int n) { Vec f(std::size_t(1) << n, cd(0, 0)); f[0] = cd(1, 0); return f; }

} // namespace qp
