# Citation Benchmark Audit - 2026-06-23

This file records the C++-only benchmark pass performed against commit
`0bec6e43b5f19cda60e00ca05dae93d1c94f9e30`.

No Python was used for the benchmark implementation or scoring. The benchmark
binary was written in C++20, built with MSVC `/O2`, and run against raw text
dataset files in the ignored `data/` directory. Generated binaries, downloaded
datasets, and temporary source files were intentionally left under ignored
paths and were not staged.

## Repository State

```text
HEAD: 0bec6e43b5f19cda60e00ca05dae93d1c94f9e30
branch: main
remote: origin/main
status before report: clean
commit subject: Clean documentation, remove V26 references, organize tests
```

Tracked source inventory at the time:

```text
tools/*.cpp:     59
tools/*.hpp:      1
research/*.cpp:  21
docs/*.md:        2
```

## Existing Contract Test Result

The existing `tools/*.cpp` contract executables were built and run from the
current checkout.

```text
PASS=59
FAIL=0
TOTAL=59
runtime_ms=169836
```

Selected existing contract outputs:

```text
graph_wave_learning_contract_test:
  examples=4   -> 83.0%
  examples=8   -> 92.0%
  examples=32  -> 98.0%
  examples=128 -> 100.0%
  unseen test  -> 99.5%
  shuffled labels -> 52.0%
  RESULT : 3 / 3 verified

graph_wave_gnn_task_contract_test:
  neighbourhood majority -> 13/13 = 100.0%
  message passing -> 100.0%
  own-feature baseline -> 69.2%
  2-hop task -> 1-round 57.1%, 2-round 92.9%
  RESULT : 4 / 4 verified

graph_wave_unitarity_test:
  max norm drift -> 1.683e-13
  return err -> 1.370e-14
  operator err -> 1.221e-15
  destructive interference -> 5.628e-31
  constructive/destructive ratio -> 3.819e11x
  RESULT: 5 / 5 physical invariants verified
```

## Benchmark Scope

Goal: test whether the Cora result was a one-off or whether the same weights-free
FLOW/prototype method generalizes to other citation graph benchmarks.

Datasets:

```text
Cora:
  source already present locally under data/cora/
  format: LINQS .content + .cites
  N=2708, edges_seen=5429, F=1433, C=7

CiteSeer:
  downloaded raw archive:
    https://linqs-data.soe.ucsc.edu/public/lbc/citeseer.tgz
  format: LINQS .content + .cites
  N=3312, edges_seen=4591, F=3703, C=6

PubMed-Diabetes:
  downloaded raw archive:
    https://linqs-data.soe.ucsc.edu/public/Pubmed-Diabetes.tgz
  format: tab-delimited NODE.paper + DIRECTED.cites
  N=19717, edges_seen=44335, F=500, C=3
```

Downloaded data remained ignored by `.gitignore`:

```text
data/
build/
```

## Benchmark Method

The benchmark intentionally mirrors `research/probe_cora.cpp`:

1. Load node features, labels, and citation graph.
2. Treat citations as an undirected graph, matching the existing Cora probe.
3. L2-normalize every node feature vector.
4. Build random low-label splits with exactly `20` labeled nodes per class.
5. Use the remaining nodes as test nodes.
6. Compute:
   - own features, no graph propagation;
   - FLOW 1-hop: self + neighbors, then L2 normalization;
   - FLOW 2-hop: repeat the same hop once more.
7. Build class prototypes from train nodes only.
8. Classify test nodes by cosine overlap with class prototypes.
9. Optionally apply decorrelation by solving `G c = s`, where `G` is the class
   prototype Gram matrix and `s` is the vector of prototype overlaps.

There is no backpropagation, no trainable weight matrix, no optimizer, and no
learned hidden layer.

## Build Command

The benchmark harness was compiled with MSVC from a Developer environment:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /O2 /EHsc /std:c++20 "C:\Users\Admin\GNNv2\build\citation_benchmark.cpp" /Fe:"C:\Users\Admin\GNNv2\build\citation_benchmark.exe" /Fo:"C:\Users\Admin\GNNv2\build\citation_benchmark.obj"
```

The source file was placed under ignored `build/` during the run. The essential
implementation contract is preserved below so this report remains self-contained.

## Validation Run: 1 Seed

This run verifies that the harness reproduces the existing Cora headline numbers
from `research/probe_cora.cpp`.

Command:

```bat
.\build\citation_benchmark.exe 1
```

Output:

```text
GraphWave citation benchmark, C++ only, no trainer, 20 labels/class.
Method: L2 features -> self+neighbor FLOW hops -> class mean prototypes -> cosine readout; optional CG G^-1 decorrelation.

