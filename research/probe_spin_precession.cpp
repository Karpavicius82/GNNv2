// probe_spin_precession.cpp   (GNNv2_phase_emit_clone — isolated exploration)
// -----------------------------------------------------------------------------
// THE DOOR: replace the expensive nonlinear spin kick
//     s.up *= exp(-i g dt rho m);  s.dn *= exp(+i g dt rho m);     // 2 transcendentals/node
// with a GEOMETRIC realization on the Bloch vector: a state-dependent PRECESSION
//     db/dt = omega(state) x b
// The Kerr Z-rotation by an angle proportional to the population difference m = b_z
// becomes a SELF-precession with rate omega_z = g * b_z.
//
// And the last foreign bodies removed: no exp, no trig, AND no renormalization sqrt.
// One precession substep is an exact CAYLEY-SO(3) rotation of the half-step vector
// k = omega*(h/2):
//     R(k) b = b + 2/(1+k.k) ( k x b + k x (k x b) )
// rational (multiply/add/divide) and length-EXACT by construction -- the spin stays on
// the sphere with NO renorm. The cross product (the precession) is still the kernel;
// Cayley just sums it to an exact rotation. The non-abelian loop HOLONOMY (precess
// about different axes in sequence) is the Coriolis geometric phase that sources the
// nonlinearity, order- and state-dependent.
// -----------------------------------------------------------------------------
#include "../tools/graph_wave_substrate.hpp"

#include <algorithm>
#include <cmath>      // std::fabs / std::max only (not transcendental); NO sqrt/cos/sin/exp
#include <cstdio>

namespace {
using V3 = gw::R3;

V3 cross(const V3& a, const V3& b) {
  return gw::cross3(a, b);
}
double dot(const V3& a, const V3& b) { return gw::dot3(a, b); }
double sqdist(const V3& a, const V3& b) {
  return gw::sqdist3(a, b);
}

// Cayley-SO(3): exact rotation by half-angle vector k. multiply/add/divide ONLY.
// |R(k) b| = |b| exactly, for any k -- no sqrt, no trig, no renorm.
V3 rotate(const V3& b, const V3& k){
  return gw::cayleySO3Rotate(b, k);
}

// GEOMETRIC kernel: one precession substep = an exact Cayley rotation about omega.
// k = omega*(h/2); as h shrinks the per-step angle -> |omega|*h. No exp/trig/renorm.
void precessStep(V3& b, const V3& omega, double h) {
  const V3 k = { omega[0]*0.5*h, omega[1]*0.5*h, omega[2]*0.5*h };
  b = rotate(b, k);
}
// Kerr-like self-precession to total "time" T in K substeps (omega_z = g*b_z, live).
V3 precessSelfRotZ(V3 b, double g, double T, int K) {
  double h = T / K;
  for (int k = 0; k < K; ++k) { V3 om = {0.0, 0.0, g * b[2]}; precessStep(b, om, h); }
  return b;
}
// fixed-axis precession by total angle ~ |omega|*T, in K substeps (for the holonomy test).
V3 precessAxis(V3 b, const V3& omega, double T, int K) {
  double h = T / K;
  for (int k = 0; k < K; ++k) precessStep(b, omega, h);
  return b;
}

bool report(const char* n, bool ok) { std::printf("   => %s  %s\n", ok ? "PASS" : "FAIL", n); return ok; }
}  // namespace

