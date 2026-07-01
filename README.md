# GNNv2 — a weights-free graph neural substrate

A graph neural network built **without weights and without a trainer**. Computation
is carried by **phase, interference and superposition** on a unitary graph-wave
substrate; meaning lives in the **gauge-invariant flux**, not in stored numbers.

```
complex state z  →  Hermitian graph Hamiltonian H (topology + flux)  →  unitary evolution
```

## The three substrates

```
   FLOW                     DECORRELATION                 SETTLING / MEMORY
   unitary propagation  →   c = G^-1 ⟨k,q⟩           →    dissipative relaxation,
   (message passing,        (separate correlated          holographic recall,
    flux is the signal)      patterns; the glue)          amplitude amplification
   mix / spread             make content separable        sharpen / decide
```

One layer reduces to a single phase-weighted multiply-accumulate over edges,
`z_i ← Σ_j e^{i·δ_ij} z_j`; depth is `U^k`. No weights — the edge flux `δ_ij` is the
structure. Full description in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Headline results

| | |
|---|---|
| substrate physics | unitarity 1.2e-15, gauge/Wilson 4.4e-16, destructive interference 5.6e-31 |
| linear engine (nodes) | **1,000,000 nodes**, ~1.1M nodes/s on this host / ~2M on reference host, unitary 2.2e-16, linear time; propagator exact at any t (1e-12) |
| nonlinear engine (tokens) | **10,000,000 tokens**, ~71.5k tok/s on this host with packet memory + prepared Cayley flow (graph grows to ~1.43M nodes), ~3.09x Kerr compression, recognition 100% real vs 27.8% random |
| GNN | classification **100%**, weights-free learning **99.5%** on unseen (shuffle 52%) |
| decorrelation glue | routing **1.000** where naive collapses to 0.48 |
| **real data (Cora)** | **77.4%** weights-free — beats label-prop (~68%), nears trained GCN (~81.5%) |

Full table in [`docs/RESULTS.md`](docs/RESULTS.md). Every number was run live;
nothing is fitted.

## Repository layout

| path | contents |
|---|---|
| [`tools/`](tools/README.md) | substrate core (`graph_wave_substrate.hpp`) + 61 GNNv2 `*_contract_test` gates (physics, GNN grammar, nonlinear substrate, memory, decision), each an exit-0 machine-precision check, alongside spectral/self-organising diagnostics. (The 62nd `*_contract_test` is the GNNv3 RC1 gate, held separate.) |
| [`research/`](research/README.md) | 43 production studies and honest exploratory probes (scaling engine, nonlinear streaming, decorrelation glue, Cora benchmark, and negative results). |
| [`docs/`](docs/) | architecture, results, the physics-only discipline, the nonlinear engine, the [`PERFORMANCE.md`](docs/PERFORMANCE.md) speed ladder, [`STREAMING_THROUGHPUT_HANDOFF.md`](docs/STREAMING_THROUGHPUT_HANDOFF.md) for graph-only/linear/nonlinear throughput boundaries, the [`SCALING_AND_QUBITS.md`](docs/SCALING_AND_QUBITS.md) 100M projection + hidden-qubit view, and [`GNNv2_HANDOFF.md`](docs/GNNv2_HANDOFF.md) (read first). |

## Build, test & reproduce

**Prerequisites:** a C++20 compiler — on Windows an MSVC *Developer* prompt (`cl`);
CMake ≥ 3.20 works on any platform. Everything is C/C++; there are no Python,
PowerShell, or other side systems, and no data is bundled (Cora is fetched into the
git-ignored `data/`).

### One command — the full contract suite (start here)

```
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure      # the 61 GNNv2 contract gates
ctest --test-dir build -L nonlinear              # just the nonlinear suite
ctest --test-dir build -j 8                       # parallel (full suite is slow serially)
```

Every `*_contract_test` returns exit 0 **only** on a full pass, so a green `ctest`
**is** the proof — no extra interpretation needed. (Diagnostics `*_diagnostic_test`
and research probes are not ctest gates; run them directly, below.)

### Reproduce any headline result — what · why · how · expected

Standalone single-file builds, from the repo root in a Developer prompt. Each prints
its own metrics and a `RESULT`/exit code; the expected values are stated so nothing
has to be re-derived.

