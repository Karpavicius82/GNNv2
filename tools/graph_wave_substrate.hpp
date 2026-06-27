// graph_wave_substrate.hpp
// ----------------------------------------------------------------------------
// SINGLE SOURCE OF TRUTH for the graph-wave substrate lab. The physics that was
// being copy-pasted into every contract -- the Hermitian graph/lattice families,
// the unitary Cayley evolution, the orthonormal-row kernel construction, and the
// real-symmetric eigensolver -- lives HERE, defined once and tested once, so the
// contracts become thin verifications on a trusted core instead of N independent
// reimplementations (and N places for a subtle bug to hide).
//
// Header-only; every consumer is a lab target. No decision, no
// learning, no trainer linkage -- substrate physics only.
// ----------------------------------------------------------------------------
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <random>
#include <utility>
#include <vector>
namespace gw {
using cd = std::complex<double>;
using Vec = std::vector<cd>;
using CMat = std::vector<std::vector<cd>>;
using RMat = std::vector<std::vector<double>>;
constexpr double kPi = 3.141592653589793238462643383279502884;

// ---- Hermitian graph and the lattice families (extended / critical / localized)
struct Graph {
 CMat h;
 explicit Graph(int n) : h(n, Vec(n, cd(0, 0))) {}
 int size() const { return (int)h.size(); }
 void addEdge(int i, int j, double w, double ph = 0.0) { cd v = w * std::exp(cd(0, ph)); h[i][j] += v; h[j][i] += std::conj(v); }
 void onsite(int i, double v) { h[i][i] += cd(v, 0); }
};
inline Graph plain(int w) { Graph g(w * w); auto id = [w](int x, int y) { return y * w + x; };
 for (int y = 0; y < w; y++) for (int x = 0; x < w; x++) { if (x + 1 < w) g.addEdge(id(x, y), id(x + 1, y), 1.0); if (y + 1 < w) g.addEdge(id(x, y), id(x, y + 1), 1.0); } return g; }
// golden = Hofstadter magnetic flux 2*pi*a*x on the vertical bonds; meaning lives
// in the gauge-invariant flux, not absolute phase.
inline Graph golden(int w, double a) { Graph g(w * w); auto id = [w](int x, int y) { return y * w + x; };
 for (int y = 0; y < w; y++) for (int x = 0; x < w; x++) { if (x + 1 < w) g.addEdge(id(x, y), id(x + 1, y), 1.0); if (y + 1 < w) g.addEdge(id(x, y), id(x, y + 1), 1.0, 2 * kPi * a * x); } return g; }
inline Graph disorder(int w, double W, unsigned seed = 0xBEEFu) { Graph g = plain(w); std::mt19937 rng(seed); std::uniform_real_distribution<double> u(-W, W);
 for (int i = 0; i < w * w; i++) g.onsite(i, u(rng)); return g; }
// random-phase bonds: a delocalized but phase-disordered control (not localized)
inline Graph randomPhase(int w, unsigned seed = 0xC0FFEEu) { Graph g(w * w); auto id = [w](int x, int y) { return y * w + x; }; std::mt19937 rng(seed); std::uniform_real_distribution<double> ph(-kPi, kPi);
 for (int y = 0; y < w; y++) for (int x = 0; x < w; x++) { if (x + 1 < w) g.addEdge(id(x, y), id(x + 1, y), 1.0, ph(rng)); if (y + 1 < w) g.addEdge(id(x, y), id(x, y + 1), 1.0, ph(rng)); } return g; }

// ---- unitary Cayley evolution: U = (I + iH dt/2)^-1 (I - iH dt/2), exactly unitary
struct LUC {
 int n = 0; CMat a; std::vector<int> piv;
 void factor(CMat in) { n = (int)in.size(); a = std::move(in); piv.resize(n); for (int i = 0; i < n; i++) piv[i] = i;
 for (int k = 0; k < n; k++) { double b = std::abs(a[k][k]); int r = k; for (int i = k + 1; i < n; i++) { double v = std::abs(a[i][k]); if (v > b) { b = v; r = i; } }
 if (r != k) { std::swap(a[r], a[k]); std::swap(piv[r], piv[k]); } cd akk = a[k][k];
 for (int i = k + 1; i < n; i++) { cd f = a[i][k] / akk; a[i][k] = f; for (int j = k + 1; j < n; j++) a[i][j] -= f * a[k][j]; } } }
 Vec solve(const Vec& b) const { Vec x(n); for (int i = 0; i < n; i++) x[i] = b[piv[i]];
 for (int i = 0; i < n; i++) { cd s = x[i]; for (int j = 0; j < i; j++) s -= a[i][j] * x[j]; x[i] = s; }
 for (int i = n - 1; i >= 0; i--) { cd s = x[i]; for (int j = i + 1; j < n; j++) s -= a[i][j] * x[j]; x[i] = s / a[i][i]; } return x; }
};
struct Stepper {
 int n = 0; LUC lu; CMat am;
 void build(const CMat& H, double dt) { n = (int)H.size(); CMat ap(n, Vec(n)); am.assign(n, Vec(n)); const cd I(0, 1);
 for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { cd k = I * H[i][j] * (dt / 2.0); cd id = (i == j) ? cd(1, 0) : cd(0, 0); ap[i][j] = id + k; am[i][j] = id - k; } lu.factor(ap); }
 Vec step(const Vec& psi) const { Vec rhs(n, cd(0, 0)); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) rhs[i] += am[i][j] * psi[j]; return lu.solve(rhs); }
 // the one-step propagator as an explicit matrix (column c = step of basis vector c)
 CMat asMatrix() const { CMat U(n, Vec(n)); for (int c = 0; c < n; c++) { Vec e(n, cd(0, 0)); e[c] = cd(1, 0); Vec col = step(e); for (int i = 0; i < n; i++) U[i][c] = col[i]; } return U; }
};

