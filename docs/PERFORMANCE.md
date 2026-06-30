# GNNv2 Performance — engines, units, and the speed ladder

This is the precise reference for "how fast is GNNv2 and why", so the numbers are
never confused again. Two rules up front:

1. There are **two distinct engines** with **different units** (see `ARCHITECTURE.md`
   §4). The **linear scaling engine** is measured in **NODES/s**; everything in the
   **streaming engine** is measured in **TOKENS/s**. `nodes/s ≠ tokens/s` — never
   compare them.
2. Absolute throughput is **container/build dependent**. Below, "this host" and a
   faster "reference host" are both quoted; the **ordering and ratios** are the
   robust facts, not the absolute numbers.

---

## 1. The node-scaling engine (unit: NODES)

`research/probe_sparse_scale.cpp` — sparse unitary Chebyshev propagation of
`e^{-iHt}` over a fixed graph of **N nodes** (`O(E)` per Chebyshev term). No token
stream, no plasticity: it propagates one state over a static graph.

| quantity | this host | reference host |
|---|---|---|
| N = 1,000,000 **nodes** | ~0.85–0.90 s → **~1.1M nodes/s** | ~0.49 s → **~2.0M nodes/s** |
| norm drift at 1e6 nodes | 2.22e-16 | 1.11e-16 |
| Chebyshev terms | 23 | 23 |

This is the only place "**1,000,000 nodes / ~1–2M nodes/s**" applies. It is **not** a
token throughput. Run: `cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp && .\probe_sparse_scale.exe`.

---

## 2. The streaming engine — the TOKEN speed ladder

The streaming engine consumes a sequence of **tokens**. Each token event does two
things: (a) update the plastic **graph**, (b) optionally evolve the **wave field**.
How much of (b) you run sets the speed. Three regimes, slowest at the bottom because
each adds work on top of the one above:

| regime | per-token hot loop | file | tok/s (this host) | tok/s (reference host) |
|---|---|---|---|---|
| **graph-stream only** (no field) | node add · `touch`/`decay`/`prune` · window · plaquette phase | `research/probe_graph_stream_only.cpp` | **~0.5M** | **~1.2–1.4M** |
| **realistic linear field** (`g=0`) | the above **+ project → edge-flow → unproject** | `research/probe_linear_stream.cpp` | **~50–57k** | **~177–184k** |
| **nonlinear Kerr** (`g=7`) | packet memory + prepared Cayley carrier + Kerr self-focusing phase | `research/probe_streaming_compression.cpp` | **~71–90k** (71.5k at 10M, ~90k at 300k) | reference not remeasured after packet/prepared |

All three process the same stream (TOPICS=3, PER=24, WIN=4, TOPK=6, seed 11, bursts
of 6, uniqueEvery 7) and grow the same graph (1,000,000 tokens → ~142,930 nodes).

### What each regime actually does

**graph-stream only** — pure topology bookkeeping. Per token: create the node if new;
`touch` the co-occurrence edges (reinforce + decay + prune to TOP-K=6); slide the
window; stamp plaquette gauge phase. The substrate field is **never touched** — no
physics is computed. This is the ceiling: "how fast if you maintained the graph but
computed nothing."

**realistic linear field** (`g=0`) — the graph bookkeeping **plus the actual wave
computation**, with no nonlinearity:
- **project**: gather the active node's 2-hop neighbourhood's stored field out of the
  per-node `unordered_map`s into a contiguous local vector;
- **edge-flow**: the unitary **linear** rotations on that local light cone — the
  propagation `e^{-iHt}` itself (symmetric split: forward bond pass + reverse bond
  pass per step, `STEPS=2`);
- **unproject**: scatter the evolved local vector back into the per-node `unordered_map`s.

**nonlinear Kerr** (`g=7`) — identical physics to the linear field path, plus a Kerr
self-focusing phase `e^{-i g|ψ|² dt}` applied between the half-steps. Current
production form keeps the active node memories in packet fields and prepares the
Cayley bond carrier directly in the light cone, so the wave carrier is not repacked
through a hash-map/SparseBond detour. This changes the transport form, not the
phase/Kerr physics; A/B tests preserved the same horizons, bridges, recognition and
compression.

### Where the time goes (the key fact)

