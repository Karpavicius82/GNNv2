// graph_wave_synchronous_phase_field_contract_test
// ----------------------------------------------------------------------------
// Local phase-field contract without Vec/readout projection.
//
// The field lives on nodes and edges:
//   rho  - local energy density
//   phi  - local phase
//   edge - coupling + gauge phase
//
// Synchronism gate:
//   every edge in the local light cone is observed from the same snapshot, then
//   all rho/phi changes are applied together. Reversing edge traversal must not
//   change the final field.
// ----------------------------------------------------------------------------
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kTopics = 3;
constexpr int kPerTopic = 24;
constexpr int kBaseNodes = kTopics * kPerTopic;
constexpr int kDefaultStream = 60000;
constexpr int kUniqueEvery = 7;
constexpr int kMaxNodes = 200000;
constexpr int kTopK = 6;
constexpr int kWindow = 4;
constexpr int kMaxLocal = 96;
constexpr int kMaxPairs = 384;
constexpr int kSMin = 4;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kEta = 1.0;
constexpr double kDecay = 0.99;
constexpr double kEdgeFloor = 1e-4;
constexpr double kWMin = 8.0;
constexpr double kInject = 0.08;
constexpr double kDt = 0.012;
constexpr double kPsiG = 7.0;
constexpr double kChiG = 30.0;
constexpr double kLeak = 0.9955;
constexpr double kTauDecay = 0.97;
constexpr double kTauLearn = 0.03;
constexpr double kFeelInject = 0.025;

enum FieldKind { kLin = 0, kPsi = 1, kChi = 2 };

struct Field {
  double rho = 1e-9;
  double phi = 0.0;
};

struct Edge {
  int to = -1;
  double w = 0.0;
  double phase = 0.0;
  int last = 0;
};

struct Node {
  Edge e[kTopK];
  int deg = 0;
  int seen = 0;
  int topic = -1;
  bool novel = false;
  Field lin;
  Field psi;
  Field chi;
  double tauRho = 0.0;
  double tauPhi = 0.0;
};

struct Pair {
  int ia = 0;
  int ib = 0;
  int a = 0;
  int b = 0;
  double w = 0.0;
  double phaseAB = 0.0;
};

double wrap(double x) {
  while (x > kPi) x -= 2.0 * kPi;
  while (x < -kPi) x += 2.0 * kPi;
  return x;
}

double clampAbs(double x, double cap) {
  return std::max(-cap, std::min(cap, x));
}

unsigned hash3(int a, int b, int c) {
  unsigned x = 2166136261u;
  x = (x ^ (unsigned)a) * 16777619u;
  x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u;
  x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u;
  return x;
}

double seededPhase(int a, int b) {
  unsigned h = hash3(std::min(a, b), std::max(a, b), 0x51u);
  double u = ((h % 2001u) / 2000.0) - 0.5;
  return 0.08 * u;
}

struct Metrics {
  long long updates = 0;
  long long stable = 0;
  long long horizons = 0;
  long long novelHorizons = 0;
  double sumLinPr = 0.0;
  double sumPsiPr = 0.0;
  double sumChiPr = 0.0;
  double sumTau = 0.0;
  double maxRho = 0.0;
  double sameEdgeVis = 0.0;
  double crossEdgeVis = 0.0;
  int sameN = 0;
  int crossN = 0;
};

struct System {
  Node* node = nullptr;
  int count = 0;
  int t = 0;
  int win[kWindow];
  int winN = 0;
  bool reversePairs = false;
  bool randomEdges = false;
  Metrics m;

  explicit System(bool reverse, bool randomize = false)
      : node(new Node[kMaxNodes]), reversePairs(reverse), randomEdges(randomize) {
    for (int tp = 0; tp < kTopics; ++tp) {
      for (int w = 0; w < kPerTopic; ++w) add(tp, false);
    }
  }

  ~System() { delete[] node; }

  int add(int topic, bool novel) {
    if (count >= kMaxNodes) {
      std::printf("  !! node capacity exceeded (%d)\n", kMaxNodes);
      std::exit(2);
    }
    int id = count++;
    node[id].topic = topic;
    node[id].novel = novel;
    double ph = 0.17 * (double)(hash3(id, topic + 1, 7) % 37);
    node[id].lin.phi = ph;
    node[id].psi.phi = ph;
    node[id].chi.phi = ph;
    node[id].tauPhi = ph;
    return id;
  }

