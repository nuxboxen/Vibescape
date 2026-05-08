// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralDistance filter primitive renderer (Inkscape extension).
 *
 * Heat-kernel distance field via Varadhan's classical asymptotic:
 *
 *     d(x, S) ≈ σ · √(-2 · log u_normalized(x))
 *
 * Operates on the alpha channel of the input (treated as a binary
 * mask: A < 128 = outside, A >= 128 = inside). Outputs an
 * 8-bit-quantized distance mapped into the alpha channel of an
 * opaque grayscale RGBA result.
 *
 * SVG 1.1 has no native distance-field primitive; this is an
 * Inkscape extension. See doc/spectral/progress.md §0 / §5.2 and
 * src/display/spectral/spectral-distance-field.h.
 *
 * --- Removal note for upstream maintainers ---
 * Cleanup map identical in shape to nr-filter-spectral-noise.h.
 */

#ifndef SEEN_NR_FILTER_SPECTRAL_DISTANCE_H
#define SEEN_NR_FILTER_SPECTRAL_DISTANCE_H

#include "display/nr-filter-primitive.h"

namespace Inkscape::Filters {

enum SpectralDistanceMode
{
    SPECTRAL_DISTANCE_UNSIGNED, // 0 inside, distance outside
    SPECTRAL_DISTANCE_SIGNED,   // negative inside, positive outside
};

class FilterSpectralDistance : public FilterPrimitive
{
public:
    FilterSpectralDistance();
    ~FilterSpectralDistance() override;

    void render_cairo(FilterSlot &slot) const override;
    double complexity(Geom::Affine const &ctm) const override;
    bool uses_background() const override { return false; }

    void set_sigma(double s) { _sigma = s; }
    void set_mode(SpectralDistanceMode m) { _mode = m; }

    Glib::ustring name() const override { return Glib::ustring("Spectral Distance"); }

private:
    double _sigma = 3.0;
    SpectralDistanceMode _mode = SPECTRAL_DISTANCE_UNSIGNED;
};

} // namespace Inkscape::Filters

#endif // SEEN_NR_FILTER_SPECTRAL_DISTANCE_H