// ---- dense complex matrix helpers
inline CMat matmul(const CMat& a, const CMat& b) { int n = (int)a.size(); CMat o(n, Vec(n, cd(0, 0)));
 for (int i = 0; i < n; i++) for (int k = 0; k < n; k++) for (int j = 0; j < n; j++) o[i][j] += a[i][k] * b[k][j]; return o; }
inline CMat dagger(const CMat& m) { int n = (int)m.size(); CMat o(n, Vec(n)); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) o[j][i] = std::conj(m[i][j]); return o; }
inline Vec matvec(const CMat& m, const Vec& x) { int n = (int)m.size(); Vec o(n, cd(0, 0)); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) o[i] += m[i][j] * x[j]; return o; }
inline double unitErr(const CMat& m) { int n = (int)m.size(); CMat g = matmul(dagger(m), m); double e = 0;
 for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) e = std::max(e, std::abs(g[i][j] - (i == j ? cd(1, 0) : cd(0, 0)))); return e; }
inline double power(const Vec& z) { double s = 0; for (auto& v : z) s += std::norm(v); return s; }
inline Vec normalized(Vec z) { double n = std::sqrt(power(z)) + 1e-300; for (auto& v : z) v /= n; return z; }
inline void normalizeInPlace(Vec& z) { double n = std::sqrt(power(z)) + 1e-300; for (auto& v : z) v /= n; }
inline double participationRatio(const Vec& z) { double a = 0, b = 0; for (auto& v : z) { double p = std::norm(v); a += p; b += p * p; } return a * a / (b + 1e-300); }
inline CMat normalizedHopping(CMat h) {
 double m = 0;
 for (const auto& row : h) for (const auto& z : row) m = std::max(m, std::abs(z));
 if (m < 1e-300) return h;
 for (auto& row : h) for (auto& z : row) z /= m;
 return h;
}

// ---- nonlinear Kerr / self-focusing evolution
// Split-step discrete nonlinear Schrodinger flow:
//   i psi_dot = -H psi - g |psi|^2 psi
// This is substrate physics, not a trainer: half linear Cayley hop, local nonlinear
// phase rotation from the field's own intensity, half linear Cayley hop.
struct KerrStepper {
 double g = 0.0, dt = 0.0;
 Stepper half;
 void build(CMat h, double dt_, double g_, bool normalize_hopping = true) {
  g = g_; dt = dt_;
  if (normalize_hopping) h = normalizedHopping(std::move(h));
  half.build(h, dt / 2.0);
 }
 Vec step(Vec psi) const {
  psi = half.step(psi);
  if (g != 0.0) for (auto& v : psi) v *= std::exp(cd(0, -g * std::norm(v) * dt));
  psi = half.step(psi);
  return psi;
 }
 Vec evolve(Vec psi, int steps) const { for (int s = 0; s < steps; ++s) psi = step(std::move(psi)); return psi; }
};

