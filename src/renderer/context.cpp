// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing context for Inkscape pixel surfaces.
 *//*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011-2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "context.h"
#include "context-paths.h"

#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"

#include "surface.h"
#include "context-pattern.h"
#include "context-pattern-complex.h"

#include "renderer/enums.h"

namespace Inkscape::Renderer {

Cairo::Matrix geom_to_cairo(const Geom::Affine &affine)
{
    return Cairo::Matrix(affine[0], affine[1], affine[2], affine[3], affine[4], affine[5]);
}

Cairo::RectangleInt geom_to_cairo(Geom::IntRect const &rect)
{
    return Cairo::RectangleInt(rect.left(), rect.top(), rect.width(), rect.height());
}

Geom::IntRect cairo_to_geom(const Cairo::RectangleInt &rect)
{
    return Geom::IntRect::from_xywh(rect.x, rect.y, rect.width, rect.height);
}

std::vector<Geom::IntRect> cairo_to_geom(const Cairo::Region &region)
{
    std::vector<Geom::IntRect> ret;
    for (int i = 0; i < region.get_num_rectangles(); i++) {
        ret.push_back(cairo_to_geom(region.get_rectangle(i)));
    }
    return ret;
}

Geom::Affine rect_to_matrix(Geom::OptRect const &bbox)
{
    return bbox ? Geom::Affine(bbox->width(), 0, 0, bbox->height(), bbox->left(), bbox->top()) : Geom::identity();
}

Geom::Affine viewbox_matrix(Geom::Affine const &m, Geom::OptRect const &rect)
{
    return m * rect_to_matrix(rect);
}


/**
 * Create a context with a saved state, restores automatically on destruction.
 *
 * The design idea here is that at each function call the context can be safely copied
 * into the function args without passing as a reference or pointer, it then is already
 * saved on call and restored when the function completes it's drawing operations.
 *
 * This mirrors the tree like structure of the painting operations themselves.
 */
Context::Context(Context const &parent)
    : _cts(parent._cts)
    , _origin(parent._origin)
    , _format(parent._format)
    , _device_scale(parent._device_scale)
    , _dimensions(parent._dimensions)
    , _surface_color_space(parent._surface_color_space)
    , _parent(&parent)
{
    save(); // See restore() in ~Context()
    if (parent._child) {
        throw Context::SaveRestoreError();
    }
    parent._child = this;
}

Context::Context(Surface &surface, Geom::IntPoint origin, Geom::Scale const &logical_scale)
    : _origin(origin)
    , _format(surface.format())
    , _device_scale(surface.getDeviceScale())
    , _dimensions(surface.dimensions())
    , _surface_color_space(surface.getColorSpace())
{
    for (auto cs : surface.getCairoSurfaces()) {
        _cts.emplace_back(Cairo::Context::create(cs));
        _cts.back()->save(); // See restore() in ~Context()
    }
    // Allow scale before origin translation
    if (logical_scale != Geom::identity()) {
        scale(logical_scale);
    }
    if (origin != Geom::IntPoint()) {
        translate(Geom::Translate(-origin));
    }
}

std::shared_ptr<Colors::Space::AnySpace> Context::getColorSpace() const
{
    // Surfaces can have an unset color-space which means "int32" surface
    // but a context must always have a color_space as it's used to convert
    static auto const srgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    return _surface_color_space ? _surface_color_space : srgb;
}

Context::Context(Cairo::RefPtr<Cairo::Context> ct)
    : _cts{std::move(ct)}
    , _format(cairo_image_surface_get_format(cairo_get_target(_cts[0]->cobj())))
{
    if (_format == CAIRO_FORMAT_RGBA128F) {
        _surface_color_space = Colors::Manager::get().find(Colors::Space::Type::RGB);
    }
    save(); // See restore() in ~Context()
}

Context::~Context()
{
    if (_child) {
        std::cout << "Renderer::Context race condition, destroying a parent before it's child. Out of order.\n";
    }
    if (_parent) {
        _parent->_child = nullptr;
    }
    restore();
    flush();
}

void Context::arc(Geom::Point const &center, double radius, Geom::AngleInterval const &angle)
{
    double from = angle.initialAngle();
    double to = angle.finalAngle();
    for (auto ct : _cts) {
        if (to > from) {
            ct->arc(center[Geom::X], center[Geom::Y], radius, from, to);
        } else {
            ct->arc_negative(center[Geom::X], center[Geom::Y], radius, to, from);
        }
    }
}

void Context::transform(Geom::Affine const &m) {
    transform(geom_to_cairo(m));
}

void Context::path(Geom::PathVector const &pv) {
    for (auto ct : _cts) {
        feed_pathvector_to_cairo(ct, pv);
    }
}

void Context::path(Geom::PathVector const &pv, Geom::Affine trans, Geom::OptRect area, bool optimize_stroke, double stroke_width) {
    for (auto ct : _cts) {
        feed_pathvector_to_cairo(ct, pv, trans, area, optimize_stroke, stroke_width);
    }
}

void Context::paint(double alpha) {
    for (auto ct : _cts) {
        if (alpha == 1.0) ct->paint();
        else ct->paint_with_alpha(alpha);
    }
}


void Context::paint(Colors::Color const &color)
{
    save();
    set_operator(Cairo::Context::Operator::SOURCE);
    setSource(color);
    paint();
    restore();
}

void Context::paint(Pattern const &pattern, double scale)
{
    save();
    transform(Geom::Scale(scale));
    set_operator(Cairo::Context::Operator::OVER);
    setSource(pattern);
    paint();
    restore();
}

void Context::paint(PatternComplex const &pc)
{
    // Painting is actually done by the complex
    pc.paint(*this);
}

void Context::mask(Surface const &surface)
{
    auto &cairo_surfaces = surface.getCairoSurfaces();
    for (unsigned i = 0; i < _cts.size(); i++) {
        _cts[i]->mask(cairo_surfaces[0], 0, 0);
    }
}

void Context::setHairline() {
    for (auto ct : _cts) {
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 17, 6)
        cairo_set_hairline(ct->cobj(), true);
#else       
        // As a backup, use a device unit of 1
        auto coord = device_to_user_distance({1, 1});
        ct->set_line_width(coord.length());
#endif  
    }
}

