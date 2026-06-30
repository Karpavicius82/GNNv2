# Physics-only, phase-only, polar-only — the discipline for the hidden-qubit lens

This file exists because the SAME mistake recurred four times while building the
hidden-qubit instrumentation. Read it before adding any readout or descriptor.

## Where it keeps going wrong (the recurring failure)

Every error this session was the same shape: **a claim made on a PARTIAL test that
happened to pass**, then caught only by a fuller test or a cross-audit — never by
the assertion itself.

1. "Kerr is forbidden to a unitary" — too BOLD. A unitary `D = diag(exp(-ig|psi|^2 dt))`
   reproduces it exactly. (Caught by building `D` and checking.)
2. Calling exact equivalent physics "crutches" — too TIMID. The arithmetic readout
   equals the physical interferometric readout to 1e-16. (Caught by computing both.)
3. Building a recognition signature from **phase-blind** observables (entropy + ZZ,
   all population/diagonal), then calling the resulting blindness a "symmetry-invariant
   descriptor". The states were EXACTLY ORTHOGONAL (fidelity 0) — it was information
   LOSS, not symmetry. (Caught by a fidelity ground-truth check.)
4. Using **Cartesian** joint components `<XX>, <XY>` in the descriptor. Each is a
   lossy projection of one phase. (Caught by the owner: replace with polar `(M, phi)`.)

Root cause, all four: **asserting without the FULL test, and reading in the wrong
coordinates (Cartesian / population-only) so the phase — which carries the answer —
is silently dropped.**

## DO NOT

- **No algebra on the amplitude array.** Never pull a number out by hand
  (`sum_i psi_i conj psi_j`, indexed `Re`/`Im`, a typed `psi[k] = -psi[k]` gate).
- **No Cartesian coordinates.** No `Re`/`Im`, no `<X>`/`<Y>` as separate components,
  no `<XX>`/`<XY>`. They split one phase into lossy projections.
- **No phase-blind descriptor trusted as complete.** Population / `|r|` / Z-only /
  `<ZZ>` readouts are blind to the joint (entangled-pair) phase. A descriptor built
  only from them WILL collapse distinct states.
- **No magnitude-only readout.** `|psi|` alone kills the phase and with it the answer
  (zeroing `Im` collapses entanglement to 0).
- **No claim without the full test** (see below).

## DO

- **Only physics.** The sole readout primitive is the Born rule: node population
  `|psi|^2`. Everything else — relative phase, coherence, entanglement — is obtained
  by arranging a physical interference (hypercube-edge beam-splitter + gauge-flux
  phase shifter, all `gw::Stepper`) so the answer appears in populations.
- **Only polar.** A qubit / a coherence = (magnitude, phase angle). Read the phase as
  **WHERE the interference fringe focuses** (an angle, `pairPhase`) and the magnitude
  as **how bright it peaks** (`pairMag`). Compare with the polar law of cosines
  (`polarCoherenceDist`). Never form the Cartesian components.
- **The phase IS the signal.** Keep it everywhere; it is the half of the complex
  number a Cartesian/magnitude readout throws away.
- **Test fully BEFORE claiming**, in this order:
  1. **Ground-truth fidelity** `|<psi_i|psi_j>|^2` — are the states actually distinct?
     (Unitary evolution preserves orthogonality: distinct injections stay orthogonal.)
  2. **Energy + phase preserved** — norm = 1 through every op; reversibility returns
     to the start to ~1e-15 (the decisive no-loss proof).
  3. **Phase-sensitive readout** — the descriptor separates states the fidelity says
     are distinct. If it does not, it is BLIND, not invariant.

## The one rule that would have caught all four

`fidelity = 1` means the same state (a real gauge/symmetry equivalence).
`fidelity = 0` means distinct states — a COMPLETE readout MUST separate them.
If your readout returns "0 distance" for `fidelity = 0` states, your readout is
blind. Run the fidelity gate first, always.

## Calibration is not physics

Operational stabilizers are allowed, but must be named honestly:

- `TOPK`, decay, support minima, horizon minima and bridge coherence thresholds are
  noise/resource gates for finite precision, finite RAM and long streams.
- They prevent machine noise and infinite history from being mistaken for signal.
- They are not the active physics and must not be tuned to hide a broken result.

The active physics remains phase, superposition, Cayley transport, Wilson flux and
local Kerr pressure. If a change modifies that chain, it needs a contract and an A/B
streaming test. If a change only carries the same local field more cheaply, it is a
backend/carrier optimization and must preserve horizons, compression, recognition and
false-bridge counts.
