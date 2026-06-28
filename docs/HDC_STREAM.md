# HDC streaming — cheap, coherent field accumulation

The per-token Kerr field stream learns and recognises (REAL 100%), but it is slow
(~47k tok/s) and heavy (5 KB/node) because **every token evolves a light-cone field**
and stores a variable-size field **per node**. The accumulation of that per-word
field is what carries recognition — and it is the expensive part.

Every cheap shortcut we tried **loses the function**, verified live:

| approach | per-token cost | recognition |
|---|---|---|
| per-token Kerr field (accumulated) | light-cone field evolve | **100%** |
| lazy Kerr readout (single-shot) | none (read at end) | 31% (~chance) |
| one shared global field | local evolve | 31% |
| phase on the edges (flux) | plasticity | 31% |
| raw graph neighbours (1/2-hop) | plasticity | 33–35% |

The diagnosis: the streaming graph carries **no gauge-invariant flux** (Wilson loop
~0, and only ~16 triangles after TopK pruning), so the meaning is **not** in the
graph rotation — it is in the **accumulated per-word field**, in one coherent phase
frame. A single-shot or graph-only read has no shared frame, so it collapses to chance.

## The mechanism

Replace the per-node neighbourhood **field** (expensive to evolve) with a fixed-`D`
**complex hypervector** (HDC/VSA). Phase is the carrier: each word gets a fixed random
**unit-phase base vector** `base[w] ∈ (e^{iθ})^D`. Per token we only **bundle** the
context words' base vectors into each other's embedding:

```
for each context word c in the window:
    emb[w] += base[c]          # O(D), no field evolve
    emb[c] += base[w]
```

Because every word bundles the **same shared base vectors**, all embeddings live in
**one coherent phase frame** — exactly the shared frame the lazy/shared/flux variants
lacked. Same-topic words accumulate overlapping sets of base vectors, so their
embeddings overlap. Recognition = hypervector overlap `|<emb_a, emb_b>|`.

This is cheap (`O(D × window)` per token, vectorisable), it **accumulates** over the
stream like the per-token field, and it stays coherent (shared frame) — the three
properties the field had, without the light-cone evolve.

## Results (`research/probe_hdc_stream.cpp`, live)

```
HDC D=256, stream = 2,000,000 tokens
  tok/s = ~460k                          (~10x the per-token Kerr field's ~47k tok/s)
  recognition REAL   = 100.0%            (matches the heavy per-token Kerr stream)
  recognition RANDOM = 31.2%  (~chance)  (topic-shuffled control: structure, not the
                                          hypervectors, carries the signal)
  gate: REAL must beat RANDOM by > 30 pp -> PASS
```

The RANDOM control (any-topic bursts, no co-occurrence structure) collapses to chance,
so the 100% is the learned topical structure, not an artifact of the representation.

## Relation to the substrate

The base vectors are **unit-phase** (complex), so the carrier is phase, consistent with
the substrate principle "meaning lives in the phase, not in stored magnitudes". The
bundle is a phase-coherent superposition; the shared base set is the gauge/reference
frame. No weights, no trainer: `base` is a fixed random identity, `emb` is a running
superposition.

## Open / next

- **Nonlinear compression (densification) on the hypervector**: a Kerr-like nonlinear
  phase / sparsification on `emb` to concentrate it (the energy-pressure analog at the
  vector level), still `O(D)`.
- **> 1M tok/s**: smaller `D` (D=128 ≈ 2x) or SIMD on the bundle loop.
- **RAM**: `base` can be hash-generated on demand instead of stored (halves memory);
  `emb` is the only state needed for `O(vocab × D)`.
