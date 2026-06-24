# Word -> substrate conversion

Two contracts on this topic are kept side by side.

## 1. The order primitive

`tools/graph_wave_semantic_superposition_phase_contract_test.cpp`

Words are injected as real boundary excitations at different times. Unitary
evolution between injections manufactures a relative phase, and a gauge-invariant
loop circulation gives a signed scalar. For `A before B` vs `B before A`, the
circulation is exactly antisymmetric and gauge-invariant.

What it proves is real but narrow:

- order can become an emergent gauge-invariant phase;
- the scalar circulation is only a one-bit reverse detector;
- a scalar loop readout is collision-prone and not a complete signal;
- the word is still a fixed node excitation, not a semantic pattern.

## 2. The complete word signal

`tools/graph_wave_semantic_word_substrate_contract_test.cpp`

The corrected word substrate keeps the good part and removes the weak readout.

| part | corrected rule |
|---|---|
| word input | word = real co-occurrence pattern, not arbitrary node id |
| time/order | order is born only by unitary evolution between real injections |
| canonical signal | the full complex field `psi`, not `|psi|^2` or edge-current alone |
| readout | gauge-invariant complete overlap `|<psi_a|psi_b>|^2`; physical interference `2 Re<psi_a|psi_b>` when relative phase is controlled |
| controls | no-evolution collapses to an order-blind bag; real semantic graph beats matched-random; local gauge leaves overlaps invariant |

This aligns the word converter with the wave-fidelity audit: the wave carries the
information completely, while truncated modes, magnitude-only projections,
real-valued edge-current projections, and greedy decoders can under-read it.

Verified by the contract:

```text
[1] word pattern semantic        same-topic > cross-topic
[2] no evolution = bag           tau=0 field is order-blind
[3] complete readout             exact forward/reverse phrase order decodes via |<q|t>|^2
[4] partial projection weak      magnitude nearly bag-like; full complex separates
[5] semantic graph value         real semantic substrate > matched-random with same complete readout
[6] gauge invariant              |<psi_a|psi_b>|^2 unchanged by local gauge
```

Practical interpretation:

```text
word -> real semantic pattern -> time-shifted unitary field -> complete complex signal
```

The edge-current channel remains useful as a diagnostic of emergent order, but it
is not the canonical readout. The canonical readout must preserve phase and collect
the full signal energy.

One more control is mandatory: complete readout alone proves recoverability, not
semantic value. A matched-random unitary substrate can also preserve information.
The semantic value claim requires the same complete readout to perform better on
the real semantic substrate than on a matched-random substrate.
