// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Bitmap image belonging to an SVG drawing.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/bezier-curve.h>

#include "renderer/context.h"
#include "renderer/pixel-filters/average-color.h"

#include "drawing.h"
#include "drawing-image.h"

namespace Inkscape::Renderer {

DrawingImage::DrawingImage(Drawing &drawing)
    : DrawingItem(drawing)
    , _extend(Cairo::Pattern::Extend::NONE) // NONE prevents artifacts in surrounding empty space
{
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
    defer([=, this] {
        _scale = Geom::Scale(sx, sy);
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setOrigin(Geom::Point const &origin)
{
    defer([=, this] {
        _origin = origin;
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingImage::setClipbox(Geom::Rect const &box)
{
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
    Geom::Rect ret = res ? *res : _clipbox;

    return ret;
}

unsigned DrawingImage::_updateItem(Geom::IntRect const &, UpdateContext const &, unsigned, unsigned)
{
    // Calculate bbox
    if (_image) {
        Geom::Rect r = bounds() * _ctm;
        _bbox = r.roundOutwards();
    } else {
        _bbox = Geom::OptIntRect();
    }

    return STATE_ALL;
}

unsigned DrawingImage::_renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &/*area*/, unsigned flags, DrawingItem const */*stop_at*/) const
{
    bool const outline = (flags & RENDER_OUTLINE) && !_drawing.imageOutlineMode();

    if (!outline) {
        if (!_image) return RENDER_OK;
        if (_scale.vector().x() * _scale.vector().y() == 0.0) return RENDER_OK;

        Context::Save save(dc);
        dc.transform(_ctm);
        dc.newPath();
        dc.rectangle(_clipbox);
        dc.clip();

        dc.translate(Geom::Translate(_origin));
        dc.scale(_scale);

        dc.setSource(*_image, 0, 0, _nrstyle.image_rendering, _extend);
        //_image->write_to_png("/tmp/rendering-image");
        //dc.getSurface()->write_to_png("/tmp/rendered-image");

        // Handle an exceptional case where the greyscale color mode needs to be applied per-image.
        bool const greyscale_exception = (flags & RENDER_OUTLINE) && _drawing.colorMode() == ColorMode::GRAYSCALE;
        if (greyscale_exception) {
            dc.pushGroup();
        }

        dc.paint();

        if (greyscale_exception) {
            // TODO dc.filter(_drawing.grayscaleMatrix());
            dc.popGroupToSource();
            dc.paint();
        }

    } else { // outline; draw a rect instead

        auto rgba = Colors::Color(_drawing.imageOutlineColor());

        {   Context::Save save(dc);
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
            dc.lineTo(c00);
            // the diagonals
            dc.lineTo(c11);
            dc.moveTo(c10);
            dc.lineTo(c01);
        }

        dc.setLineWidth(0.5);
        dc.setSource(rgba);
        dc.stroke();
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