  Field& field(int id, FieldKind k) {
    if (k == kLin) return node[id].lin;
    if (k == kPsi) return node[id].psi;
    return node[id].chi;
  }

  const Field& field(int id, FieldKind k) const {
    if (k == kLin) return node[id].lin;
    if (k == kPsi) return node[id].psi;
    return node[id].chi;
  }

  int findEdge(int a, int b) const {
    for (int i = 0; i < node[a].deg; ++i) {
      if (node[a].e[i].to == b) return i;
    }
    return -1;
  }

  void removeDirected(int a, int b) {
    int s = findEdge(a, b);
    if (s < 0) return;
    node[a].e[s] = node[a].e[node[a].deg - 1];
    node[a].deg--;
  }

  int slotFor(int a, int b) {
    int s = findEdge(a, b);
    if (s >= 0) return s;
    if (node[a].deg < kTopK) return node[a].deg++;
    int weakest = 0;
    for (int i = 1; i < kTopK; ++i) {
      if (node[a].e[i].w < node[a].e[weakest].w) weakest = i;
    }
    int old = node[a].e[weakest].to;
    if (old >= 0) removeDirected(old, a);
    return weakest;
  }

  void decayEdge(int a, int slot) {
    Edge& e = node[a].e[slot];
    int dt = t - e.last;
    if (dt > 0) {
      e.w *= std::pow(kDecay, dt);
      e.last = t;
      int back = findEdge(e.to, a);
      if (back >= 0) {
        node[e.to].e[back].w = e.w;
        node[e.to].e[back].last = t;
      }
    }
  }

  void relaxNode(int a) {
    for (int i = 0; i < node[a].deg;) {
      decayEdge(a, i);
      int b = node[a].e[i].to;
      if (node[a].e[i].w < kEdgeFloor) {
        removeDirected(b, a);
        removeDirected(a, b);
      } else {
        ++i;
      }
    }
  }

  void touchDirected(int a, int b, double phase) {
    int s = slotFor(a, b);
    node[a].e[s].to = b;
    node[a].e[s].w += kEta;
    node[a].e[s].phase = phase;
    node[a].e[s].last = t;
  }

  void touch(int a, int b) {
    if (a == b) return;
    if (randomEdges) {
      int r = (int)(hash3(a, b, t) % (unsigned)count);
      b = r == a ? (r + 1) % count : r;
    }
    relaxNode(a);
    relaxNode(b);
    double ph = seededPhase(a, b);
    touchDirected(a, b, ph);
    touchDirected(b, a, ph);
  }

  double orientedPhase(int a, const Edge& e) const {
    return a < e.to ? e.phase : -e.phase;
  }

  bool contains(const int* ids, int n, int id) const {
    for (int i = 0; i < n; ++i) {
      if (ids[i] == id) return true;
    }
    return false;
  }

  int localIndex(const int* ids, int n, int id) const {
    for (int i = 0; i < n; ++i) {
      if (ids[i] == id) return i;
    }
    return -1;
  }

  int gatherLocal(int src, int* ids) const {
    int n = 0;
    ids[n++] = src;
    for (int i = 0; i < node[src].deg && n < kMaxLocal; ++i) {
      int v = node[src].e[i].to;
      if (v >= 0 && !contains(ids, n, v)) ids[n++] = v;
    }
    for (int scan = 1; scan < n && n < kMaxLocal; ++scan) {
      int u = ids[scan];
      for (int i = 0; i < node[u].deg && n < kMaxLocal; ++i) {
        int v = node[u].e[i].to;
        if (v >= 0 && !contains(ids, n, v)) ids[n++] = v;
      }
    }
    return n;
  }

  int gatherPairs(const int* ids, int n, Pair* pairs) const {
    int pc = 0;
    for (int i = 0; i < n; ++i) {
      int a = ids[i];
      for (int s = 0; s < node[a].deg && pc < kMaxPairs; ++s) {
        const Edge& e = node[a].e[s];
        int j = localIndex(ids, n, e.to);
        if (j > i) {
          pairs[pc++] = {i, j, a, e.to, e.w, orientedPhase(a, e)};
        }
      }
    }
    return pc;
  }

