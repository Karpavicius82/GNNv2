# The nonlinear term `-g|ψ|²ψ`: densification and computation are one

## Claim

The single Kerr/DNLS term `-g|ψ|²ψ` in the substrate is **two things at once**:

1. **Energy-pressure densification** — the attractive self-interaction makes energy
   gravitate into energy and collapse to a dense soliton core (the black-hole /
   gravitational-collapse analog). Proven in
   `tools/graph_wave_horizon_densification_contract_test.cpp` and wired into the
   online loop in `tools/graph_wave_streaming_densification_contract_test.cpp`.

2. **A nonlinear compute engine** — the same term mixes modes (cross-phase
   modulation / four-wave mixing), generating the **high-order products** that a
   purely unitary `e^{-iHt}` substrate **cannot** produce. A linear readout on the
   linear substrate can realize only affine functions; on the Kerr-evolved field it
   computes and interpolates genuinely nonlinear functions. Proven in
   `tools/graph_wave_nonlinear_compute_contract_test.cpp`.

**Densification and nonlinear computation are two faces of the same term.**

## Why this matters

The benefit of the nonlinearity is **not memory** — topic recognition is linearly
separable (bag-of-co-occurrence) and needs no nonlinearity; the field even stores
more, not less. The benefit is **computational power the linear/unitary system does
not have**. A linear reservoir + linear readout is not a universal approximator; a
nonlinear one is.

## Evidence (reservoir with a LINEAR ridge readout; encoding affine, NOT renormalized
so g=0 features are exactly affine in the input — any gap is the Kerr engine)

| gate | result |
|---|---|
| linear reservoir is COMPETENT | `g=0` fits `y = x1 − x2`, **R² = 1.00** |
| ...but LIMITED | `g=0` cannot do 3-bit parity, **acc = 50.0%** (chance) |
| Kerr COMPUTES parity | `g=4` **acc = 100.0%** (vs `g=0` 50.0%) |
| Kerr INTERPOLATES `y=(2x1−1)(2x2−1)` (pure degree-2) | `g=0` **R² = −0.01** → `g=1` **R² = 1.00** |

Optimal nonlinearity is **task-dependent (edge of chaos)**: a parity decision wants
more mixing (`g≈4`), a degree-2 continuous map wants less (`g≈1`); strong `g≥8`
over-mixes into chaos and degrades both; `g=0` does neither.

## Honest note

An earlier draft renormalized the field per step, injecting a nonlinearity that made
`g=0` look capable — a confound, since removed. Without it, the linear substrate
fails every nonlinear task and the Kerr engine is decisively responsible for the gap.

## Production streaming status (2026-07-01)

The production streaming path is `research/probe_streaming_compression.cpp` with:

- packet memory for tiny per-node `lin` / `ker` / `sense` fields;
- prepared Cayley flow carrier built directly from the local light cone;
- the same phase/Kerr physics: Cayley transport, superposition, local Kerr pressure,
  and phase-preserving overlap readout.

The packet/prepared changes are carrier optimizations, not new physics. A rejected
small-adjacency experiment changed the trajectory and was not kept. The accepted path
preserves the same horizons, bridges, compression and recognition while reducing RAM
and repacking cost.

Latest CMake Release anchor:

```text
probe_streaming_compression.exe 10000000 7
stream=10000000 uniqueEvery=7 field_updates=10000000 nodes=1428644
horizon PR linear=6.61 nonlinear=2.14 compression=3.09x
bridges total=36 true=36 false=0
value REAL=100.0% RANDOM=27.8%
train_sec=139.95 tokens_per_sec=71452 peak_ram_mb=1227
```

Regression gate:

```bat
ctest --test-dir build -C Release -L nonlinear --output-on-failure
```

Current nonlinear label contains:

- `graph_wave_horizon_densification_contract_test`
- `graph_wave_nonlinear_compute_contract_test`
- `graph_wave_pure_physics_chain_contract_test`
- `graph_wave_streaming_densification_contract_test`
- `probe_streaming_compression_smoke`

Do not tune weights or thresholds to improve this benchmark. Thresholds such as
`TOPK`, `WMIN`, `SMIN`, and bridge coherence are operational stabilizers/noise gates,
not the active nonlinear physics. If they are changed, document them as calibration
and re-run the full nonlinear gate plus a 1M+ streaming run.
