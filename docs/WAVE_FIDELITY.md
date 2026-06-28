# Wave fidelity & energy completeness

A broad audit of whether the substrate behaves like a real wave across the full set
of physical invariants — not just unitarity — and what that implies for readouts.
Sources: `research/probe_wave_fidelity.cpp`, `research/probe_wave_complete.cpp`.

## The substrate is a faithful wave

On plain, golden-flux, and directional-flux lattices, to machine precision:

| invariant | value |
|---|---|
| norm conservation (unitarity) | ~1e-15 |
| energy conservation `<H>` | ~1e-15 |
| time reversal `U(-t)U(t)=I` | ~1e-15 |
| gauge invariance of observables | ~1e-16 |
| continuity `d\|psi_i\|^2/dt = sum_j 2 Im(conj psi_i H_ij psi_j)` | 2e-9 (finite-diff; analytic 1e-16) |
| superposition / linearity | 1e-15 |
| homogeneity (translation invariance) | 1e-16 |
| energy conserved through interference (2 sources) | total = 1.000000 |

## Causality — the one place the *method* matters

A delta on a 1D chain spreads at a bounded speed (group velocity ~2). Beyond the
wavefront the field must be exponentially small (a light cone):

```
node:            d=8     d=12    d=20    d=40   (front ~ d=10)
fine-step / Chebyshev:  5e-1   8e-2   1e-5   1e-21   light cone respected
one large Cayley step:  1e-1   6e-2   1e-2   2e-4    instantaneous far-field leak
```

The true propagator `e^{-iHt}` is causal. The sparse **Chebyshev** engine is a
polynomial of `H`, so term `H^k` reaches exactly `k` hops — strictly causal by
construction. A single large **Cayley** step uses `(I+iK)^{-1}` (a matrix inverse =
non-local), so it leaks instantly to far nodes — it is *not* a faithful wave. Use
fine Cayley steps or Chebyshev. The production *propagation* engine — the linear,
node-scaled `e^{-iHt}` sparse-Chebyshev engine that walks over graph nodes (distinct
from the separate nonlinear Kerr token-streaming engine) — is Chebyshev, so it is
causal.

## Energy completeness — and what it corrects

The eigenbasis is complete (Parseval): `sum_k |<v_k|psi>|^2 = ||psi||^2 = 1.0`, and
the field reconstructs from all modes to `4e-13`. Collecting only part of the modes
collects only part of the energy:

```
energy collected by top-K modes:  K=8: 45%   K=16: 71%   K=32: 92%   K=64(all): 100%
```

**This corrects an earlier conclusion.** The order/phase channel was reported as
"weak" (~53%) — but the wave carries 100% of the information (Parseval = 1). Every
`<100%` result came from a **partial energy collection**: a truncated set of modes, a
real-valued projection (edge currents discard the phase), or a greedy decoder. The
information was never lost in the substrate; it was lost in the readout.

Practical rule: to approach 100%, read **completely and phase-preservingly** —
all modes, full complex phase, energy conserved. Physical interference
(`|psi_a + psi_b|^2 = ... + 2 Re<psi_a|psi_b>`) is one way to perform a complete,
phase-preserving comparison without discarding energy.
