// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralDistance> implementation (Inkscape extension).
 */

#include "spectral-distance.h"

#include <cstring>

#include "attributes.h"
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

namespace {
Inkscape::Filters::SpectralDistanceMode read_mode(char const *value)
{
    using M = Inkscape::Filters::SpectralDistanceMode;
    if (!value)
        return M::SPECTRAL_DISTANCE_UNSIGNED;
    if (std::strcmp(value, "signed") == 0)
        return M::SPECTRAL_DISTANCE_SIGNED;
    if (std::strcmp(value, "unsigned") == 0)
        return M::SPECTRAL_DISTANCE_UNSIGNED;
    return M::SPECTRAL_DISTANCE_UNSIGNED;
}

char const *mode_name(Inkscape::Filters::SpectralDistanceMode m)
{
    using M = Inkscape::Filters::SpectralDistanceMode;
    return m == M::SPECTRAL_DISTANCE_SIGNED ? "signed" : "unsigned";
}
} // anonymous namespace

void SPFeSpectralDistance::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    SPFilterPrimitive::build(document, repr);
    readAttr(SPAttr::SPECTRAL_SIGMA_SPATIAL);
    readAttr(SPAttr::SPECTRAL_DISTANCE_MODE);
}

void SPFeSpectralDistance::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::SPECTRAL_SIGMA_SPATIAL: {
            double const v = value ? Inkscape::Util::read_number(value) : 3.0;
            if (v != sigma_spatial) {
                sigma_spatial = v;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::SPECTRAL_DISTANCE_MODE: {
            auto const m = read_mode(value);
            if (m != mode) {
                mode = m;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        default:
            SPFilterPrimitive::set(key, value);
            break;
    }
}

Inkscape::XML::Node *SPFeSpectralDistance::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr,
                                                 unsigned flags)
{
    if (!repr) {
        repr = getRepr()->duplicate(doc);
    }
    SPFilterPrimitive::write(doc, repr, flags);
    repr->setAttribute("spectralDistanceMode", mode_name(mode));
    return repr;
}

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeSpectralDistance::build_renderer(Inkscape::DrawingItem *) const
{
    auto sdf = std::make_unique<Inkscape::Filters::FilterSpectralDistance>();
    build_renderer_common(sdf.get());
    sdf->set_sigma(sigma_spatial);
    sdf->set_mode(mode);
    return sdf;
}
