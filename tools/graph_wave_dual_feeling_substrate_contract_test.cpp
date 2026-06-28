// graph_wave_dual_feeling_substrate_contract_test
// ----------------------------------------------------------------------------
// Parallel psi/chi substrate experiment.
//
// psi: information / energy phase field, unchanged Kerr pressure path.
// chi: feeling / proprioception phase field. It is not a bridge and not a SQL
// metric. It receives local impulses from psi motion and phase twist, then
// evolves as its own wave field on the same local substrate.
//
// Gate: in a normal, non-corrupted stream, chi alone must preserve useful topic
// structure and show whether it can replace psi as a readout substrate. This is
// NOT a destructive shared-support stress test; that belongs to a separate
// fault-injection probe.
// ----------------------------------------------------------------------------
#define NOMINMAX
#include "graph_wave_substrate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <unordered_map>
#include <vector>

namespace {
using gw::cd;
using gw::Vec;

constexpr int kTopics = 3;
constexpr int kPerTopic = 24;
constexpr int kTopicNodes = kTopics * kPerTopic;
constexpr int kDefaultStream = 60000;
constexpr int kUniqueEvery = 7;
constexpr int kWindow = 4;
constexpr int kTopK = 6;
constexpr double kEta = 1.0;
constexpr double kDecay = 0.99;
constexpr double kEdgeFloor = 1e-3;
constexpr double kWMin = 8.0;
constexpr double kInject = 0.35;
constexpr double kDefaultFeelInject = 0.55;
constexpr int kSMin = 4;
constexpr int kPsiSteps = 2;
constexpr int kDefaultChiSteps = 2;
constexpr double kDt = 0.3;
constexpr double kG = 7.0;
constexpr double kDefaultChiDt = 0.12;
constexpr double kDefaultChiG = 30.0;

double gFeelInject = kDefaultFeelInject;
double gChiDt = kDefaultChiDt;
double gChiG = kDefaultChiG;
int gChiSteps = kDefaultChiSteps;

struct Edge {
  double w = 0.0;
  double phase = 0.0;
  int last = 0;
};

struct Graph {
  std::vector<std::unordered_map<int, Edge>> adj;
  std::vector<int> seen;
  std::deque<int> win;
  int t = 0;

  int add() {
    adj.push_back({});
    seen.push_back(0);
    return (int)adj.size() - 1;
  }

  double incident(int a) const {
    double s = 0.0;
    for (const auto& kv : adj[a]) s += kv.second.w;
    return s;
  }

  void decay(int a, int b) {
    auto it = adj[a].find(b);
    if (it == adj[a].end()) return;
    int dt = t - it->second.last;
    if (dt > 0) {
      it->second.w *= std::pow(kDecay, dt);
      it->second.last = t;
      auto back = adj[b].find(a);
      if (back != adj[b].end()) back->second = it->second;
    }
  }

  void eraseEdge(int a, int b) {
    adj[a].erase(b);
    adj[b].erase(a);
  }

  static double wrapPhase(double x) {
    while (x > gw::kPi) x -= 2.0 * gw::kPi;
    while (x < -gw::kPi) x += 2.0 * gw::kPi;
    return x;
  }

  bool hasEdge(int a, int b) const {
    return adj[a].find(b) != adj[a].end();
  }

  double orientedPhase(int u, int v) const {
    auto it = adj[u].find(v);
    if (it == adj[u].end()) return 0.0;
    return u < v ? it->second.phase : -it->second.phase;
  }

  void addSignedPhase(int u, int v, double dphi) {
    auto it = adj[u].find(v);
    if (it == adj[u].end()) return;
    Edge e = it->second;
    const double sign = u < v ? 1.0 : -1.0;
    e.phase = std::clamp(wrapPhase(e.phase + sign * dphi), -1.4, 1.4);
    adj[u][v] = e;
    adj[v][u] = e;
  }

  void addPlaquetteFlux(int a, int b, int c) {
    constexpr double dphi = 0.018;
    addSignedPhase(a, b, dphi / 3.0);
    addSignedPhase(b, c, dphi / 3.0);
    addSignedPhase(c, a, dphi / 3.0);
  }

  void closePlaquettes(int node) {
    for (int j = 1; j < (int)win.size(); ++j) {
      for (int i = 0; i < j; ++i) {
        int a = win[i], b = win[j];
        if (hasEdge(a, b) && hasEdge(a, node) && hasEdge(b, node)) addPlaquetteFlux(a, b, node);
      }
    }
  }

