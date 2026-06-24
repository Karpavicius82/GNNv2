// graph_wave_streaming_word_plasticity_contract_test
// ----------------------------------------------------------------------------
// Physics-native word learning contract. This is a CONTRACT, not a text8
// benchmark: a small deterministic stream with planted topic structure verifies
// that learning is local stream plasticity, not global embedding recomputation.
//
// Hard regressions this must catch:
//   - fixed vocab caps (node_count < unique_tokens_seen)
//   - VxV/global scans (ops_per_token grows with node count)
//   - unbounded local degree
//   - full dictionary readout scans
//   - generic unitary memory without semantic graph value
//   - phase-collapsing canonical readout
// ----------------------------------------------------------------------------
#include "graph_wave_substrate.hpp"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
using gw::cd;

constexpr int kWindow = 4;
constexpr int kTopK = 5;
constexpr int kOneOff = 320;
constexpr double kEta = 1.0;
constexpr double kDecay = 0.985;
constexpr double kDrop = 0.035;
constexpr double kTau = 0.42;

struct Edge {
  double w = 0.0;
  int last = 0;
};

struct Field {
  std::unordered_map<int, cd> a;
};

static double power(const Field& f) {
  double s = 0.0;
  for (const auto& kv : f.a) s += std::norm(kv.second);
  return s;
}

static void normalize(Field& f) {
  double n = std::sqrt(power(f)) + 1e-15;
  for (auto& kv : f.a) kv.second /= n;
}

static cd inner(const Field& a, const Field& b) {
  const auto* small = &a.a;
  const auto* large = &b.a;
  bool swapped = false;
  if (small->size() > large->size()) {
    std::swap(small, large);
    swapped = true;
  }
  cd s(0, 0);
  for (const auto& kv : *small) {
    auto it = large->find(kv.first);
    if (it == large->end()) continue;
    s += swapped ? std::conj(it->second) * kv.second : std::conj(kv.second) * it->second;
  }
  return s;
}

static double overlapPower(const Field& a, const Field& b) {
  cd z = inner(a, b);
  return std::norm(z) / (power(a) * power(b) + 1e-15);
}

static double magnitudeCos(const Field& a, const Field& b) {
  double d = 0.0, na = 0.0, nb = 0.0;
  for (const auto& kv : a.a) {
    double av = std::norm(kv.second);
    na += av * av;
    auto it = b.a.find(kv.first);
    if (it != b.a.end()) d += av * std::norm(it->second);
  }
  for (const auto& kv : b.a) {
    double bv = std::norm(kv.second);
    nb += bv * bv;
  }
  return d / (std::sqrt(na * nb) + 1e-15);
}

struct StreamGraph {
  bool randomize = false;
  int t = 0;
  long long op_total = 0;
  long long op_max = 0;
  int max_degree_seen = 0;
  std::unordered_map<std::string, int> id;
  std::vector<std::string> words;
  std::vector<int> last_seen;
  std::vector<std::unordered_map<int, Edge>> adj;
  std::deque<int> window;
  std::unordered_set<std::string> unique_seen;

  explicit StreamGraph(bool random_edges = false) : randomize(random_edges) {}

  int findOrCreate(const std::string& token) {
    unique_seen.insert(token);
    auto it = id.find(token);
    if (it != id.end()) return it->second;
    int n = (int)words.size();
    id[token] = n;
    words.push_back(token);
    last_seen.push_back(-1);
    adj.push_back({});
    return n;
  }

  int node(const std::string& token) const {
    auto it = id.find(token);
    return it == id.end() ? -1 : it->second;
  }

  static unsigned hash3(int a, int b, int c) {
    unsigned x = 2166136261u;
    x = (x ^ (unsigned)a) * 16777619u;
    x = (x ^ (unsigned)(b + 0x9e3779b9u)) * 16777619u;
    x = (x ^ (unsigned)(c * 2654435761u)) * 16777619u;
    return x;
  }

  int randomEndpoint(int a, int b) const {
    int n = (int)words.size();
    if (n <= 1) return a;
    int r = (int)(hash3(a, b, t) % (unsigned)n);
    if (r == a) r = (r + 1) % n;
    return r;
  }

