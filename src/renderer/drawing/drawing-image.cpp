// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Bitmap image belonging to an SVG drawing.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/bezier-curve.h>

#include "renderer/code-builder.h"
#include "renderer/context.h"
#include "renderer/pixel-filters/average-color.h"

#include "drawing.h"
#include "drawing-image.h"

namespace Inkscape::Renderer {

DrawingImage::DrawingImage(Drawing &drawing)
    : DrawingItem(drawing)
    , _extend(Cairo::Pattern::Extend::NONE) // NONE prevents artifacts in surrounding empty space
{
    if (drawing._code_build) CodeBuilder::Construct(*this, "DrawingImage", "image", "make_drawingitem") << drawing;
}

void DrawingImage::setImage(std::shared_ptr<Surface const> image)
{
    defer([this, image = std::move(image)] () mutable {
        _image = std::move(image);
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setScale(double sx, double sy)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "setScale") << sx << sy;

    defer([=, this] {
        _scale = Geom::Scale(sx, sy);
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setOrigin(Geom::Point const &origin)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "setOrigin") << origin;

    defer([=, this] {
        _origin = origin;
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setClipbox(Geom::Rect const &box)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "setClipbox") << box;

    defer([=, this] {
        _clipbox = box;
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setExtend(Cairo::Pattern::Extend extend)
{
    defer([=, this] {
        _extend = extend;
        _markForUpdate(STATE_ALL, false);
    });
}

Geom::Rect DrawingImage::imageBounds() const
{
    auto wh = Geom::Point(_image->dimensions()) * Geom::Scale(_scale);
    return Geom::Rect(_origin, _origin+wh);
}

Geom::Rect DrawingImage::bounds() const
{
    if (!_image) return _clipbox;
    Geom::OptRect res = _clipbox & imageBounds();
    return res ? *res : _clipbox;
}

unsigned DrawingImage::_updateItem(Geom::IntRect const &, UpdateContext const &, unsigned, unsigned)
{
    _bbox = (bounds() * _ctm).roundOutwards();
    return STATE_ALL;
}

// Broken image and image outline are the same shapes with a
// different stroke width and color.
void DrawingImage::_renderImageOutline(Context dc) const
{
    dc.transform(_ctm);

    dc.newPath();
    Geom::Rect r = bounds();
    Geom::Point c00 = r.corner(0);
    Geom::Point c01 = r.corner(3);
    Geom::Point c11 = r.corner(2);
    Geom::Point c10 = r.corner(1);

    dc.moveTo(c00);
    // the box
    dc.lineTo(c10);
    dc.lineTo(c11);
    dc.lineTo(c01);
    dc.closePath();

    // the diagonals
    dc.moveTo(c00);
    dc.lineTo(c11);
    dc.moveTo(c10);
    dc.lineTo(c01);

    dc.setLineWidth(0.5);
    dc.setSource(Colors::Color(_drawing.imageOutlineColor()));
    dc.stroke();
}

void DrawingImage::_renderImageBroken(Context dc) const
{
    dc.transform(viewbox_matrix(_ctm, bounds()));

    // Red Box
    dc.rectangle(0, 0, 1, 1);
    dc.setLineWidth(0.15);
    dc.setSource(Colors::Color(0xffffffff));
    dc.fillPreserve();
    dc.setSource(Colors::Color(0xcc0000ff));
    dc.stroke();
    // Red Circle
    dc.circle({0.5, 0.5}, 0.3);
    dc.setSource(Colors::Color(0xcc0000ff));
    dc.fill();
    // White X
    dc.setLineWidth(0.075);
    dc.setSource(Colors::Color(0xffffffff));
    dc.move_to(0.5 - 0.15, 0.5 - 0.15);
    dc.line_to(0.5 + 0.15, 0.5 + 0.15);
    dc.move_to(0.5 + 0.15, 0.5 - 0.15);
    dc.line_to(0.5 - 0.15, 0.5 + 0.15);
    dc.stroke();
}

void DrawingImage::_renderImage(Context dc) const
{
    dc.transform(_ctm);
    dc.newPath();
    dc.rectangle(_clipbox);
    dc.clip();

    dc.translate(Geom::Translate(_origin));
    dc.scale(_scale);

    auto image = _image->convertedToCompatible(dc.getSurfaceFormat());
    if (dc.getSurfaceColorSpace()) {
        image.convertToColorSpace(dc.getSurfaceColorSpace());
    }
    dc.setSource(image, 0, 0, _style.image_rendering, _extend);
    dc.paint();
}

unsigned DrawingImage::_renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &/*area*/, unsigned flags, DrawingItem const */*stop_at*/) const
{
    if (_scale.vector().x() * _scale.vector().y() == 0.0) return RENDER_OK;

    if ((flags & RENDER_OUTLINE) && !_drawing.imageOutlineMode()) {
        _renderImageOutline(dc);
    } else if (!_image) {
        _renderImageBroken(dc);
    } else {
        _renderImage(dc);
    }

    return RENDER_OK;
}

/** Calculates the closest distance from p to the segment a1-a2*/
static double distance_to_segment(Geom::Point const &p, Geom::Point const &a1, Geom::Point const &a2)
{
    Geom::LineSegment l(a1, a2);
    Geom::Point np = l.pointAt(l.nearestTime(p));
    return Geom::distance(np, p);
}

DrawingItem *DrawingImage::_pickItem(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags)
{
    if (!_image) return nullptr;

    bool outline = (flags & PICK_OUTLINE) && !_drawing.imageOutlineMode();

    if (outline) {
        Geom::Rect r = bounds();
        Geom::Point pick = p * _ctm.inverse();

        // find whether any side or diagonal is within delta
        // to do so, iterate over all pairs of corners
        for (unsigned i = 0; i < 3; ++i) { // for i=3, there is nothing to do
            for (unsigned j = i+1; j < 4; ++j) {
                if (distance_to_segment(pick, r.corner(i), r.corner(j)) < delta) {
                    return this;
                }
            }
        }
        return nullptr;

    } else {
        Geom::Point tp = p * _ctm.inverse();
        Geom::Rect img_box = imageBounds();
        Geom::Rect px_box = Geom::Rect({0,0}, _image->dimensions());
        Geom::IntPoint px = (tp * Geom::Scale(img_box.dimensions()).inverse() * Geom::Scale(px_box.dimensions())).floor();

        if (!bounds().contains(tp) || !px_box.contains(px))
            return nullptr;

        // Get the pixel color at this coordinate
        auto color = _image->run_pixel_filter(PixelFilter::PickColor(px[Geom::X], px[Geom::Y]));
        return color.back() > 0.01 ? this : nullptr;
    }
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
