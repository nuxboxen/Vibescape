// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: power-spectrum-controlled synthetic noise.
 *
 * Where Perlin/Simplex/feTurbulence produce noise procedurally with
 * whatever spectrum the algorithm happens to give, this primitive
 * lets the caller specify the power spectrum P(λ) directly. Random
 * DCT coefficients are drawn with E[|ĉ_{k,l}|²] = P(λ_{k,l}),
 * inverse-DCT'd into a tile of grayscale noise, and normalized to
 * uint8.
 *
 * Reproducibility: a given (W, H, profile, seed) produces a
 * byte-identical tile across runs.
 *
 * Ported from `src/core/SkSpectralNoise.{h,cpp}` of the Skia
 * spectral-faithful branch. The shader factory variant
 * (`SkShaders::SpectralNoise`) is not ported here — that's tied to
 * Skia's image-shader machinery; Inkscape's equivalent happens at
 * the SVG-filter primitive layer (Tier 3.4 — separate commit).
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_NOISE_H
#define INKSCAPE_DISPLAY_SPECTRAL_NOISE_H

#include <cstdint>

namespace Inkscape::Spectral {

enum class NoiseProfile {
    kWhite,   ///< P(λ) = 1.       Flat spectrum; high-frequency salt-and-pepper.
    kPink,    ///< P(λ) = 1 / √λ.  Canonical 1/f, "natural" look.
    kBrown,   ///< P(λ) = 1 / λ.   Very smooth, blob-like.
    kBlue,    ///< P(λ) = √λ.      High-frequency emphasis; useful for dithering.
};

// Generate a (W × H) A8 noise tile into caller-supplied storage `out`.
// `out` must have at least W*H bytes.
void noise_generate_a8(int W, int H,
                        NoiseProfile profile,
                        std::uint32_t seed,
                        std::uint8_t *out);

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_NOISE_H