=== Cora ===
N=2708 edges_seen=5429 F=1433 C=7 split=20/class seeds=1..1 time_ms=784
seed=1: own 58.3 / 58.6 dec, h1 74.6 / 74.8 dec, h2 77.4 / 77.9 dec
  own           naive  58.29 +- 0.00   decor  58.64 +- 0.00
  FLOW 1-hop    naive  74.57 +- 0.00   decor  74.84 +- 0.00
  FLOW 2-hop    naive  77.38 +- 0.00   decor  77.92 +- 0.00
  delta h2-own  naive +19.08   decor +19.28

=== CiteSeer ===
N=3312 edges_seen=4591 F=3703 C=6 split=20/class seeds=1..1 time_ms=2344
seed=1: own 56.7 / 56.4 dec, h1 70.5 / 70.2 dec, h2 71.4 / 71.6 dec
  own           naive  56.70 +- 0.00   decor  56.42 +- 0.00
  FLOW 1-hop    naive  70.49 +- 0.00   decor  70.24 +- 0.00
  FLOW 2-hop    naive  71.43 +- 0.00   decor  71.59 +- 0.00
  delta h2-own  naive +14.72   decor +15.16

=== PubMed ===
N=19717 edges_seen=44335 F=500 C=3 split=20/class seeds=1..1 time_ms=1548
seed=1: own 72.4 / 72.3 dec, h1 72.5 / 72.5 dec, h2 73.3 / 73.3 dec
  own           naive  72.35 +- 0.00   decor  72.35 +- 0.00
  FLOW 1-hop    naive  72.53 +- 0.00   decor  72.49 +- 0.00
  FLOW 2-hop    naive  73.27 +- 0.00   decor  73.32 +- 0.00
  delta h2-own  naive  +0.92   decor  +0.97
```

## Stability Run: 10 Seeds

Command:

```bat
.\build\citation_benchmark.exe 10
```

Output:

```text
GraphWave citation benchmark, C++ only, no trainer, 20 labels/class.
Method: L2 features -> self+neighbor FLOW hops -> class mean prototypes -> cosine readout; optional CG G^-1 decorrelation.

=== Cora ===
N=2708 edges_seen=5429 F=1433 C=7 split=20/class seeds=1..10 time_ms=7060
seed=1: own 58.3 / 58.6 dec, h1 74.6 / 74.8 dec, h2 77.4 / 77.9 dec
  own           naive  57.17 +- 1.73   decor  57.42 +- 1.64
  FLOW 1-hop    naive  75.40 +- 1.32   decor  75.67 +- 1.21
  FLOW 2-hop    naive  79.05 +- 1.21   decor  79.16 +- 1.05
  delta h2-own  naive +21.88   decor +21.74

=== CiteSeer ===
N=3312 edges_seen=4591 F=3703 C=6 split=20/class seeds=1..10 time_ms=21805
seed=1: own 56.7 / 56.4 dec, h1 70.5 / 70.2 dec, h2 71.4 / 71.6 dec
  own           naive  59.23 +- 1.52   decor  59.30 +- 1.53
  FLOW 1-hop    naive  69.06 +- 1.65   decor  69.26 +- 1.59
  FLOW 2-hop    naive  70.40 +- 1.36   decor  70.46 +- 1.30
  delta h2-own  naive +11.17   decor +11.17

=== PubMed ===
N=19717 edges_seen=44335 F=500 C=3 split=20/class seeds=1..10 time_ms=9438
seed=1: own 72.4 / 72.3 dec, h1 72.5 / 72.5 dec, h2 73.3 / 73.3 dec
  own           naive  69.73 +- 1.57   decor  69.70 +- 1.53
  FLOW 1-hop    naive  72.79 +- 1.76   decor  72.89 +- 1.82
  FLOW 2-hop    naive  73.67 +- 2.46   decor  73.84 +- 2.71
  delta h2-own  naive  +3.94   decor  +4.14
```

## Main Run: 30 Seeds

Command:

```bat
.\build\citation_benchmark.exe 30
```

Output:

```text
GraphWave citation benchmark, C++ only, no trainer, 20 labels/class.
Method: L2 features -> self+neighbor FLOW hops -> class mean prototypes -> cosine readout; optional CG G^-1 decorrelation.

=== Cora ===
N=2708 edges_seen=5429 F=1433 C=7 split=20/class seeds=1..30 time_ms=20260
seed=1: own 58.3 / 58.6 dec, h1 74.6 / 74.8 dec, h2 77.4 / 77.9 dec
  own           naive  57.28 +- 1.34   decor  57.58 +- 1.32
  FLOW 1-hop    naive  75.45 +- 1.40   decor  75.72 +- 1.31
  FLOW 2-hop    naive  79.00 +- 1.23   decor  79.03 +- 1.16
  delta h2-own  naive +21.72   decor +21.45