  void canonicalizePairs(Pair* pairs, int pc) const {
    if (reversePairs) {
      for (int i = 0; i < pc / 2; ++i) std::swap(pairs[i], pairs[pc - 1 - i]);
    }
    std::sort(pairs, pairs + pc, [](const Pair& x, const Pair& y) {
      int xa = std::min(x.a, x.b), xb = std::max(x.a, x.b);
      int ya = std::min(y.a, y.b), yb = std::max(y.a, y.b);
      if (xa != ya) return xa < ya;
      return xb < yb;
    });
  }

  void inject(int src) {
    Field& l = node[src].lin;
    Field& p = node[src].psi;
    l.rho += kInject / (1.0 + l.rho);
    p.rho += kInject / (1.0 + p.rho);
  }

  void flowStep(const int* ids, int n, const Pair* pairs, int pc, FieldKind kind, double g) {
    double r0[kMaxLocal], p0[kMaxLocal], dr[kMaxLocal], dp[kMaxLocal];
    for (int i = 0; i < n; ++i) {
      const Field& f = field(ids[i], kind);
      r0[i] = f.rho;
      p0[i] = f.phi;
      dr[i] = 0.0;
      dp[i] = -0.25 * g * r0[i] * kDt;
    }

    for (int kk = 0; kk < pc; ++kk) {
      const Pair& e = pairs[kk];
      int a = e.ia, b = e.ib;
      double root = std::sqrt(std::max(0.0, r0[a] * r0[b]));
      double theta = wrap(p0[b] - p0[a] + e.phaseAB);
      double current = 2.0 * e.w * root * std::sin(theta);
      if (g > 0.0) current += 0.060 * g * e.w * (r0[a] - r0[b]);
      double flow = kDt * current;
      double cap = flow >= 0.0 ? 0.06 * r0[b] : 0.06 * r0[a];
      flow = clampAbs(flow, cap + 1e-18);
      dr[a] += flow;
      dr[b] -= flow;

      double ca = -0.03 * kDt * e.w * std::cos(theta) *
                  std::sqrt((r0[b] + 1e-12) / (r0[a] + 1e-12));
      double cb = -0.03 * kDt * e.w * std::cos(theta) *
                  std::sqrt((r0[a] + 1e-12) / (r0[b] + 1e-12));
      dp[a] += clampAbs(ca, 0.08);
      dp[b] += clampAbs(cb, 0.08);
    }

    for (int i = 0; i < n; ++i) {
      Field& f = field(ids[i], kind);
      f.rho = std::max(0.0, kLeak * r0[i] + dr[i]);
      f.phi = wrap(p0[i] + dp[i]);
      m.maxRho = std::max(m.maxRho, f.rho);
    }
  }

  void updateTauAndChi(const int* ids, int n, const Pair* pairs, int pc,
                       const double* beforeR, const double* beforeP) {
    double drive[kMaxLocal], sinAcc[kMaxLocal], cosAcc[kMaxLocal];
    for (int i = 0; i < n; ++i) {
      const Field& p = node[ids[i]].psi;
      double motion = std::abs(p.rho - beforeR[i]);
      double twist = std::abs(wrap(p.phi - beforeP[i])) *
                     std::sqrt(std::max(0.0, p.rho * beforeR[i]));
      drive[i] = motion + twist;
      sinAcc[i] = drive[i] * std::sin(p.phi);
      cosAcc[i] = drive[i] * std::cos(p.phi);
    }

    for (int kk = 0; kk < pc; ++kk) {
      const Pair& e = pairs[kk];
      const Field& a = node[e.a].psi;
      const Field& b = node[e.b].psi;
      double root = std::sqrt(std::max(0.0, a.rho * b.rho));
      double theta = wrap(b.phi - a.phi + e.phaseAB);
      double tension = e.w * root * std::abs(std::sin(theta));
      drive[e.ia] += 0.5 * tension;
      drive[e.ib] += 0.5 * tension;
      sinAcc[e.ia] += tension * std::sin(b.phi + e.phaseAB);
      cosAcc[e.ia] += tension * std::cos(b.phi + e.phaseAB);
      sinAcc[e.ib] += tension * std::sin(a.phi - e.phaseAB);
      cosAcc[e.ib] += tension * std::cos(a.phi - e.phaseAB);
    }

    for (int i = 0; i < n; ++i) {
      Node& nd = node[ids[i]];
      double target = std::atan2(sinAcc[i], cosAcc[i]);
      nd.tauRho = kTauDecay * nd.tauRho + kTauLearn * drive[i];
      nd.tauPhi = wrap(nd.tauPhi + 0.22 * wrap(target - nd.tauPhi));

      double pulse = kFeelInject * nd.tauRho / (1.0 + nd.tauRho);
      nd.chi.rho += pulse;
      nd.chi.phi = wrap(nd.chi.phi + 0.35 * wrap(nd.tauPhi - nd.chi.phi));
      m.sumTau += nd.tauRho;
    }
  }