  void decayEdge(int a, int b, long long& ops) {
    auto it = adj[a].find(b);
    if (it == adj[a].end()) return;
    int dt = t - it->second.last;
    if (dt > 0) {
      it->second.w *= std::pow(kDecay, dt);
      it->second.last = t;
    }
    ops++;
  }

  void eraseEdge(int a, int b) {
    adj[a].erase(b);
    adj[b].erase(a);
  }

  void pruneNode(int a, long long& ops) {
    std::vector<int> drop;
    drop.reserve(adj[a].size());
    for (auto& kv : adj[a]) {
      int b = kv.first;
      decayEdge(a, b, ops);
      if (kv.second.w < kDrop) drop.push_back(b);
      ops++;
    }
    for (int b : drop) eraseEdge(a, b);

    while ((int)adj[a].size() > kTopK) {
      int weakest = -1;
      double ww = 1e100;
      for (const auto& kv : adj[a]) {
        if (kv.second.w < ww) {
          ww = kv.second.w;
          weakest = kv.first;
        }
        ops++;
      }
      eraseEdge(a, weakest);
    }
    max_degree_seen = std::max(max_degree_seen, (int)adj[a].size());
  }

  void touchEdge(int a, int b, long long& ops) {
    if (a == b) return;
    decayEdge(a, b, ops);
    decayEdge(b, a, ops);
    adj[a][b].w += kEta;
    adj[a][b].last = t;
    adj[b][a].w = adj[a][b].w;
    adj[b][a].last = t;
    ops += 2;
    pruneNode(a, ops);
    pruneNode(b, ops);
  }

  void process(const std::string& token) {
    t++;
    long long ops = 0;
    int a = findOrCreate(token);
    last_seen[a] = t;
    ops += 2;
    pruneNode(a, ops);
    for (int c : window) {
      int b = randomize ? randomEndpoint(a, c) : c;
      touchEdge(a, b, ops);
    }
    window.push_back(a);
    if ((int)window.size() > kWindow) window.pop_front();
    op_total += ops;
    op_max = std::max(op_max, ops);
  }

  bool hasEdge(int a, int b) const {
    auto it = adj[a].find(b);
    return it != adj[a].end() && it->second.w >= kDrop;
  }

  int degree(int a) const { return (int)adj[a].size(); }

  int nodeCount() const { return (int)words.size(); }
};

static std::vector<std::string> topicWords(int topic) {
  const char prefix = (topic == 0 ? 'a' : topic == 1 ? 'b' : 'c');
  std::vector<std::string> out;
  for (int i = 0; i < 6; i++) out.push_back(std::string(1, prefix) + std::to_string(i));
  return out;
}

static std::vector<std::string> supportWords(int topic) {
  auto w = topicWords(topic);
  return {w[0], w[1], w[2], w[3]};
}

static std::vector<std::string> makeStream() {
  std::vector<std::string> s;
  for (int r = 0; r < kOneOff; r++) {
    auto topic = topicWords(r % 3);
    s.insert(s.end(), topic.begin(), topic.end());
    s.push_back("spark_" + std::to_string(r));
  }
  for (int r = 0; r < 24; r++) {
    for (int g = 0; g < 3; g++) {
      auto topic = topicWords(g);
      s.insert(s.end(), topic.begin(), topic.end());
    }
  }
  return s;
}

static Field seed(int node) {
  Field f;
  f.a[node] = cd(1, 0);
  return f;
}

static Field propagate(const StreamGraph& g, Field f, int steps, long long* ops = nullptr) {
  for (int step = 0; step < steps; step++) {
    Field next;
    for (const auto& kv : f.a) {
      int u = kv.first;
      cd amp = kv.second;
      for (const auto& e : g.adj[u]) {
        next.a[e.first] += cd(0, -kTau * e.second.w) * amp;
        if (ops) (*ops)++;
      }
    }
    f = next;
    normalize(f);
  }
  return f;
}

static Field prototype(const StreamGraph& g, const std::vector<std::string>& words) {
  Field p;
  for (const auto& w : words) {
    int id = g.node(w);
    Field z = propagate(g, seed(id), 2);
    for (const auto& kv : z.a) p.a[kv.first] += kv.second;
  }
  normalize(p);
  return p;
}

