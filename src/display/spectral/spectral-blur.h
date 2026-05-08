// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: lattice-Laplacian heat kernel as the
 * single-source-of-truth (SSoT) operator behind every spectral
 * effect (blur, RGBA blur, future bilateral / SDF / drop shadow).
 *
 * The operator: lift uint8 → double, pad to next-pow-2, DCT-II →
 * multiply by per-mode transfer exp(-(σ²/2) · λ_{k,l}) → DCT-III,
 * crop and clamp back to uint8.
 *
 * Naming and structure ported from `src/core/SkSpectralBlur.{h,cpp}`
 * of the Skia spectral-faithful branch. Math is byte-identical.
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_BLUR_H
#define INKSCAPE_DISPLAY_SPECTRAL_BLUR_H

#include <cstddef>
#include <cstdint>

namespace Inkscape::Spectral {

// Apply the lattice-Laplacian heat kernel e^{-tL} (with t = σ² / 2)
// to a single-channel uint8 buffer in place.
//
// The pad-to-pow2 introduces a small numerical drift vs the unpadded
// natural-grid Laplacian (Neumann reflections occur at the padded
// boundary, not the natural one). For callers using a 3σ halo around
// their content the difference is bounded — see the parity test in
// `testfiles/src/display/spectral_blur_parity_test.cpp` (port of the
// Skia branch's `SpectralBlurParityTest`).
void apply_heat_kernel_a8(int W, int H, std::uint8_t *buf,
                           double sigma_x, double sigma_y);

// 4-channel blur on packed BGRA pixels (Cairo's
// CAIRO_FORMAT_ARGB32 native layout on little-endian).
//
// `src` and `dst` must have at least `stride` bytes per row,
// `stride >= 4 * W`. `src == dst` is allowed (operates in place via
// internal channel planes).
//
// Channel-independent: each of the 4 channels is blurred separately
// using the same heat kernel. For *joint* similarity (bilateral)
// see `spectral-bilateral.h` (Tier 3).
void blur_bgra(int W, int H,
                const std::uint8_t *src, std::size_t src_stride,
                std::uint8_t *dst, std::size_t dst_stride,
                double sigma_x, double sigma_y);

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_BLUR_H
