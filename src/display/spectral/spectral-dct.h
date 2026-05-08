// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: lattice DCT-II / DCT-III (orthonormalized) for
 * the discrete Laplacian eigendecomposition on a finite grid with
 * Neumann (zero-flux) boundary conditions.
 *
 * The discrete 1D Laplacian on N points with Neumann BC has
 * eigenvectors
 *
 *     v_k[n] = α_k · cos(π · (n + 1/2) · k / N)         k, n in [0, N)
 *
 * with α_0 = √(1/N), α_{k>0} = √(2/N), and eigenvalues
 *
 *     λ_k = 2 · (1 − cos(π k / N))   = 4 · sin²(π k / (2N)).
 *
 * Forward DCT-II projects onto this basis; inverse DCT-III
 * reconstructs. Together they form an orthonormal pair
 * (DCT-III(DCT-II(x)) = x exactly, up to floating-point).
 *
 * The 2D Laplacian on a W×H grid with Neumann BC is separable; its
 * eigenvalues are λ_{k,l} = λ_k^x + λ_l^y, so the 2D DCT-II/III is the
 * row pass composed with the column pass.
 *
 * This is the substrate for the heat-kernel blur (pointwise multiply
 * of DCT coefficients by exp(-(σ²/2) · λ_{k,l})).
 *
 * Initial port from `src/core/SkLatticeDCT.{h,cpp}` of the Skia
 * spectral-faithful branch. The Skia version uses an FFT-accelerated
 * Makhoul DCT for O(N log N); this port starts with the simpler direct
 * O(N²) implementation. Tier 5 will reintroduce the FFT path when
 * bench numbers show it mattering.
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_DCT_H
#define INKSCAPE_DISPLAY_SPECTRAL_DCT_H

namespace Inkscape::Spectral {

// Forward DCT-II of length N. `in` and `out` may not alias.
//   out[k] = α_k · sum_n in[n] · cos(π · (n + 1/2) · k / N)
void dct2_1d(double const *in, double *out, int N);

// Inverse DCT-III of length N (mathematical inverse of dct2_1d).
//   out[n] = sum_k α_k · in[k] · cos(π · (n + 1/2) · k / N)
void dct3_1d(double const *in, double *out, int N);

// 2D forward DCT-II on a row-major (W × H) image.
// `in` and `out` may alias (internal scratch is used).
void dct2_2d(double const *in, double *out, int W, int H);

// 2D inverse DCT-III on a row-major (W × H) image.
// `in` and `out` may alias.
void dct3_2d(double const *in, double *out, int W, int H);

// Multiply every (k, l) DCT coefficient by exp(-(σ²/2) · λ_{k,l}),
// where λ_{k,l} = 2(2 - cos(π k / W) - cos(π l / H)) is the 2D
// lattice Laplacian eigenvalue with Neumann BC. Operates in place.
// Supports anisotropic σ.
void apply_lattice_heat_kernel(double *dct_coeffs, int W, int H, double sigma_x, double sigma_y);

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_DCT_H
