// Nonlinearity AS PHYSICS, no math crutch. Kerr self-focusing on a lattice:
//   i psi_dot = -H psi - g |psi|^2 psi   (discrete nonlinear Schrodinger).
// The nonlinear term is the literal physical Kerr effect -- the local intensity
// |psi_i|^2 shifts the local refractive index, which rotates the local phase. We
// evolve by real-space split-step: a unitary hopping half-step (Cayley), then a
// purely LOCAL nonlinear phase rotation psi_i *= exp(-i g |psi_i|^2 dt), then
// another half-step. No FFT, no analytic soliton (sech), no fitted profile -- only
// the wave's own intensity acting on itself. Readout = participation ratio (how
// many sites hold the energy), a physical intensity measure.
// Test: a broad packet DISPERSES when g=0 (linear) but SELF-FOCUSES when g>0 --
// the wave compresses its own energy. That is a physical compressor, not a bundle.
#include "../tools/graph_wave_substrate.hpp"
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
using namespace gw;

static double partRatio(const Vec& z){ double s2=0,s4=0; for(auto&v:z){double p=std::norm(v);s2+=p;s4+=p*p;} return s2*s2/(s4+1e-300); }

int main(){
  const int N=160; const double dt=0.1, T=60.0; const int steps=(int)(T/dt);
  Graph g(N); for(int i=0;i<N;i++) g.addEdge(i,(i+1)%N,1.0);   // ring, linear hopping
  Stepper Uh; Uh.build(g.h, dt/2);                            // unitary half-step

  auto run=[&](double gkerr){
    Vec psi(N,cd(0,0)); double c=N/2.0, sig=14;
    for(int x=0;x<N;x++){ double d=x-c; psi[x]=std::exp(cd(-d*d/(2*sig*sig),0)); }
    double nn=0;for(auto&v:psi)nn+=std::norm(v);nn=std::sqrt(nn);for(auto&v:psi)v/=nn;
    double pr0=partRatio(psi); std::vector<double> trace;
    for(int s=0;s<steps;s++){
      psi=Uh.step(psi);                                       // physical: hopping
      for(int i=0;i<N;i++) psi[i]*=std::exp(cd(0,-gkerr*std::norm(psi[i])*dt)); // physical: Kerr local phase
      psi=Uh.step(psi);
      if(s%(steps/6)==0) trace.push_back(partRatio(psi));
    }
    trace.push_back(partRatio(psi));
    return std::make_pair(pr0,trace);
  };

  std::printf("=== KERR SELF-FOCUSING: the wave compresses its own energy (physics only) ===\n");
  std::printf("ring N=%d, broad packet (sigma=14). participation ratio = sites holding energy.\n",N);
  std::printf("linear (g=0) should DISPERSE (PR grows); nonlinear (g>0) should SELF-FOCUS (PR shrinks).\n\n");
  std::printf("  g     | PR over time (start -> end)\n");
  for(double gk : {0.0, 30.0, 60.0, 120.0}){
    auto [pr0,tr]=run(gk);
    std::printf("  %-5.0f | %.1f ", gk, pr0);
    for(double p:tr) std::printf("-> %.1f ",p);
    std::printf("\n");
  }
  std::printf("\nIf PR shrinks for g>0 while it grows for g=0, the Kerr nonlinearity physically\nFOCUSED the energy into a soliton -- a real compressor from the wave acting on itself.\n");
  return 0;
}