| what | why it exists | command | expected |
|---|---|---|---|
| **substrate physics** | exact unitarity, interference, gauge flux at machine precision | `cl /O2 /EHsc /std:c++20 /I tools tools\graph_wave_unitarity_test.cpp && .\graph_wave_unitarity_test.exe` | `5/5`; norm drift ~1e-13, destructive interference 5.6e-31 |
| **node-scaling engine** · unit **NODES** | the global GNN engine: stays unitary + linear-time at 10⁶ nodes | `cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp && .\probe_sparse_scale.exe` | `N=1,000,000` nodes, drift 2.22e-16; **~1.1M nodes/s here / ~2M reference** |
| **graph-stream only** · unit **TOKENS** (no field) | throughput ceiling — graph bookkeeping with the field OFF | `cl /O2 /EHsc /std:c++20 research\probe_graph_stream_only.cpp && .\probe_graph_stream_only.exe 1000000` | `RESULT PASS`, 1M tokens → 142,930 nodes; **~0.5M tok/s here / ~1.2–1.4M reference** |
| **linear field stream** · unit **TOKENS** (g=0) | streaming baseline: recognition with the nonlinearity OFF | `cl /O2 /EHsc /std:c++20 /I tools research\probe_linear_stream.cpp && .\probe_linear_stream.exe 1000000` | PR avg ~3.85, **value_LINEAR 100%**, `RESULT PASS`; **~50k tok/s here / ~150–185k reference** |
| **nonlinear Kerr stream** · unit **TOKENS** (g=7) | the production recognition + compression engine | `cl /O2 /EHsc /std:c++20 /I tools research\probe_streaming_compression.cpp && .\probe_streaming_compression.exe 10000000 7` | **~3.09x compression**, REAL 100% / RANDOM 27.8%, exit 0; **~71.5k tok/s here**, peak RAM ~1.23 GB at 10M tokens |
| **real data — Cora** · unit **NODES** | weights-free GNN on a real citation graph | place LINQS data so `cora/cora.content` + `cora/cora.cites` sit under `data/`, then build and run **from `data/`** (the probe opens `cora/...` relative to the working dir): `cl /O2 /EHsc /std:c++20 /I tools research\probe_cora.cpp && pushd data && ..\probe_cora.exe && popd` | own 58.3%, FLOW 1-hop 74.6%, **FLOW 2-hop 77.4%** |

Notes:
- `probe_sparse_scale` takes **no arguments** (sweeps N to 1,000,000 nodes). Streaming
  probes take `[stream_tokens] [uniqueEvery]` (default `60000 7`) — pass `1000000` for
  the 1M-token run. The closed Kerr engine `research/probe_nonlinear_engine.cpp` runs
  the same way: `… && .\probe_nonlinear_engine.exe 1000000`.
- **Never conflate the two engines:** `1,000,000 nodes` (node engine) ≠ `1,000,000
  tokens` (streaming engine); `nodes/s ≠ tokens/s`. Absolute throughput is
  host-dependent — the full **speed ladder** (node engine plus the three token
  regimes: graph-stream-only ~1.2–1.4M, realistic linear field ~150–185k reference,
  current nonlinear Kerr ~71.5k tok/s on this host at 10M, and exactly where the cost goes) is in
  [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md). Current nonlinear CTest gate is
  `ctest --test-dir build -C Release -L nonlinear --output-on-failure` and includes
  the pure-physics substrate contract plus the streaming compression smoke.

## Principle & discipline

- **Weights-free.** No trainer, no fitted parameters; the only structure is the graph
  topology and its gauge flux.
- **Physics, not arithmetic.** Computation is the wave itself — phase, interference,
  Kerr self-focusing. No ad-hoc algebra, no magnitude-only or phase-clipped readout;
  the full complex phase carrier is preserved end to end (see
  [`docs/PHYSICS_ONLY_DISCIPLINE.md`](docs/PHYSICS_ONLY_DISCIPLINE.md)).
- **C/C++ only.** All executable logic and tests are C++20. No Python, PowerShell,
  notebooks, or SQL/database framing; CMake + ctest is the only harness, and build
  artifacts (`*.obj`, `*.exe`, `build/`) are git-ignored — no clutter in the tree.
- **Verified & honest.** Every number is run live and checked by exact finite-N
  identities (unitarity, gauge invariance, exact interference); uncertain or negative
  results are recorded, not hidden.

## GNNv3 RC1 checkpoint

This repository now also contains the GNNv3 RC1 research checkpoint:

- `tools/graph_wave_v3_feeling_gate_contract_test.cpp`
- `docs/GNNv3_RC1_REPORT.md`
- `docs/GNNv3_RC1_HANDOFF.md`

GNNv3 RC1 tests a self-sensing carrier-field substrate where `chi/tau` narrows
the active physics window. The main gate is C++ only, physics-only, uses no
trigonometric readout in the gate, and includes a signed-current/stress audit to
detect phase clipping. Project discipline for RC1: no Python/JS/notebook side
systems for core work, no SQL/database framing, no hidden negative results.

Latest 1M RC1 result:

```text
adaptive feeling-gated:
  active_pairs=0.631
  psi compression=2.05x
  chi compression=3.56x
  sync_error=0
  CONTRACT RESULT: 8 / 8 PASS
```

See `docs/GNNv3_RC1_REPORT.md` for commands, test matrix, negative results, and
architecture notes. Read `docs/GNNv3_RC1_HANDOFF.md` before continuing research.
