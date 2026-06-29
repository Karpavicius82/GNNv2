# GNNv2 — scaling to 100M and the hidden-qubit view

This projects the two engines to the 100M scale and gives the "hidden-qubit"
interpretation of what is being computed. Two honesty notes up front:

- The **measured anchors** below are real runs. The **100M figures are theoretical
  extrapolations** (linear in the measured cost), not executed at 100M — they are
  labelled as such.
- "Hidden qubits" = `log2(field dimension)`. It is a **capacity / structure lens**,
  not a claim of quantum advantage: the substrate is **classically simulable**, so
  there is no exponential speedup — the qubit count is `log2 N`, and entanglement is
  bounded (≲ ln 2). See [`docs/PERFORMANCE.md`](PERFORMANCE.md) for the two engines and
  their units (NODES vs TOKENS — never conflate).

## Measured anchors

| engine | run | RAM | time | nodes |
|---|---|---|---|---|
| linear flow (node-scale, `g=0`) | 10,000,000 nodes | 2.17 GB | 9 s | 10M |
| linear flow (node-scale, `g=0`) | 20,000,000 nodes | 4.33 GB | 21 s | 20M |
| nonlinear streaming (`g=7`) | 1,000,000 tokens | 793 MB | ~24 s | 142,930 |

Linear flow scales ~`0.217 GB` and ~`0.9–1.0 s` per million **nodes**. Nonlinear
streaming grows the graph by `≈ tokens / 7` (uniqueEvery=7) → ~`0.143` **nodes per
token**, and its per-token work is bounded (a 2-hop light cone), so both engines are
**linear in their own unit**.

## 100M projection (theoretical)

These are **two different operations** at "100M", hence very different costs:

| | **linear — 100M nodes** | **nonlinear streaming — 100M tokens** |
|---|---|---|
| what it is | **one** propagation over a 100M-element field | **100M sequential** token events |
| field built | the 100M-node field itself | a graph grown to **~14.3M nodes** (100M / 7) |
| **RAM** | **~22 GB** (0.217 GB/M × 100M) | **~75–80 GB** (793 MB / 143k ≈ 5.5 KB/node × 14.3M) |
| **time** | **~100 s** | **~2400 s (~40 min)** this host @42k tok/s · **~1150 s (~19 min)** reference @~87k tok/s |
| **scaling** | linear in nodes | linear in tokens (bounded per-token work) |

(RAM for the streaming case is a linear-in-nodes extrapolation; it may come out lower
because most of the 14.3M nodes are short-lived "novel" tokens with tiny per-node
memory. The 100M-token run would need ~80 GB and tens of minutes, so it is estimated,
not executed.)

## The hidden-qubit view

Field over `N` elements ⇒ `n = log2 N` hidden qubits. Three independent axes (not one
number): **capacity** `log2 N` · **occupancy** `q(t)` · **entanglement** (≲ ln 2).

| | **linear — 100M nodes** | **nonlinear streaming — 100M tokens** |
|---|---|---|
| total capacity | `log2(10^8) ≈ 26.6` (**~27 qubits**), one register | `log2(14.3M) ≈ 23.8` (**~24 qubits**), the sparse memory |
| active per step | the whole ~27-qubit register | `log2(local cone ~16–30) ≈ 4–5` qubits per token |
| entanglement | **≈ none** — a linear unitary evolves the qubits like parallel single-qubit rotations (separable) | **engaged** — Kerr is a state-adaptive unitary that **entangles** the local cone (bounded ≲ ln 2) |

**The key contrast.** Nonlinearity does **not add qubits** — it **engages** them:

- **Linear 100M nodes** = one ~27-qubit register evolving **without entanglement**
  (parallel rotations); cheap (~22 GB, ~100 s) because it is one global linear jump
  (e.g. a single Chebyshev expansion).
- **Nonlinear 100M tokens** = **100M sequential ~4–5-qubit *entangling* local
  computations**, accumulated into a ~24-qubit-capacity sparse memory; heavier
  (~75–80 GB, ~40 min) because each token is a full local Kerr evolution plus the
  `project → edge-flow → unproject` round trip, not a single global jump.

So the nonlinear engine, at "100M", is **a stream of small entangled interactions**,
whereas the linear engine is **one large non-interacting register**. The nonlinearity
buys interaction (entanglement, energy compression), not more qubits — and it stays
classically simulable, so this is a capacity/structure description, not a quantum
speedup claim.

## Caveats

- 100M numbers are **extrapolations** from the measured 1M / 10M / 20M anchors, not
  runs at 100M. Absolute time is **host-dependent** (this host ≈ 2× slower than the
  reference; see `PERFORMANCE.md`).
- Qubit counts are `log2(field dimension)` — an information-capacity lens. The
  substrate is classically simulable; entanglement is bounded (≲ ln 2). No quantum
  advantage is claimed.
