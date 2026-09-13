// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing patterns in cairo
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/affine.h>
#include <2geom/bezier-curve.h>

#include "context-pattern.h"
#include "colors/color.h"
#include "colors/spaces/base.h"

#include "surface.h"
#include "context.h"

namespace Inkscape::Renderer {

CubicBezierEasingSteps::CubicBezierEasingSteps(Geom::Point P1, Geom::Point P2, unsigned steps)
{
    auto curve = Geom::CubicBezier({0,0}, P1, P2, {1,1});
    reserve(steps);
    for (unsigned t = 1; t <= steps; t++) {
        emplace_back(curve.roots((double)t / steps, Geom::X)[0]);
    }
}

Pattern::Pattern(Surface const &surface)
    : _color_space(surface.getColorSpace())
{
    setSurface(surface);
}

void Pattern::setSurface(Surface const &surface)
{
    surface.sanityCheckSurface(_color_space);

    for (auto &s : surface.getCairoSurfaces()) {
        _pts.emplace_back(Cairo::SurfacePattern::create(s));
    }
}

void Pattern::setFilter(Cairo::SurfacePattern::Filter filter)
{
    for (auto &pt : _pts) {
        if (auto sp = dynamic_cast<Cairo::SurfacePattern *>(&*pt)) {
            sp->set_filter(filter);
        }
    }
}

void Pattern::setExtend(Cairo::Pattern::Extend extend)
{
    for (auto &pt : _pts) {
        pt->set_extend(extend);
    }
}

void Pattern::setExtend(SPGradientSpread spread)
{
    switch (spread) {
        case SP_GRADIENT_SPREAD_REFLECT:
            setExtend(Cairo::Pattern::Extend::REFLECT);
            break;
        case SP_GRADIENT_SPREAD_REPEAT:
            setExtend(Cairo::Pattern::Extend::REPEAT);
            break;
        case SP_GRADIENT_SPREAD_PAD:
        default:
            setExtend(Cairo::Pattern::Extend::PAD);
            break;
    }
}

void Pattern::setMatrix(Geom::Affine const &m, Geom::OptRect const &rect)
{
    for (auto &pt : _pts) {
        pt->set_matrix(geom_to_cairo(rect ? viewbox_matrix(m, rect).inverse() : m.inverse()));
    }
}

void Pattern::setDither(bool enabled)
{
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 18, 0)
    for (auto &pt : _pts) {
        cairo_pattern_set_dither(pt->cobj(), enabled ? CAIRO_DITHER_BEST : CAIRO_DITHER_NONE);
    }
#endif
}

void Pattern::addColorStop(double offset, Colors::Color color)
{
    color.convert(_color_space);
    auto c = color.getValues();
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0, j = 0; i < _pts.size(); i++, j+=3) {
        cairo_pattern_add_color_stop_rgba(_pts[i]->cobj(), offset, c[j], c[j+1], c[j+2], color.getOpacity());
    }
}

void Pattern::addColorStop(double offset, Colors::Color const &color_b, std::vector<double> const &ease)
{
    if (numColorStops() == 0) {
        throw FirstColorStopInEasingError();
    }
    auto [pos, color_a] = getColorStop(numColorStops()-1);
    assert(offset > pos);
    double advance = (offset - pos) / ease.size();
    // Start at 1 because easing's first entry is always the existing stop
    for (auto v : ease) {
        pos += advance; // advance to prevent duplicate first pos 
        addColorStop(pos, color_a.averaged(color_b, v));
    }
}


void Pattern::copyColorStops(Pattern const &other)
{
    assert(other._color_space == _color_space);
    assert(numColorStops() == 0);
    int count = other.numColorStops();
    double pos, r, g, b, a;
    // Copy all stops from one gradient to another
    for (unsigned i = 0; i < _pts.size(); i++) {
        for (int s = 0; s < count; s++) {
            cairo_pattern_get_color_stop_rgba(other._pts[i]->cobj(), s, &pos, &r, &g, &b, &a);
            cairo_pattern_add_color_stop_rgba(_pts[i]->cobj(), pos, r, g, b, a);
        }
    }
}

std::pair<double, Colors::Color> Pattern::getColorStop(int stop) const
{
    Colors::Color c = {_color_space};
    unsigned cc = _color_space->getComponentCount();
    double offset = 0.0;
    double r, g, b, a;
    for (unsigned i = 0, j = 0; i < _pts.size(); i++, j+=3) {
        cairo_pattern_get_color_stop_rgba(_pts[i]->cobj(), stop, &offset, &r, &g, &b, &a);
        if (j+0 < cc) c.set(j+0, r);
        if (j+1 < cc) c.set(j+1, g);
        if (j+2 < cc) c.set(j+2, b);
    }
    c.setOpacity(a);
    return {offset, std::move(c)};
}