  double localPr(const int* ids, int n, FieldKind kind) const {
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i) {
      double r = field(ids[i], kind).rho;
      s1 += r;
      s2 += r * r;
    }
    return (s1 * s1) / (s2 + 1e-300);
  }

  double incident(int a) const {
    double s = 0.0;
    for (int i = 0; i < node[a].deg; ++i) s += node[a].e[i].w;
    return s;
  }

  void physicalTick(int src) {
    int ids[kMaxLocal];
    Pair pairs[kMaxPairs];
    int n = gatherLocal(src, ids);
    int pc = gatherPairs(ids, n, pairs);
    canonicalizePairs(pairs, pc);

    double beforeR[kMaxLocal], beforeP[kMaxLocal];
    for (int i = 0; i < n; ++i) {
      beforeR[i] = node[ids[i]].psi.rho;
      beforeP[i] = node[ids[i]].psi.phi;
    }

    inject(src);
    flowStep(ids, n, pairs, pc, kLin, 0.0);
    flowStep(ids, n, pairs, pc, kPsi, kPsiG);
    updateTauAndChi(ids, n, pairs, pc, beforeR, beforeP);
    flowStep(ids, n, pairs, pc, kChi, kChiG);

    bool stable = node[src].deg >= kTopK && incident(src) >= kWMin && node[src].seen >= kSMin;
    if (stable) {
      double prLin = localPr(ids, n, kLin);
      double prPsi = localPr(ids, n, kPsi);
      double prChi = localPr(ids, n, kChi);
      m.stable++;
      m.sumLinPr += prLin;
      m.sumPsiPr += prPsi;
      m.sumChiPr += prChi;
      if (prPsi < 0.75 * prLin) {
        m.horizons++;
        if (node[src].novel) m.novelHorizons++;
      }
    }
  }

  void process(int id) {
    t++;
    m.updates++;
    node[id].seen++;
    relaxNode(id);
    for (int i = 0; i < winN; ++i) touch(id, win[i]);
    if (winN < kWindow) {
      win[winN++] = id;
    } else {
      for (int i = 1; i < kWindow; ++i) win[i - 1] = win[i];
      win[kWindow - 1] = id;
    }
    physicalTick(id);
  }

  void clearWindow() { winN = 0; }

  void finalizeCoherence() {
    for (int a = 0; a < kBaseNodes; ++a) {
      for (int b = a + 1; b < kBaseNodes; ++b) {
        double vis = 0.0;
        int es = findEdge(a, b);
        if (es >= 0) {
          const Edge& e = node[a].e[es];
          const Field& fa = node[a].chi;
          const Field& fb = node[b].chi;
          double c = std::cos(wrap(fb.phi - fa.phi + orientedPhase(a, e)));
          vis = e.w * std::sqrt(std::max(0.0, fa.rho * fb.rho)) * std::max(0.0, c);
        }
        if (node[a].topic == node[b].topic) {
          m.sameEdgeVis += vis;
          m.sameN++;
        } else {
          m.crossEdgeVis += vis;
          m.crossN++;
        }
      }
    }
    if (m.sameN) m.sameEdgeVis /= m.sameN;
    if (m.crossN) m.crossEdgeVis /= m.crossN;
  }
};

