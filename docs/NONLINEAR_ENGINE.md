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