// ---- matrix-free local Kerr flow
// This is the streaming/native path: no dense H, no matrix multiply, no LU.
// The field moves by local beam-splitter rotations on already-existing bonds,
// while density writes a local nonlinear phase. Energy stays in the field; the
// only change is where it is concentrated.
struct SparseBond {
 int a = 0, b = 0;
 double w = 0.0;
 double phase = 0.0; // gauge flux on this bond; 0 = phaseless (old behavior)
};
struct LocalFlowStats {
 long long bond_visits = 0;
 double current_abs = 0.0;
 double max_bond_speed = 0.0;
};
inline double bondCurrent(cd a, cd b, double w) {
 return 2.0 * w * std::imag(std::conj(a) * b);
}
// One bond's slice of the unitary hop exp(-i theta (|a><b| + |b><a|)), i.e. the
// SAME convention as Stepper's e^{-iHt}: na = cos a - i sin b. (The earlier +i sin
// was the time-reversed e^{+iHt}, which put the Kerr term in the defocusing regime
// and disagreed with the rest of the substrate.)
inline void rotateBond(cd& a, cd& b, double theta) {
 const double c = std::cos(theta), s = std::sin(theta);
 const cd ia(0, -s);
 cd na = c * a + ia * b;
 cd nb = c * b + ia * a;
 a = na;
 b = nb;
}
// Phase-faithful version of the 2-site hop carrying the gauge flux phi. The bond
// Hamiltonian is H = w(e^{i phi}|a><b| + e^{-i phi}|b><a|); its exact unitary
// e^{-i theta H} is na = cos(theta) a - i sin(theta) e^{+i phi} b,
//                  nb = cos(theta) b - i sin(theta) e^{-i phi} a.
// At phi == 0 this is identical to rotateBond, so phaseless callers are unchanged.
inline void phaseRotateBond(cd& a, cd& b, double theta, double phi) {
 const double c = std::cos(theta), s = std::sin(theta);
 const cd ep = std::exp(cd(0, phi));   // e^{+i phi}
 const cd em = std::conj(ep);          // e^{-i phi}
 const cd msi(0, -s);                  // -i sin(theta)
 cd na = c * a + msi * ep * b;
 cd nb = c * b + msi * em * a;
 a = na;
 b = nb;
}
// DNLS Hamiltonian energy on a bond list: E = -sum_bonds 2 w Re(conj a b) - (g/2) sum |psi|^4.
// The split-step flow is symplectic, so E stays bounded (no secular drift) while the wave
// self-focuses -- this is the "energy pressure" of the collapse made measurable and shown
// lossless, rather than an ad-hoc density slowdown grafted onto the hop.
inline double kerrEnergy(const Vec& psi, const std::vector<SparseBond>& bonds, double g) {
 double hop = 0.0, quartic = 0.0;
 for (const auto& e : bonds) hop += e.w * std::real(std::conj(psi[e.a]) * psi[e.b]);
 for (const auto& v : psi) { double p = std::norm(v); quartic += p * p; }
 return -2.0 * hop - 0.5 * g * quartic;
}
inline Vec edgeLocalKerrFlow(Vec psi, const std::vector<SparseBond>& bonds,
                             double dt, double g, int steps,
                             LocalFlowStats* stats = nullptr) {
 double max_w = 0.0;
 for (const auto& e : bonds) max_w = std::max(max_w, std::abs(e.w));
 const double inv = max_w > 1e-300 ? 1.0 / max_w : 0.0;
 for (int s = 0; s < steps; ++s) {
  for (const auto& e : bonds) {
   const double w = e.w * inv;
   if (stats) {
    const double j = bondCurrent(psi[e.a], psi[e.b], w);
    const double rho = std::norm(psi[e.a]) + std::norm(psi[e.b]) + 1e-300;
    stats->current_abs += std::abs(j);
    stats->max_bond_speed = std::max(stats->max_bond_speed, std::abs(j) / rho);
    stats->bond_visits++;
   }
   phaseRotateBond(psi[e.a], psi[e.b], 0.5 * dt * w, e.phase);
  }
  if (g != 0.0) {
   for (auto& v : psi) v *= std::exp(cd(0, -g * std::norm(v) * dt));
  }
  for (const auto& e : bonds) {
   const double w = e.w * inv;
   if (stats) {
    const double j = bondCurrent(psi[e.a], psi[e.b], w);
    const double rho = std::norm(psi[e.a]) + std::norm(psi[e.b]) + 1e-300;
    stats->current_abs += std::abs(j);
    stats->max_bond_speed = std::max(stats->max_bond_speed, std::abs(j) / rho);
    stats->bond_visits++;
   }
   phaseRotateBond(psi[e.a], psi[e.b], 0.5 * dt * w, e.phase);
  }
 }
 return psi;
}

