# GNNv2 Handoff

Date: 2026-07-01

Read this before touching GNNv2. It exists so the next session does not rediscover
the same path or re-run settled negatives.

## What GNNv2 Is (and Is Not)

GNNv2 is a **weights-free graph-wave substrate**: computation is carried by phase,
interference and superposition on a unitary graph; meaning lives in the
gauge-invariant flux, not in stored numbers. On top of that substrate, GNNv2 adds a
**limited bridge-integration mechanism for solving nonlinear problems efficiently** —
the principle is "do not compute everywhere, compute only WHERE it matters and WHEN
it matters."

GNNv2 is **not** GNNv3 RC1. GNNv3 RC1 is a separate system (a self-sensing
two-component **carrier-field** substrate with a feeling-gate aperture). Do not mix
the two. GNNv3 lives only in `tools/graph_wave_v3_feeling_gate_contract_test.cpp` +
`docs/GNNv3_RC1_*.md`; everything else in this repo is GNNv2.

## Non-Negotiable Discipline

- C/C++ only for executable project logic and tests. No Python/JS/PowerShell/notebook
  side systems. (The old `scripts/*.ps1` runners were removed; use CMake + ctest.)
- Physics with phase only. No fitted weights, no trainer, no SQL/database framing.
- Do not strip phase to magnitude, and never half-wave-clip phase (`max(0, cos·)`) —
  it hides destructive interference and has produced false positives. Preserve the
  full complex/phase carrier end to end (see `PHYSICS_ONLY_DISCIPLINE.md`,
  `WAVE_FIDELITY.md`).
- Labels/oracles may only CHECK a result after the physics has spoken — never used to
  CREATE physics. Do not benchmark this substrate like an ML classifier (no
  label-noise robustness sweeps, no k-fold): there is no external ground truth, the
  emergent field structure IS the result.
- Keep negative results in the docs, not hidden.

## Substrate Roles

These roles are substrate primitives. In production they are packaged into **two
distinct engines that must never be conflated** (canonical: `ARCHITECTURE.md` §4):

- **Linear scaling engine** — graph propagation, **unit = NODES**. Sparse unitary
  Chebyshev `e^{-iHt}` over graph nodes: ~1,000,000 **nodes** in ~0.9 s (≈1.1M
  **nodes/s**), linear in N, norm drift ~2e-16. Source `research/probe_sparse_scale.cpp`.
- **Nonlinear streaming engine** — Kerr compression, **unit = TOKENS**. Streams a
  sequence of **tokens**; each token grows a plastic graph and evolves a local 2-hop
  Kerr field. Current production path uses packet memory plus prepared Cayley flow:
  10,000,000 **tokens** in 139.95 s (**71,452 tokens/s**), graph growing to 1,428,644
  nodes, 1.23 GB peak RAM, **3.09x compression** (REAL 100% vs RANDOM 27.8%),
  36 true / 0 false bridges. Source `research/probe_streaming_compression.cpp`,
  `tools/graph_wave_substrate.hpp`, `tools/graph_wave_nonlinear_engine.hpp`.

`nodes/s ≠ tokens/s`: 1,000,000 **nodes** = the linear engine; 1,000,000 **tokens** =
the nonlinear engine. The ≈3× compression belongs to the nonlinear **token** engine.
Do not mix the throughput hosts: on this host, token throughput is ~0.5M tok/s for
graph-only, ~50–57k tok/s for the older linear-field path, and 71,452 tok/s for the
current 10M nonlinear packet/prepared path. The ~1.2–1.4M tok/s number is only the
faster reference-host graph-only ceiling, not a linear/nonlinear field number. Full
speed ladder (node engine + the three token regimes, with where the cost goes):
`docs/PERFORMANCE.md`. For the specific graph-only vs linear-field vs nonlinear-Kerr
throughput boundary, read `docs/STREAMING_THROUGHPUT_HANDOFF.md`. The g=0 streaming
baseline is `research/probe_linear_stream.cpp`.

Run them (Windows / MSVC, from the repo root):

```
:: LINEAR engine (nodes) -- no args, sweeps N to 1,000,000 nodes:
cl /O2 /EHsc /std:c++20 /I tools research\probe_sparse_scale.cpp && .\probe_sparse_scale.exe

:: NONLINEAR engine (tokens) -- args = stream length and unique cadence, here 10,000,000 tokens:
cl /O2 /EHsc /std:c++20 /I tools research\probe_streaming_compression.cpp && .\probe_streaming_compression.exe 10000000 7
```

- **Flow** — unitary propagation `z_i ← Σ_j e^{i·δ_ij} z_j`; the edge flux `δ_ij` is
  the structure (no weights). Depth is `U^k`. See `ARCHITECTURE.md`.
- **Decorrelation** — separates correlated patterns so content is addressable; this
  is phase-coherence physics, the glue, not a fitted layer.
- **Settling / memory** — dissipative relaxation, holographic recall, amplitude
  amplification.
- **Nonlinear (Kerr) pressure** — `i ψ̇ = -H ψ - g|ψ|²ψ`, split-step DNLS. Energy
  densifies (compression ≈ 3x on the **token** stream); a horizon is a detector over the already-evolved
  field, not a separate operation. Solitonic: stored energy stays localized under
  field noise (it does not disperse; it also does not reconstruct identity — measure
  by participation ratio, not template overlap). See `NONLINEAR_ENGINE.md`.

