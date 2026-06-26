// graph_wave_kerr_entanglement_contract_test  (PHYSICS-ONLY)
// ----------------------------------------------------------------------------
// The substrate's local nonlinear step  psi_i -> psi_i * exp(-i g |psi_i|^2 dt)
// is a polar phase from the field's OWN intensity -- the one genuinely native,
// weights-free operation. On a non-uniform product state its phase carries multi-
// qubit cross terms (|psi_i|^2 = a PRODUCT of per-qubit probabilities), so it
// ENTANGLES. Everything here is physical: the state is PREPARED by physical
// rotations, the entanglement is READ by populations (qubit_physics.hpp).
//
// HONEST BOUNDARY: this is NOT "forbidden to a unitary". Gate [4] shows a physical
// UNITARY (flux-CZ, evolved by gw::Stepper) entangles the same product state too.
// Kerr's real distinction is that its entangling phase is generated from the data
// (input-adaptive, weights-free), not that a unitary could not do it. No QUANTUM
// advantage here (qubits stay = log2 N; a unitary reaches the same entanglement).
// This does NOT contradict docs/NONLINEAR_ENGINE.md: that documents the Kerr term's
// CLASSICAL compute value (a nonlinear reservoir is a universal approximator, a
// linear one is not) -- a different axis than entanglement / unitary-forbiddenness.
//
// One question: does the local nonlinear step entangle, ONLY with both superposition
// AND nonlinearity, and is the same entanglement reachable by a unitary?
//   [1] non-uniform product (physically prepared), ONE Kerr step -> S: 0 -> >0
//   [2] control g=0                                              -> S stays 0
//   [3] control no superposition (an injected basis state)       -> S stays 0
//   [4] a physical UNITARY flux-CZ entangles the same state too  -> not unitary-forbidden
// Substrate-only, deterministic. No weights, no trainer.
// ----------------------------------------------------------------------------
#include "qubit_physics.hpp"

#include <cmath>
#include <cstdio>

namespace {
using gw::Vec;
bool report(const char* w, bool ok) { std::printf("   => %s\n", ok ? "PASS" : "FAIL"); if (!ok) std::printf("   !! %s\n", w); return ok; }

// a non-uniform product state, prepared by DISTINCT physical rotations (none pi/2,
// so the per-qubit amplitudes are unbalanced and |psi_i|^2 varies across nodes).
Vec prepNonUniformProduct(int n) {
 Vec f = qp::inject0(n);
 f = qp::edgeRotX(f, n, 0, 2.0 * std::tan(qp::kPi / 12.0));  // alpha = pi/3
 f = qp::edgeRotX(f, n, 1, 2.0 * std::tan(qp::kPi / 16.0));  // alpha = pi/4
 f = qp::edgeRotX(f, n, 2, 2.0 * std::tan(qp::kPi / 6.0));   // alpha = 2pi/3
 return f;
}
} // namespace

int main() {
 std::printf("=====================================================================\n");
 std::printf(" GRAPH-WAVE KERR ENTANGLEMENT CONTRACT (physics-only)\n");
 std::printf(" A local nonlinear step entangles -- a state-adaptive diagonal unitary.\n");
 std::printf("=====================================================================\n");
 int pass = 0, total = 0;
 const double g = 6.0, dt = 1.0; const int n = 3;
 const double czDt = 2.0 * std::tan(qp::kPi / 64.0); const int czSteps = 32;

 std::printf("\n[1] LOCAL NONLINEAR STEP ENTANGLES A PRODUCT STATE (S: 0 -> >0)\n");
 {
  Vec f = prepNonUniformProduct(n);
  const double before = qp::maxEntropy(f, n);
  f = qp::kerrStep(f, g, dt);
  const double after = qp::maxEntropy(f, n);
  std::printf("   max qubit entropy before = %.3e   after one Kerr step = %.12f\n", before, after);
  ++total; pass += report("local nonlinear step did not generate entanglement", before < 1e-9 && after > 0.02) ? 1 : 0;
 }

 std::printf("\n[2] CONTROL g=0 (no nonlinearity) -> NO ENTANGLEMENT\n");
 {
  Vec f = prepNonUniformProduct(n);
  f = qp::kerrStep(f, 0.0, dt);
  const double after = qp::maxEntropy(f, n);
  std::printf("   max qubit entropy after g=0 step = %.3e\n", after);
  ++total; pass += report("g=0 produced entanglement (must not)", after < 1e-9) ? 1 : 0;
 }

 std::printf("\n[3] CONTROL no superposition (injected basis state) -> NO ENTANGLEMENT\n");
 {
  Vec f = qp::inject0(n);                 // |000>: a single node, no superposition
  f = qp::kerrStep(f, g, dt);             // Kerr = global phase only here
  const double after = qp::maxEntropy(f, n);
  std::printf("   max qubit entropy after Kerr on a basis state = %.3e\n", after);
  ++total; pass += report("nonlinearity entangled without superposition (must not)", after < 1e-9) ? 1 : 0;
 }

 std::printf("\n[4] A PHYSICAL UNITARY (flux-CZ) ENTANGLES THE SAME STATE -> NOT unitary-forbidden\n");
 {
  Vec f = prepNonUniformProduct(n);
  const double kerr = qp::maxEntropy(qp::kerrStep(f, g, dt), n);
  Vec u = qp::fluxCZ(f, n, 0, 1, czDt, czSteps);   // a UNITARY entangler, grown by gw::Stepper
  const double uni = qp::maxEntropy(u, n);
  std::printf("   nonlinear Kerr entropy = %.6f   physical UNITARY flux-CZ entropy = %.6f\n", kerr, uni);
  std::printf("   => a unitary entangles too; Kerr is special only by being input-adaptive & weights-free.\n");
  ++total; pass += report("a unitary did not entangle the same state (claim of unitary-forbiddenness)", uni > 0.02) ? 1 : 0;
 }

 std::printf("\n=====================================================================\n");
 std::printf(" RESULT : %d / %d verified\n", pass, total);
 std::printf(" %s\n", pass == total
             ? "the local nonlinear term is a state-adaptive, weights-free entangling unitary (not unitary-forbidden)"
             : "claim not established; inspect the failing gate");
 std::printf("=====================================================================\n");
 return pass == total ? 0 : 1;
}