void run(System& s, int stream) {
  unsigned rng = 11u;
  auto next = [&]() {
    rng = 1664525u * rng + 1013904223u;
    return rng;
  };

  for (int i = 0; i < stream;) {
    int tp = (int)(next() % kTopics);
    s.clearWindow();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) {
      int node = tp * kPerTopic + (int)(next() % kPerTopic);
      if (kUniqueEvery > 0 && i % kUniqueEvery == 0) node = s.add(tp, true);
      s.process(node);
    }
  }
  s.finalizeCoherence();
}

double syncError(const System& a, const System& b) {
  double e = 0.0;
  int n = std::min(a.count, b.count);
  for (int i = 0; i < n; ++i) {
    const Field& ap = a.node[i].psi;
    const Field& bp = b.node[i].psi;
    const Field& ac = a.node[i].chi;
    const Field& bc = b.node[i].chi;
    e += std::abs(ap.rho - bp.rho) + std::abs(ac.rho - bc.rho);
    e += std::abs(wrap(ap.phi - bp.phi)) * std::sqrt(ap.rho + bp.rho + 1e-300);
    e += std::abs(wrap(ac.phi - bc.phi)) * std::sqrt(ac.rho + bc.rho + 1e-300);
  }
  return e / (double)std::max(1, n);
}

bool report(const char* name, bool ok) {
  std::printf("   => %s  %s\n", ok ? "PASS" : "FAIL", name);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  int stream = argc > 1 ? std::atoi(argv[1]) : kDefaultStream;
  std::printf("=====================================================================\n");
  std::printf("  SYNCHRONOUS PHASE-FIELD CONTRACT  (no Vec projection)\n");
  std::printf("=====================================================================\n");
  std::printf("  stream=%d max_nodes=%d topK=%d dt=%.3f psi_g=%.1f chi_g=%.1f\n",
              stream, kMaxNodes, kTopK, kDt, kPsiG, kChiG);

  System forward(false, false);
  System reverse(true, false);
  System random(false, true);
  run(forward, stream);
  run(reverse, stream);
  run(random, std::min(stream, 200000));

  double prLin = forward.m.stable ? forward.m.sumLinPr / forward.m.stable : 0.0;
  double prPsi = forward.m.stable ? forward.m.sumPsiPr / forward.m.stable : 0.0;
  double prChi = forward.m.stable ? forward.m.sumChiPr / forward.m.stable : 0.0;
  double compression = prPsi > 0.0 ? prLin / prPsi : 0.0;
  double chiCompression = prChi > 0.0 ? prLin / prChi : 0.0;
  double tauMean = forward.m.updates ? forward.m.sumTau / forward.m.updates : 0.0;
  double tauRandom = random.m.updates ? random.m.sumTau / random.m.updates : 0.0;
  double err = syncError(forward, reverse);

  std::printf("  stable=%lld horizons=%lld novel_horizons=%lld nodes=%d\n",
              forward.m.stable, forward.m.horizons, forward.m.novelHorizons, forward.count);
  std::printf("  PR lin=%.3f psi=%.3f chi=%.3f compression psi=%.2fx chi=%.2fx\n",
              prLin, prPsi, prChi, compression, chiCompression);
  std::printf("  tau_mean real=%.6f random=%.6f max_rho=%.3f sync_error=%.6e\n",
              tauMean, tauRandom, forward.m.maxRho, err);
  std::printf("  chi edge visibility same=%.6f cross=%.6f pairs=%d/%d\n",
              forward.m.sameEdgeVis, forward.m.crossEdgeVis, forward.m.sameN, forward.m.crossN);

  int pass = 0, total = 0;
  ++total;
  pass += report("synchronous edge order is invariant", err < 1e-9);
  ++total;
  pass += report("nonlinear pressure densifies psi field", compression > 1.05);
  ++total;
  pass += report("chi feeling field is active", tauMean > 1e-4 && tauMean > 0.5 * tauRandom);
  ++total;
  pass += report("field stays bounded", forward.m.maxRho < 50.0);
  ++total;
  pass += report("same-topic chi edge visibility is above cross-topic",
                 forward.m.sameEdgeVis > forward.m.crossEdgeVis + 1e-6);

  std::printf("  CONTRACT RESULT : %d / %d verified\n", pass, total);
  std::printf("\n  RESULT : %s\n", pass == total ? "PASS" : "FAIL");
  return pass == total ? 0 : 1;
}