  void relaxNode(int a) {
    std::vector<int> dead;
    for (auto& kv : adj[a]) {
      int b = kv.first;
      int dt = t - kv.second.last;
      if (dt > 0) {
        kv.second.w *= std::pow(kDecay, dt);
        kv.second.last = t;
        auto back = adj[b].find(a);
        if (back != adj[b].end()) back->second = kv.second;
      }
      if (kv.second.w < kEdgeFloor) dead.push_back(b);
    }
    for (int b : dead) eraseEdge(a, b);
  }

  void prune(int a) {
    while ((int)adj[a].size() > kTopK) {
      int weakest = -1;
      double ww = 1e100;
      for (const auto& kv : adj[a]) {
        if (kv.second.w < ww) {
          ww = kv.second.w;
          weakest = kv.first;
        }
      }
      eraseEdge(a, weakest);
    }
  }

  void touch(int a, int b) {
    if (a == b) return;
    decay(a, b);
    decay(b, a);
    adj[a][b].w += kEta;
    adj[a][b].last = t;
    adj[b][a] = adj[a][b];
    if ((int)adj[a].size() > kTopK) prune(a);
    if ((int)adj[b].size() > kTopK) prune(b);
  }
};

struct Mem {
  std::unordered_map<int, cd> lin;
  std::unordered_map<int, cd> psi;
  std::unordered_map<int, cd> chi;
  double prLin = 1.0;
  double prPsi = 1.0;
  double prChi = 1.0;
};

struct Update {
  bool horizon = false;
  bool stable = false;
  double prLin = 1.0;
  double prPsi = 1.0;
  double prChi = 1.0;
  double chiDrive = 0.0;
  double maxPhaseSpeed = 0.0;
  long long ops = 0;
};

unsigned hash3(int a, int b, int c) {
  unsigned x = 2166136261u;
  x = (x ^ (unsigned)a) * 16777619u;
  x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u;
  x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u;
  return x;
}

std::vector<int> hood(const Graph& g, int src) {
  std::vector<int> nodes = {src};
  std::unordered_map<int, int> seen;
  seen[src] = 0;
  for (int hop = 0; hop < 2; ++hop) {
    int sz = (int)nodes.size();
    for (int h = 0; h < sz; ++h) {
      int u = nodes[h];
      for (const auto& kv : g.adj[u]) {
        if (!seen.count(kv.first)) {
          seen[kv.first] = (int)nodes.size();
          nodes.push_back(kv.first);
        }
      }
    }
  }
  return nodes;
}

Vec project(const std::unordered_map<int, cd>& f,
            const std::vector<int>& nodes,
            const std::unordered_map<int, int>& idx,
            int src,
            double inject) {
  Vec psi(nodes.size(), cd(0, 0));
  for (const auto& kv : f) {
    auto it = idx.find(kv.first);
    if (it != idx.end()) psi[it->second] += kv.second;
  }
  if (inject != 0.0) psi[idx.at(src)] += cd(inject, 0);
  if (gw::power(psi) < 1e-300) psi[idx.at(src)] = cd(1, 0);
  gw::normalizeInPlace(psi);
  return psi;
}

std::unordered_map<int, cd> unproject(const std::vector<int>& nodes, const Vec& psi) {
  std::unordered_map<int, cd> out;
  for (int i = 0; i < (int)nodes.size(); ++i) {
    if (std::norm(psi[i]) > 1e-14) out[nodes[i]] = psi[i];
  }
  return out;
}

std::vector<gw::SparseBond> bondsInLightCone(const Graph& g,
                                             const std::vector<int>& nodes,
                                             const std::unordered_map<int, int>& idx) {
  std::vector<gw::SparseBond> bonds;
  for (int i = 0; i < (int)nodes.size(); ++i) {
    int u = nodes[i];
    for (const auto& kv : g.adj[u]) {
      auto it = idx.find(kv.first);
      if (it != idx.end() && i < it->second) {
        bonds.push_back({i, it->second, kv.second.w, g.orientedPhase(u, kv.first)});
      }
    }
  }
  return bonds;
}

Vec feelingDrive(const Vec& before, const Vec& after, double* total_drive) {
  Vec drive(after.size(), cd(0, 0));
  double total = 0.0;
  for (int i = 0; i < (int)after.size(); ++i) {
    const double motion = std::abs(after[i] - before[i]);
    const double twist =
        std::abs(std::arg(std::conj(before[i]) * after[i])) *
        std::sqrt(std::norm(before[i]) * std::norm(after[i]));
    const double flow = std::sqrt(std::norm(after[i]));
    const double amp = motion + twist + 0.10 * flow;
    if (amp <= 1e-14) continue;
    cd dz = after[i] - before[i];
    const double phase = std::abs(dz) > 1e-14 ? std::arg(dz) : std::arg(after[i]);
    drive[i] = std::polar(std::sqrt(amp), phase);
    total += amp;
  }
  if (gw::power(drive) > 1e-300) gw::normalizeInPlace(drive);
  if (total_drive) *total_drive = total;
  return drive;
}

cd overlap(const std::unordered_map<int, cd>& a, const std::unordered_map<int, cd>& b) {
  cd z(0, 0);
  for (const auto& kv : a) {
    auto it = b.find(kv.first);
    if (it != b.end()) z += std::conj(kv.second) * it->second;
  }
  return z;
}

double coherence(const std::unordered_map<int, cd>& a, const std::unordered_map<int, cd>& b) {
  double pa = gw::fieldPower(a), pb = gw::fieldPower(b);
  return std::abs(overlap(a, b)) / (std::sqrt(pa * pb) + 1e-300);
}

Update evolve(Graph& g, int src, Mem& mem) {
  Update u;
  auto nodes = hood(g, src);
  int n = (int)nodes.size();
  if (n < 3) {
    mem.lin[src] = cd(1, 0);
    mem.psi[src] = cd(1, 0);
    mem.chi[src] = cd(1, 0);
    return u;
  }

  std::unordered_map<int, int> idx;
  for (int i = 0; i < n; ++i) idx[nodes[i]] = i;
  auto bonds = bondsInLightCone(g, nodes, idx);

  Vec lin = project(mem.lin, nodes, idx, src, kInject);
  Vec psi = project(mem.psi, nodes, idx, src, kInject);
  Vec chi = project(mem.chi, nodes, idx, src, 0.0);
  Vec psiBefore = psi;

  gw::LocalFlowStats psiStats;
  gw::edgeLocalKerrFlowPair(lin, psi, bonds, kDt, kG, kPsiSteps, &psiStats);
  gw::normalizeInPlace(lin);
  gw::normalizeInPlace(psi);

  double driveTotal = 0.0;
  Vec drive = feelingDrive(psiBefore, psi, &driveTotal);
  for (int i = 0; i < n; ++i) chi[i] += gFeelInject * drive[i];
  gw::normalizeInPlace(chi);

  gw::LocalFlowStats chiStats;
  chi = gw::edgeLocalKerrFlow(chi, bonds, gChiDt, gChiG, gChiSteps, &chiStats);
  gw::normalizeInPlace(chi);

  mem.lin = unproject(nodes, lin);
  mem.psi = unproject(nodes, psi);
  mem.chi = unproject(nodes, chi);
  mem.prLin = u.prLin = gw::participationRatio(lin);
  mem.prPsi = u.prPsi = gw::participationRatio(psi);
  mem.prChi = u.prChi = gw::participationRatio(chi);
  u.ops = (long long)bonds.size() * (kPsiSteps * 4 + gChiSteps * 2) +
          psiStats.bond_visits + chiStats.bond_visits;
  u.maxPhaseSpeed = std::max(psiStats.max_bond_speed, chiStats.max_bond_speed);
  u.chiDrive = driveTotal;
  u.stable = ((int)g.adj[src].size() >= kTopK) && g.incident(src) >= kWMin &&
             g.seen[src] >= kSMin;
  u.horizon = u.stable && u.prPsi < 0.5 * u.prLin;
  return u;
}

struct System {
  Graph g;
  std::vector<Mem> mem;
  std::vector<bool> novel;
  bool randomize = false;
  long long updates = 0;
  long long horizons = 0;
  long long novelHorizons = 0;
  long long horizonSamples = 0;
  long long stableSamples = 0;
  long long maxOps = 0;
  double sumHLin = 0.0;
  double sumHPsi = 0.0;
  double sumStableChi = 0.0;
  double sumChiDrive = 0.0;
  double maxPhaseSpeed = 0.0;