void Context::setLineCap(SPStrokeCapType cap)
{
    switch (cap) {
        case SP_STROKE_LINECAP_SQUARE:
            set_line_cap(Cairo::Context::LineCap::SQUARE);
            return;
        case SP_STROKE_LINECAP_ROUND:
            set_line_cap(Cairo::Context::LineCap::ROUND);
            return;
        case SP_STROKE_LINECAP_BUTT:
            set_line_cap(Cairo::Context::LineCap::BUTT);
            return;
    }
}
void Context::setLineJoin(SPStrokeJoinType join)
{
    switch (join) {
        case SP_STROKE_LINEJOIN_BEVEL:
            set_line_join(Cairo::Context::LineJoin::BEVEL);
            return;
        case SP_STROKE_LINEJOIN_ROUND:
            set_line_join(Cairo::Context::LineJoin::ROUND);
            return;
        case SP_STROKE_LINEJOIN_MITER:
            set_line_join(Cairo::Context::LineJoin::MITER);
            return;
    }
}

void Context::setOperator(SPBlendMode op)
{
    static auto get_op = [](SPBlendMode op)
    {
        // Must return cairo enums because cariomm is missing them.
        switch (op) {
        case SP_CSS_BLEND_MULTIPLY:
            return CAIRO_OPERATOR_MULTIPLY;
        case SP_CSS_BLEND_SCREEN:
            return CAIRO_OPERATOR_SCREEN;
        case SP_CSS_BLEND_DARKEN:
            return CAIRO_OPERATOR_DARKEN;
        case SP_CSS_BLEND_LIGHTEN:
            return CAIRO_OPERATOR_LIGHTEN;
        // New in CSS Compositing and Blending Level 1
        case SP_CSS_BLEND_OVERLAY:
            return CAIRO_OPERATOR_OVERLAY;
        case SP_CSS_BLEND_COLORDODGE:
            return CAIRO_OPERATOR_COLOR_DODGE;
        case SP_CSS_BLEND_COLORBURN:
            return CAIRO_OPERATOR_COLOR_BURN;
        case SP_CSS_BLEND_HARDLIGHT:
            return CAIRO_OPERATOR_HARD_LIGHT;
        case SP_CSS_BLEND_SOFTLIGHT:
            return CAIRO_OPERATOR_SOFT_LIGHT;
        case SP_CSS_BLEND_DIFFERENCE:
            return CAIRO_OPERATOR_DIFFERENCE;
        case SP_CSS_BLEND_EXCLUSION:
            return CAIRO_OPERATOR_EXCLUSION;
        case SP_CSS_BLEND_HUE:
            return CAIRO_OPERATOR_HSL_HUE;
        case SP_CSS_BLEND_SATURATION:
            return CAIRO_OPERATOR_HSL_SATURATION;
        case SP_CSS_BLEND_COLOR:
            return CAIRO_OPERATOR_HSL_COLOR;
        case SP_CSS_BLEND_LUMINOSITY:
            return CAIRO_OPERATOR_HSL_LUMINOSITY;
        case SP_CSS_BLEND_NORMAL:
        default:
            return CAIRO_OPERATOR_OVER;
        }
    };
    set_operator((Cairo::Context::Operator)get_op(op));
}

