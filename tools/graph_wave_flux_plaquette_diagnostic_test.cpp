// graph_wave_flux_plaquette_diagnostic_test
// ----------------------------------------------------------------------------
// DIAGNOSTIC (gathers data, returns 0): what actually happens on the 4-node flux
// plaquette? The flux-GNN contract took a single snapshot; here we look at the
// real dynamics so we are not fooling ourselves:
// A) flux sweep -- node-2 reachability (max population over time) vs flux.
// B) full time traces of all four node populations at flux 0, pi, golden.
// C) is node 2 REALLY decoupled at flux=pi for all time, or only at the snapshot?
// D) snapshot stability -- does the single-time reading swing with the time?
// Built on the shared substrate core (graph_wave_substrate.hpp). No decision, no
// learning, no trainer.
// ----------------------------------------------------------------------------
#include <array>
#include <cstdio>
#include <string>
#include "graph_wave_substrate.hpp"
namespace {
using namespace gw;
const double kGolden = 2 * kPi * 8.0 / 13.0;
// plaquette 0-1-2-3-0; flux carried on edge 3->0
Graph plaquette(double flux) { Graph G(4); G.addEdge(0, 1, 1, 0); G.addEdge(1, 2, 1, 0); G.addEdge(2, 3, 1, 0); G.addEdge(3, 0, 1, flux); return G; }
// record per-node populations at each step; start at node 0
std::vector<std::array<double, 4>> trace(double flux, double dt, int n) {
 Graph G = plaquette(flux); Stepper S; S.build(G.h, dt); Vec psi(4, cd(0, 0)); psi[0] = cd(1, 0);
 std::vector<std::array<double, 4>> out;
 for (int s = 0; s < n; s++) { psi = S.step(psi); std::array<double, 4> p; for (int i = 0; i < 4; i++) p[i] = std::norm(psi[i]); out.push_back(p); }
 return out;
}
double maxP2(double flux, double dt, int n) { double m = 0; for (auto& p : trace(flux, dt, n)) m = std::max(m, p[2]); return m; }
}
int main() {
 std::printf("=====================================================================\n");
 std::printf(" GRAPH-WAVE FLUX-PLAQUETTE DIAGNOSTIC (what really happens with 4 nodes)\n");
 std::printf("=====================================================================\n");
 const double dt = 0.1; const int N = 200; // t up to 20, fine resolution

 std::printf("\n[A] FLUX SWEEP -- node-2 reachability (max population over time t<=20)\n");
 std::printf(" flux/pi | maxP2 bar\n");
 for (int k = 0; k <= 12; k++) { double f = k * (2 * kPi / 12.0); double m = maxP2(f, dt, N);
 int bars = (int)(m * 40 + 0.5); std::string b(bars, '#');
 std::printf(" %6.3f | %.4f %s\n", f / kPi, m, b.c_str()); }
 std::printf(" (golden flux/pi = %.4f)\n", kGolden / kPi);
 { double g = maxP2(kGolden, dt, N); std::printf(" golden maxP2 = %.4f\n", g); }

 std::printf("\n[B] TIME TRACES of node populations [n0 n1 n2 n3] (dt=%.2f)\n", dt);
 for (double f : { 0.0, kPi, kGolden }) {
 const char* nm = (f == 0.0 ? "flux=0 " : (f == kPi ? "flux=pi " : "golden "));
 std::printf(" --- %s ---\n", nm);
 auto tr = trace(f, dt, N);
 for (int s = 9; s < N; s += 20) { auto& p = tr[s]; std::printf(" t=%5.2f [%.3f %.3f %.3f %.3f]\n", (s + 1) * dt, p[0], p[1], p[2], p[3]); }
 }

 std::printf("\n[C] IS NODE 2 REALLY DECOUPLED AT flux=pi FOR ALL TIME?\n");
 { double m = maxP2(kPi, dt, N); std::printf(" max node-2 population over t<=20 at flux=pi = %.2e\n", m);
 std::printf(" => node 2 is %s by destructive interference (not just at the snapshot)\n", m < 1e-9 ? "FULLY decoupled" : "only partially suppressed"); }

 std::printf("\n[D] SNAPSHOT STABILITY -- node-2 population vs evolution time (the contract reads ONE time)\n");
 std::printf(" steps | P2(flux=0) P2(golden) P2(flux=pi)\n");
 for (int ns : { 6, 12, 20, 40, 80 }) { double dt2 = 0.25;
 auto t0 = trace(0.0, dt2, ns), tg = trace(kGolden, dt2, ns), tp = trace(kPi, dt2, ns);
 std::printf(" %5d | %.4f %.4f %.4f\n", ns, t0.back()[2], tg.back()[2], tp.back()[2]); }
 std::printf(" (constructive at 0 and decoupled at pi are robust; the golden SNAPSHOT value moves with time)\n");

 std::printf("\n RESULT : DIAGNOSTIC (data gathered, not a pass/fail gate)\n");
 return 0;
}
