// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <feSpectralBilateral> implementation (Inkscape extension).
 */

#include "spectral-bilateral.h"

#include "attributes.h"
#include "display/nr-filter-spectral-bilateral.h"
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

void SPFeSpectralBilateral::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    SPFilterPrimitive::build(document, repr);
    readAttr(SPAttr::SPECTRAL_SIGMA_SPATIAL);
    readAttr(SPAttr::SPECTRAL_SIGMA_RANGE);
}

void SPFeSpectralBilateral::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::SPECTRAL_SIGMA_SPATIAL: {
            double const v = value ? Inkscape::Util::read_number(value) : 4.0;
            if (v != sigma_spatial) {
                sigma_spatial = v;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::SPECTRAL_SIGMA_RANGE: {
            double const v = value ? Inkscape::Util::read_number(value) : 16.0;
            if (v != sigma_range) {
                sigma_range = v;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        default:
            SPFilterPrimitive::set(key, value);
            break;
    }
}

Inkscape::XML::Node *SPFeSpectralBilateral::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr,
                                                  unsigned flags)
{
    if (!repr) {
        repr = getRepr()->duplicate(doc);
    }
    SPFilterPrimitive::write(doc, repr, flags);
    return repr;
}

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeSpectralBilateral::build_renderer(Inkscape::DrawingItem *) const
{
    auto bilateral = std::make_unique<Inkscape::Filters::FilterSpectralBilateral>();
    build_renderer_common(bilateral.get());
    bilateral->set_sigma_spatial(sigma_spatial);
    bilateral->set_sigma_range(sigma_range);
    return bilateral;
}
