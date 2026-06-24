#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace sem_sparse {

struct SparseText8 {
  bool ok = false;
  int tokens = 0;
  int n = 0;
  double total_co = 0.0;
  size_t co_nnz = 0;
  size_t ppmi_nnz = 0;
  std::unordered_map<std::string, int> id;
  std::vector<std::string> word;
  std::vector<int> ids;
  std::vector<std::unordered_map<int, float>> fwd;
  std::vector<std::vector<std::pair<int, float>>> emb;
  std::vector<std::vector<std::pair<int, float>>> emb_in;
};

static float lookup(const std::vector<std::unordered_map<int, float>>& rows, int i, int j) {
  auto it = rows[i].find(j);
  return it == rows[i].end() ? 0.0f : it->second;
}

static SparseText8 load_text8_sparse(const char* path, int tok, int vocab, int win) {
  SparseText8 out;
  std::ifstream in(path);
  if (!in) return out;

  std::unordered_map<std::string, int> cnt;
  std::string w;
  int nread = 0;
  while (nread < tok && (in >> w)) {
    cnt[w]++;
    nread++;
  }

  std::vector<std::pair<std::string, int>> wv(cnt.begin(), cnt.end());
  std::sort(wv.begin(), wv.end(), [](const auto& a, const auto& b) {
    return a.second != b.second ? a.second > b.second : a.first < b.first;
  });
  if ((int)wv.size() > vocab) wv.resize(vocab);

  for (const auto& p : wv) {
    out.id[p.first] = (int)out.word.size();
    out.word.push_back(p.first);
  }
  out.n = (int)out.word.size();
  out.tokens = nread;

  in.clear();
  in.seekg(0);
  out.ids.reserve(nread);
  int k = 0;
  while (k < tok && (in >> w)) {
    auto it = out.id.find(w);
    out.ids.push_back(it == out.id.end() ? -1 : it->second);
    k++;
  }

  std::vector<std::unordered_map<int, float>> co(out.n);
  out.fwd.assign(out.n, {});
  for (size_t i = 0; i < out.ids.size(); i++) {
    int a = out.ids[i];
    if (a < 0) continue;
    for (int d = 1; d <= win && i + d < out.ids.size(); d++) {
      int b = out.ids[i + d];
      if (b < 0 || a == b) continue;
      float ww = 1.0f / (float)d;
      co[a][b] += ww;
      co[b][a] += ww;
      out.fwd[a][b] += ww;
    }
  }

  std::vector<double> rsum(out.n, 0.0);
  for (int i = 0; i < out.n; i++) {
    out.co_nnz += co[i].size();
    for (const auto& kv : co[i]) {
      rsum[i] += kv.second;
      out.total_co += kv.second;
    }
  }

  std::vector<std::unordered_map<int, float>> emb_map(out.n);
  out.emb.assign(out.n, {});
  out.emb_in.assign(out.n, {});
  for (int i = 0; i < out.n; i++) {
    double norm = 0.0;
    std::vector<std::pair<int, float>> row;
    for (const auto& kv : co[i]) {
      int j = kv.first;
      double c = kv.second;
      if (c <= 0.0 || rsum[i] <= 0.0 || rsum[j] <= 0.0) continue;
      double pmi = std::log(c * out.total_co / (rsum[i] * rsum[j]));
      if (pmi <= 0.0) continue;
      row.push_back({j, (float)pmi});
      norm += pmi * pmi;
    }
    norm = std::sqrt(norm) + 1e-12;
    for (auto& kv : row) {
      kv.second = (float)(kv.second / norm);
      emb_map[i][kv.first] = kv.second;
      out.emb_in[kv.first].push_back({i, kv.second});
    }
    std::sort(row.begin(), row.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
    });
    out.ppmi_nnz += row.size();
    out.emb[i].swap(row);
  }

  out.ok = true;
  return out;
}

static std::vector<std::vector<int>> topk_directed(const SparseText8& s, int topk) {
  std::vector<std::vector<int>> graph(s.n);
  for (int i = 0; i < s.n; i++) {
    std::unordered_map<int, float> score;
    for (const auto& kv : s.emb[i]) {
      if (kv.first != i) score[kv.first] += 0.5f * kv.second;
    }
    for (const auto& kv : s.emb_in[i]) {
      if (kv.first != i) score[kv.first] += 0.5f * kv.second;
    }

    std::vector<std::pair<float, int>> cand;
    cand.reserve(score.size());
    for (const auto& kv : score) {
      if (kv.second > 0.0f) cand.push_back({kv.second, kv.first});
    }
    std::sort(cand.begin(), cand.end(), [](const auto& a, const auto& b) {
      return a.first != b.first ? a.first > b.first : a.second < b.second;
    });

    int lim = std::min(topk, (int)cand.size());
    graph[i].reserve(lim);
    for (int t = 0; t < lim; t++) graph[i].push_back(cand[t].second);
  }
  return graph;
}

static double symmetric_score(const SparseText8& s, int i, int j) {
  double a = 0.0, b = 0.0;
  for (const auto& kv : s.emb[i]) {
    if (kv.first == j) {
      a = kv.second;
      break;
    }
  }
  for (const auto& kv : s.emb[j]) {
    if (kv.first == i) {
      b = kv.second;
      break;
    }
  }
  return 0.5 * (a + b);
}

}  // namespace sem_sparse
