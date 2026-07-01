# research/ — production studies and exploratory probes

**43 standalone C++20 probes** from the build-out of the system (plus the shared
header `semantic_sparse_text8.hpp`). Some include the substrate core via
`../tools/graph_wave_substrate.hpp`; build from a checkout that keeps `tools/` and
`research/` siblings. The tables below are a representative map of the directory, not
an exhaustive listing.

The first four groups are the **production-verified** results that back the core
engines (linear node scaling + linear/nonlinear token streaming), the glue, and the real-data
benchmark. The last group is the **honest exploratory record** — including probes
that failed or were superseded. Negative results are kept on purpose; they mark the
dead-ends so they are not re-walked.

GNNv2 ships **two distinct engines measured in different units — never conflate them**
(canonical: `docs/ARCHITECTURE.md` §4). The **linear scaling engine** propagates over
graph **NODES** (throughput in nodes/s); the streaming engines consume **TOKENS**
(throughput in tokens/s): `probe_linear_stream` is `g=0`, and
`probe_streaming_compression` is `g=7`. `1,000,000 nodes` is not `1,000,000 tokens`;
**nodes/s ≠ tokens/s**.

## Linear scaling engine — graph propagation, unit: NODES (verified)
| file | result |
|---|---|
| `probe_sparse_scale` | sparse unitary propagation, **1,000,000 graph nodes**, norm drift 2.22e-16, linear time (linear engine; throughput in nodes/s) |
| `probe_physics` | propagator exact at any t (1e-12 vs exact eig), gauge-invariant 6.9e-17, flux=π 4.95e-16 |
| `probe_crosscheck` | sparse Chebyshev engine vs exact eigendecomposition: 1.13e-12 |

## Nonlinear streaming engine — Kerr compression, unit: TOKENS (verified)
| file | result |
|---|---|
| `probe_streaming_compression` | streams **TOKENS** into a plastic graph + local 2-hop Kerr field; current packet/prepared production path: 10M tokens, 3.09x compression, REAL 100% / RANDOM 27.8%, 36 true / 0 false bridges, ~71.5k tokens/s, peak RAM ~1.23 GB. See `docs/ARCHITECTURE.md` §4, `docs/NONLINEAR_ENGINE.md` |
| `probe_nonlinear_engine` | driver for the closed Kerr engine (`tools/graph_wave_nonlinear_engine.hpp`): psi/chi densification + tau structure-sensing over the same **TOKEN** stream |
| `probe_linear_stream` | LINEAR token-stream control (`g = 0`, no Kerr): same stream generator and plastic-graph params, packet memory + prepared Cayley flow, **103,552 tok/s at 1M** with value_LINEAR 100%. This is the streaming engine with the nonlinearity off — not the node-scaling engine |
| `probe_graph_stream_only` | GRAPH-STREAM-ONLY ceiling (no field at all): same stream + plastic-graph bookkeeping, but no `project`/edge-flow/`unproject`. The top of the token speed ladder (~1.2–1.4M tok/s reference). The gap to `probe_linear_stream` IS the per-token field cost. See `docs/PERFORMANCE.md` |
| `probe_flow_microbench` | isolates the local nonlinear flow operator from graph/memory/projection costs; use it to compare rational/no-stats, rational/stats, exp/no-stats and exp/stats carrier paths without changing streaming physics |

## Decorrelation glue (verified)
| file | result |
|---|---|
| `probe_whiten` | whitened routing 1.000 at correlation 0.0–0.98 where naive collapses to 0.48 |
| `probe_lateral` | native conjugate gradient ≡ exact G⁻¹ (1e-16…1e-10); naive Jacobi only holds to ~0.8 |

## Real-data benchmark
| file | result |
|---|---|
| `probe_cora` | weights-free GNN on the Cora citation graph: **77.4%** (see `CORA_DATA.md` to fetch data) |

## Exploratory record (honest — includes negatives)
| file | what it showed |
|---|---|
| `probe_a4_abcage` | Aharonov–Bohm cage: exact flux-gated confinement, length-invariant |
| `probe_a5_compose` | composed flux gates: exact, independent, conserving |
| `probe_steer` | phase-ramp beamforming: phase alone steers the field, linearly |
| `probe_diffract` | the field as a diffraction map; holographic refocus 7.3× (phase carries it) |
| `probe_gnn1` | weights-free GNN exceeds the 1-WL ceiling (distinguishes C6 vs 2·C3) |
| `probe_a1_levelspacing` | **failed** — level-spacing collapses to 0 on degenerate lattice spectra |
| `probe_a3_diffusion`, `probe_a3b_robust` | diffusion exponent **drifts** with box size — not a converged result |
| `probe_a2_phasedepth` | phase survives depth (chirality), with a snapshot caveat |
| `probe_holoclf` | diffractive matched-filter classifier — **focus-limited**, principle exact / realization weak |
| `probe_depth_close`, `probe_settle_route` | single Born / settling **do not** close the correlated-content gap |
| `probe_gnn_prod`, `probe_pipeline`, `probe_pipeline2` | scaled synthetic pipelines — message passing helps (13%→100%) but the synthetic tasks were either trivial or regime-mismatched; superseded by `probe_cora` on real data |

## Wave-physics audit
| file | result |
|---|---|
| `probe_wave_fidelity` | faithful wave: norm/energy/time-reversal/gauge/continuity ~1e-15; light cone respected by Chebyshev, broken by large-step Cayley |
| `probe_wave_complete` | superposition/homogeneity exact; Parseval complete (reconstruction 4e-13); energy by top-K modes 45/71/92/100% -> `<100%` results were partial energy collection, not lost info |

See `docs/WAVE_FIDELITY.md`. This supersedes the earlier "phase channel is weak"
note (commit 4534038): the phase carries 100% of the information; weak readings were
partial/real-projected collection.
