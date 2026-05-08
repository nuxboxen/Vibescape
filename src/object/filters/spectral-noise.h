// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralNoise> filter primitive (Inkscape extension).
 *
 * Power-spectrum-controlled synthetic noise as a generator filter
 * primitive — see doc/spectral/progress.md §0 and Tier 3.4 for design
 * rationale. Wraps the substrate
 * `Inkscape::Spectral::noise_generate_a8`.
 *
 * Attributes:
 *   spectralNoiseProfile  one of "white" | "pink" | "brown" | "blue"
 *                         (default "pink")
 *   seed                  integer RNG seed (reuses SVG turbulence
 *                         attribute since the semantics match)
 *
 * --- Removal note for upstream maintainers ---
 * See nr-filter-spectral-noise.h for the cleanup map if Inkscape
 * decides not to carry this non-standard primitive.
 */

#ifndef SP_FESPECTRAL_NOISE_H_SEEN
#define SP_FESPECTRAL_NOISE_H_SEEN

#include "display/spectral/spectral-noise.h"
#include "sp-filter-primitive.h"

class SPFeSpectralNoise final : public SPFilterPrimitive
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

private:
    Inkscape::Spectral::NoiseProfile profile = Inkscape::Spectral::NoiseProfile::kPink;
    std::uint32_t seed = 0;

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned flags) override;

    std::unique_ptr<Inkscape::Filters::FilterPrimitive> build_renderer(Inkscape::DrawingItem *item) const override;
};

#endif // SP_FESPECTRAL_NOISE_H_SEEN