## Current Nonlinear Production Closure (2026-07-01)

Accepted low-level optimizations:

- `GW_STREAM_PACKET_MEM=1`: tiny local `lin` / `ker` / `sense` fields are carried as
  packet arrays instead of hash maps. This changes memory shape, not phase physics.
- `GW_STREAM_PREPARED_CAYLEY=1`: the local light cone prepares the Cayley flow carrier
  directly. This removes a `SparseBond -> LocalCayleyFlowCarrier` repack in the hot
  loop while preserving bond order, phase, normalization and Kerr pressure.
- `graph_wave_pure_physics_chain_contract_test`: substrate-level contract for dynamic
  U(1) flux, Cayley SO(3) holonomy, and rational local Kerr densification.
- `probe_streaming_compression_smoke`: operational CTest smoke for the production
  streaming path.

Rejected optimization:

- Small adjacency carrier for `Graph::adj`: rejected because it changed the streaming
  trajectory (horizons/bridges shifted). Do not revive it without a new physical reason
  and a strict A/B gate.

Latest checks:

```text
ctest --test-dir build -C Release -L nonlinear --output-on-failure
5 / 5 passed

probe_streaming_compression.exe 10000000 7
compression=3.09x
REAL=100.0% RANDOM=27.8%
bridges=36 true / 0 false
tokens_per_sec=71452
peak_ram_mb=1227
```

Calibration/stabilization note: `TOPK`, `WMIN`, `SMIN`, decay, and bridge coherence
thresholds are operational gates that prevent finite-precision/noise/history
bleeding from becoming "information". They are not the active physics. The active
physics remains phase, superposition, Cayley transport and local Kerr pressure.

## Research Timeline (settled)

1. Phase substrate — weights-free flow, gauge/Wilson flux, interference, superposition
   at machine precision (`RESULTS.md`).
2. Nonlinear compression / horizon — Kerr densification, ≈3x token-stream compression.
   Data excites the system, but energy is what densifies.
3. Bridges and Wilson flux — a bare edge phase is almost invisible without loops;
   local plaquettes matter because Wilson flux is gauge-visible.
4. The integration FILTER ("where to stop"):
   - Relative-phase DRIFT works only between structurally-matched (isomorphic) ports;
     on real non-isomorphic hubs the graph-geometry floor swamps it — drift was
     REJECTED in situ.
   - **Field overlap on shared support** `|Σ conj(ψ_a)·ψ_b| / √(P_a P_b)` is the
     useful primitive: a static interference in a common frame, no geometry confound,
     phase-driven (it survives shared vocabulary). This is the same measure
     recognition already uses. It also makes candidate selection trivial (the
     persistent hub core is tiny and does not grow with stream length).
5. The "when" — `stable` + `horizon` flags say when local structure has crystallized.

## Settled Negatives (do NOT re-derive)

- A long-range **energy-transport bridge is anti-physics for the nonlinear field**: a
  long bond lets energy delocalize, which OPPOSES Kerr self-focusing → it destroys
  compression and does not speed up the local streaming engine. Verified by
  derivation AND adversarial refutation. The engine's per-token work is already
  confined to a 2-hop light cone, so there is no long-range reach for a bridge to
  shortcut. Association in this substrate is a **phase-coherence relationship, not an
  energy channel** — read it with overlap, never by pumping energy across.
- A single-number compression ratio (PR_lin/PR_ker) is fooled by neighbourhood size;
  always decompose PR_lin vs PR_ker before claiming compression.
- Destructive shared-support / label-contamination "corruption" tests are invalid as
  evidence about normal physics (only meaningful as fault injection for impossible
  transport/library damage).

## Build & Test (C/C++ only)

Windows / MSVC, from a Developer prompt or via CMake:

```
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure     # the 61 GNNv2 contract gates
ctest --test-dir build -C Release -L nonlinear # just the nonlinear suite
ctest --test-dir build -j 8                     # parallel (the full suite is slow serially)
```

Single file:

```
cl /O2 /EHsc /std:c++20 /I tools tools\graph_wave_unitarity_test.cpp && .\graph_wave_unitarity_test.exe
```

Each `*_contract_test` returns exit 0 only on a full pass. Diagnostics
(`*_diagnostic_test`) and research probes are not wired into ctest (exploratory /
data-dependent); run them directly.

## Open / Next Work

- The bridge integration is best expressed as: bridge ONLY where overlap says
  (coherent shared support) and WHEN horizon says (crystallized) — its value is
  associative long-range linking in the LINEAR regime, kept out of the nonlinear
  compression path.
- Next performance work should target `bonds` / graph light-cone gathering and
  `sense` / `unproject`, but only as carrier/backend work with A/B tests that preserve
  horizons, compression, recognition, bridge counts, and false bridges.
- Keep GNNv2 separate from GNNv3 RC1 until the carrier-field gate is independently
  stable.
- Doc upkeep: `tools/README.md` / `research/README.md` are the file inventories;
  keep counts and negatives honest as files change.
