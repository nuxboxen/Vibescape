// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: bilateral / state-dependent diffusion.
 *
 * Operator (Perona-Malik 1990, evaluated as forward Euler on the
 * 4-neighbour lattice):
 *
 *   du_i/dt = ∑_j W_ij · (u_j - u_i)
 *
 * with state-dependent edge weight
 *
 *   W_ij = exp(-(u_i - u_j)² / (2 σ_range²))    (single channel)
 *
 *   W_ij = exp(-||p_i - p_j||² / (2 σ_range²))  (RGBA, joint)
 *
 * In flat regions (small Δu) the operator reduces to the linear heat
 * equation and behaves like a Gaussian blur of σ = σ_spatial. At
 * edges (large Δu) the weight collapses toward zero and diffusion
 * stalls, preserving the edge.
 *
 * Δt = 1/4 (max-stable for the 4-neighbour stencil; tighter weights
 * only help). Total integration time t = N·Δt = σ_spatial²/2 matches
 * the heat-kernel / Gaussian correspondence in the linear limit.
 *
 * Inkscape doesn't ship a bilateral filter primitive today; this is
 * the substrate for a new `feSpectralBilateral` SVG extension.
 *
 * Ported from `src/core/SkSpectralBilateral.{h,cpp}` of the Skia
 * spectral-faithful branch with these adjustments:
 *  - Raw uint8 buffer signatures (no SkMask).
 *  - Neumann BC (missing neighbour = self) for both single-channel
 *    and RGBA variants — image-filter natural BC.
 *  - BIP variant from Skia not ported (recorded `[-]` for structural
 *    cross-channel cancellation; see Skia branch
 *    Skia branch's progress notebook §11.2 retry analysis).
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_BILATERAL_H
#define INKSCAPE_DISPLAY_SPECTRAL_BILATERAL_H

#include <cstddef>
#include <cstdint>

namespace Inkscape::Spectral {

// Single-channel bilateral on a packed uint8 buffer.
//
//   sigma_spatial: blur radius (controls pass count: N = ceil(2 σ²)).
//                  Must be > 0.
//   sigma_range:   edge sensitivity in 0..255 alpha units (typical
//                  8..64). Smaller → sharper edges preserved; larger
//                  → closer to a plain Gaussian. Must be > 0.
//
// `src == dst` is allowed (operates via internal scratch).
void bilateral_a8(int W, int H,
                   const std::uint8_t *src,
                   std::uint8_t *dst,
                   double sigma_spatial,
                   double sigma_range);

// 4-channel bilateral on packed BGRA pixels (Cairo native ARGB32).
// Joint similarity across all four channels: W_ij = exp(-Σ_c
// (c_i - c_j)² / (2σ_range²)). The same scalar weight diffuses every
// channel.
//
// `src == dst` is allowed.
//
// Pixel layout assumes 4-byte-per-pixel packed format. Premultiplied
// alpha: callers are responsible for unpremultiplying before and
// re-premultiplying after if their content uses premultiplied alpha;
// the operator treats the alpha channel like any other channel.
void bilateral_bgra(int W, int H,
                     const std::uint8_t *src, std::size_t src_stride,
                     std::uint8_t *dst,       std::size_t dst_stride,
                     double sigma_spatial,
                     double sigma_range);

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_BILATERAL_H