int Pattern::numColorStops() const
{
    int count = 0;
    if (!_pts.empty()) {
        cairo_pattern_get_color_stop_count(_pts[0]->cobj(), &count);
    }
    return count;
}

SolidColorPattern::SolidColorPattern(Colors::Color solid_color)
    : Pattern(solid_color.getSpace())
{
    auto a = 0.0;
    bool has_a = solid_color.hasOpacity();
    auto c = solid_color.getValues();

    if (has_a) {
        // Steal alpha
        std::swap(a, c.back());
    }
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0; i < solid_color.size() - has_a; i += 3) {
        if (has_a) {
            _pts.emplace_back(Cairo::SolidPattern::create_rgba(c[i], c[i+1], c[i+2], a));
        } else {
            _pts.emplace_back(Cairo::SolidPattern::create_rgb(c[i], c[i+1], c[i+2]));
        }
    }
}

LinearGradientPattern::LinearGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double x0, double y0, double x1, double y1)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        _pts.emplace_back(Cairo::LinearGradient::create(x0, y0, x1, y1));
    }
}

RadialGradientPattern::RadialGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double cx0, double cy0, double cr0, double cx1, double cy1, double cr1)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        _pts.emplace_back(Cairo::RadialGradient::create(cx0, cy0, cr0, cx1, cy1, cr1));
    }
}

MeshGradientPattern::MeshGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        // UPSTREAM BUG: C++ API is unavailable in cairomm 1.16
        _pts.emplace_back(new Cairo::Pattern(cairo_pattern_create_mesh()));
    }
}

MeshGradientPattern::~MeshGradientPattern()
{
#if CAIROMM_VERSION <= CAIRO_VERSION_ENCODE(1, 16, 0)
    // UPSTREAM BUG: cairo_pattern_create_mesh pattern isn't destroyed by cairomm (leak)
    for (auto &pt : _pts) {
        cairo_pattern_destroy(pt->cobj());
    }
#endif
}

void MeshGradientPattern::beginPatch()
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_begin_patch(pt->cobj());
    }
}

void MeshGradientPattern::endPatch()
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_end_patch(pt->cobj());
    }
}

void MeshGradientPattern::moveTo(Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_move_to(pt->cobj(), p.x(), p.y());
    }
}

void MeshGradientPattern::lineTo(Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_line_to(pt->cobj(), p.x(), p.y());
    }
}

void MeshGradientPattern::curveTo(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_curve_to(pt->cobj(), p0.x(), p0.y(), p1.x(), p1.y(), p2.x(), p2.y());
    }
}

void MeshGradientPattern::setControlPoint(int point_num, Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_set_control_point(pt->cobj(), point_num, p.x(), p.y());
    }
}

void MeshGradientPattern::setCornerColor(int corner, Colors::Color color)
{
    color.convert(_color_space);
    auto c = color.getValues();
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0, j = 0; i < _pts.size(); i++, j+=3) {
        cairo_mesh_pattern_set_corner_color_rgba(_pts[i]->cobj(), corner, c[j], c[j+1], c[j+2], color.getOpacity());
    }
}

CheckerboardPattern::CheckerboardPattern(Colors::Color color, int size)
    : CheckerboardPattern(color.withoutOpacity(), Colors::make_contrasted_color(color), size)
{}

CheckerboardPattern::CheckerboardPattern(Colors::Color color1, Colors::Color color2, int size)
    : Pattern(color1.getSpace())
{
    _surface = std::make_shared<Surface>(Geom::IntPoint(2 * size, 2 * size), 1.0, _color_space);
    {
        auto ctx = Context(*_surface);
        ctx.set_operator(Cairo::Context::Operator::SOURCE);
        ctx.setSource(color1);
        ctx.paint();
        ctx.setSource(color2);
        ctx.rectangle(0, 0, size, size);
        ctx.fill();
        ctx.rectangle(size, size, size, size);
        ctx.fill();
    }

    setSurface(*_surface);
    setExtend(Cairo::Pattern::Extend::REPEAT);
    setFilter(Cairo::SurfacePattern::Filter::NEAREST);
}

StripesPattern::StripesPattern(Colors::Color color)
    : Pattern(color.getSpace())
{
    constexpr int width = 10;
    constexpr int line_width = 10;

    _surface = std::make_shared<Surface>(Geom::IntPoint(width, 1), 1.0, _color_space);
    {
        auto ctx = Context(*_surface);
        ctx.rectangle(Geom::Rect::from_xywh(0, 0, line_width / 2.0, 1));
        ctx.setSource(color);
        ctx.fill();
    }

    setSurface(*_surface);
    setExtend(Cairo::Pattern::Extend::REPEAT);
    setFilter(Cairo::SurfacePattern::Filter::NEAREST);
    setMatrix(Geom::Rotate(3 * M_PI / 4));
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
