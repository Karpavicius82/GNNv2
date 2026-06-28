# GNNv3 RC1 Technical Report

Date: 2026-06-28

## Scope

GNNv3 RC1 is the current research checkpoint for a self-sensing graph-wave
substrate. It is not a trainer and not a SQL/feature pipeline. The core idea is:

- `psi` carries content / energy.
- `chi` carries feeling / proprioception.
- `tau` carries local structural sensing.
- The system uses `chi/tau` to choose where the expensive local physics should
  run.

Discipline:

- C/C++ only for executable project logic and tests.
- Physics-only framing.
- No SQL/database view.
- No Python/JS/notebook side systems for core project work.
- No trigonometric gate/readout in the GNNv3 RC1 carrier gate.
- No half-wave phase clipping.
- Keep negative results visible.

The main RC1 result is the no-trigonometric carrier-field gate:

`tools/graph_wave_v3_feeling_gate_contract_test.cpp`

This test stores phase as a two-component carrier, not as an angle. The gate does
not call `sin`, `cos`, `atan2`, `wrap`, or `std::exp`. It observes local
current/stress activity between transported carriers.

## Key Result

1M stream, adaptive aperture, no trigonometric gate:

```text
full physics:
  active_pairs=1.000
  psi compression=2.68x
  chi compression=3.23x
  max_rho=42.619

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

Interpretation: the system keeps synchronous physics, reduces heavy edge-flow
interactions by about 36.9%, and does not one-sidedly clip signed phase current.

## Main Files

- `tools/graph_wave_v3_feeling_gate_contract_test.cpp`
  - GNNv3 RC1 candidate.
  - Carrier-field phase representation.
  - Adaptive `aperture` gate from fresh local feeling activity.
  - Formal phase audit for signed current/stress clipping.

- `tools/graph_wave_synchronous_phase_field_contract_test.cpp`
  - Earlier local `rho/phi` synchronous phase-field contract.
  - Proved order-invariant synchronous update and `chi` densification.

- `tools/graph_wave_dual_feeling_substrate_contract_test.cpp`
  - Parallel `psi/chi` readout contract.
  - Normal non-corrupted stream, not fault injection.

- `tools/graph_wave_nonlinear_engine.hpp`
  - Experimental closed nonlinear GNNv2 engine header.
  - Polar `rho/phi`, synchronous local physics.
  - Kept as research context, not the final GNNv3 no-trig path.

- `research/probe_nonlinear_engine.cpp`
  - Driver for the experimental closed nonlinear engine.
  - Current result is mixed: 3/4 gates at 60k because `tau` does not beat random.

## Verification Commands

Run from repo root with MSVC x64 environment:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /O2 /EHsc /std:c++20 tools\graph_wave_v3_feeling_gate_contract_test.cpp /Fe:build\tmp_bridge\v3_feeling_gate.exe
build\tmp_bridge\v3_feeling_gate.exe 1000000
```

Additional local checks:

```bat
cl /nologo /O2 /EHsc /std:c++20 /I tools tools\graph_wave_dual_feeling_substrate_contract_test.cpp /Fe:build\tmp_bridge\dual_feeling.exe
build\tmp_bridge\dual_feeling.exe 60000

cl /nologo /O2 /EHsc /std:c++20 tools\graph_wave_synchronous_phase_field_contract_test.cpp /Fe:build\tmp_bridge\sync_phase_field.exe
build\tmp_bridge\sync_phase_field.exe 1000000

cl /nologo /O2 /EHsc /std:c++20 /I tools research\probe_nonlinear_engine.cpp /Fe:build\tmp_bridge\probe_nonlinear_engine.exe
build\tmp_bridge\probe_nonlinear_engine.exe 60000
```

## Test Matrix

| Test | Stream | Result | Notes |
|---|---:|---|---|
| `graph_wave_v3_feeling_gate_contract_test.cpp` | 60k | PASS 8/8 | adaptive carrier gate, no trig |
| `graph_wave_v3_feeling_gate_contract_test.cpp` | 1M | PASS 8/8 | active pairs 0.631, skipped current bias 0.008 |
| `graph_wave_synchronous_phase_field_contract_test.cpp` | 60k | PASS 5/5 | sync error 0 |
| `graph_wave_synchronous_phase_field_contract_test.cpp` | 1M | PASS 5/5 | `psi=2.97x`, `chi=3.61x` in prior run |
| `graph_wave_dual_feeling_substrate_contract_test.cpp` | 60k | PASS 5/5 | `chi` readout 100% in prior run |
| `probe_nonlinear_engine.cpp` | 60k | FAIL 3/4 | useful negative result: tau random separation weak |

## Important Negative Results

1. Earlier `max(0, cos(phase))` gate was rejected.
   It half-wave-rectified phase and could hide destructive phase. The current
   carrier-field gate removed that mechanism.

2. `probe_nonlinear_engine.cpp` currently fails the `tau structured >> random`
   gate at 60k:

```text
tau structured=17.381 random=18.119 (1.0x)
CLOSURE: 3 / 4 physics gates
```

This remains in the repository because it is useful research evidence, not a
production gate.

## Current Architecture Judgment

The strongest GNNv3 direction is:

- keep phase as a local carrier, not an angle;
- keep updates synchronous;
- let `chi/tau` adaptively narrow the active physics window;
- audit signed current/stress to ensure the gate does not erase one phase side;
- keep failed probes visible rather than tuning them into fake wins.

RC1 is ready as a research checkpoint, not a final production architecture.

For session-to-session continuity, read `docs/GNNv3_RC1_HANDOFF.md` before
changing the substrate.