=== CiteSeer ===
N=3312 edges_seen=4591 F=3703 C=6 split=20/class seeds=1..30 time_ms=51386
seed=1: own 56.7 / 56.4 dec, h1 70.5 / 70.2 dec, h2 71.4 / 71.6 dec
  own           naive  59.05 +- 1.17   decor  59.15 +- 1.14
  FLOW 1-hop    naive  68.78 +- 1.39   decor  68.96 +- 1.36
  FLOW 2-hop    naive  70.30 +- 1.14   decor  70.39 +- 1.19
  delta h2-own  naive +11.25   decor +11.24

=== PubMed ===
N=19717 edges_seen=44335 F=500 C=3 split=20/class seeds=1..30 time_ms=24824
seed=1: own 72.4 / 72.3 dec, h1 72.5 / 72.5 dec, h2 73.3 / 73.3 dec
  own           naive  70.67 +- 2.23   decor  70.57 +- 2.16
  FLOW 1-hop    naive  73.11 +- 2.19   decor  73.08 +- 2.17
  FLOW 2-hop    naive  73.59 +- 2.35   decor  73.59 +- 2.46
  delta h2-own  naive  +2.93   decor  +3.02
```

## Result Summary

Mean accuracy over 30 random low-label splits:

```text
Dataset   Own naive       FLOW 1-hop      FLOW 2-hop      h2-own
Cora      57.28 +- 1.34   75.45 +- 1.40   79.00 +- 1.23   +21.72
CiteSeer  59.05 +- 1.17   68.78 +- 1.39   70.30 +- 1.14   +11.25
PubMed    70.67 +- 2.23   73.11 +- 2.19   73.59 +- 2.35    +2.93
```

Decorrelated FLOW 2-hop:

```text
Cora      79.03 +- 1.16
CiteSeer  70.39 +- 1.19
PubMed    73.59 +- 2.46
```

## Interpretation

The original Cora number was not a one-off. Under the same protocol, FLOW gives
a large gain on Cora and CiteSeer:

```text
Cora:     +21.72 percentage points over own-feature prototypes
CiteSeer: +11.25 percentage points over own-feature prototypes
```

PubMed behaves differently. The raw features are already much stronger, and the
graph adds only about three percentage points:

```text
PubMed: +2.93 percentage points over own-feature prototypes
```

The `G^-1` decorrelation layer is not the driver on these three citation
benchmarks. It is nearly neutral here. The main effect is FLOW/message passing.
This is consistent with the existing Cora note: decorrelation matters strongly
in high-correlation binding probes, but only marginally on these citation
classification datasets.

## Limits

This benchmark is a serious sanity check, not a full graph ML leaderboard.

Known limits:

- It uses random `20 labels/class` splits, not every historical fixed split.
- It does not train or tune hyperparameters.
- It does not compare against a locally re-run GCN/GAT/MLP baseline.
- It treats citations as undirected, matching the current Cora probe.
- It records downloaded raw data paths, but does not commit datasets.

The finding is therefore narrow and defensible:

```text
The weights-free FLOW/prototype pipeline generalizes beyond Cora to CiteSeer
with a strong gain, and to PubMed with a smaller but positive gain, under the
same low-label split protocol.
```

## C++ Benchmark Harness Contract

The temporary harness was a C++20 executable. The core algorithm below is the
logic used for all three datasets.

```cpp
// C++20 benchmark contract, no trainer, no Python.
//
// Data:
//   Cora/CiteSeer: LINQS .content + .cites
//   PubMed: Pubmed-Diabetes.NODE.paper.tab + DIRECTED.cites.tab
//
// Method:
//   1. L2 normalize node features.
//   2. Make undirected citation adjacency.
//   3. Pick 20 training nodes per class with std::mt19937(seed).
//   4. FLOW hop = L2(self feature + sum(neighbor features)).
//   5. Class prototype = L2(mean of train node features per class).
//   6. Readout = argmax cosine(prototype, node feature).
//   7. Optional decorrelation solves G c = s by conjugate gradient, where
//      G is the class-prototype Gram matrix and s is the overlap vector.

struct Scores {
  double own, own_dec;
  double h1, h1_dec;
  double h2, h2_dec;
};

static void l2_normalize(std::vector<std::vector<float>>& x) {
  for (auto& row : x) {
    double n = 0.0;
    for (float v : row) n += double(v) * double(v);
    n = std::sqrt(n) + 1e-9;
    for (float& v : row) v = float(v / n);
  }
}

static std::vector<std::vector<float>> hop(
    const Dataset& d,
    const std::vector<std::vector<float>>& x) {
  std::vector<std::vector<float>> y(d.n, std::vector<float>(d.f, 0.0f));
  for (int i = 0; i < d.n; ++i) {
    for (int k = 0; k < d.f; ++k) y[i][k] = x[i][k];
    for (int j : d.adj[i])
      for (int k = 0; k < d.f; ++k) y[i][k] += x[j][k];
  }
  l2_normalize(y);
  return y;
}

