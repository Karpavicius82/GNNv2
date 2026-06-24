// The decisive cut, now without a dense NxN substrate: build sparse text8 PPMI
// rows by streaming local windows, keep a sparse top-K semantic graph, and ask
// whether FLOW can denoise corrupted word features from real neighbours better
// than from degree-matched random neighbours.
#include "../tools/graph_wave_substrate.hpp"
#include "semantic_sparse_text8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
using sem_sparse::SparseText8;

using SVec = vector<pair<int, double>>;

static SVec materialize(unordered_map<int, double>& m) {
  SVec out;
  out.reserve(m.size());
  for (const auto& kv : m) {
    if (fabs(kv.second) > 1e-15) out.push_back(kv);
  }
  sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  return out;
}

static SVec clean_row(const SparseText8& s, int wd) {
  SVec out;
  out.reserve(s.emb[wd].size());
  for (const auto& kv : s.emb[wd]) out.push_back({kv.first, kv.second});
  return out;
}

static double dot_sparse(const SVec& a, const SVec& b) {
  double d = 0.0;
  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i].first == b[j].first) {
      d += a[i].second * b[j].second;
      i++;
      j++;
    } else if (a[i].first < b[j].first) {
      i++;
    } else {
      j++;
    }
  }
  return d;
}

static double norm2_sparse(const SVec& a) {
  double n = 0.0;
  for (const auto& kv : a) n += kv.second * kv.second;
  return n;
}

static double cos_sparse(const SVec& a, const SVec& b) {
  return dot_sparse(a, b) / (sqrt(norm2_sparse(a) * norm2_sparse(b)) + 1e-15);
}

static SVec corrupt_row(const SVec& row, mt19937& rng) {
  uniform_real_distribution<double> U(0.0, 1.0);
  normal_distribution<double> G(0.0, 1.0);
  SVec out;
  out.reserve(row.size() / 20 + 4);
  for (const auto& kv : row) {
    if (U(rng) < 0.95) continue;
    double v = kv.second + 0.02 * G(rng);
    if (fabs(v) > 1e-15) out.push_back({kv.first, v});
  }
  return out;
}

static SVec flow_rep(const vector<vector<int>>& graph, const vector<SVec>& x, int source, int hops) {
  unordered_map<int, double> node_w, next;
  node_w[source] = 1.0;
  for (int h = 0; h < hops; h++) {
    next.clear();
    for (const auto& kv : node_w) {
      int u = kv.first;
      double w = kv.second;
      next[u] += w;
      for (int v : graph[u]) next[v] += w;
    }
    node_w.swap(next);
  }

  unordered_map<int, double> acc;
  for (const auto& nw : node_w) {
    for (const auto& fv : x[nw.first]) acc[fv.first] += nw.second * fv.second;
  }
  return materialize(acc);
}

int main() {
  const int TOK = 1000000, VOCAB = 4096, WIN = 4, TOPK = 32;
  SparseText8 s = sem_sparse::load_text8_sparse("data/text8", TOK, VOCAB, WIN);
  if (!s.ok) {
    printf("no data/text8\n");
    return 2;
  }

  vector<vector<int>> real_graph = sem_sparse::topk_directed(s, TOPK);
  vector<vector<int>> rand_graph(s.n);
  mt19937 graph_rng(7);
  uniform_int_distribution<int> rn(0, s.n - 1);
  size_t real_edges = 0, rand_edges = 0;
  for (int i = 0; i < s.n; i++) {
    real_edges += real_graph[i].size();
    rand_graph[i].reserve(real_graph[i].size());
    for (size_t e = 0; e < real_graph[i].size(); e++) {
      int j = rn(graph_rng);
      if (j != i) rand_graph[i].push_back(j);
    }
    rand_edges += rand_graph[i].size();
  }

  vector<vector<string>> CATS = {
      {"france", "germany", "spain", "italy", "russia", "china", "japan", "england", "india", "greece"},
      {"war", "army", "military", "soldiers", "battle", "navy", "troops", "forces", "enemy", "weapons"},
      {"church", "god", "jesus", "christian", "catholic", "bible", "religious", "faith", "holy", "priest"},
      {"music", "song", "band", "album", "jazz", "guitar", "songs", "pop", "musical", "rock"},
      {"theory", "energy", "physics", "mathematics", "chemistry", "quantum", "particles", "equation", "mass", "atoms"},
      {"blood", "heart", "brain", "bone", "muscle", "skin", "cells", "body", "disease", "organs"}};
  vector<vector<int>> cat;
  for (auto& c : CATS) {
    vector<int> v;
    for (auto& word : c) {
      auto it = s.id.find(word);
      if (it != s.id.end()) v.push_back(it->second);
    }
    if (v.size() >= 6) cat.push_back(v);
  }
  int C = (int)cat.size();

  vector<SVec> clean(s.n), corrupted(s.n);
  mt19937 corrupt_rng(13);
  size_t clean_nnz = 0, corrupt_nnz = 0;
  for (int i = 0; i < s.n; i++) {
    clean[i] = clean_row(s, i);
    corrupted[i] = corrupt_row(clean[i], corrupt_rng);
    clean_nnz += clean[i].size();
    corrupt_nnz += corrupted[i].size();
  }

  printf("=== SUBSTRATE VALUE: sparse semantic graph beats random/identity ===\n");
  printf("tokens=%d nodes=%d co_nnz=%zu ppmi_nnz=%zu clean_nnz=%zu corrupt_nnz=%zu\n",
         s.tokens, s.n, s.co_nnz, s.ppmi_nnz, clean_nnz, corrupt_nnz);
  printf("topK=%d real_edges=%zu random_edges=%zu categories=%d (words/cat: ",
         TOPK, real_edges, rand_edges, C);
  for (auto& v : cat) printf("%zu ", v.size());
  printf(")\n\n");

  auto eval = [&](function<SVec(int)> rep) {
    int ok = 0, tot = 0;
    vector<SVec> proto(C);
    for (int c = 0; c < C; c++) {
      unordered_map<int, double> acc;
      int h = (int)cat[c].size() / 2;
      for (int t = 0; t < h; t++) {
        for (const auto& kv : clean[cat[c][t]]) acc[kv.first] += kv.second;
      }
      proto[c] = materialize(acc);
    }
    for (int c = 0; c < C; c++) {
      int h = (int)cat[c].size() / 2;
      for (int t = h; t < (int)cat[c].size(); t++) {
        int wd = cat[c][t];
        SVec r = rep(wd);
        int best = 0;
        double bs = -2.0;
        for (int cc = 0; cc < C; cc++) {
          double score = cos_sparse(r, proto[cc]);
          if (score > bs) {
            bs = score;
            best = cc;
          }
        }
        ok += (best == c);
        tot++;
      }
    }
    return 100.0 * ok / tot;
  };

  printf("method                         | category accuracy (sparse corrupted features)\n");
  printf("  PPMI-only (clean, ceiling)    | %.1f%%\n", eval([&](int wd) { return clean[wd]; }));
  printf("  identity (corrupted sparse)   | %.1f%%\n", eval([&](int wd) { return corrupted[wd]; }));
  printf("  FLOW on MATCHED-RANDOM sparse | %.1f%%\n", eval([&](int wd) { return flow_rep(rand_graph, corrupted, wd, 2); }));
  printf("  FLOW on REAL SEMANTIC sparse  | %.1f%%\n", eval([&](int wd) { return flow_rep(real_graph, corrupted, wd, 2); }));
  printf("\nSparse check: no dense NxN PPMI/feature matrix is allocated; the graph is local top-K and FLOW is computed on demand.\n");
  return 0;
}