static std::vector<int> activeCandidates(const StreamGraph& g) {
  std::vector<int> c;
  for (int topic = 0; topic < 3; topic++) {
    int anchor = g.node(topicWords(topic)[0]);
    if (anchor >= 0 && g.t - g.last_seen[anchor] <= 32) c.push_back(topic);
  }
  return c;
}

static int readout(const std::vector<Field>& proto, const std::vector<int>& candidates,
                   const Field& query, int& scanned) {
  scanned = 0;
  int best = -1;
  double bs = -1.0;
  for (int c : candidates) {
    scanned++;
    double s = overlapPower(proto[c], query);
    if (s > bs) {
      bs = s;
      best = c;
    }
  }
  return best;
}

static int classifyAnchors(const StreamGraph& g, const std::vector<Field>& proto,
                           const std::vector<int>& candidates, int& scanned_total) {
  int ok = 0;
  scanned_total = 0;
  for (int topic = 0; topic < 3; topic++) {
    for (int w : {4, 5}) {
      int id = g.node(topicWords(topic)[w]);
      Field q = propagate(g, seed(id), 2);
      int scanned = 0;
      int pred = readout(proto, candidates, q, scanned);
      scanned_total += scanned;
      ok += (pred == topic);
    }
  }
  return ok;
}

static int bfsWithin(const StreamGraph& g, int start, int depth) {
  std::unordered_set<int> seen;
  std::vector<int> frontier = {start};
  seen.insert(start);
  for (int d = 0; d < depth; d++) {
    std::vector<int> next;
    for (int u : frontier) {
      for (const auto& kv : g.adj[u]) {
        if (seen.insert(kv.first).second) next.push_back(kv.first);
      }
    }
    frontier.swap(next);
  }
  return (int)seen.size();
}

static bool fieldInsideCone(const StreamGraph& g, int start, const Field& f, int depth) {
  std::unordered_set<int> seen;
  std::vector<int> frontier = {start};
  seen.insert(start);
  for (int d = 0; d < depth; d++) {
    std::vector<int> next;
    for (int u : frontier) {
      for (const auto& kv : g.adj[u]) {
        if (seen.insert(kv.first).second) next.push_back(kv.first);
      }
    }
    frontier.swap(next);
  }
  for (const auto& kv : f.a) {
    if (!seen.count(kv.first)) return false;
  }
  return true;
}

static bool report(const char* n, bool ok) {
  std::printf(" => %s\n", ok ? "PASS" : "FAIL");
  if (!ok) std::printf(" !! %s\n", n);
  return ok;
}
}  // namespace

