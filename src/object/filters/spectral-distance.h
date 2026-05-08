// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralDistance> filter primitive (Inkscape extension).
 *
 * Heat-kernel distance field. See doc/spectral/progress.md §0 / §5.2 and
 * src/display/spectral/spectral-distance-field.h for the operator.
 *
 * Attributes:
 *   spectralSigmaSpatial  diffusion length scale in pixels. Smaller →
 *                         tighter near-boundary accuracy but smaller
 *                         meaningful range. Default 3.
 *   spectralDistanceMode  "signed" | "unsigned" (default "unsigned")
 *   in                    standard SVG filter primitive input
 *                         (alpha channel used as the binary mask;
 *                         A < 128 = outside, A >= 128 = inside).
 *
 * --- Removal note for upstream maintainers ---
 * See nr-filter-spectral-distance.h for the cleanup map.
 */

#ifndef SP_FESPECTRAL_DISTANCE_H_SEEN
#define SP_FESPECTRAL_DISTANCE_H_SEEN

#include "display/nr-filter-spectral-distance.h"
#include "sp-filter-primitive.h"

class SPFeSpectralDistance final : public SPFilterPrimitive
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

private:
    double sigma_spatial = 3.0;
    Inkscape::Filters::SpectralDistanceMode mode =
        Inkscape::Filters::SPECTRAL_DISTANCE_UNSIGNED;

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned flags) override;

    std::unique_ptr<Inkscape::Filters::FilterPrimitive> build_renderer(Inkscape::DrawingItem *item) const override;
};

#endif // SP_FESPECTRAL_DISTANCE_H_SEEN
