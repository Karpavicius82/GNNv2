# Results

All numbers were produced by running the code; nothing is fitted or estimated.
Substrate results are exact finite-N identities at machine precision.

## Substrate physics — machine precision

| quantity | value |
|---|---|
| norm drift over 1000 steps | 1.68e-13 |
| operator unitarity `‖U†U − I‖` | 1.2e-15 |
| destructive interference (flux = π) | 5.6e-31 (constructive/destructive ratio 3.8e11×) |
| Wilson-loop gauge invariance | 4.4e-16 |
| Landau-gauge plaquette flux error | 0 |
| interferometer routing | flux = 0 → out+ 0.99999, flux = π → out− 0.99999 |

## Linear scaling engine — sparse unitary propagation (unit: NODES)

This is the graph-propagation engine: sparse Chebyshev evolution of `e^{-iHt}` over
GRAPH NODES, `O(E)` per term. Input and throughput are in **nodes** (`nodes/s`).

| quantity | value |
|---|---|
| scale | **N = 1,000,000 nodes**, ~1 s (~1.1M nodes/s), **linear time** (Chebyshev, 23 terms) |
| unitarity at scale (norm drift) | **2.22e-16** |
| propagator exact at any t (vs exact eigendecomposition) | 1.13e-12 @t1, 1.26e-12 @t5, 1.8e-12 @t20 |
| gauge invariance of the engine | \|ψ\|² unchanged under local gauge = 6.9e-17 |
| exact flux = π decoupling through the engine | 4.95e-16 |

Source: `research/probe_sparse_scale.cpp`, `research/probe_physics.cpp`,
`research/probe_crosscheck.cpp`.

## Nonlinear streaming / compression engine — Kerr (unit: TOKENS)

A **separate** engine, not the one above. It streams a sequence of **tokens**; each
token event grows a plastic graph and evolves a local 2-hop Kerr field
`iψ̇ = −Hψ − g|ψ|²ψ`. Its input and throughput are in **tokens** (`tokens/s`); the
~143k-node graph it builds is a by-product of the stream, not its input — `nodes/s`
does not apply here.

| quantity | value |
|---|---|
| scale | **10,000,000 tokens**, 139.95 s (**71,452 tokens/s**) |
| graph grown (by-product of the stream) | 1,428,644 nodes |
| compression | **3.09x** (horizon PR linear 6.61 / nonlinear 2.14) |
| recognition | REAL **100%** vs RANDOM 27.8% |
| bridges | 36 true / 0 false |
| peak RAM | 1,227 MB |
| implementation | packet memory + prepared Cayley flow carrier (same phase/Kerr physics; lower repacking cost) |

Source: `research/probe_streaming_compression.cpp`,
`tools/graph_wave_substrate.hpp`, `tools/graph_wave_nonlinear_engine.hpp`,
`research/probe_nonlinear_engine.cpp`. See `docs/NONLINEAR_ENGINE.md`.

## The GNN

| task | result |
|---|---|
| node classification (`gnn_task`) | **100%**; message passing necessary (own-feature 69% ≈ chance) |
| weights-free learning (`learning`) | **99.5%** on unseen features; shuffled-label control 52% (genuinely learned) |
| depth, 2-hop (`gnn_task`) | 1-round 57% → 2-round 93% |
| deep attention (`deep_attention`) | 2 layers route (overlap 1.000); 1 layer / linear cannot (0.048) |

## Decorrelation glue

| quantity | value |
|---|---|
| whitened 2-hop routing, correlated keys (`probe_whiten`) | **1.000** at correlation 0.0–0.98 |
| same task, naive readout (Born) | 0.48–0.65 (collapses) |
| native conjugate gradient ≡ exact G⁻¹ (`probe_lateral`) | match to 1e-16…1e-10 |

## Memory / cleanup / decision

| component | result |
|---|---|
| holographic unbind (`holographic_memory`) | sim 1.000000000000 |
| flat-band memory (`flatband_memory`) | recall 100%, fidelity 1.0 to t = 30 |
| resonator cleanup (`resonator_cleanup`) | corrupted → 1.000000, recall 24/24 |
| amplitude amplification (`amplitude_amplification`) | 7.56× over uniform, winner = target |

## Real data — Cora citation graph (weights-free)

2708 nodes, 5429 edges, 7 classes, 20 labels/class, test = 2568 nodes.

| method | naive | + decorrelation |
|---|---|---|
| own features (no message passing) | 58.3% | 58.6% |
| FLOW 1-hop | 74.6% | 74.8% |
| FLOW 2-hop | **77.4%** | 77.9% |

No weights, no backprop (prototypes = class means, `G⁻¹` = the prototype Gram, FLOW
= graph aggregation). Own-features 58.3% matches the known raw-feature reference
(~59%); message passing lifts it to **77.4%**, beating label-propagation (~68%) and
approaching the **trained** GCN (~81.5%). Decorrelation is marginal on Cora because
its classes are only mildly correlated; it is decisive in the binding /
high-correlation regime (`probe_whiten`, 1.000 vs 0.48). Source: `research/probe_cora.cpp`.

## Summary

- 61 GNNv2 contract gates (substrate / GNN / decorrelation / memory / nonlinear) — all green; the substrate identities hold at machine precision. The GNNv3 RC1 contract is held separate.
- Linear engine (NODES) scales to 10⁶ nodes, exact propagator, gauge-invariant, exact interference.
- Nonlinear streaming engine (TOKENS) streams 10⁷ tokens at ~71.5k tokens/s, 3.09x Kerr compression, 100% recognition (vs 27.8% random), 36 true / 0 false bridges — a separate engine; nodes/s ≠ tokens/s.
- GNN: 100% classification, 99.5% weights-free learning, 77.4% on real Cora.
- Decorrelation glue closes the correlated-content gap (1.000 where naive gives 0.48).
