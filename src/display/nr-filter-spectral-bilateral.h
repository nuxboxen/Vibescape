// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralBilateral filter primitive renderer (Inkscape extension).
 *
 * Edge-preserving smoothing — Perona-Malik anisotropic diffusion on
 * the image lattice. Runs N forward-Euler passes (N = ⌈2·σ²⌉) with
 * state-dependent edge weights W_ij = exp(-‖p_i - p_j‖² / (2σ_range²))
 * that collapse across colour discontinuities, preserving edges
 * while smoothing interiors.
 *
 * SVG 1.1 has no native bilateral filter; this is an Inkscape
 * extension. See SPECTRAL_PROGRESS.md §0 / §5.2 and
 * src/display/spectral/spectral-bilateral.h.
 *
 * --- Removal note for upstream maintainers ---
 * Cleanup map identical in shape to nr-filter-spectral-noise.h:
 * delete this {h,cpp} pair, the matching object/filters/spectral-bilateral
 * {h,cpp}, the NR_FILTER_SPECTRAL_BILATERAL enum entry, and the
 * sp-factory / tags / filter-enums references. Substrate
 * (Inkscape::Spectral::bilateral_*) lives in src/display/spectral/
 * and stays available regardless.
 */

#ifndef SEEN_NR_FILTER_SPECTRAL_BILATERAL_H
#define SEEN_NR_FILTER_SPECTRAL_BILATERAL_H

#include "display/nr-filter-primitive.h"

namespace Inkscape::Filters {

class FilterSpectralBilateral : public FilterPrimitive
{
public:
    FilterSpectralBilateral();
    ~FilterSpectralBilateral() override;

    void render_cairo(FilterSlot &slot) const override;
    double complexity(Geom::Affine const &ctm) const override;
    bool uses_background() const override { return false; }

    void set_sigma_spatial(double s) { _sigma_spatial = s; }
    void set_sigma_range(double s)   { _sigma_range = s; }

    Glib::ustring name() const override { return Glib::ustring("Spectral Bilateral"); }

private:
    double _sigma_spatial = 4.0;
    double _sigma_range   = 16.0;
};

} // namespace Inkscape::Filters

#endif // SEEN_NR_FILTER_SPECTRAL_BILATERAL_H
