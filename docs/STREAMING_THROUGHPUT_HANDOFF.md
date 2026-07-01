# Streaming Throughput Handoff

Date: 2026-07-01

This document exists to prevent one recurring mistake: mixing the streaming
throughput ceiling with the linear or nonlinear field engines. GNNv2 has several
token paths that all accept the same stream, but they do very different work per
token.

## The Three Token Paths

| path | field work | what it measures | last known throughput |
|---|---|---|---|
| `probe_graph_stream_only` | none | token ingestion + plastic graph bookkeeping only | 388k tokens/s latest on this host (~0.5M prior), ~1.2-1.4M tokens/s on the faster reference host |
| `probe_linear_stream` | linear local field, `g=0` | recognition baseline with wave physics but no Kerr pressure | 103,552 tokens/s at 1M on this host, current packet/prepared path |
| `probe_streaming_compression` | nonlinear local field, `g=7` | production Kerr compression + recognition | 71,452 tokens/s at 10M on this host |

The 1.2-1.4M tokens/s number is **not** the linear model and **not** this host's
measurement. It is the faster reference-host graph-only ceiling: the stream updates
the plastic graph, but the substrate field is not evolved. On this host, the same
graph-only class is ~0.5M tokens/s.

The 71.5k tokens/s number is the current production nonlinear path:
packet memory, prepared Cayley flow carrier, local Kerr pressure, horizon/sense
readout, and bridge checks.

## Which Number To Quote

Quote the number that matches both the machine and the physics path:

| where | graph only, no field | linear field, `g=0` | nonlinear Kerr, `g=7` |
|---|---:|---:|---:|
| this host | 388k latest (~0.5M prior) | 103,552 tokens/s at 1M, current packet/prepared path | 71,452 tokens/s at 10M, current packet/prepared path |
| faster reference host | ~1.2-1.4M tokens/s | ~150-185k tokens/s, older map-backed path; current packet/prepared path not remeasured | not remeasured after packet/prepared |

Interpretation:

- If the quote is **~1.2-1.4M tokens/s**, it means reference-host graph bookkeeping
  only. It does not include field evolution, recognition physics, Kerr pressure, or
  compression.
- If the quote is **388k to ~0.5M tokens/s**, it means this-host graph bookkeeping only.
- If the quote is **103,552 tokens/s**, it means this-host production linear
  recognition at `g=0` with packet memory and prepared Cayley flow.
- If the quote is **71.5k tokens/s**, it means this-host production nonlinear
  recognition + Kerr compression at 10M tokens.

## Why Throughput Drops When Physics Is Enabled

Graph-only does this per token:

```text
node/window update -> touch/decay/prune -> plaquette phase bookkeeping
```

Linear field adds:

```text
2-hop light cone -> packet project field -> prepared Cayley edge-flow -> packet unproject field
```

Nonlinear Kerr adds:

```text
same local flow -> Kerr phase pressure -> sense/horizon/bridge readout
```

So the drop is not a regression. It is the cost of computing the field. A full
field path cannot honestly be compared to graph-only throughput.

## Current Production Linear Anchor

Command, from a CMake Release build:

```bat
build\Release\probe_linear_stream.exe 1000000 7
```

Observed result:

```text
updates=1000000 nodes=142930 edges=205 stable_events=60237
PR avg=3.830 min=1.000 max=15.915 value_LINEAR=100.0%
local_nodes avg=14.01 max=29   bonds avg=25.17 max=63
bond_visits total=100699520 avg=100.70 max=252 max_phase_speed=0.000
train_sec=9.657 tokens_per_sec=103552 peak_rss_mb=113
RESULT : 1 / 1  (PASS)
```

Interpretation:

- The linear field now uses the same packet/prepared Cayley carrier family as the
  nonlinear engine.
- There is no Kerr phase, no horizon detector and no bridge materialization.
- Recognition stayed 100%.
- Throughput is now faster than the nonlinear Kerr path, as expected.

## Current Linear Regression Gate

Run:

