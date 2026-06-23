// graph_wave_semantic_superposition_phase_contract_test
// ----------------------------------------------------------------------------
// Minimal semantic transduction contract:
// words are injected as real boundary excitations, not as pre-assigned phase
// codes. Their ordered superposition is allowed to evolve on a Hermitian
// substrate. The semantic orientation is then read as a gauge-invariant phase
// circulation. Bag-of-words cannot distinguish the phrases; phase erasure kills
// the signal; a local gauge change leaves the observable unchanged.
// ----------------------------------------------------------------------------
#include "graph_wave_substrate.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

namespace {

using gw::cd;
using gw::CMat;
using gw::Graph;
using gw::Stepper;
using gw::Vec;

constexpr int kA = 0;
constexpr int kRel = 1;
constexpr int kB = 2;
constexpr double kTau = 0.72;

double norm2(const Vec& z) {
 double s = 0.0;
 for (const auto& v : z) s += std::norm(v);
 return s;
}

Vec normalized(Vec z) {
 double n = std::sqrt(norm2(z)) + 1e-15;
 for (auto& v : z) v /= n;
 return z;
}

double maxDiff(const Vec& a, const Vec& b) {
 double e = 0.0;
 for (int i = 0; i < static_cast<int>(a.size()); ++i) e = std::max(e, std::abs(a[i] - b[i]));
 return e;
}

Graph triangle() {
 Graph g(3);
 g.addEdge(kA, kRel, 1.0);
 g.addEdge(kRel, kB, 1.0);
 g.addEdge(kB, kA, 1.0);
 return g;
}

Vec bagState() {
 Vec z(3, cd(0, 0));
 z[kA] += cd(1, 0);
 z[kRel] += cd(1, 0);
 z[kB] += cd(1, 0);
 return normalized(z);
}

Vec drivePhrase(const CMat& h, const std::vector<int>& tokens, double tau) {
 Stepper stepper;
 stepper.build(h, tau);
 Vec psi(3, cd(0, 0));
 bool has_field = false;

 for (int token : tokens) {
 if (has_field) psi = stepper.step(psi);
 psi[token] += cd(1, 0); // real word impulse; phase is not assigned to the word
 has_field = true;
 }

 psi = stepper.step(psi);
 return normalized(psi);
}

double edgeCurrent(const CMat& h, const Vec& psi, int i, int j) {
 return 2.0 * std::imag(std::conj(psi[i]) * h[i][j] * psi[j]);
}

double circulation(const CMat& h, const Vec& psi) {
 return edgeCurrent(h, psi, kA, kRel) + edgeCurrent(h, psi, kRel, kB) +
 edgeCurrent(h, psi, kB, kA);
}

Vec phaseErased(Vec z) {
 for (auto& v : z) v = cd(std::abs(v), 0.0);
 return normalized(z);
}

void gaugeTransform(const CMat& h, const Vec& psi, CMat& hg, Vec& psig) {
 const double theta[3] = {0.19, -0.73, 1.11};
 hg = h;
 psig = psi;
 for (int i = 0; i < 3; ++i) psig[i] *= std::exp(cd(0, theta[i]));
 for (int i = 0; i < 3; ++i) {
 for (int j = 0; j < 3; ++j) hg[i][j] *= std::exp(cd(0, theta[i] - theta[j]));
 }
}

bool report(const char* name, bool ok) {
 std::printf(" => %s\n", ok ? "PASS" : "FAIL");
 if (!ok) std::printf(" !! %s\n", name);
 return ok;
}

} // namespace

int main() {
 std::printf("=====================================================================\n");
 std::printf(" SEMANTIC SUPERPOSITION -> EMERGENT PHASE CONTRACT\n");
 std::printf(" words are real excitations; phase must be born from unitary superposition\n");
 std::printf("=====================================================================\n");

 Graph g = triangle();
 const CMat& h = g.h;
 const std::vector<int> before = {kA, kRel, kB};
 const std::vector<int> after = {kB, kRel, kA};

 int pass = 0;
 int total = 0;

 std::printf("\n[1] BAG-OF-WORDS CONTROL: same words are indistinguishable\n");
 Vec bag_ab = bagState();
 Vec bag_ba = bagState();
 double bag_diff = maxDiff(bag_ab, bag_ba);
 std::printf(" bag state diff(A before B, B before A) = %.2e\n", bag_diff);
 ++total;
 pass += report("bag-of-words should not encode order", bag_diff < 1e-14);

 std::printf("\n[2] WORD SUPERPOSITION CREATES AN ORIENTED PHASE OBSERVABLE\n");
 Vec psi_ab = drivePhrase(h, before, kTau);
 Vec psi_ba = drivePhrase(h, after, kTau);
 double c_ab = circulation(h, psi_ab);
 double c_ba = circulation(h, psi_ba);
 std::printf(" circulation(A before B) = %+ .12f\n", c_ab);
 std::printf(" circulation(B before A) = %+ .12f\n", c_ba);
 std::printf(" antisymmetry error       = %.2e\n", std::abs(c_ab + c_ba));
 ++total;
 pass += report("order did not become an opposite phase circulation",
 std::abs(c_ab) > 0.05 && std::abs(c_ba) > 0.05 && std::abs(c_ab + c_ba) < 1e-12);

 std::printf("\n[3] NO UNITARY EVOLUTION: no phase is born\n");
 Vec no_phase_ab = drivePhrase(h, before, 0.0);
 Vec no_phase_ba = drivePhrase(h, after, 0.0);
 double c0_ab = circulation(h, no_phase_ab);
 double c0_ba = circulation(h, no_phase_ba);
 std::printf(" tau=0 circulation(A before B) = %+ .12e\n", c0_ab);
 std::printf(" tau=0 circulation(B before A) = %+ .12e\n", c0_ba);
 ++total;
 pass += report("phase appeared without evolution", std::abs(c0_ab) < 1e-14 && std::abs(c0_ba) < 1e-14);

 std::printf("\n[4] ERASE PHASE AFTER SUPERPOSITION: the semantic orientation collapses\n");
 double ce_ab = circulation(h, phaseErased(psi_ab));
 double ce_ba = circulation(h, phaseErased(psi_ba));
 std::printf(" phase-erased circulation(A before B) = %+ .12e\n", ce_ab);
 std::printf(" phase-erased circulation(B before A) = %+ .12e\n", ce_ba);
 ++total;
 pass += report("orientation survived phase erasure", std::abs(ce_ab) < 1e-14 && std::abs(ce_ba) < 1e-14);

 std::printf("\n[5] GAUGE CHECK: circulation is an observable, not absolute phase\n");
 CMat hg;
 Vec psig;
 gaugeTransform(h, psi_ab, hg, psig);
 double cg = circulation(hg, psig);
 std::printf(" native circulation = %+ .12f\n", c_ab);
 std::printf(" gauge circulation  = %+ .12f\n", cg);
 std::printf(" gauge diff         = %.2e\n", std::abs(c_ab - cg));
 ++total;
 pass += report("circulation changed under local gauge", std::abs(c_ab - cg) < 1e-12);

 std::printf("\n RESULT : %d / %d verified\n", pass, total);
 return pass == total ? 0 : 1;
}
