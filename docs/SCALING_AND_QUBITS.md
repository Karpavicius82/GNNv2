# GNNv2 — scaling and the hidden-qubit view

Consolidated comparison of both engines: measured anchors, the 100M projection, and
the hidden-qubit interpretation. Read with [`PERFORMANCE.md`](PERFORMANCE.md).

**Ground rules (so nothing is conflated):**
- There are **two engines with different units**. The **node-scaling engine** is in
  **NODES**; the **streaming engine** is in **TOKENS**. They are **different
  operations and are never compared head-to-head** — each is scaled within its own
  unit. `nodes/s ≠ tokens/s`.
- **Measured** rows are real runs; **100M** rows are linear **extrapolations** (not
  executed at 100M), and absolute time is **host-dependent** ("this host" ≈ 2× slower
  than a "reference host").
- **Hidden qubits = `log2(field dimension)`** — an information-capacity lens. The
  substrate is **classically simulable**: no quantum advantage; entanglement is
  bounded (≲ ln 2). "Linear vs nonlinear" changes **entanglement**, not qubit count.

---

## A. Node-scaling engine — unit NODES

One unitary propagation `e^{-iHt}` over a single field of `N` graph nodes
(`research/probe_sparse_scale.cpp`, sparse Chebyshev). The whole field is one register.

| N (nodes) | RAM | time (this host) | nodes/s | hidden qubits `log2 N` | entanglement |
|---|---|---|---|---|---|
| 1,000,000 | ~0.22 GB | ~0.85 s | ~1.1M (ref ~2M) | 20.0 | none (linear) |
| 10,000,000 *(measured)* | 2.17 GB | 9 s | ~1.1M | 23.3 | none |
| 20,000,000 *(measured)* | 4.33 GB | 21 s | ~0.95M | 24.3 | none |
| **100,000,000 *(projected)*** | **~22 GB** | **~100 s** | **~1M** | **26.6 (~27)** | **none** |

A linear unitary evolves the `log2 N` qubits **without building entanglement**
(parallel single-qubit-like rotations). A **Kerr (nonlinear)** propagation on the
**same** `N`-node field would keep the **same `log2 N` capacity** but **engage
entanglement** (≲ ln 2) — at higher time cost, because a nonlinear field needs explicit
small-`dt` split-step stepping instead of one Chebyshev jump. *(A global nonlinear
field at 100M was not run.)*

---

## B. Streaming engine — unit TOKENS (three regimes, apples-to-apples)

A stream of token events; each grows the plastic graph and (optionally) evolves a
**local 2-hop field**. The three regimes share the **same stream and graph** (so they
ARE comparable) and differ only by how much per-token field work runs. The graph
grows by `≈ tokens / 7` → ~14.3M nodes at 100M tokens.

| regime (file) | per-token work | RAM @1M *(meas.)* | time @1M *(meas., this host)* | RAM @100M *(proj.)* | time @100M *(proj.)* |
|---|---|---|---|---|---|
| **graph-stream only** (`probe_graph_stream_only`) | graph bookkeeping, **no field** | 76 MB | 1.9 s (~537k tok/s) | ~7.6 GB | ~3 min (ref ~1 min) |
| **linear field** `g=0` (`probe_linear_stream`) | + project → edge-flow → unproject | 380 MB | 18.7 s (~53k tok/s) | ~38 GB | ~31 min (ref ~9 min) |
| **nonlinear Kerr** `g=7` (`probe_streaming_compression`) | + Kerr self-focusing, packet memory, prepared Cayley flow | 1.23 GB @10M *(meas.)* | 139.95 s @10M (~71.5k tok/s) | ~12.3 GB | ~23 min |

Both RAM and time scale **linearly in tokens** (per-token work is bounded by the 2-hop
cone). RAM ordering is physical: no field < one field (linear) < two fields (Kerr keeps
the `g=0` control alongside).

### The apples-to-apples linear vs nonlinear (within TOKENS, `g=0` vs `g=7`)

| @ 100M tokens | linear field (`g=0`) | nonlinear Kerr (`g=7`) |
|---|---|---|
| RAM | ~38 GB | ~12.3 GB for current packet/prepared Kerr path (not apples-to-apples with the old linear map backend) |
| time (this host) | ~31 min old linear map backend | ~23 min current nonlinear packet/prepared backend |
| compression | none (linear disperses) | **~3×** (energy concentrates) |
| recognition | 100% (vs ~31% random, older linear baseline) | 100% (vs 27.8% random at 10M) |

The current nonlinear path is no longer a pure "same backend plus Kerr" comparison:
packet memory and prepared Cayley flow remove much of the previous carrier overhead.
The physics result is unchanged — **~3× compression** at **100% recognition** — but the
engineering projection is now materially lower RAM/time than the older map-backed
nonlinear row.

### Hidden qubits — streaming

| | value | note |
|---|---|---|
| memory capacity | `log2(14.3M) ≈ 23.8` (~24 qubits) | the whole sparse memory at 100M tokens |
| active per token | `log2(local cone ~16–30) ≈ 4–5` qubits | only the 2-hop cone is evolved per step |
| entanglement | `g=0`: **none** · `g=7`: **engaged**, ≲ ln 2 | Kerr is a state-adaptive (entangling) unitary |

---

## The one-line picture

- **Node engine (NODES):** one large `log2 N`-qubit register (≈27 qubits at 100M
  nodes), evolved **without entanglement**, very fast (~22 GB, ~100 s).
- **Streaming engine (TOKENS):** a **stream of ~4–5-qubit local computations** into a
  ~24-qubit-capacity sparse memory; with Kerr these local computations **entangle**
  (≲ ln 2) and compress ~3×.
- **Nonlinearity engages qubits (entanglement / compression); it does not add them.**
  Everything is classically simulable — this is a capacity/structure description, not
  a quantum-speedup claim.

*(100M figures are extrapolations from the 1M / 3M / 10M / 20M anchors; not run at 100M.
Time is host-dependent — see `PERFORMANCE.md`.)*