void Context::setFillRule(SPWindRule rule) {
    switch (rule) {
        case SP_WIND_RULE_EVENODD:
            set_fill_rule(Cairo::Context::FillRule::EVEN_ODD);
            return;
        case SP_WIND_RULE_NONZERO:
            set_fill_rule(Cairo::Context::FillRule::WINDING);
            return;
        case SP_WIND_RULE_POSITIVE: // Missing in cairo
            set_fill_rule(Cairo::Context::FillRule::WINDING);
            return;
        case SP_WIND_RULE_INTERSECT: // Missing in cairo
            set_fill_rule(Cairo::Context::FillRule::WINDING);
            return;
    }
}

void Context::setSource(Colors::Color const &color) {
    bool has_alpha = color.hasOpacity();
    auto c = color.converted(getColorSpace())->getValues();
    auto a = 0.0;
    if (has_alpha) {
        // Remove the alpha so it's not copied into other channels
        std::swap(a, c.back());
    }
    c.resize(_cts.size() * 3);
    int i = 0;
    for (auto ct : _cts) {
        auto r = c[i++];
        auto g = c[i++];
        auto b = c[i++];
        if (has_alpha) {
            ct->set_source_rgba(r, g, b, a);
        } else {
            ct->set_source_rgb(r, g, b);
        }
    }
}

void Context::setSource(Surface const &surface, double x, double y,
                        std::optional<Cairo::SurfacePattern::Filter> filter,
                        std::optional<Cairo::Pattern::Extend> extend)
{
    auto &cairo_surfaces = surface.getCairoSurfaces();

    // Invalid surface format types come from GdkSnapshot and we don't know their
    // format mixing rules so let's just LFDI....
    if (_format != CAIRO_FORMAT_INVALID) {
        // We're going to forbid data mixing in this layer; see PixelFilters instead.
        surface.sanityCheckSurface(getSurfaceColorSpace());
    }

    for (unsigned i = 0; i < _cts.size(); i++) {
        _cts[i]->set_source(cairo_surfaces[i], x, y);
        // Cairo converts Surfaces to SurfacePatterns internally.
        auto pattern = _cts[i]->get_source_for_surface();
        if (filter) {
            pattern->set_filter(*filter);
        }
        if (extend) {
            pattern->set_extend(*extend);
        }
    }
}

void Context::setSource(Pattern const &pattern)
{
    auto &cairo_patterns = pattern.getCairoPatterns();
    if (!Surface::sanityCheckColorSpace(pattern.getColorSpace(), getColorSpace())) {
        return; // skip painting.
    }

    for (unsigned i = 0; i < _cts.size(); i++) {
        _cts[i]->set_source(cairo_patterns[i]);
    }
}

void Context::setAntialiasing(Antialiasing antialias)
{
    for (auto &ct : _cts) {
        switch (antialias) {
            case Antialiasing::None:
                ct->set_antialias(Cairo::Antialias::ANTIALIAS_NONE);
                break;
            case Antialiasing::Fast:
                // Unavailable in Cairomm 1.16
                ct->set_antialias((Cairo::Antialias)CAIRO_ANTIALIAS_FAST);
                break;
            case Antialiasing::Good:
                ct->set_antialias((Cairo::Antialias)CAIRO_ANTIALIAS_GOOD);
                break;
            case Antialiasing::Best:
                ct->set_antialias((Cairo::Antialias)CAIRO_ANTIALIAS_BEST);
            break;
            default:
                g_assert_not_reached();
        }
    }
}

/**
 * Uses pango to write text, this is not used by SVG rendering, but by some canvas rendering
 */
void Context::paintLayout(Glib::RefPtr<Pango::Layout> layout)
{
    for (auto ctx : _cts) {
        layout->show_in_cairo_context(ctx);
    }
}

void Context::paintText(std::string const &font_desc, std::string const &text)
{
    // Call Pango to render text with fallback fonts
    auto desc = Pango::FontDescription(font_desc);
    for (auto ctx : _cts) {
        auto layout = Pango::Layout::create(ctx);
        layout->set_font_description(desc);
        layout->set_text(text);
        layout->show_in_cairo_context(ctx);
    }
}

void Context::rectangles(Cairo::RefPtr<Cairo::Region> const &region)
{
    for (int i = 0; i < region->get_num_rectangles(); i++) {
        rectangle(region->get_rectangle(i));
    }
}

} // end namespace Inkscape

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
