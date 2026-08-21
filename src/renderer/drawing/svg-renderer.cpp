// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Render out an SVG into a raster surface with lots of options.
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "svg-renderer.h"

#include <2geom/affine.h>

#include "document.h"
#include "object/sp-item.h"
#include "object/sp-root.h"

#include "util/scope_exit.h"
#include "util/units.h"

#include "renderer/context-pattern.h"
#include "renderer/surface.h"
#include "renderer/drawing/drawing.h"

namespace Inkscape::Renderer {

double SvgRenderer::get_xscale() const
{
    return Inkscape::Util::Quantity::convert(_xdpi, "px", "in");
}
double SvgRenderer::get_yscale() const
{
    return Inkscape::Util::Quantity::convert(_ydpi, "px", "in");
}
Geom::OptRect SvgRenderer::get_area(Geom::OptRect const &default_bounds) const
{
    auto area = _area ? _area : default_bounds;
    return (!area || area->hasZeroArea()) ? Geom::OptRect() : area;
}
std::optional<Geom::IntPoint> SvgRenderer::get_dimensions(SPDocument *document) const
{
    return get_dimensions(*get_area(document->preferredBounds()));
}
Geom::IntPoint SvgRenderer::get_dimensions(Geom::Rect const &area) const
{
    return Geom::IntPoint(
        std::ceil(get_xscale() * area.width()),
        std::ceil(get_yscale() * area.height()));
}

std::shared_ptr<Surface> SvgRenderer::render(SPObject const *object) const
{
    // There are two ways to do this, one is to render just the object itself directly,
    // this involves a Drawing with only one DrawingItem, the object's invoke_show
    // The second involves drawing the whole stack but hiding all the other objects.
    // The flexibility of the first approach is that you can render things in defs without
    // needing to create a document first.

    // some other types of objects like gradients, would need actual rendering in some special
    // way, although their rendering output can just be done by widgets directly, patterns are
    // more interesting as they would like to form a new document too but this is very wasteful
    // and instead we only really need to paint the pattern directly onto the widget's target surface.
    if (auto item = cast<SPItem>(object)) {
        if (auto area = item->visualBounds()) {
            SvgRenderer factory = *this;
            if (!_area) {
                // If not set, document box is used
                factory.set_area(*area);
            }
            if (!_color_space) {
                // If not set, document color space is used
                factory.set_final_color_space(item->getColorSpace());
            }
            if (_items.empty()) {
                factory.set_item_limit({item});
            }
            return factory.render(item->document);
        }
    }
    return {};
}

std::shared_ptr<Surface> SvgRenderer::render(SPDocument *document) const
{
    auto area = get_area(document->preferredBounds());
    if (!area) {
        return {};
    }
    auto color_space = _color_space ? _color_space : document->getColorSpace();
    auto dimensions = get_dimensions(*area);
    auto surface = std::make_shared<Surface>(Geom::IntPoint(dimensions.x(), dimensions.y()), _device_scale, color_space);
    render(*surface, document);
    return surface;
}

void SvgRenderer::render(Surface &surface, SPDocument *document) const
{
    auto area = get_area(document->preferredBounds());
    Geom::Point origin = area->min();
    Geom::Affine affine = Geom::Translate(-origin) * Geom::Scale(get_xscale(), get_yscale());

    // Document
    document->ensureUpToDate();
    unsigned dkey = SPItem::display_key_new(1);

    // Drawing
    Drawing drawing; // New drawing for offscreen rendering.
    if (_code_build) {
        // Debugging and testing code output
        drawing.setCodeBuild();
    }
    drawing.setRoot(document->getRoot()->invoke_show(drawing, dkey, SP_ITEM_SHOW_DISPLAY));
    auto invoke_hide_guard = scope_exit([&] { document->getRoot()->invoke_hide(dkey); });
    drawing.root()->setTransform(affine);
    drawing.setExact(); // Maximum quality for blurs.

    if (_antialiasing_override) {
        drawing.setAntialiasingOverride(*_antialiasing_override);
    }

    // Hide all items we don't want, instead of showing only requested items,
    // because that would not work if the shown item references something in defs.
    if (!_items.empty()) {
        document->getRoot()->invoke_hide_except(dkey, _items);
    }

    auto final_area = Geom::IntRect({0, 0}, surface.dimensions());
    drawing.update(final_area);

    if (_is_opaque) {
        // Required by sp_asbitmap_render().
        for (auto item : _items) {
            if (item->get_arenaitem(dkey)) {
                item->get_arenaitem(dkey)->setOpacity(1.0);
            }
        }
    }

    // Rendering
    Context dc(surface);

    if (_background_color) {
        dc.paint(*_background_color);
    }

    if (_checkerboard_color) {
        auto color1 = *_checkerboard_color->converted(dc.getColorSpace());
        auto color2 = _checkerboard_color2 ? _checkerboard_color2->converted(dc.getColorSpace())
                                           : std::optional<Colors::Color>();
        Renderer::Pattern pattern = color2 ? CheckerboardPattern(color1, *color2, 3)
                                           : CheckerboardPattern(color1, 3);
        dc.paint(pattern, _device_scale);
    }

    // render items
    drawing.render(dc, final_area, DrawingItem::RENDER_BYPASS_CACHE);
}

} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
