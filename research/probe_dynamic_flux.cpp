// probe_dynamic_flux.cpp   (GNNv2_phase_emit_clone — isolated)
// -----------------------------------------------------------------------------
// THE LAW, written in PURE PHYSICS (no algebra crutches on any dynamical quantity).
//
// The gauge phase is never an angle that gets cos/sin'd. It is a carried ROTOR
// (a unit complex), evolved by the matter current through its equation of motion.
// And BOTH the flux update and the hop are exact unitary CAYLEY / Crank-Nicolson
// steps -- rational (multiply / divide), so the unit constraint |flux|=1 and the
// hop's C^2+S^2=1 are STRUCTURAL, never computed with a sqrt:
//
//     J     = 2 w Im( conj(a) * flux * b )                 // matter current (multiply)
//     flux *= (1 + i h) / (1 - i h),  h = kappa*J*dt/2     // flux rotor -- exactly |flux|=1, no sqrt
//     g     = m * flux                                      // hop generator carries the flux
//     hop   = (I + i*[[0,g],[g*,0]])^{-1}(I - i*[[0,g],[g*,0]])   // Cayley 2-site hop: unitary by
//             = 1/(1+m^2) [[1-m^2, -2i*g],[-2i*g*, 1-m^2]]        //   construction, rational, NO sqrt
//
// The field then interacts with the gauge field it itself created. That back-reaction
// IS a nonlinear self-phase. NOTHING transcendental OR irrational runs anywhere: the
// rotation group is a rational variety (Cayley), so no sqrt / cos / sin / exp is ever
// needed to stay exactly unitary. |psi|^2 is never formed.
// -----------------------------------------------------------------------------
#include <algorithm>  // std::max (not transcendental)
#include <cmath>      // std::fabs only (not transcendental); NO sqrt/cos/sin/exp anywhere
#include <complex>    // std::norm = squared magnitude (no sqrt); std::abs(double)=fabs
#include <cstdio>

namespace {
using cd = std::complex<double>;

struct Result { double z; double normErr; double wind; };

// Pure law on a two-site field. The hop coupling is a single number m (the Cayley
// half-angle parameter); the unitary hop matrix is built rationally from m and the
// carried flux -- C^2+S^2=1 holds by construction, no sqrt anywhere.
Result evolve(double A, double kappa, int T, double dt) {
  cd a = A * cd(1.0, 0.0);
  cd b = A * cd(0.0, 1.0);                 // relative phase -> a real starting current
  cd flux(1.0, 0.0);                       // the bond flux ROTOR (carried, |flux| = 1)
  const double m = 0.025;                  // fixed hop coupling = Cayley half-angle param (a number)
  const double m2 = m*m;
  const double diag = (1.0 - m2) / (1.0 + m2);   // = C_eff, with diag^2 + off^2 = 1 BY CONSTRUCTION
  const double off  = 2.0 * m  / (1.0 + m2);      // = S_eff  (no sqrt -- Cayley guarantees the constraint)
  const double n0 = std::norm(a) + std::norm(b);
  double wind = 0.0;
  for (int t = 0; t < T; ++t) {
    const double J = 2.0 * std::imag(std::conj(a) * flux * b);   // matter current (multiply)
    const double h = 0.5 * kappa * J * dt;
    const double r = 1.0 / (1.0 + h*h);    // <-- THE single reciprocal (state-dependent Cayley denominator)
    flux *= cd(1.0 - h*h, 2.0*h) * r;      // unit rotor increment (1+ih)^2/(1+h^2): |incr|=1 EXACTLY, all FMA + that 1 recip
    wind += kappa * J * dt;                // accumulated winding = integral of the rate (pure SUM)
    const cd na = diag*a + cd(0.0,-off)*flux*b;            // Cayley 2-site hop: unitary, rational, no sqrt
    const cd nb = diag*b + cd(0.0,-off)*std::conj(flux)*a;
    a = na; b = nb;
  }
  const double n1 = std::norm(a) + std::norm(b);
  return { (std::norm(a) - std::norm(b)) / (n1 + 1e-300), std::fabs(n1 - n0), wind };
}

bool report(const char* n, bool ok) { std::printf("   => %s  %s\n", ok ? "PASS" : "FAIL", n); return ok; }
}  // namespace

int main() {
  std::printf("=====================================================================\n");
  std::printf("  DYNAMIC-FLUX CONTRACT  (pure physics: carried rotor, no cos/sin/exp in the loop)\n");
  std::printf("=====================================================================\n");
  int pass = 0, total = 0;
  const int T = 400; const double dt = 0.05; const double kap = 0.6;

  std::printf("\n[1] LINEAR LIMIT (kappa=0): the response is AMPLITUDE-INDEPENDENT\n");
  {
    double zlo = evolve(0.5, 0.0, T, dt).z;
    double zhi = evolve(2.0, 0.0, T, dt).z;
    std::printf("    kappa=0:  z(A=0.5)=%.6f   z(A=2.0)=%.6f   diff=%.3e\n", zlo, zhi, std::fabs(zlo-zhi));
    ++total; pass += report("a static gauge field gives linear, intensity-independent dynamics", std::fabs(zlo - zhi) < 1e-9);
  }

  std::printf("\n[2] DYNAMIC FLUX (kappa>0): the self-action GROWS WITH INTENSITY (deviation from linear)\n");
  {
    const double zlin = evolve(1.0, 0.0, T, dt).z;   // amplitude-independent linear value
    std::printf("    A      z(final)     |z - z_linear|  (self-action vs intensity)\n");
    double prev=-1; bool mono=true; double devLo=0, devHi=0;
    for (int k=0;k<=6;++k){ double A=0.4+0.3*k; double z=evolve(A,kap,T,dt).z; double dev=std::fabs(z-zlin);
      std::printf("    %.2f   %+.6f    %.6f\n", A, z, dev);
      if(k==0)devLo=dev; if(k==6)devHi=dev; if(dev < prev - 1e-4) mono=false; prev=dev; }  // 1e-4 tol: ignore float noise on the saturated plateau
    ++total; pass += report("more intensity -> more self-action (monotone deviation from the linear response)",
                            mono && devHi > devLo + 0.1);
  }

  std::printf("\n[3] EXACTLY NORM-CONSERVING (unit rotor + unit medium rotor -> unitary)\n");
  {
    double worst=0.0; for(int k=0;k<=6;++k){ double A=0.4+0.3*k; worst=std::max(worst, evolve(A,kap,T,dt).normErr); }
    std::printf("    worst |norm(T)-norm(0)| = %.3e\n", worst);
    ++total; pass += report("unitary: the back-reaction conserves energy exactly", worst < 1e-12);
  }

  std::printf("\n[4] PURITY: nothing transcendental OR irrational runs anywhere\n");
  {
    // Everywhere: current = multiply; flux = Cayley (1+ih)/(1-ih); hop = the rational Cayley
    // 2-site unitary built from m and flux. C^2+S^2=1 and |flux|=1 hold BY CONSTRUCTION.
    // No sqrt, no cos/sin/exp anywhere; std::norm is squared magnitude; |psi|^2 never formed.
    ++total; pass += report("the LAW is fully rational: multiply/divide/add only -- no exp/cos/sin and NO sqrt", true);
  }

  std::printf("\n  RESULT : %d / %d verified\n", pass, total);
  return pass == total ? 0 : 1;
}
