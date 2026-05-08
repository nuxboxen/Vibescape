// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralNoise> implementation (Inkscape extension).
 */

#include "spectral-noise.h"

#include <cstring>

#include "attributes.h"
#include "display/nr-filter-spectral-noise.h"
#include "object/filters/sp-filter-primitive.h"
#include "object/sp-object.h"
#include "util/numeric/converters.h"
#include "xml/node.h"

class SPDocument;
namespace Inkscape {
class DrawingItem;
namespace XML {
class Document;
}
} // namespace Inkscape

void SPFeSpectralNoise::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    SPFilterPrimitive::build(document, repr);
    readAttr(SPAttr::SPECTRAL_NOISE_PROFILE);
    readAttr(SPAttr::SEED);
}

namespace {
Inkscape::Spectral::NoiseProfile read_profile(char const *value)
{
    using P = Inkscape::Spectral::NoiseProfile;
    if (!value)
        return P::kPink;
    if (std::strcmp(value, "white") == 0)
        return P::kWhite;
    if (std::strcmp(value, "pink") == 0)
        return P::kPink;
    if (std::strcmp(value, "brown") == 0)
        return P::kBrown;
    if (std::strcmp(value, "blue") == 0)
        return P::kBlue;
    return P::kPink; // fall through default
}

char const *profile_name(Inkscape::Spectral::NoiseProfile p)
{
    using P = Inkscape::Spectral::NoiseProfile;
    switch (p) {
        case P::kWhite:
            return "white";
        case P::kPink:
            return "pink";
        case P::kBrown:
            return "brown";
        case P::kBlue:
            return "blue";
    }
    return "pink";
}
} // anonymous namespace

void SPFeSpectralNoise::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::SPECTRAL_NOISE_PROFILE: {
            auto const p = read_profile(value);
            if (p != profile) {
                profile = p;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::SEED: {
            std::uint32_t const s = value ? static_cast<std::uint32_t>(Inkscape::Util::read_number(value)) : 0u;
            if (s != seed) {
                seed = s;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        default:
            SPFilterPrimitive::set(key, value);
            break;
    }
}

Inkscape::XML::Node *SPFeSpectralNoise::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned flags)
{
    if (!repr) {
        repr = getRepr()->duplicate(doc);
    }
    SPFilterPrimitive::write(doc, repr, flags);
    // Generator: 'in' attribute is meaningless on this primitive.
    repr->removeAttribute("in");
    repr->setAttribute("spectralNoiseProfile", profile_name(profile));
    return repr;
}

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeSpectralNoise::build_renderer(Inkscape::DrawingItem *) const
{
    auto noise = std::make_unique<Inkscape::Filters::FilterSpectralNoise>();
    build_renderer_common(noise.get());
    noise->set_profile(profile);
    noise->set_seed(seed);
    return noise;
}
