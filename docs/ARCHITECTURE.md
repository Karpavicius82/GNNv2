# Architecture

GNNv2 is a graph neural network with **no weights and no trainer**. Computation is
carried by **phase, interference and superposition** on a unitary graph-wave
substrate. The only structure is the graph topology and the **gauge flux** on its
edges; there are no stored, tuned numbers.

```
complex state z  →  Hermitian graph Hamiltonian H (topology + flux)  →  unitary evolution
```

## 1. The substrate

State `z ∈ ℂ^N`, one complex amplitude per node. The graph is a Hermitian operator

```
H_ij = A_ij · e^{i·δ_ij},   H_ji = conj(H_ij)
```

where `A_ij` is the (real) coupling and `δ_ij` is the edge phase. Evolution is the
Cayley / Crank–Nicolson step, which is **exactly unitary**:

```
U = (I + iK)^{-1} (I − iK),   K = H·dt/2
```

Meaning lives in the **gauge-invariant flux** (the Wilson loop `arg ∏ H_ij/|H_ij|`),
never in absolute phase. A local gauge change leaves every observable unchanged.

## 2. The irreducible operation

Every layer reduces to one **phase-weighted multiply-accumulate over edges**:

```
z_i  ←  Σ_j  e^{i·δ_ij} · z_j
```

This is message passing: a node aggregates its neighbours' amplitudes, each rotated
by the edge phase, and they interfere. There are no weights — `δ_ij` (the flux) is
the structure. Depth is `U^k`; the receptive field grows one hop per layer.

## 3. The three substrates (the closed system)

A unitary substrate alone cannot make sharp decisions (linear composition collapses
to a single linear operator). Real computation needs three layers, composed as a
stream:

```
   FLOW                     DECORRELATION                 SETTLING / MEMORY
   unitary propagation  →   c = G^-1 ⟨k,q⟩           →    dissipative relaxation
   (message passing,        (separate correlated          onto attractors,
    flux is the signal)      patterns; the glue)          holographic recall,
                                                          amplitude amplification
   mix / spread             make content separable        sharpen / decide
```

- **FLOW** spreads and superposes information across the graph. It is exact and
  conserving (unitary at any depth), but linear: stacking layers does not by itself
  make a decision.

- **DECORRELATION** is the missing glue. Raw overlaps `⟨k_i, q⟩` are contaminated
  when patterns are correlated (off-diagonal terms of the Gram matrix `G`). The dual
  coordinates `c = G⁻¹⟨k,q⟩` remove the contamination exactly. Its native form is a
  recurrent competitive dynamics — lateral inhibition / conjugate gradient on `G`
  (positive-definite, so it converges in ≤K steps). Weights-free: `G` is the
  patterns' own overlap matrix, not a tuned parameter.

- **SETTLING / MEMORY** sharpens. Dissipative relaxation drops a mixed state onto the
  nearest attractor; holographic binding/unbinding stores and recalls structured
  data; a degenerate (flat) band keeps memory alive over time; amplitude
  amplification turns a soft signal into a decision.

The flux gates (`flux = 0` transmits, `flux = π` blocks exactly) are not a separate
mechanism — they are the exact dark fringes of the same interference field.

## 4. Two engines (never conflate)

GNNv2 ships **two distinct production engines**. They operate on different objects and
are measured in different units — keep them separate.

**Linear scaling engine — graph propagation. Unit: NODES.**
The contracts run on small dense systems for exactness. At scale the dense Cayley step
(`O(N³)`) is replaced by a **sparse Chebyshev expansion of `e^{-iHt}`** — only sparse
matrix-vector products, `O(E)` per term. Same physics, verified against exact
eigendecomposition; machine-precision unitarity (norm drift `2.2e-16`) at
**N = 1,000,000 graph nodes in ~0.9 s (≈1.1M nodes/s), linear in N**. This is the path
from the lab contracts to a global GNN. Source: `research/probe_sparse_scale.cpp`.

**Nonlinear streaming engine — Kerr compression. Unit: TOKENS.**
A separate engine streams a sequence of **tokens**; each token event grows a plastic
graph and evolves a local 2-hop field with Kerr nonlinearity
`i ψ̇ = −H ψ − g|ψ|²ψ`. It concentrates energy (**≈3× compression** vs the linear
`g = 0` baseline) while preserving recognition (REAL 100% vs RANDOM ≈31%). Throughput
is **≈42,000 tokens/s** (1,000,000 tokens in ~24 s, the graph growing to ~143,000
nodes, ~0.8 GB). Source: `research/probe_streaming_compression.cpp`,
`tools/graph_wave_nonlinear_engine.hpp`. See `docs/NONLINEAR_ENGINE.md`.

The two never mix: 1,000,000 **nodes** = the linear engine; 1,000,000 **tokens** = the
nonlinear engine; **nodes/s ≠ tokens/s**. When any doc says "the engine", it must say
WHICH. Numbers in `docs/RESULTS.md` and `docs/NONLINEAR_ENGINE.md`.

## 5. Discipline

Everything is **weights-free** and verified by **exact finite-N identities**
(unitarity, gauge invariance, exact interference) at machine precision — not by
fitted exponents or asymptotic claims. Where a result is uncertain or a probe
failed, that is recorded honestly (see `research/README.md`) rather than hidden.