int main() {
  std::printf("=====================================================================\n");
  std::printf("  SPIN PRECESSION CONTRACT  (Cayley-SO(3): NO exp / trig / renorm sqrt)\n");
  std::printf("=====================================================================\n");
  int pass = 0, total = 0;
  const double g = 1.30, T = 1.00;

  std::printf("\n[1] PRECESSION (cross-product, no exp/trig) IS A CLEAN ROTATION: reversible + convergent\n");
  {
    V3 b0 = {0.6, 0.0, 0.8};            // tilted (b_z = 0.8 != 0)
    // REVERSIBILITY (no exact/trig reference): precess forward, then reverse the sense
    // (-g) for the same time -> a genuine rotation returns exactly to the start.
    V3 fwd  = precessSelfRotZ(b0,  g, T, 4096);
    V3 back = precessSelfRotZ(fwd, -g, T, 4096);
    double rev = sqdist(b0, back);
    // CONVERGENCE (Cauchy, squared): refining substeps approaches a fixed limit.
    V3 k16  = precessSelfRotZ(b0, g, T, 16);
    V3 k256 = precessSelfRotZ(b0, g, T, 256);
    V3 k4096= precessSelfRotZ(b0, g, T, 4096);
    double c1 = sqdist(k16, k256), c2 = sqdist(k256, k4096);
    std::printf("    reversibility^2 = %.3e   convergence^2 d(16,256)=%.3e -> d(256,4096)=%.3e\n", rev, c1, c2);
    ++total; pass += report("the precession is an invertible rotation that converges to a limit (no exp/trig used)",
                            rev < 1e-9 && c2 < c1 && c2 < 1e-6);
  }

  std::printf("\n[2] STATE-DEPENDENT = NONLINEAR: scalar-blind (b_z=0) does not move; tilted does\n");
  {
    V3 flat = {1.0, 0.0, 0.0};          // equator, b_z = 0  -> omega_z = g*b_z = 0
    V3 tilt = {0.6, 0.0, 0.8};          // b_z = 0.8
    double mFlat = sqdist(flat, precessSelfRotZ(flat, g, T, 256));
    double mTilt = sqdist(tilt, precessSelfRotZ(tilt, g, T, 256));
    std::printf("    same |b|=1: flat(b_z=0) motion^2=%.6f   tilted(b_z=0.8) motion^2=%.6f\n", mFlat, mTilt);
    ++total; pass += report("nonlinearity is carried by the spin state, invisible to a b_z=0 point", mFlat < 1e-18 && mTilt > 0.25);
  }

  std::printf("\n[3] LENGTH-EXACT BY CONSTRUCTION: Cayley keeps the spin on the sphere with NO renorm sqrt\n");
  {
    V3 b = {0.3, -0.5, 0.8};
    const double L0 = dot(b, b);
    double worst = 0.0;
    double h = T/256;
    for (int k = 0; k < 256; ++k) { V3 om = {0.0,0.0,g*b[2]}; precessStep(b, om, h); worst = std::max(worst, std::fabs(dot(b,b)-L0)); }
    std::printf("    worst | |b|^2 - |b0|^2 | over the run = %.3e   (no renormalization applied)\n", worst);
    ++total; pass += report("geometric step stays unitary by construction -- the unit constraint is not computed", worst < 1e-12);
  }

  std::printf("\n[4] NON-ABELIAN HOLONOMY = CORIOLIS: order of two precessions matters\n");
  {
    V3 b0 = {0.0, 0.0, 1.0};
    V3 ox = {1.0, 0.0, 0.0}, oz = {0.0, 0.0, 1.0};
    double a1 = 0.7, a2 = 0.5;
    // NON-commuting: different axes (X then Z vs Z then X) -> a real geometric holonomy.
    V3 xz = precessAxis(precessAxis(b0, ox, a1, 512), oz, a2, 512);
    V3 zx = precessAxis(precessAxis(b0, oz, a2, 512), ox, a1, 512);
    double holonomy = sqdist(xz, zx);
    // COMMUTING control: SAME axis (z) in both orders -> order is physically irrelevant.
    V3 zz1 = precessAxis(precessAxis(b0, oz, a1, 512), oz, a2, 512);
    V3 zz2 = precessAxis(precessAxis(b0, oz, a2, 512), oz, a1, 512);
    double commute = sqdist(zz1, zz2);
    std::printf("    non-commuting (X.Z vs Z.X) holonomy^2 = %.6f   commuting (Z.Z) residual^2 = %.3e\n",
                holonomy, commute);
    ++total; pass += report("a closed spin loop carries a geometric (Coriolis) phase; ~zero when axes commute",
                            holonomy > 0.01 && commute < 1e-6 * holonomy + 1e-18);
  }

  std::printf("\n[5] THE GEOMETRIC PHASE IS STATE-DEPENDENT (nonlinear holonomy)\n");
  {
    // self-rotation angle phi = g*b_z*T grows with b_z -> nonlinear in the state. Read the
    // accumulated angle RATIONALLY (no sqrt/trig) as cos(phi) = (xy_initial . xy_final)/|xy|^2:
    // larger b_z -> larger phi -> SMALLER cos(phi). States built by Cayley-rotating Z (no sqrt).
    auto cosPhi = [&](V3 s){ V3 f = precessSelfRotZ(s, g, T, 256);
      double xy = s[0]*s[0] + s[1]*s[1]; return (s[0]*f[0] + s[1]*f[1]) / (xy + 1e-300); };
    V3 A = rotate({0.0,0.0,1.0}, {0.3, 0.0, 0.0});   // larger b_z (small tilt off Z)
    V3 B = rotate({0.0,0.0,1.0}, {0.6, 0.0, 0.0});   // smaller b_z (large tilt off Z)
    double cA = cosPhi(A), cB = cosPhi(B);
    std::printf("    b_z(A)=%.3f cos(phi_A)=%.4f   b_z(B)=%.3f cos(phi_B)=%.4f   (more b_z -> more phase -> smaller cos)\n",
                A[2], cA, B[2], cB);
    ++total; pass += report("accumulated phase grows with the state b_z (nonlinear, not a fixed gate)",
                            A[2] > B[2] && cA < cB - 0.05);
  }

  std::printf("\n  RESULT : %d / %d verified\n", pass, total);
  std::printf("  (kernel = cross-product Cayley rotation; NO exp, NO trig, NO renorm sqrt)\n");
  return pass == total ? 0 : 1;
}
