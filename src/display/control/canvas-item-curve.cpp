// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * A class to represent a single Bezier control curve, either a line or a cubic Bezier.
 */

/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCtrlLine and SPCtrlCurve
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/bezier-curve.h>

#include "canvas-item-curve.h"

#include "colors/color.h"
#include "helper/geom.h"
#include "ui/widget/canvas.h"

namespace Inkscape {

/**
 * Create an null control curve.
 */
CanvasItemCurve::CanvasItemCurve(CanvasItemGroup *group)
    : CanvasItem(group)
{
    _name = "CanvasItemCurve:Null";
}

/**
 * Create a linear control curve. Points are in document coordinates.
 */
CanvasItemCurve::CanvasItemCurve(CanvasItemGroup *group, Geom::Point const &p0, Geom::Point const &p1)
    : CanvasItem(group)
    , _curve(std::make_unique<Geom::LineSegment>(p0, p1))
{
    _name = "CanvasItemCurve:Line";
}

/**
 * Create a cubic Bezier control curve. Points are in document coordinates.
 */
CanvasItemCurve::CanvasItemCurve(CanvasItemGroup *group,
                                 Geom::Point const &p0, Geom::Point const &p1,
                                 Geom::Point const &p2, Geom::Point const &p3)
    : CanvasItem(group)
    , _curve(std::make_unique<Geom::CubicBezier>(p0, p1, p2, p3))
{
    _name = "CanvasItemCurve:CubicBezier";
}

/**
 * Set a linear control curve. Points are in document coordinates.
 */
void CanvasItemCurve::set_coords(Geom::Point const &p0, Geom::Point const &p1)
{
    defer([=, this] {
        _name = "CanvasItemCurve:Line";
        _curve = std::make_unique<Geom::LineSegment>(p0, p1);
        request_update();
    });
}

/**
 * Set a cubic Bezier control curve. Points are in document coordinates.
 */
void CanvasItemCurve::set_coords(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2, Geom::Point const &p3)
{
    defer([=, this] {
        _name = "CanvasItemCurve:CubicBezier";
        _curve = std::make_unique<Geom::CubicBezier>(p0, p1, p2, p3);
        request_update();
    });
}

/**
 * Set stroke width.
 */
void CanvasItemCurve::set_width(int width)
{
    defer([=, this] {
        if (_width == width) return;
        _width = width;
        request_update();
    });
}

/**
 * Set background stroke alpha.
 */
void CanvasItemCurve::set_bg_alpha(float alpha)
{
    defer([=, this] {
        if (bg_alpha == alpha) return;
        bg_alpha = alpha;
        request_update();
    });
}

/**
 * Returns distance between point in canvas units and nearest point on curve.
 */
double CanvasItemCurve::closest_distance_to(Geom::Point const &p) const
{
    double d = Geom::infinity();
    if (_curve) {
        Geom::BezierCurve curve = *_curve;
        curve *= affine(); // Document to canvas.
        Geom::Point n = curve.pointAt(curve.nearestTime(p));
        d = Geom::distance(p, n);
    }
    return d;
}

/**
 * Returns true if point p (in canvas units) is within tolerance (canvas units) distance of curve.
 */
bool CanvasItemCurve::contains(Geom::Point const &p, double tolerance)
{
    return closest_distance_to(p) <= tolerance;
}

/**
 * Update and redraw control curve.
 */
void CanvasItemCurve::_update(bool)
{
    // Queue redraw of old area (erase previous content).
    request_redraw(); // This is actually never useful as curves are always deleted
    // and recreated when a node is moved! But keep it in case we change that.

    if (!_curve || _curve->isDegenerate()) {
        _bounds = {};
        return; // No curve! Can happen - see node.h.
    }

    // Tradeoff between updating a larger area (typically twice for Beziers?) vs computation time for bounds.
    _bounds = expandedBy(_curve->boundsExact() * affine(), 2); // Room for stroke.

    // Queue redraw of new area
    request_redraw();
}

/**
 * Render curve to screen via Cairo.
 */
void CanvasItemCurve::_render(Inkscape::CanvasItemBuffer buf) const
{
    assert(_curve); // Not called if _curve is null, since _bounds would be null.

    // Todo: Transform, rather than copy.
    Geom::BezierCurve curve = *_curve;
    curve *= affine();                          // Document to canvas.
    curve *= Geom::Translate(-buf.rect.min());  // Canvas to screen.

    auto cr = buf.cr;

    cr.newPath();

    if (curve.size() == 2) {
        // Line
        cr.moveTo({curve[0].x(), curve[0].y()});
        cr.lineTo({curve[1].x(), curve[1].y()});
    } else {
        // Curve
        cr.moveTo({curve[0].x(), curve[0].y()});
        cr.curveTo({curve[1].x(), curve[1].y()}, {curve[2].x(), curve[2].y()}, {curve[3].x(), curve[3].y()});
    }

    auto c = Colors::Color(_stroke);
    cr.setSource(Colors::Color(c.getSpace(), {1.0, 1.0, 1.0, bg_alpha}));
    cr.setLineWidth(background_width);
    cr.strokePreserve();

    cr.setSource(c);
    cr.setLineWidth(_width);
    cr.stroke();

    // Uncomment to show bounds
    // Geom::Rect bounds = _bounds;
    // bounds.expandBy(-1);
    // bounds -= buf.rect.min();
    // cr.setSource(olors::Color(c.getSpace(), {1.0, 0.0, 0.0, 1.0});
    // cr.rectangle(bounds);
    // cr.stroke();
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
