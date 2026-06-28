# GNNv3 RC1 Handoff

Date: 2026-06-28

This document exists so the next session does not rediscover the same path.

## Non-Negotiable Discipline

- C/C++ only for executable project logic and tests.
- No Python, JavaScript, PowerShell, notebooks, or side systems for core work.
- No SQL/database framing.
- Physics-only substrate thinking.
- Do not use label/oracle information to create physics.
- Do not hide negative probes.
- Do not call a probe "working" if it only passes after destructive or artificial
  data reshaping.
- No trigonometric gate/readout in GNNv3 RC1.
- No half-wave phase clipping such as `max(0, cos(phase))`.
- No `Vec` projection/readout as the GNNv3 gate primitive.

In GNNv3 RC1, phase is carried as a local carrier field. The gate observes local
current/stress activity between transported carriers. The gate must not interpret
phase through an external algebraic/trigonometric lens.

## Field Roles

`psi`

- Content / energy field.
- Must remain physical and nonlinear.
- It is not "data"; it is the energetic carrier being shaped by the substrate.

`chi`

- Feeling / proprioception field.
- It is not a bridge.
- It is not a metric layer.
- It is a separate substrate state that senses where the system is alive.

`tau`

- Translator / structural sensing substrate.
- It helps `chi` read local structure without forcing `chi` to use `psi` rules.
- In RC1, `tau` contributes to the adaptive aperture that selects active physics.

`aperture`

- Local self-sensing gate memory.
- It replaces fixed `kChiGate/kTauGate` thresholds.
- It must be driven by fresh local activity, not by runaway accumulated `tauRho`.

## Research Timeline

1. GNNv2 phase substrate
   - Weight-free graph-wave substrate.
   - Phase, interference, superposition, gauge flux.

2. Nonlinear compression / horizon pressure
   - Kerr-like nonlinear pressure showed energy densification.
   - Streaming compression reached about 3x in earlier probes.
   - Important distinction: data is used to excite the system, but energy is what
     must densify.

3. Bridges and Wilson flux
   - Simple edge phase was almost invisible without loops.
   - Local loops / plaquettes matter because Wilson flux is gauge-visible.
   - Drift-based in-situ detection failed because graph geometry confounded the
     signal.
   - Field overlap on shared support was the useful primitive.

4. Feeling field
   - `chi` was separated from bridge logic.
   - `chi` could replace `psi` readout in normal stream tests.
   - Early destructive shared-support tests were rejected as invalid fault
     injection.

5. Synchronous phase-field
   - Moved toward local `rho/phi` physics without `Vec` projection.
   - Two-phase update: observe all local edges from a snapshot, then apply all
     changes together.
   - Canonicalized edge pairs to prevent traversal order from becoming hidden
     physics.

6. GNNv3 carrier gate
   - Rejected angle/trigonometric gate logic.
   - Replaced `phi` angle gate with two-component carrier field.
   - `chi/tau` now narrows the active physics window.
   - Added phase audit for signed current/stress clipping.

7. Adaptive aperture
   - Replaced fixed `kChiGate/kTauGate` with local aperture state.
   - First attempt used `drive + tauRho`, which self-choked and pushed max_rho
     above the bound.
   - Corrected aperture to learn from fresh `drive` only.

## Current Strongest Gate

File:

`tools/graph_wave_v3_feeling_gate_contract_test.cpp`

Latest 1M result:

```text
adaptive feeling-gated:
  active_pairs=0.631
  psi compression=2.05x
  chi compression=3.56x
  max_rho=47.371
  sync_error=0

phase audit:
  skipped current bias=0.008
  skipped stress bias=0.080

CONTRACT RESULT: 8 / 8 PASS
```

Meaning:

- Heavy edge-flow interactions reduced by about 36.9%.
- `chi` remains denser than `psi`.
- Signed phase current is not one-sidedly clipped.
- Field stays bounded.
- Matched-random control remains much weaker.

## Important Rejected Paths

`max(0, cos(phase))`

- Rejected.
- It half-wave-rectifies phase and can hide destructive phase.
- This was the source of a suspicious early result.

Shared-support destructive corruption

- Rejected as a normal system test.
- It can be useful only as fault injection for impossible transport/library
  damage, not as evidence about normal physics.

Bare drift coherence

- Rejected for in-situ bridge selection.
- Real hub geometry creates a floor that overwhelms the signal.

`probe_nonlinear_engine.cpp`

- Kept, but marked as mixed/failing research.
- 60k result: 3/4 gates.
- Failure: `tau structured` did not beat random.
- Do not present it as a production gate.

## Verification Commands

Use `cmd.exe` / MSVC x64:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /O2 /EHsc /std:c++20 tools\graph_wave_v3_feeling_gate_contract_test.cpp /Fe:build\tmp_bridge\v3_feeling_gate.exe
build\tmp_bridge\v3_feeling_gate.exe 1000000
```

No Developer PowerShell runner is required for the project logic.

## Next Work

1. Keep GNNv3 RC1 separate from GNNv2 production code until the carrier gate is
   stable across more adversarial physics probes.
2. Add C++-only noise/corruption probes for the carrier gate.
3. Add C++-only speed/memory measurements for 10M when RAM is checked first.
4. Try replacing remaining angle-based older probes only after the carrier-field
   contract remains stable.
5. Keep negative results in docs, not hidden in local memory.
