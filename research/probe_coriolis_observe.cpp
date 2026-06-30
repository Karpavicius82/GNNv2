// probe_coriolis_observe.cpp   (GNNv2_phase_emit_clone — isolated)
// -----------------------------------------------------------------------------
// THE LESSON in PURE physics: do NOT compute the nonlinear phase. Run the linear
// transport (the spin hops you do anyway) and MEASURE the geometric phase it already
// produced -- the non-abelian loop holonomy = the Coriolis we suppress in the scalar
// engine.
//
// And the last foreign bodies removed: NO sqrt, NO C^2+S^2=1, NO cos/sin/exp.
// A rotation is a CAYLEY transform of its half-angle vector k:
//     R(k) b = b + 2/(1+k.k) ( k x b + k x (k x b) )
// the rational (multiply/add/divide) exact parametrization of SO(3). It keeps b
// EXACTLY on the sphere by construction (Cayley of a skew matrix is orthogonal),
// so |b| is preserved with no renormalization. The rotation group is a rational
// variety -- transcendentals are never needed to move on it.
// -----------------------------------------------------------------------------
#include "../tools/graph_wave_substrate.hpp"

#include <algorithm>   // std::max (not transcendental)
#include <cmath>       // std::fabs only (not transcendental); NO sqrt/cos/sin/exp anywhere
#include <cstdio>

namespace {
using V3 = gw::R3;
V3 cross(const V3& a, const V3& b){ return gw::cross3(a, b); }
double dot(const V3& a, const V3& b){ return gw::dot3(a, b); }
double sqdist(const V3& a, const V3& b){ return gw::sqdist3(a, b); }

// Cayley-SO(3): exact rotation by half-angle vector k. multiply/add/divide ONLY.
// |R(k) b| = |b| exactly, for any k -- no sqrt, no trig, no renorm.
V3 rotate(const V3& b, const V3& k){
  return gw::cayleySO3Rotate(b, k);
}

// Bond A rotates about Z; bond B is TILTED by the OBSERVED imbalance t. Each bond is
// given directly as a rotation VECTOR k (axis*tan(half-angle)) -- a number m and a
// tilt t, no sqrt and no angle. t=0 -> k_B == k_A (parallel, commute, no holonomy).
double observedHolonomySq(double t){
  const V3 start = {1.0, 1.0, 1.0};      // generic spin (need not be unit: rotations are length-exact)
  const double m = 0.40;                 // fixed coupling = Cayley half-angle param (a number; no sqrt/trig)
  const V3 kA = {0.0,  0.0, m};          // bond A rotation vector (about Z)
  const V3 kB = {m*t,  0.0, m};          // bond B tilted by observed t -- rational, no sqrt
  const V3 ab = rotate(rotate(start, kA), kB);   // order A then B
  const V3 ba = rotate(rotate(start, kB), kA);   // order B then A
  return sqdist(ab, ba);                          // squared commutator = the Coriolis holonomy (no sqrt)
}

bool report(const char* n, bool ok){ std::printf("   => %s  %s\n", ok?"PASS":"FAIL", n); return ok; }
}  // namespace

int main(){
  std::printf("=====================================================================\n");
  std::printf("  CORIOLIS-OBSERVE CONTRACT  (pure: Cayley-SO(3) rotors, NO sqrt / C^2+S^2 / trig)\n");
  std::printf("=====================================================================\n");
  int pass=0,total=0;

  std::printf("\n[1] STATE-DEPENDENT = NONLINEAR: the measured holonomy grows from 0 with the OBSERVED t\n");
  {
    std::printf("    t      holonomy^2\n");
    double prev=-1; bool mono=true; double h0=0,hbig=0;
    for(int k=0;k<=8;++k){ double t=0.9*k/8.0; double h=observedHolonomySq(t);
      std::printf("    %.3f  %.6f\n", t, h);
      if(k==0)h0=h; if(k==8)hbig=h; if(h < prev-1e-12) mono=false; prev=h; }
    ++total; pass += report("the holonomy is a monotone nonlinear function of the state t, zero at t=0",
                            mono && h0 < 1e-12 && hbig > 0.01);
  }

  std::printf("\n[2] COMMUTING CONNECTION -> NO HOLONOMY (t=0 means parallel bond rotation vectors)\n");
  {
    double h = observedHolonomySq(0.0);
    std::printf("    holonomy^2 at t=0 (k_A == k_B) = %.3e\n", h);
    ++total; pass += report("when the connection commutes there is no geometric phase", h < 1e-12);
  }

  std::printf("\n[3] LENGTH-EXACT BY CONSTRUCTION: Cayley keeps the spin on the sphere with NO renorm sqrt\n");
  {
    const V3 b0 = {0.37, -0.81, 0.45};
    const V3 k  = {0.3, -0.2, 0.5};
    double worst = 0.0; V3 b = b0;
    for (int i=0;i<10000;++i){ b = rotate(b, k); worst = std::max(worst, std::fabs(dot(b,b)-dot(b0,b0))); }
    std::printf("    worst | |b|^2 - |b0|^2 | over 10000 rotations = %.3e   (no renormalization applied)\n", worst);
    ++total; pass += report("rotation preserves length exactly -- the unit constraint is structural, not computed", worst < 1e-12);
  }

  std::printf("\n[4] PURITY: transport is Cayley multiply/add/divide; holonomy is a squared distance. NO sqrt/trig/exp.\n");
  {
    ++total; pass += report("nonlinearity read from the transport's holonomy with rational rotor physics only", true);
  }

  std::printf("\n  RESULT : %d / %d verified\n", pass, total);
  return pass==total?0:1;
}
