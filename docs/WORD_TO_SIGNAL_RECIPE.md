# Word -> signal: the fix (for the parallel work)

The conversion was crashing not because phase is weak, but because the READOUT
threw the phase away. Fix = keep the wave complete and phase-preserving end to end.

## Recipe (each step matters)

1. **Word = real semantic pattern**, not a node id and not a pre-assigned phase.
   Inject the co-occurrence / PPMI row as a REAL boundary excitation.
2. **Order is born, not assigned.** Inject the words at different times; let the
   unitary substrate evolve BETWEEN injections. The relative phase that appears is
   the order. (tau=0 -> no evolution -> collapses to an order-blind bag = the control.)
3. **The canonical signal is the COMPLETE complex field psi.** Not |psi|^2, not the
   edge-current channel. Those are real projections = lossy diagnostics only.
4. **Read phase-preservingly and completely.** The canonical observable is
   `|<psi_a|psi_b>|^2`: it preserves phase information and is gauge-invariant.
   Physical interference reads the related cross-term
   `2 Re<psi_a|psi_b>`, which is native and useful when the relative phase setup is
   controlled. NEVER collapse to real scalars + a linear classifier before the
   comparison -- that is exactly where order (53% -> chance) was lost.
5. **To approach 100%, collect ALL the energy.** Parseval: the complete mode basis
   carries 100% (reconstruction ~1e-13). Truncated modes / real projections / greedy
   decoders collect only part (top-8 modes ~ 45-53% -- that IS the old "weak phase").
6. **Physical interference is the native complete readout.**
   |psi_a + psi_b|^2 = |psi_a|^2 + |psi_b|^2 + 2 Re<psi_a|psi_b>. The cross-term is the
   phase-preserving overlap, measured as intensity -- the wave reads itself, no FFT,
   no classifier.

## What proves it works (not grey noise)

decode-back with COMPLEX overlap recovers words+order well above chance; the same
field read by real edge-currents + linear classifier does not. Same wave, opposite
result -> the axis is phase-preserving (complex) vs phase-collapsing (real) readout,
not "physics vs arithmetic". See docs/WAVE_FIDELITY.md.

## What proves the semantic substrate adds value

Complete readout alone proves that the wave did not lose the information. It does
not prove that the graph is semantic: a matched-random unitary substrate can also
decode well because invertible memory is generic. The required control is:

```text
real semantic substrate > matched-random substrate
with the SAME complete phase-preserving readout
```

That is the value cut. It isolates semantic structure from generic unitary memory.
