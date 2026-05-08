// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: heat-kernel (Varadhan) distance field.
 *
 * For a binary indicator function χ_S, the heat-equation evolution
 * u(x, t) = (e^{-tL} χ_S)(x) carries geometric distance information.
 * Varadhan's classical asymptotic:
 *
 *     lim_{t → 0}  -4t · log u(x, t)  =  d²(x, S)
 *
 * In our convention σ² = 2t, so
 *
 *     d(x, S)  ≈  σ · √(-2 · log u_normalized(x))
 *
 * Implementation: re-use the SSoT apply_heat_kernel_a8 for diffusion,
 * then evaluate Varadhan per pixel. No new mathematical primitives —
 * the primary object remains the lattice-Laplacian heat kernel.
 *
 * The smaller σ (and therefore t), the more accurate Varadhan's
 * approximation; but smaller σ also means the diffused signal
 * collapses to ≈0 farther from the boundary (taking log fails at the
 * uint8 quantization floor). σ ≈ 1–4 pixels is the practical sweet
 * spot — accurate within ~3σ of the boundary.
 *
 * Ported from `src/core/SkSpectralDistanceField.{h,cpp}` of the
 * Skia spectral-faithful branch.
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_DISTANCE_FIELD_H
#define INKSCAPE_DISPLAY_SPECTRAL_DISTANCE_FIELD_H

#include <cstdint>

namespace Inkscape::Spectral {

// Pixels deeper than ~3σ from any foreground content underflow the
// uint8 quantization floor; their distance estimate is meaningless
// and gets clamped to this sentinel rather than +inf.
constexpr float kDistanceFieldFar = 1.0e9f;

// Unsigned distance from each pixel to the nearest "inside"
// (mask >= 128) pixel.
//   binary_mask:    W × H uint8 (0 = outside, 255 = inside; partial
//                   coverage in between is handled linearly).
//   out_distance:   W × H float; on return out_distance[y*W + x] is
//                   the approximate Euclidean distance in pixel units.
void distance_field_a8(int W, int H, std::uint8_t const *binary_mask, float *out_distance, double sigma_spatial);

// Signed variant: negative inside, positive outside, zero on the
// boundary (the standard SDF convention; mask >= 128 ↔ inside).
void signed_distance_field_a8(int W, int H, std::uint8_t const *binary_mask, float *out_distance, double sigma_spatial);

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_DISTANCE_FIELD_H
