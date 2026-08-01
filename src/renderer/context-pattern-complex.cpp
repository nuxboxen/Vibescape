// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing multiple patterns in a complex
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "context-pattern-complex.h"

#include "context.h"

namespace Inkscape::Renderer {

GradientShadow::GradientShadow(Geom::Rect const &rect, double size, Geom::Point const &bias, Colors::Color const &color)
    : _color(color)
    , _ease{{0.25, 0.1}, {0.25, 1}}
{
    auto shadow = rect.expandedBy(size) + bias;

    for (int corner = TOP_LEFT; corner <= BOTTOM_LEFT; corner++) {
        _rects[corner] = {shadow.corner(corner), rect.corner(corner)};
    }

    _rects[TOP]    = {_rects[TOP_LEFT].corner(TOP_RIGHT),       _rects[TOP_RIGHT].corner(BOTTOM_LEFT)};
    _rects[RIGHT]  = {_rects[TOP_RIGHT].corner(BOTTOM_RIGHT),   _rects[BOTTOM_RIGHT].corner(TOP_LEFT)};
    _rects[BOTTOM] = {_rects[BOTTOM_RIGHT].corner(BOTTOM_LEFT), _rects[BOTTOM_LEFT].corner(TOP_RIGHT)};
    _rects[LEFT]   = {_rects[BOTTOM_LEFT].corner(TOP_LEFT),     _rects[TOP_LEFT].corner(BOTTOM_RIGHT)};

    _default_gradients = _build_gradients(color);
}

GradientShadow::Gradients GradientShadow::_build_gradients(Colors::Color const &color) const
{
    // Radius from bottom_right of the unit square
    auto rg = RadialGradientPattern(color.getSpace(), 1, 1, 0, 1, 1, 1);
    rg.addColorStop(0.0, color.withOpacity(1.0));
    rg.addColorStop(1.0, color.withOpacity(0.0), _ease);
    // Linear from the bottom of the unit square
    auto lg = LinearGradientPattern(color.getSpace(), 1, 1, 1, 0);
    lg.setExtend(Cairo::Pattern::Extend::PAD);
    lg.copyColorStops(rg);
    return {rg, lg};
}

void GradientShadow::paint(Context ct) const
{
    if (ct.getColorSpace() == _color.getSpace()) {
        _paint(ct, *_default_gradients);
    } else {
        auto temp_gradients = _build_gradients(*_color.converted(ct.getColorSpace()));
        _paint(ct, temp_gradients);
    }
}

void GradientShadow::_paint(Context &ct, GradientShadow::Gradients &gr) const
{
    static Geom::Translate center(0.5, 0.5);

    for (int corner = TOP_LEFT; corner <= BOTTOM_LEFT; corner++) {
        gr.first.setMatrix(center.inverse() * Geom::Rotate::from_degrees(90 * (corner - TOP)) * center, _rects[corner]);
        ct.setSource(gr.first);
        ct.rectangle(_rects[corner]);
        ct.fill();
        //ct.paint(gr.second);
    }
    for (int side = TOP; side <= LEFT; side++) {
        gr.second.setMatrix(center.inverse() * Geom::Rotate::from_degrees(90 * (side - TOP)) * center, _rects[side]);
        ct.setSource(gr.second);
        ct.rectangle(_rects[side]);
        ct.fill();
    }
}

} // end namespace Inkscape::Renderer

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
