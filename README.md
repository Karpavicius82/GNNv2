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
| linear engine (nodes) | **1,000,000 nodes**, ~1.1M nodes/s, unitary 2.2e-16, linear time; propagator exact at any t (1e-12) |
| nonlinear engine (tokens) | **1,000,000 tokens**, ~42k tokens/s (graph grows to ~143k nodes), ~3× Kerr compression, recognition 100% real vs 31% random |
| GNN | classification **100%**, weights-free learning **99.5%** on unseen (shuffle 52%) |
| decorrelation glue | routing **1.000** where naive collapses to 0.48 |
| **real data (Cora)** | **77.4%** weights-free — beats label-prop (~68%), nears trained GCN (~81.5%) |

Full table in [`docs/RESULTS.md`](docs/RESULTS.md). Every number was run live;
nothing is fitted.

## Repository layout

| path | contents |
|---|---|
| [`tools/`](tools/README.md) | substrate core (`graph_wave_substrate.hpp`) + 60 GNNv2 `*_contract_test` gates (physics, GNN grammar, the working GNN, memory, decision), each an exit-0 machine-precision check, alongside spectral/self-organising diagnostics. (The 61st `*_contract_test` is the GNNv3 RC1 gate, held separate.) |
| [`research/`](research/README.md) | production studies (scaling engine, decorrelation glue, the Cora benchmark) and honest exploratory probes, including negative results. |
| [`docs/`](docs/) | architecture, results, the physics-only discipline, the nonlinear engine, and [`GNNv2_HANDOFF.md`](docs/GNNv2_HANDOFF.md) (read first). |

## Build & run (Windows / MSVC)

Each file is standalone C++20. From a Developer prompt:

```bat
cl /O2 /EHsc /std:c++20 /I tools tools\graph_wave_unitarity_test.cpp && .\graph_wave_unitarity_test.exe
cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp       && .\probe_sparse_scale.exe
```

Build everything and run the contract gates with CMake + ctest (C/C++ only, no
scripts):

```
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure        # all contract gates
ctest --test-dir build -L nonlinear               # just the nonlinear suite
```

Each `*_contract_test` returns exit 0 only on a full pass, so ctest is the suite.

### Run the two engines (no guessing)

GNNv2 has **two separate engines** (see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) §4).
Here is exactly where each lives and how to run it:

| engine | file | run it (1M) | it prints |
|---|---|---|---|
| **linear** — graph propagation, unit **NODES** | `research/probe_sparse_scale.cpp` | `cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp && .\probe_sparse_scale.exe` | a sweep up to **N = 1,000,000 nodes**: norm drift, neighbour overlap, ms → **≈1.1M nodes/s** |
| **nonlinear** — Kerr streaming, unit **TOKENS** | `research/probe_streaming_compression.cpp` | `cl /O2 /EHsc /std:c++20 /I tools research\probe_streaming_compression.cpp && .\probe_streaming_compression.exe 1000000` | `stream`, `nodes`, `compression`, `REAL/RANDOM`, **`tokens_per_sec`** at **1,000,000 tokens** → **≈42k tokens/s**; exits 0 only if the contract passes |

Notes:
- `probe_sparse_scale` takes **no arguments** — it sweeps N internally to 1,000,000 nodes. Speed = nodes ÷ ms.
- The streaming probe takes `[stream_tokens] [uniqueEvery]` (default `60000 7`); pass `1000000` to stream 1M tokens. The closed nonlinear engine `research/probe_nonlinear_engine.cpp` runs the same way: `… && .\probe_nonlinear_engine.exe 1000000`.
- **`1,000,000 nodes` is the linear engine; `1,000,000 tokens` is the nonlinear engine. `nodes/s ≠ tokens/s`.**

## Principle

Everything is **weights-free** and verified by **exact finite-N identities**
(unitarity, gauge invariance, exact interference) at machine precision — not by
fitted exponents or asymptotic claims. Where a result is uncertain or a probe
failed, that is recorded honestly rather than hidden.

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
