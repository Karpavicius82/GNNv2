// probe_nonlinear_engine
// ----------------------------------------------------------------------------
// Driver for the CLOSED nonlinear GNNv2 engine (tools/graph_wave_nonlinear_engine.hpp).
// The engine is label-agnostic; this driver owns the stream shape (topics) and is the
// ONLY place topic labels exist -- used purely as an after-the-fact oracle, never to
// gate physics.
//
// Closure gates (physics, scale-robust only):
//   - psi densifies (content compression vs the linear reference)
//   - chi consolidates MORE than psi (the slow layer is tighter)
//   - tau senses structure (structured stream >> structureless stream)
//   - field stays bounded
// chi/psi relational edge structure is REPORTED (not gated): it is a small-data effect
// that fades as psi self-organizes at scale -- so it is shown, not asserted.
// ----------------------------------------------------------------------------
#define NOMINMAX
#include "../tools/graph_wave_nonlinear_engine.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr int kTopics = 3, kPerTopic = 24, kBaseNodes = kTopics * kPerTopic;
constexpr int kUniqueEvery = 7;

// same structured same-topic-burst stream for both the real engine and the scrambled-edge
// control (the control destroys structure inside the engine, not in the token order).
void run(gwn::Engine& eng, int stream) {
  unsigned rng = 11u;
  auto nxt = [&]() { rng = 1664525u * rng + 1013904223u; return rng; };
  for (int i = 0; i < stream;) {
    int tp = (int)(nxt() % kTopics);
    eng.beginContext();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) {
      int node = tp * kPerTopic + (int)(nxt() % kPerTopic);
      if (kUniqueEvery > 0 && i % kUniqueEvery == 0) node = eng.addNode();
      eng.process(node);
    }
  }
}

// relational structure readout: mean edge visibility (phase coherence on existing edges),
// same-topic minus cross-topic, for psi and chi. Topic = id/kPerTopic (oracle only).
void edgeVisibility(const gwn::Engine& eng, double& psiSep, double& chiSep) {
  double pS = 0, pC = 0, cS = 0, cC = 0; int nS = 0, nC = 0;
  for (int a = 0; a < kBaseNodes; ++a) {
    const gwn::Node& na = eng.nodeAt(a);
    for (int s = 0; s < na.deg; ++s) {
      int b = na.e[s].to; if (b <= a || b >= kBaseNodes) continue;
      const gwn::Node& nb = eng.nodeAt(b);
      double op = a < b ? na.e[s].phase : -na.e[s].phase;
      double vp = na.e[s].w * std::sqrt(std::max(0.0, na.psi.rho * nb.psi.rho)) * std::max(0.0, std::cos(gwn::wrapPhase(nb.psi.phi - na.psi.phi + op)));
      double vc = na.e[s].w * std::sqrt(std::max(0.0, na.chi.rho * nb.chi.rho)) * std::max(0.0, std::cos(gwn::wrapPhase(nb.chi.phi - na.chi.phi + op)));
      if (a / kPerTopic == b / kPerTopic) { pS += vp; cS += vc; ++nS; } else { pC += vp; cC += vc; ++nC; }
    }
  }
  if (nS) { pS /= nS; cS /= nS; } if (nC) { pC /= nC; cC /= nC; }
  psiSep = pS - pC; chiSep = cS - cC;
}

// long-range chi phase resonance between non-connected stable hubs (the emergent association)
void longRangeResonance(const gwn::Engine& eng, double& same, double& cross) {
  double s = 0, c = 0; int ns = 0, nc = 0;
  for (int a = 0; a < kBaseNodes; ++a) {
    if (!eng.isStable(a)) continue;
    for (int b = a + 1; b < kBaseNodes; ++b) {
      if (!eng.isStable(b)) continue;
      bool connected = false;
      const gwn::Node& na = eng.nodeAt(a);
      for (int k = 0; k < na.deg; ++k) if (na.e[k].to == b) { connected = true; break; }
      if (connected) continue;
      const gwn::Node& nb = eng.nodeAt(b);
      double res = std::sqrt(std::max(0.0, na.chi.rho * nb.chi.rho)) * std::cos(gwn::wrapPhase(na.chi.phi - nb.chi.phi));
      if (a / kPerTopic == b / kPerTopic) { s += res; ++ns; } else { c += res; ++nc; }
    }
  }
  if (ns) s /= ns; if (nc) c /= nc; same = s; cross = c;
}

bool gate(const char* name, bool ok) { std::printf("   %s  %s\n", ok ? "PASS" : "FAIL", name); return ok; }
}  // namespace

int main(int argc, char** argv) {
  int stream = argc > 1 ? std::atoi(argv[1]) : 60000;

  gwn::Config scrambled; scrambled.scrambleEdges = true;
  gwn::Engine eng;
  gwn::Engine rnd(scrambled);
  auto t0 = std::chrono::steady_clock::now();
  run(eng, stream);
  auto t1 = std::chrono::steady_clock::now();
  run(rnd, std::min(stream, 200000));

  const gwn::Stats& st = eng.stats();
  double psiSep, chiSep, lrSame, lrCross;
  edgeVisibility(eng, psiSep, chiSep);
  longRangeResonance(eng, lrSame, lrCross);
  double sec = std::chrono::duration<double>(t1 - t0).count();

  std::printf("=====================================================================\n");
  std::printf("  CLOSED NONLINEAR GNNv2 ENGINE  (polar, synchronous, lin+psi+chi+tau)\n");
  std::printf("=====================================================================\n");
  std::printf("  stream=%d nodes=%d stable=%lld horizons=%lld  %.0f tok/s\n",
              stream, eng.nodeCount(), st.stable, st.horizons, stream / std::max(sec, 1e-9));
  std::printf("  compression  psi=%.2fx  chi=%.2fx   max_rho=%.2f\n", st.psiCompression(), st.chiCompression(), st.maxRho);
  std::printf("  tau  structured=%.3f  random=%.3f  (%.1fx)\n", st.meanTau(), rnd.stats().meanTau(), st.meanTau() / (rnd.stats().meanTau() + 1e-9));
  std::printf("  relational edge visibility (same-cross)  psi=%.5f  chi=%.5f\n", psiSep, chiSep);
  std::printf("  long-range chi resonance (non-connected stable hubs)  same=%.4f  cross=%.4f\n", lrSame, lrCross);

  int p = 0, t = 0;
  ++t; p += gate("psi densifies (content compression > 2.5x)", st.psiCompression() > 2.5);
  ++t; p += gate("chi consolidates MORE than psi", st.chiCompression() > st.psiCompression());
  ++t; p += gate("tau senses structure (structured >> random)", st.meanTau() > 5.0 * rnd.stats().meanTau());
  ++t; p += gate("field bounded (no blowup)", st.maxRho < 60.0);
  std::printf("\n  CLOSURE : %d / %d physics gates\n", p, t);
  std::printf("  RESULT : %d / %d\n", p, t);
  return p == t ? 0 : 1;
}