  double sumHChi = 0.0;

  explicit System(bool randomEdges) : randomize(randomEdges) {
    for (int i = 0; i < kTopicNodes; ++i) add(false);
  }

  int add(bool isNovel) {
    int id = g.add();
    mem.push_back({});
    novel.push_back(isNovel);
    return id;
  }

  int randomEndpoint(int a, int b) const {
    int n = (int)g.adj.size();
    int r = (int)(hash3(a, b, g.t) % (unsigned)n);
    return r == a ? (r + 1) % n : r;
  }

  void process(int node) {
    g.t++;
    g.seen[node]++;
    g.relaxNode(node);
    for (int c : g.win) {
      g.relaxNode(c);
      g.touch(node, randomize ? randomEndpoint(node, c) : c);
    }
    g.closePlaquettes(node);
    g.win.push_back(node);
    if ((int)g.win.size() > kWindow) g.win.pop_front();

    Update u = evolve(g, node, mem[node]);
    updates++;
    maxOps = std::max(maxOps, u.ops);
    maxPhaseSpeed = std::max(maxPhaseSpeed, u.maxPhaseSpeed);
    sumChiDrive += u.chiDrive;
    if (u.stable) {
      stableSamples++;
      sumStableChi += u.prChi;
    }
    if (u.horizon) {
      horizons++;
      horizonSamples++;
      sumHLin += u.prLin;
      sumHPsi += u.prPsi;
      sumHChi += u.prChi;
      if (novel[node]) novelHorizons++;
    }
  }
};

void run(System& s, int stream) {
  std::mt19937 r(11);
  std::uniform_int_distribution<int> tt(0, kTopics - 1), ww(0, kPerTopic - 1);
  for (int i = 0; i < stream;) {
    int tp = tt(r);
    s.g.win.clear();
    for (int b = 0; b < 6 && i < stream; ++b, ++i) {
      int node = tp * kPerTopic + ww(r);
      if (kUniqueEvery > 0 && i % kUniqueEvery == 0) {
        node = s.add(true);
      }
      s.process(node);
    }
  }
}

double evalField(const System& s, bool useChi) {
  std::vector<std::unordered_map<int, cd>> proto(kTopics);
  for (int tp = 0; tp < kTopics; ++tp) {
    for (int w = 0; w < kPerTopic / 2; ++w) {
      int node = tp * kPerTopic + w;
      const auto& f = useChi ? s.mem[node].chi : s.mem[node].psi;
      for (const auto& kv : f) proto[tp][kv.first] += kv.second;
    }
  }

  int ok = 0, total = 0;
  for (int tp = 0; tp < kTopics; ++tp) {
    for (int w = kPerTopic / 2; w < kPerTopic; ++w) {
      int node = tp * kPerTopic + w;
      const auto& f = useChi ? s.mem[node].chi : s.mem[node].psi;
      int best = -1;
      double bs = -1.0;
      for (int c = 0; c < kTopics; ++c) {
        double score = std::abs(overlap(f, proto[c]));
        if (score > bs) {
          bs = score;
          best = c;
        }
      }
      ok += (best == tp);
      total++;
    }
  }
  return 100.0 * ok / total;
}

struct Separation {
  double same = 0.0;
  double cross = 0.0;
  int sameN = 0;
  int crossN = 0;
  double maxCross = 0.0;
};

Separation chiSeparation(const System& s) {
  Separation out;
  std::vector<int> hubs;
  for (int i = 0; i < kTopicNodes; ++i) {
    if (s.g.seen[i] >= kSMin && !s.mem[i].chi.empty()) hubs.push_back(i);
  }
  for (int i = 0; i < (int)hubs.size(); ++i) {
    for (int j = i + 1; j < (int)hubs.size(); ++j) {
      int a = hubs[i], b = hubs[j];
      double c = coherence(s.mem[a].chi, s.mem[b].chi);
      if (a / kPerTopic == b / kPerTopic) {
        out.same += c;
        out.sameN++;
      } else {
        out.cross += c;
        out.crossN++;
        out.maxCross = std::max(out.maxCross, c);
      }
    }
  }
  if (out.sameN) out.same /= out.sameN;
  if (out.crossN) out.cross /= out.crossN;
  return out;
}

bool report(const char* name, bool ok) {
  std::printf("   => %s\n", ok ? "PASS" : "FAIL");
  if (!ok) std::printf("   !! %s\n", name);
  return ok;
}

struct ScenarioResult {
  int stream = 0;
  double compression = 0.0;
  double chiCompression = 0.0;
  double horizonPsiPr = 0.0;
  double horizonChiPr = 0.0;
  double psiAcc = 0.0;
  double chiAcc = 0.0;
  double chiRandom = 0.0;
  Separation sep;
  long long horizons = 0;
  long long novelHorizons = 0;
  long long maxOps = 0;
  long long opBound = 0;
  double maxPhaseSpeed = 0.0;
  double meanChiDrive = 0.0;
  double meanStableChiPr = 0.0;
};

ScenarioResult runScenario(int stream) {
  System real(false), random(true);
  run(real, stream);
  run(random, std::min(stream, 200000));
  ScenarioResult out;
  out.stream = stream;
  out.compression = real.sumHPsi > 0.0 ? real.sumHLin / real.sumHPsi : 0.0;
  out.chiCompression = real.sumHChi > 0.0 ? real.sumHLin / real.sumHChi : 0.0;
  out.horizonPsiPr = real.horizonSamples ? real.sumHPsi / real.horizonSamples : 0.0;
  out.horizonChiPr = real.horizonSamples ? real.sumHChi / real.horizonSamples : 0.0;
  out.psiAcc = evalField(real, false);
  out.chiAcc = evalField(real, true);
  out.chiRandom = evalField(random, true);
  out.sep = chiSeparation(real);
  out.horizons = real.horizons;
  out.novelHorizons = real.novelHorizons;
  out.maxOps = real.maxOps;
  out.opBound = 64LL * kTopK * 4 * (2 + std::max(1, gChiSteps));
  out.maxPhaseSpeed = real.maxPhaseSpeed;
  out.meanChiDrive = real.updates ? real.sumChiDrive / real.updates : 0.0;
  out.meanStableChiPr = real.stableSamples ? real.sumStableChi / real.stableSamples : 0.0;
  return out;
}

bool printScenario(const ScenarioResult& r) {
  std::printf("  stream=%d random_control=%d\n", r.stream, std::min(r.stream, 200000));
  std::printf("  psi compression=%.2fx horizon_PR=%.2f horizons=%lld novel=%lld recognition=%.1f%%\n",
              r.compression, r.horizonPsiPr, r.horizons, r.novelHorizons, r.psiAcc);
  std::printf("  chi compression=%.2fx horizon_PR=%.2f recognition=%.1f%% random=%.1f%%\n",
              r.chiCompression, r.horizonChiPr, r.chiAcc, r.chiRandom);
  std::printf("  chi recognition=%.1f%% random=%.1f%% coherence same=%.4f cross=%.4f max_cross=%.4f pairs=%d/%d\n",
              r.chiAcc, r.chiRandom, r.sep.same, r.sep.cross, r.sep.maxCross,
              r.sep.sameN, r.sep.crossN);
  std::printf("  chi field mean_drive=%.4f mean_stable_PR=%.2f max_ops=%lld bound=%lld max_phase_speed=%.3f\n",
              r.meanChiDrive, r.meanStableChiPr, r.maxOps, r.opBound, r.maxPhaseSpeed);

  int pass = 0, total = 0;
  ++total;
  pass += report("psi physics remains compressed and useful",
                 r.compression > 2.8 && r.psiAcc > 75.0 && r.novelHorizons == 0);
  ++total;
  pass += report("chi-only field carries topic structure",
                 r.chiAcc > 90.0 && r.chiAcc > r.chiRandom + 20.0);
  ++total;
  pass += report("chi can replace psi readout without losing value",
                 r.chiAcc >= r.psiAcc - 1e-9);
  ++total;
  pass += report("chi separates same-topic from cross-topic without labels",
                 r.sep.same > r.sep.cross + 0.05 && r.sep.maxCross < 0.35);
  ++total;
  pass += report("parallel chi remains local and bounded",
                 r.maxOps <= r.opBound && r.maxPhaseSpeed < 8.0);
  std::printf("  SCENARIO RESULT : %d / %d verified\n", pass, total);
  return pass == total;
}
}  // namespace

int main(int argc, char** argv) {
  const int stream = argc > 1 ? std::atoi(argv[1]) : kDefaultStream;
  if (argc > 2) gChiG = std::atof(argv[2]);
  if (argc > 3) gFeelInject = std::atof(argv[3]);
  if (argc > 4) gChiSteps = std::max(1, std::atoi(argv[4]));
  if (argc > 5) gChiDt = std::atof(argv[5]);
  std::printf("=====================================================================\n");
  std::printf("  CHI REPLACEMENT CONTRACT  (normal stream, psi + parallel chi)\n");
  std::printf("=====================================================================\n");
  std::printf("  chi_params: g=%.3f inject=%.3f steps=%d dt=%.3f\n",
              gChiG, gFeelInject, gChiSteps, gChiDt);

  bool ok = printScenario(runScenario(stream));
  std::printf("\n  RESULT : %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