The graph update is **cheap**. The dominant cost is the **field round trip**:
`project → edge-flow → unproject` — building a local vector from hash maps, the 2-hop
gather, the unitary rotations, and writing the result back. Stripping the field
evolution alone jumps throughput from ~180k to ~1.2–1.4M tok/s on the reference host
(~7–8×). The Kerr term adds a further cost on top of the linear field (reference host
~2× on the older reference path; this host now measures the optimized packet/prepared
path at 10M tokens. The exact Kerr/linear ratio is build/container dependent.

### Current nonlinear production anchor

`research/probe_streaming_compression.cpp` with default packet memory and prepared
Cayley flow, CMake Release build, `probe_streaming_compression.exe 10000000 7`:

| quantity | value |
|---|---|
| stream | 10,000,000 tokens |
| graph grown | 1,428,644 nodes |
| horizons | 174,303 stable, 0 novel |
| compression | 3.09x (`PR linear=6.61`, `PR nonlinear=2.14`) |
| recognition | REAL 100.0%, RANDOM 27.8% |
| bridges | 36 true, 0 false |
| throughput | 71,452 tokens/s |
| peak RAM | 1,227 MB |

Regression gate:

```bat
ctest --test-dir build -C Release -L nonlinear --output-on-failure
```

Current nonlinear label contains five gates: horizon densification, nonlinear compute,
pure-physics chain, streaming densification, and the streaming compression smoke.

So the practical question "why didn't I see ~1.2M tok/s?" has a precise answer:
**~1.2M tok/s is the graph-stream-only path, with the field computation removed.**
The real engine computes the field, so the realistic figure is ~150–185k tok/s
(reference host). Anything claiming ~1M+ tok/s *with* a full per-token field is no
longer the `unordered_map` architecture here — it would be a fixed-array / top-k /
cache-local backend, and should be named accordingly (e.g. `linear_stream_array_backend`),
not `linear_stream`.

---

## 3. Quality is preserved across the field regimes

Recognition does **not** require the nonlinearity — the field accumulation already
separates topics:

| regime | recognition (REAL / RANDOM) | compression |
|---|---|---|
| realistic linear field (`g=0`) | **100% / ~31%** | none (linear disperses) |
| nonlinear Kerr (`g=7`) | **100% / 27.8%** at 10M | **3.09x** (energy concentrates) |

So the linear field is the honest **baseline**: same recognition, no compression. The
Kerr layer buys ~3× energy compression for ~2× more time (reference host).

---

## 4. How to run each (Windows / MSVC, from the repo root)

```bat
:: node-scaling engine (NODES) — sweeps N to 1,000,000:
cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp        && .\probe_sparse_scale.exe

:: graph-stream only (TOKENS, no field) — the throughput ceiling:
cl /O2 /EHsc /std:c++20 research\probe_graph_stream_only.cpp             && .\probe_graph_stream_only.exe 1000000

:: linear field streaming baseline (TOKENS, g=0) — 1,000,000 tokens:
cl /O2 /EHsc /std:c++20 /I tools research\probe_linear_stream.cpp        && .\probe_linear_stream.exe 1000000

:: nonlinear Kerr streaming (TOKENS, g=7) — 10,000,000-token production anchor:
cl /O2 /EHsc /std:c++20 /I tools research\probe_streaming_compression.cpp && .\probe_streaming_compression.exe 10000000 7
```

Each prints its own metrics and exits 0 on its contract. `probe_linear_stream` also
reports `local_nodes`, `bonds`, `bond_visits` (the per-token light-cone sizes), so the
cost is visible directly.

---

## 5. The never-conflate checklist

- `1,000,000 nodes` (node-scaling engine) ≠ `1,000,000 tokens` (streaming engine).
- `nodes/s` (≈1–2M) ≠ `tokens/s` (≈71k–1.4M depending on regime).
- `~1.2–1.4M tok/s` = graph-stream **without** the field; it is **not** the engine's
  real recognition throughput.
- `~150–185k tok/s` = realistic linear field (the honest streaming baseline).
- `~71.5k tok/s at 10M` = current nonlinear Kerr streaming production anchor on this
  host after packet memory + prepared Cayley flow.
- When a number is quoted, name the **engine**, the **unit**, and the **regime**.

For the 100M-scale projection (RAM/time for both engines) and the hidden-qubit
interpretation (capacity `log2 N`, linear non-entangling vs nonlinear entangling),
see [`SCALING_AND_QUBITS.md`](SCALING_AND_QUBITS.md).