int main() {
  std::printf("=====================================================================\n");
  std::printf(" STREAMING WORD PLASTICITY CONTRACT (physics-native learning)\n");
  std::printf(" dynamic nodes, local edges, decay, light cone, active complex readout\n");
  std::printf("=====================================================================\n");

  StreamGraph real(false), random(true);
  auto stream = makeStream();
  for (const auto& tok : stream) {
    real.process(tok);
    random.process(tok);
  }

  int pass = 0, total = 0;
  const int unique = (int)real.unique_seen.size();
  const long long op_bound = 16LL * (kWindow + kTopK) + 32;

  std::printf("\n[1] NO VOCAB CAP: every unique token becomes a node\n");
  std::printf(" unique_tokens=%d  real_nodes=%d  random_nodes=%d\n", unique, real.nodeCount(), random.nodeCount());
  ++total;
  pass += report("a fixed vocab cap or token drop appeared",
                 real.nodeCount() == unique && random.nodeCount() == unique && unique > op_bound);

  std::printf("\n[2] LOCAL WORK: per-token ops stay O(window + degree), not O(node_count)\n");
  std::printf(" max_ops/token real=%lld random=%lld  bound=%lld  node_count=%d\n",
              real.op_max, random.op_max, op_bound, real.nodeCount());
  ++total;
  pass += report("per-token work suggests a global scan",
                 real.op_max <= op_bound && random.op_max <= op_bound && real.nodeCount() > op_bound);

  std::printf("\n[3] LOCAL DEGREE CAP: degree[node] <= topK\n");
  int max_deg = 0;
  for (int i = 0; i < real.nodeCount(); i++) max_deg = std::max(max_deg, real.degree(i));
  std::printf(" max degree=%d  topK=%d\n", max_deg, kTopK);
  ++total;
  pass += report("local degree cap was violated", max_deg <= kTopK);

  std::printf("\n[4] DECAY/PRUNING: repeated context survives; one-off trace disappears\n");
  int a0 = real.node("a0"), a1 = real.node("a1"), sp0 = real.node("spark_0");
  bool strong = real.hasEdge(a0, a1);
  bool oneoffGone = (sp0 >= 0 && real.degree(sp0) == 0);
  std::printf(" edge(a0,a1)=%s  degree(spark_0)=%d\n", strong ? "alive" : "missing", real.degree(sp0));
  ++total;
  pass += report("decay/pruning did not preserve strong traces and remove weak traces", strong && oneoffGone);

  std::printf("\n[5] LIGHT CONE: propagation support stays inside local 2-hop cone\n");
  long long prop_ops = 0;
  Field q = propagate(real, seed(a0), 2, &prop_ops);
  int cone = bfsWithin(real, a0, 2);
  std::printf(" support=%zu  2-hop-cone=%d  propagation_edge_ops=%lld\n", q.a.size(), cone, prop_ops);
  ++total;
  pass += report("propagation escaped the local light cone", fieldInsideCone(real, a0, q, 2));

  std::printf("\n[6] ACTIVE-ONLY READOUT: candidates are active, not the whole dictionary\n");
  std::vector<Field> support_proto;
  for (int topic = 0; topic < 3; topic++) support_proto.push_back(prototype(real, supportWords(topic)));
  auto candidates = activeCandidates(real);
  int scanned_total = 0;
  int real_ok = classifyAnchors(real, support_proto, candidates, scanned_total);
  std::printf(" active_candidates=%zu  node_count=%d  scanned_per_query=%.1f\n",
              candidates.size(), real.nodeCount(), scanned_total / 6.0);
  ++total;
  pass += report("readout scanned the full dictionary or inactive candidates",
                 candidates.size() == 3 && (int)candidates.size() < real.nodeCount() &&
                     scanned_total == 6 * (int)candidates.size());

  std::printf("\n[7] PHASE-PRESERVING CANONICAL READOUT: complex overlap, gauge-invariant\n");
  std::vector<double> theta(real.nodeCount());
  for (int i = 0; i < real.nodeCount(); i++) theta[i] = 1.7 * std::sin(0.13 * i + 0.2);
  Field gq = q, gp = support_proto[0];
  for (auto& kv : gq.a) kv.second *= std::exp(cd(0, theta[kv.first]));
  for (auto& kv : gp.a) kv.second *= std::exp(cd(0, theta[kv.first]));
  double native = overlapPower(q, support_proto[0]);
  double gauged = overlapPower(gq, gp);
  double mag = magnitudeCos(q, support_proto[0]);
  std::printf(" |<q|p>|^2=%.6f  gauged_diff=%.2e  magnitude_cos=%.6f\n",
              native, std::abs(native - gauged), mag);
  ++total;
  pass += report("canonical readout collapsed phase or absolute gauge leaked",
                 std::abs(native - gauged) < 1e-12 && std::abs(native - mag) > 1e-3);

  std::printf("\n[8] SEMANTIC VALUE: real stream graph > matched-random stream graph\n");
  std::vector<Field> random_proto;
  for (int topic = 0; topic < 3; topic++) random_proto.push_back(prototype(random, supportWords(topic)));
  auto random_candidates = activeCandidates(random);
  int random_scanned = 0;
  int random_ok = classifyAnchors(random, random_proto, random_candidates, random_scanned);
  std::printf(" real_acc=%d/6  random_acc=%d/6  candidates real=%zu random=%zu\n",
              real_ok, random_ok, candidates.size(), random_candidates.size());
  ++total;
  pass += report("stream graph did not isolate semantic value over matched-random",
                 real_ok >= 5 && random_ok <= 3 && real_ok > random_ok &&
                     random_candidates.size() == candidates.size());

  std::printf("\n RESULT : %d / %d verified\n", pass, total);
  return pass == total ? 0 : 1;
}
