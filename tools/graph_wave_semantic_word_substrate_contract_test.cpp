// graph_wave_semantic_word_substrate_contract_test
// ----------------------------------------------------------------------------
// Polished after the wave-fidelity audit: word -> substrate conversion must not
// throw away phase at the readout boundary. A word is still injected as a REAL
// semantic pattern (co-occurrence row, not a node id), and order is still born
// only through unitary evolution between injections. The canonical signal is now
// the COMPLETE complex field psi. Edge-current / magnitude projections are only
// diagnostics: they can lose energy and under-read order.
//
// Contract:
//   [1] word patterns carry topic meaning;
//   [2] no evolution collapses to an order-blind bag;
//   [3] complete phase-preserving template readout recovers word order;
//   [4] magnitude-only projection is order-weak while full complex readout works;
//   [5] real semantic substrate beats matched-random with the SAME full readout;
//   [6] full-signal overlaps are gauge-invariant observables.
// ----------------------------------------------------------------------------
#include "graph_wave_substrate.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>
using namespace gw;

namespace {
const int N = 12;  // 3 topics x 4 words: royalty(0-3) ocean(4-7) forest(8-11)
const int L = 3;
const std::vector<std::vector<int>> SENT = {
    {0, 1, 2}, {1, 2, 3}, {0, 3, 1}, {2, 0, 3},
    {4, 5, 6}, {5, 6, 7}, {4, 7, 5}, {6, 4, 7},
    {8, 9, 10}, {9, 10, 11}, {8, 11, 9}, {10, 8, 11}};

int topic(int w) { return w / 4; }

double norm2(const Vec& z) {
  double s = 0.0;
  for (const auto& v : z) s += std::norm(v);
  return s;
}

Vec normalized(Vec z) {
  double n = std::sqrt(norm2(z)) + 1e-15;
  for (auto& v : z) v /= n;
  return z;
}

cd inner(const Vec& a, const Vec& b) {
  cd s(0, 0);
  for (int i = 0; i < N; i++) s += std::conj(a[i]) * b[i];
  return s;
}

double overlapPower(const Vec& a, const Vec& b) {
  cd z = inner(a, b);
  return std::norm(z) / (norm2(a) * norm2(b) + 1e-15);
}

double cosv(const std::vector<double>& a, const std::vector<double>& b) {
  double d = 0.0, na = 0.0, nb = 0.0;
  for (size_t i = 0; i < a.size(); i++) {
    d += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  return d / (std::sqrt(na * nb) + 1e-15);
}

std::vector<std::vector<double>> embeddings() {
  std::vector<std::vector<double>> m(N, std::vector<double>(N, 0.0));
  for (const auto& s : SENT) {
    for (int a : s) {
      for (int b : s) {
        if (a != b) m[a][b] += 1.0;
      }
    }
  }
  for (auto& r : m) {
    double n = 0.0;
    for (double v : r) n += v * v;
    n = std::sqrt(n) + 1e-9;
    for (double& v : r) v /= n;
  }
  return m;
}

Graph substrate() {
  Graph g(N);
  for (int t = 0; t < 3; t++) {
    int b = 4 * t;
    for (int i = 0; i < 4; i++) {
      for (int j = i + 1; j < 4; j++) {
        g.addEdge(b + i, b + j, 1.0);
      }
    }
  }
  return g;
}

Graph matchedRandomSubstrate() {
  Graph g(N);
  const int edge_count = 18;  // same as three K4 semantic clusters
  const int a[edge_count] = {0, 0, 0, 1, 1, 2, 4, 4, 4, 5, 5, 6, 8, 8, 8, 9, 9, 10};
  const int b[edge_count] = {5, 7, 10, 4, 8, 11, 1, 3, 9, 0, 10, 2, 3, 6, 11, 2, 7, 4};
  for (int e = 0; e < edge_count; e++) g.addEdge(a[e], b[e], 1.0);
  return g;
}

Vec wordPattern(const std::vector<std::vector<double>>& emb, int w) {
  Vec p(N, cd(0, 0));
  for (int i = 0; i < N; i++) p[i] = cd(emb[w][i], 0.0);
  return p;
}

Vec drivePhrase(const CMat& h, const std::vector<std::vector<double>>& emb,
                const std::vector<int>& phrase, double tau) {
  Stepper st;
  st.build(h, tau);
  Vec psi(N, cd(0, 0));
  bool has_field = false;
  for (int w : phrase) {
    if (has_field) psi = st.step(psi);
    Vec p = wordPattern(emb, w);
    for (int i = 0; i < N; i++) psi[i] += p[i];
    has_field = true;
  }
  psi = st.step(psi);
  return normalized(psi);
}

std::vector<double> bag(const std::vector<std::vector<double>>& emb, const std::vector<int>& phrase) {
  std::vector<double> v(N, 0.0);
  for (int w : phrase) {
    for (int i = 0; i < N; i++) v[i] += emb[w][i];
  }
  return v;
}

std::vector<double> magnitude(const Vec& z) {
  std::vector<double> out(N);
  for (int i = 0; i < N; i++) out[i] = std::norm(z[i]);
  return out;
}

std::vector<std::vector<double>> cueEmbeddings(const std::vector<std::vector<double>>& emb) {
  std::vector<std::vector<double>> cue(N, std::vector<double>(N, 0.0));
  for (int w = 0; w < N; w++) {
    int best = -1;
    double bs = -1.0;
    for (int i = 0; i < N; i++) {
      if (emb[w][i] > bs) {
        bs = emb[w][i];
        best = i;
      }
    }
    cue[w][best] = bs;
  }
  return cue;
}

struct Candidate {
  std::vector<int> phrase;
  Vec signal;
};

std::vector<std::vector<int>> candidatePhrases(const std::vector<std::vector<int>>& probes) {
  std::vector<std::vector<int>> out;
  for (const auto& p : probes) {
    out.push_back(p);
    out.push_back(std::vector<int>(p.rbegin(), p.rend()));
  }
  return out;
}

std::vector<Candidate> buildDictionary(const CMat& h, const std::vector<std::vector<double>>& emb,
                                       const std::vector<std::vector<int>>& phrases, double tau) {
  std::vector<Candidate> dict;
  dict.reserve(phrases.size());
  for (const auto& p : phrases) dict.push_back({p, drivePhrase(h, emb, p, tau)});
  return dict;
}

int findCandidate(const std::vector<Candidate>& dict, const std::vector<int>& phrase) {
  for (int i = 0; i < (int)dict.size(); i++) {
    if (dict[i].phrase == phrase) return i;
  }
  return -1;
}

struct Decode {
  int best = -1;
  double bestScore = -1.0;
  double secondScore = -1.0;
};

Decode decodeComplete(const std::vector<Candidate>& dict, const Vec& query) {
  Decode d;
  for (int i = 0; i < (int)dict.size(); i++) {
    double s = overlapPower(dict[i].signal, query);
    if (s > d.bestScore) {
      d.secondScore = d.bestScore;
      d.bestScore = s;
      d.best = i;
    } else if (s > d.secondScore) {
      d.secondScore = s;
    }
  }
  return d;
}

std::string phraseName(const std::vector<int>& p) {
  std::string s;
  for (int i = 0; i < (int)p.size(); i++) {
    if (i) s += ",";
    s += std::to_string(p[i]);
  }
  return s;
}

double semanticMargin(const Graph& template_graph, const Graph& query_graph, const std::vector<std::vector<double>>& clean,
                      const std::vector<std::vector<double>>& cue,
                      const std::vector<std::vector<int>>& phrases, double tau) {
  std::vector<Candidate> clean_dict = buildDictionary(template_graph.h, clean, phrases, tau);
  double margin = 0.0;
  int count = 0;
  for (const auto& q : phrases) {
    Vec signal = drivePhrase(query_graph.h, cue, q, tau);
    int qt = topic(q[0]);
    double best_same = -1.0, best_cross = -1.0;
    for (const auto& cand : clean_dict) {
      double s = overlapPower(cand.signal, signal);
      if (topic(cand.phrase[0]) == qt) best_same = std::max(best_same, s);
      else best_cross = std::max(best_cross, s);
    }
    margin += best_same - best_cross;
    count++;
  }
  return margin / count;
}

Vec gauge(Vec z, const std::vector<double>& theta) {
  for (int i = 0; i < N; i++) z[i] *= std::exp(cd(0, theta[i]));
  return z;
}

bool report(const char* n, bool ok) {
  std::printf(" => %s\n", ok ? "PASS" : "FAIL");
  if (!ok) std::printf(" !! %s\n", n);
  return ok;
}
}  // namespace

int main() {
  std::printf("=====================================================================\n");
  std::printf(" SEMANTIC WORD -> COMPLETE SIGNAL CONTRACT\n");
  std::printf(" real word patterns; order born by evolution; readout preserves phase\n");
  std::printf("=====================================================================\n");

  auto emb = embeddings();
  Graph g = substrate();
  Graph rg = matchedRandomSubstrate();
  const CMat& h = g.h;
  const double tau = 0.45;
  std::vector<std::vector<int>> probes = {
      {0, 1, 2}, {3, 0, 1}, {4, 5, 6}, {7, 4, 5}, {8, 9, 10}, {11, 8, 9}};
  std::vector<std::vector<int>> phrases = candidatePhrases(probes);

  int pass = 0, total = 0;

  std::printf("\n[1] WORD PATTERNS CARRY MEANING: same-topic words closer than cross-topic\n");
  double intra = 0.0, inter = 0.0;
  int ni = 0, nx = 0;
  for (int a = 0; a < N; a++) {
    for (int b = a + 1; b < N; b++) {
      double c = cosv(emb[a], emb[b]);
      if (topic(a) == topic(b)) {
        intra += c;
        ni++;
      } else {
        inter += c;
        nx++;
      }
    }
  }
  intra /= ni;
  inter /= nx;
  std::printf(" mean cos intra=%.3f  inter=%.3f\n", intra, inter);
  ++total;
  pass += report("word pattern is not semantic", intra > inter + 0.15);

  std::printf("\n[2] NO EVOLUTION = BAG: real injection alone is order-blind\n");
  double tau0Diff = 0.0, bagSim = 0.0;
  for (const auto& p : probes) {
    std::vector<int> r(p.rbegin(), p.rend());
    Vec a = drivePhrase(h, emb, p, 0.0);
    Vec b = drivePhrase(h, emb, r, 0.0);
    tau0Diff = std::max(tau0Diff, std::abs(1.0 - overlapPower(a, b)));
    bagSim += cosv(bag(emb, p), bag(emb, r));
  }
  bagSim /= probes.size();
  std::printf(" tau=0 field reverse diff=%.2e  bag forward/reverse cos=%.4f\n", tau0Diff, bagSim);
  ++total;
  pass += report("order appeared without unitary evolution", tau0Diff < 1e-12 && bagSim > 0.9999);

  std::printf("\n[3] COMPLETE PHASE-PRESERVING READOUT: exact phrase order decodes\n");
  auto dict = buildDictionary(h, emb, phrases, tau);
  int ok = 0;
  double minMargin = 10.0;
  for (const auto& p : probes) {
    std::vector<std::vector<int>> qs = {p, std::vector<int>(p.rbegin(), p.rend())};
    for (const auto& q : qs) {
      Decode d = decodeComplete(dict, drivePhrase(h, emb, q, tau));
      int truth = findCandidate(dict, q);
      bool hit = (d.best == truth);
      ok += hit;
      minMargin = std::min(minMargin, d.bestScore - d.secondScore);
      std::printf("  query [%s] -> [%s]  |<q|t>|^2=%.6f second=%.6f margin=%.3e %s\n",
                  phraseName(q).c_str(), phraseName(dict[d.best].phrase).c_str(),
                  d.bestScore, d.secondScore, d.bestScore - d.secondScore, hit ? "OK" : "X");
    }
  }
  ++total;
  pass += report("complete complex signal did not decode exact phrase order",
                 ok == 2 * (int)probes.size() && minMargin > 1e-4);

  std::printf("\n[4] PARTIAL PROJECTION UNDER-READS ORDER: magnitude is almost a bag\n");
  double magSim = 0.0, fullSim = 0.0;
  for (const auto& p : probes) {
    std::vector<int> r(p.rbegin(), p.rend());
    Vec a = drivePhrase(h, emb, p, tau);
    Vec b = drivePhrase(h, emb, r, tau);
    magSim += cosv(magnitude(a), magnitude(b));
    fullSim += overlapPower(a, b);
  }
  magSim /= probes.size();
  fullSim /= probes.size();
  std::printf(" forward vs reverse avg: magnitude cos=%.4f  full |<a|b>|^2=%.4f\n", magSim, fullSim);
  ++total;
  pass += report("full field did not separate order better than magnitude projection",
                 magSim > 0.90 && fullSim < magSim - 0.02);

  std::printf("\n[5] SEMANTIC SUBSTRATE VALUE: real > matched-random with the same complete readout\n");
  auto cue = cueEmbeddings(emb);
  double realMargin = semanticMargin(g, g, emb, cue, phrases, tau);
  double randMargin = semanticMargin(g, rg, emb, cue, phrases, tau);
  std::printf(" cue-corrupted semantic margin: real=%.4f  matched-random=%.4f  delta=%.4f\n",
              realMargin, randMargin, realMargin - randMargin);
  ++total;
  pass += report("complete readout worked but did not isolate semantic substrate value",
                 realMargin > randMargin + 0.08 && realMargin > 0.10);

  std::printf("\n[6] GAUGE INVARIANCE: |<psi_a|psi_b>|^2 overlaps are observables\n");
  std::vector<Vec> sig;
  for (const auto& p : probes) sig.push_back(drivePhrase(h, emb, p, tau));
  std::vector<double> theta(N);
  for (int i = 0; i < N; i++) theta[i] = 2.3 * std::sin(0.17 * i + 0.4);
  double gd = 0.0;
  for (int i = 0; i < (int)sig.size(); i++) {
    for (int j = 0; j < (int)sig.size(); j++) {
      double native = overlapPower(sig[i], sig[j]);
      double gauged = overlapPower(gauge(sig[i], theta), gauge(sig[j], theta));
      gd = std::max(gd, std::abs(native - gauged));
    }
  }
  std::printf(" max pairwise overlap diff under local gauge = %.2e\n", gd);
  ++total;
  pass += report("full complex readout changed under gauge", gd < 1e-12);

  std::printf("\n RESULT : %d / %d verified\n", pass, total);
  return pass == total ? 0 : 1;
}