// ---- orthonormal-row kernel construction (fan-in / message materialization)
inline cd rinner(const Vec& a, const Vec& b) { cd s(0, 0); for (size_t i = 0; i < a.size(); ++i) s += a[i] * std::conj(b[i]); return s; }
inline bool addOrthRow(CMat& rows, Vec c) { for (const auto& b : rows) { cd co = rinner(c, b); for (size_t i = 0; i < c.size(); ++i) c[i] -= co * b[i]; }
 double n = std::sqrt(std::max(0.0, rinner(c, c).real())); if (n < 1e-10) return false; for (auto& v : c) v /= n; rows.push_back(c); return true; }
// complete the meaningful rows to a full orthonormal basis, return the unitary
// whose row r reads <x|row_r>: A[r][c] = conj(row_r[c]).
inline CMat unitaryFromRows(std::vector<Vec> meaningful, int dim) { CMat rows = meaningful;
 for (int i = 0; i < dim && (int)rows.size() < dim; i++) { Vec c(dim, cd(0, 0)); c[i] = cd(1, 0); addOrthRow(rows, c); }
 CMat A(dim, Vec(dim)); for (int r = 0; r < dim; r++) for (int c = 0; c < dim; c++) A[r][c] = std::conj(rows[r][c]); return A; }

// ---- real-symmetric eigensolver (complex Hermitian via the 2N real embedding)
inline RMat realRep(const CMat& h) { int n = (int)h.size(); RMat r(2 * n, std::vector<double>(2 * n, 0.0));
 for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { double a = h[i][j].real(), b = h[i][j].imag(); r[i][j] = a; r[i][j + n] = -b; r[i + n][j] = b; r[i + n][j + n] = a; } return r; }
struct Eig { std::vector<double> ev; RMat V; };
inline Eig jacobiV(RMat a) { int n = (int)a.size(); RMat V(n, std::vector<double>(n, 0.0)); for (int i = 0; i < n; i++) V[i][i] = 1.0;
 for (int sw = 0; sw < 200; sw++) { double off = 0; for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) off += a[p][q] * a[p][q]; if (off < 1e-18) break;
 for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) { double apq = a[p][q]; if (std::abs(apq) < 1e-300) continue; double app = a[p][p], aqq = a[q][q];
 double tau = (aqq - app) / (2 * apq); double t = tau >= 0 ? 1.0 / (tau + std::sqrt(1 + tau * tau)) : -1.0 / (-tau + std::sqrt(1 + tau * tau)); double c = 1.0 / std::sqrt(1 + t * t), s = t * c;
 for (int k = 0; k < n; k++) { if (k == p || k == q) continue; double akp = a[k][p], akq = a[k][q]; a[k][p] = a[p][k] = c * akp - s * akq; a[k][q] = a[q][k] = s * akp + c * akq; }
 a[p][p] = c * c * app - 2 * s * c * apq + s * s * aqq; a[q][q] = s * s * app + 2 * s * c * apq + c * c * aqq; a[p][q] = a[q][p] = 0;
 for (int k = 0; k < n; k++) { double vkp = V[k][p], vkq = V[k][q]; V[k][p] = c * vkp - s * vkq; V[k][q] = s * vkp + c * vkq; } } }
 std::vector<double> ev(n); for (int i = 0; i < n; i++) ev[i] = a[i][i]; return { ev, V }; }
// eigenvalues only (sorted ascending), same Jacobi core
inline std::vector<double> eigvals(const CMat& h) { Eig e = jacobiV(realRep(h)); std::sort(e.ev.begin(), e.ev.end()); return e.ev; }
} // namespace gw