```bat
ctest --test-dir build -C Release -L stream_linear --output-on-failure
```

Last result:

```text
2 / 2 passed
```

The label contains:

```text
graph_wave_linear_cayley_flow_contract_test
probe_linear_stream_smoke
```

## Current Production Nonlinear Anchor

Command, from a CMake Release build:

```bat
build\Release\probe_streaming_compression.exe 10000000 7
```

Observed result:

```text
stream=10000000 uniqueEvery=7 field_updates=10000000 nodes=1428644
horizons=174303 novel_horizons=0
horizon PR linear=6.61 nonlinear=2.14 compression=3.09x
bridges total=36 true=36 false=0
value REAL=100.0% RANDOM=27.8%
max_bond_visits=536 max_phase_speed=0.000
train_sec=139.95 tokens_per_sec=71452 peak_ram_mb=1227
```

Interpretation:

- The model accepted 10M tokens and grew a 1.43M-node graph.
- Energy compression remained stable at 3.09x.
- Recognition stayed 100% on real stream structure.
- False bridges stayed at zero.
- RAM stayed far below available host memory.
- Throughput stayed near 71.5k tokens/s at production scale.

## Current Nonlinear Regression Gate

Run:

```bat
ctest --test-dir build -C Release -L nonlinear --output-on-failure
```

Last result:

```text
5 / 5 passed
```

The label contains:

```text
graph_wave_horizon_densification_contract_test
graph_wave_nonlinear_compute_contract_test
graph_wave_pure_physics_chain_contract_test
graph_wave_streaming_densification_contract_test
probe_streaming_compression_smoke
```

This gate is intentionally mixed:

- substrate-level physics contract;
- densification contract;
- nonlinear compute contract;
- streaming densification contract;
- operational streaming smoke.

## Accepted Carrier Optimizations

These are production defaults:

```cpp
GW_STREAM_PACKET_MEM=1
GW_STREAM_PREPARED_CAYLEY=1
```

`GW_STREAM_PACKET_MEM=1`:

- stores tiny local `lin`, `ker`, and `sense` fields as packet arrays;
- replaces hash-map memory shape on the hot path;
- does not change phase, support, Kerr pressure, or overlap physics.

`GW_STREAM_PREPARED_CAYLEY=1`:

- builds the local Cayley carrier directly while gathering the light cone;
- removes repeated `SparseBond -> LocalCayleyFlowCarrier` repacking;
- preserves bond order, phase carrier, normalization, Kerr pressure and trajectory.

Rejected optimization:

- replacing `Graph::adj` with a small edge carrier changed the trajectory
  (horizons/bridges shifted). It was removed. Do not reintroduce it without a new
  physical reason and a strict A/B gate.

## A/B Acceptance Rules

A carrier/backend optimization may be kept only if these remain stable:

```text
horizon count
novel_horizons = 0
compression
REAL recognition
RANDOM control
bridge total
false bridges = 0
max_bond_visits bound
RAM trend
exit code 0
```

If a change improves speed but shifts horizons/bridges without a physical reason,
reject it. That happened with the small adjacency carrier.

## Calibration Boundary

`TOPK`, decay, `WMIN`, `SMIN`, and bridge coherence thresholds are operational
stabilizers. They prevent finite-precision noise, long-stream history and graph
growth from being mistaken for signal. They are not the active nonlinear physics.

The active physics is:

```text
phase -> superposition -> Cayley transport -> local Kerr pressure -> phase overlap
```

Do not tune stabilizers to hide a broken result. If a stabilizer changes, rerun the
nonlinear CTest gate and at least a 1M streaming run. For production closure,
rerun the 10M anchor.

## Next Safe Work

The next performance wall is not "more nonlinear physics". The safe work is carrier
work around:

- light-cone/bond gathering;
- `sense` and `unproject`;
- cache-local storage that preserves trajectory.

Do not chase graph-only throughput with a full field engine. The honest production
target is higher nonlinear `tokens/s` while preserving the 10M anchor metrics above.
