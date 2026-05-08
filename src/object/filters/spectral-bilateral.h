// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralBilateral> filter primitive (Inkscape extension).
 *
 * Edge-preserving smoothing via Perona-Malik anisotropic diffusion.
 * See SPECTRAL_PROGRESS.md §0 / §5.2 and substrate header
 * src/display/spectral/spectral-bilateral.h for the operator.
 *
 * Attributes:
 *   spectralSigmaSpatial  blur radius (analogous to Gaussian σ);
 *                         pass count = ⌈2σ²⌉. Default 4. Must be > 0.
 *   spectralSigmaRange    edge sensitivity in 0..255 alpha units.
 *                         Smaller → sharper edges preserved; larger →
 *                         closer to plain Gaussian. Default 16.
 *                         Must be > 0.
 *   in                    standard SVG filter primitive input.
 *
 * --- Removal note for upstream maintainers ---
 * See nr-filter-spectral-bilateral.h for the cleanup map.
 */

#ifndef SP_FESPECTRAL_BILATERAL_H_SEEN
#define SP_FESPECTRAL_BILATERAL_H_SEEN

#include "sp-filter-primitive.h"

class SPFeSpectralBilateral final : public SPFilterPrimitive
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

private:
    double sigma_spatial = 4.0;
    double sigma_range   = 16.0;

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned flags) override;

    std::unique_ptr<Inkscape::Filters::FilterPrimitive> build_renderer(Inkscape::DrawingItem *item) const override;
};

#endif // SP_FESPECTRAL_BILATERAL_H_SEEN
