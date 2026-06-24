// Check the other half without a dense NxN substrate: sparse text8 PPMI rows feed
// a sparse directional-flux graph, and the phase/current readout is measured only
// on real top-K edges. Task = distinguish attested word order from its reverse.
#include "../tools/graph_wave_substrate.hpp"
#include "semantic_sparse_text8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>
using namespace std;
using gw::cd;

struct Sp {
  int n = 0;
  vector<vector<pair<int, cd>>> adj;
  vector<pair<int, int>> edges;
};

static vector<double> bessel(double x, int km) {
  if (fabs(x) < 1e-14) {
    vector<double> j(km + 1, 0.0);
    j[0] = 1.0;
    return j;
  }
  int m = max(km, (int)fabs(x)) + 80;
  vector<double> j(m + 2, 0.0);
  j[m] = 1e-300;
  for (int k = m; k >= 1; k--) j[k - 1] = (2.0 * k / x) * j[k] - j[k + 1];
  double nm = j[0];
  for (int k = 2; k <= m; k += 2) nm += 2.0 * j[k];
  for (int k = 0; k <= m; k++) j[k] /= nm;
  j.resize(km + 1);
  return j;
}

static vector<cd> mv(const Sp& g, const vector<cd>& x) {
  vector<cd> y(g.n, cd(0, 0));
  for (int i = 0; i < g.n; i++) {
    cd sum(0, 0);
    for (const auto& e : g.adj[i]) sum += e.second * x[e.first];
    y[i] = sum;
  }
  return y;
}

static vector<cd> cheb(const Sp& g, const vector<cd>& p, double t, double a) {
  int km = (int)(a * t) + 40;
  auto j = bessel(a * t, km);
  vector<cd> tm = p, tk = mv(g, p);
  for (auto& v : tk) v /= a;
  vector<cd> out(g.n, cd(0, 0));
  cd mi(0, -1);
  for (int k = 0; k <= km; k++) {
    cd cf = (k == 0 ? 1.0 : 2.0) * pow(mi, k) * j[k];
    if (k == 0) {
      for (int i = 0; i < g.n; i++) out[i] += cf * p[i];
    } else if (k == 1) {
      for (int i = 0; i < g.n; i++) out[i] += cf * tk[i];
    } else {
      auto tp = mv(g, tk);
      for (auto& v : tp) v /= a;
      for (int i = 0; i < g.n; i++) tp[i] = 2.0 * tp[i] - tm[i];
      tm.swap(tk);
      tk.swap(tp);
      for (int i = 0; i < g.n; i++) out[i] += cf * tk[i];
    }
  }
  return out;
}

static unsigned long long edge_key(int a, int b) {
  return (unsigned long long)(unsigned int)a << 32 | (unsigned int)b;
}

int main() {
  const int TOK = 1000000, VOCAB = 4096, WIN = 4, TOPK = 32;
  const double tau = 0.65, pscale = 0.65;
  auto s = sem_sparse::load_text8_sparse("data/text8", TOK, VOCAB, WIN);
  if (!s.ok) {
    printf("no data/text8\n");
    return 2;
  }

  auto top = sem_sparse::topk_directed(s, TOPK);
  Sp g;
  g.n = s.n;
  g.adj.assign(s.n, {});
  unordered_set<unsigned long long> seen;
  seen.reserve((size_t)s.n * TOPK);

  for (int i = 0; i < s.n; i++) {
    for (int j : top[i]) {
      int a = min(i, j), b = max(i, j);
      unsigned long long key = edge_key(a, b);
      if (!seen.insert(key).second) continue;
      double ww = sem_sparse::symmetric_score(s, a, b);
      if (ww <= 0.0) continue;
      double dab = sem_sparse::lookup(s.fwd, a, b);
      double dba = sem_sparse::lookup(s.fwd, b, a);
      double asym = (dab - dba) / (dab + dba + 1e-9);
      cd h = ww * exp(cd(0, pscale * asym));
      g.adj[a].push_back({b, h});
      g.adj[b].push_back({a, conj(h)});
      g.edges.push_back({a, b});
    }
  }

  double mr = 0.0;
  for (int i = 0; i < s.n; i++) {
    double row = 0.0;
    for (const auto& e : g.adj[i]) row += abs(e.second);
    mr = max(mr, row);
  }
  double bound = 1.05 * mr + 1e-9;

  auto drive_cur = [&](const vector<int>& phrase) {
    vector<cd> psi(s.n, cd(0, 0));
    bool has = false;
    for (int x : phrase) {
      if (x < 0) return vector<double>();
      if (has) psi = cheb(g, psi, tau, bound);
      for (const auto& kv : s.emb[x]) psi[kv.first] += cd(kv.second, 0.0);
      has = true;
    }
    psi = cheb(g, psi, tau, bound);

    double nn = 0.0;
    for (const auto& v : psi) nn += norm(v);
    nn = sqrt(nn) + 1e-15;
    for (auto& v : psi) v /= nn;

    vector<double> cur;
    cur.reserve(g.edges.size());
    for (const auto& p : g.edges) {
      int i = p.first, j = p.second;
      cd hij(0, 0);
      for (const auto& e : g.adj[i]) {
        if (e.first == j) {
          hij = e.second;
          break;
        }
      }
      cur.push_back(2.0 * imag(conj(psi[i]) * hij * psi[j]));
    }
    return cur;
  };

  vector<vector<int>> tri;
  for (size_t i = 0; i + 2 < s.ids.size() && tri.size() < 600; i += 997) {
    int a = s.ids[i], b = s.ids[i + 1], c = s.ids[i + 2];
    if (a < 0 || b < 0 || c < 0 || a == b || b == c || a == c) continue;
    tri.push_back({a, b, c});
  }

  printf("=== ORDER VALUE: sparse phase channel, attested vs reversed ===\n");
  printf("tokens=%d nodes=%d co_nnz=%zu ppmi_nnz=%zu topK=%d edges=%zu trigrams=%zu\n\n",
         s.tokens, s.n, s.co_nnz, s.ppmi_nnz, TOPK, g.edges.size(), tri.size());

  vector<vector<double>> X;
  vector<int> y;
  for (const auto& t : tri) {
    auto ca = drive_cur(t);
    if (ca.empty()) continue;
    vector<int> r = {t[2], t[1], t[0]};
    auto cr = drive_cur(r);
    if (cr.empty()) continue;
    X.push_back(ca);
    y.push_back(1);
    X.push_back(cr);
    y.push_back(-1);
  }

  int E = (int)g.edges.size();
  int n2 = (int)X.size();
  int ntr = n2 / 2;
  vector<double> dir(E, 0.0);
  for (int i = 0; i < ntr; i++) {
    for (int e = 0; e < E; e++) dir[e] += y[i] * X[i][e];
  }

  int ok = 0, ntst = 0;
  for (int i = ntr; i < n2; i++) {
    double score = 0.0;
    for (int e = 0; e < E; e++) score += X[i][e] * dir[e];
    int pred = score > 0.0 ? 1 : -1;
    ok += (pred == y[i]);
    ntst++;
  }

  printf("  PHASE (edge-current) sparse channel : attested-vs-reversed acc = %.1f%%  (chance 50%%)\n",
         100.0 * ok / ntst);
  printf("  bag-of-words                       : 50.0%%  (order-blind by construction)\n");
  printf("\nSparse check: no dense NxN PPMI, feature, or duplicate-edge matrix is allocated.\n");
  return 0;
}
