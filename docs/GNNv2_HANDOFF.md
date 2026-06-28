# GNNv2 Handoff

Date: 2026-06-28

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

- **Flow** — unitary propagation `z_i ← Σ_j e^{i·δ_ij} z_j`; the edge flux `δ_ij` is
  the structure (no weights). Depth is `U^k`. See `ARCHITECTURE.md`.
- **Decorrelation** — separates correlated patterns so content is addressable; this
  is phase-coherence physics, the glue, not a fitted layer.
- **Settling / memory** — dissipative relaxation, holographic recall, amplitude
  amplification.
- **Nonlinear (Kerr) pressure** — `i ψ̇ = -H ψ - g|ψ|²ψ`, split-step DNLS. Energy
  densifies (compression ≈ 3x); a horizon is a detector over the already-evolved
  field, not a separate operation. Solitonic: stored energy stays localized under
  field noise (it does not disperse; it also does not reconstruct identity — measure
  by participation ratio, not template overlap). See `NONLINEAR_ENGINE.md`.

## Research Timeline (settled)

1. Phase substrate — weights-free flow, gauge/Wilson flux, interference, superposition
   at machine precision (`RESULTS.md`).
2. Nonlinear compression / horizon — Kerr densification, ≈3x streaming compression.
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
ctest --test-dir build --output-on-failure     # all *_contract_test gates
ctest --test-dir build -L nonlinear            # just the nonlinear suite
```

Single file:

```
cl /O2 /EHsc /std:c++20 /I tools tools\graph_wave_unitarity_test.cpp && graph_wave_unitarity_test.exe
```

Each `*_contract_test` returns exit 0 only on a full pass. Diagnostics
(`*_diagnostic_test`) and research probes are not wired into ctest (exploratory /
data-dependent); run them directly.

## Open / Next Work

- The bridge integration is best expressed as: bridge ONLY where overlap says
  (coherent shared support) and WHEN horizon says (crystallized) — its value is
  associative long-range linking in the LINEAR regime, kept out of the nonlinear
  compression path.
- Keep GNNv2 separate from GNNv3 RC1 until the carrier-field gate is independently
  stable.
- Doc upkeep: `tools/README.md` / `research/README.md` are the file inventories;
  keep counts and negatives honest as files change.
