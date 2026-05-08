// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralNoise filter primitive renderer (Inkscape extension).
 *
 * SVG 1.1 ships feTurbulence (Perlin-style procedural noise);
 * feSpectralNoise is an Inkscape-specific extension that lets the
 * caller specify the *power spectrum* P(λ) of the noise directly:
 * white (flat), pink (1/√λ, "natural"), brown (1/λ, blob-like), or
 * blue (√λ, dither-friendly). Built on the spectral substrate
 * (DCT eigenmode framing) — see doc/spectral/progress.md §0 and
 * src/display/spectral/spectral-noise.h.
 *
 * --- Removal note for upstream maintainers ---
 * If carrying a non-standard SVG filter primitive isn't a fit,
 * deleting this file + nr-filter-spectral-noise.cpp + the
 * corresponding object/filters/spectral-noise.{h,cpp} +
 * NR_FILTER_SPECTRAL_NOISE entries in nr-filter-types.h /
 * filter-enums.cpp / sp-factory.cpp / tags.h is a clean removal.
 * The substrate (`Inkscape::Spectral::noise_generate_a8`) lives in
 * src/display/spectral/ and stays available for any internal
 * consumer regardless of the public-API decision.
 */

#ifndef SEEN_NR_FILTER_SPECTRAL_NOISE_H
#define SEEN_NR_FILTER_SPECTRAL_NOISE_H

#include "display/nr-filter-primitive.h"
#include "display/spectral/spectral-noise.h"

namespace Inkscape::Filters {

class FilterSpectralNoise : public FilterPrimitive
{
public:
    FilterSpectralNoise();
    ~FilterSpectralNoise() override;

    void render_cairo(FilterSlot &slot) const override;
    double complexity(Geom::Affine const &ctm) const override;
    bool uses_background() const override { return false; }

    void set_profile(Spectral::NoiseProfile p) { _profile = p; }
    void set_seed(std::uint32_t s) { _seed = s; }

    Glib::ustring name() const override { return Glib::ustring("Spectral Noise"); }

private:
    Spectral::NoiseProfile _profile = Spectral::NoiseProfile::kPink;
    std::uint32_t _seed = 0;
};

} // namespace Inkscape::Filters

#endif // SEEN_NR_FILTER_SPECTRAL_NOISE_H