static std::vector<int> make_train_mask(
    const Dataset& d,
    int per_class,
    unsigned seed) {
  std::vector<int> order(d.n);
  std::iota(order.begin(), order.end(), 0);
  std::mt19937 rng(seed);
  std::shuffle(order.begin(), order.end(), rng);

  std::vector<int> used(d.c, 0), train(d.n, 0);
  for (int i : order) {
    int c = d.y[i];
    if (used[c] < per_class) {
      train[i] = 1;
      ++used[c];
    }
  }
  return train;
}

static std::vector<double> cg_solve(
    const std::vector<std::vector<double>>& g,
    const std::vector<double>& s) {
  const int c = int(s.size());
  auto mv = [&](const std::vector<double>& x) {
    std::vector<double> y(c, 0.0);
    for (int i = 0; i < c; ++i)
      for (int j = 0; j < c; ++j) y[i] += g[i][j] * x[j];
    return y;
  };
  auto dot = [&](const std::vector<double>& a, const std::vector<double>& b) {
    double r = 0.0;
    for (int i = 0; i < c; ++i) r += a[i] * b[i];
    return r;
  };

  std::vector<double> x(c, 0.0), r = s, p = s;
  double rs = dot(r, r);
  for (int it = 0; it < c + 3; ++it) {
    std::vector<double> gp = mv(p);
    double a = rs / (dot(p, gp) + 1e-12);
    for (int i = 0; i < c; ++i) {
      x[i] += a * p[i];
      r[i] -= a * gp[i];
    }
    double rs2 = dot(r, r);
    if (rs2 < 1e-20) break;
    double b = rs2 / rs;
    for (int i = 0; i < c; ++i) p[i] = r[i] + b * p[i];
    rs = rs2;
  }
  return x;
}

static double evaluate(
    const Dataset& d,
    const std::vector<std::vector<float>>& x,
    const std::vector<int>& train,
    bool decor) {
  std::vector<std::vector<double>> proto(d.c, std::vector<double>(d.f, 0.0));

  for (int i = 0; i < d.n; ++i) {
    if (!train[i]) continue;
    int c = d.y[i];
    for (int k = 0; k < d.f; ++k) proto[c][k] += x[i][k];
  }

  for (int c = 0; c < d.c; ++c) {
    double n = 0.0;
    for (int k = 0; k < d.f; ++k) n += proto[c][k] * proto[c][k];
    n = std::sqrt(n) + 1e-9;
    for (int k = 0; k < d.f; ++k) proto[c][k] /= n;
  }

  std::vector<std::vector<double>> gram(d.c, std::vector<double>(d.c, 0.0));
  for (int a = 0; a < d.c; ++a)
    for (int b = 0; b < d.c; ++b)
      for (int k = 0; k < d.f; ++k) gram[a][b] += proto[a][k] * proto[b][k];

  int ok = 0, tot = 0;
  for (int i = 0; i < d.n; ++i) {
    if (train[i]) continue;
    std::vector<double> s(d.c, 0.0);
    for (int c = 0; c < d.c; ++c)
      for (int k = 0; k < d.f; ++k) s[c] += proto[c][k] * x[i][k];
    if (decor) s = cg_solve(gram, s);
    int best = 0;
    for (int c = 1; c < d.c; ++c)
      if (s[c] > s[best]) best = c;
    ok += (best == d.y[i]);
    ++tot;
  }
  return tot ? 100.0 * ok / tot : 0.0;
}

static Scores run_one_seed(
    const Dataset& d,
    const std::vector<std::vector<float>>& h1,
    const std::vector<std::vector<float>>& h2,
    int seed) {
  std::vector<int> train = make_train_mask(d, 20, unsigned(seed));
  return Scores{
      evaluate(d, d.x, train, false),
      evaluate(d, d.x, train, true),
      evaluate(d, h1, train, false),
      evaluate(d, h1, train, true),
      evaluate(d, h2, train, false),
      evaluate(d, h2, train, true),
  };
}
```

The dataset loaders were direct text parsers:

```text
LINQS loader:
  token[0]      -> node id
  token[1..n-2] -> binary feature vector
  token[n-1]    -> class label
  .cites lines  -> undirected adjacency entries when both ids are present

PubMed loader:
  NODE.paper.tab schema -> numeric feature names
  each node row         -> label=..., w-token=value features
  DIRECTED.cites.tab    -> paper:id fields converted to undirected adjacency
```

## Preservation Decision

Only this report should be committed. The benchmark source and data remain in
ignored locations to avoid polluting the repository with generated files or
external dataset archives.

