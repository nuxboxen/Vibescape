// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2026 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "renderer/drawing-filters/gaussian-blur.h"
#include "renderer/pixel-filters/gaussian-blur.h"
#include "renderer/surface.h"
#include "renderer/context.h"

#include "util/fixed_point.h"

#include "slot.h"
#include "enums.h"

namespace Inkscape::Renderer::DrawingFilter {

template <typename T>
static inline T clip(T const &v, T const &a, T const &b) 
{
    if (v < a)
        return a;
    if (v > b)
        return b;
    return v;
}

static int _effect_area_scr(double const deviation)
{
    return (int)std::ceil(std::fabs(deviation) * 3.0);
}

// Return value (v) should satisfy:
//  2^(2*v)*255<2^32
//  255<2^(32-2*v)
//  2^8<=2^(32-2*v)
//  8<=32-2*v
//  2*v<=24
//  v<=12
static int _effect_subsample_step_log2(double const deviation, BlurQuality const quality)
{
    // To make sure FIR will always be used (unless the kernel is VERY big):
    //  deviation/step <= 3
    //  deviation/3 <= step
    //  log(deviation/3) <= log(step)
    // So when x below is >= 1/3 FIR will almost always be used.
    // This means IIR is almost only used with the modes BETTER or BEST.
    int stepsize_l2;
    switch (quality) {
        case BlurQuality::WORST:
            // 2 == log(x*8/3))
            // 2^2 == x*2^3/3
            // x == 3/2
            stepsize_l2 = clip(static_cast<int>(log(deviation * (3. / 2.)) / log(2.)), 0, 12);
            break;
        case BlurQuality::WORSE:
            // 2 == log(x*16/3))
            // 2^2 == x*2^4/3
            // x == 3/2^2
            stepsize_l2 = clip(static_cast<int>(log(deviation * (3. / 4.)) / log(2.)), 0, 12);
            break;
        case BlurQuality::BETTER:
            // 2 == log(x*32/3))
            // 2 == x*2^5/3
            // x == 3/2^4
            stepsize_l2 = clip(static_cast<int>(log(deviation * (3. / 16.)) / log(2.)), 0, 12);
            break;
        case BlurQuality::BEST:
            stepsize_l2 = 0; // no subsampling at all
            break;
        case BlurQuality::NORMAL:
        default:
            // 2 == log(x*16/3))
            // 2 == x*2^4/3
            // x == 3/2^3
            stepsize_l2 = clip(static_cast<int>(log(deviation * (3. / 8.)) / log(2.)), 0, 12);
            break;
    }
    return stepsize_l2;
}

void GaussianBlur::render(Slot &slot) const
{
    // zero deviation = no change in output
    if (_deviation_x > 0 && _deviation_y > 0) {
        slot.set(_output, _render(slot, _input));
    } else {
        slot.set(_output, slot.get(_input));
    }
}

std::shared_ptr<Surface> GaussianBlur::_render(Slot &slot, int input) const
{

    auto out = slot.get_copy(input, _color_space);
    Geom::IntPoint size = out->dimensions();
    if (size.x() <= 0 || size.y() <= 0) {
        return out; // nothing to do, prevent transform crash
    }

    // Handle bounding box case.
    auto &item_opt = slot.get_item_options();
    double dx = _deviation_x;
    double dy = _deviation_y;
    if( item_opt.get_primitive_units() == SP_FILTER_UNITS_OBJECTBOUNDINGBOX ) {
        Geom::OptRect const bbox = item_opt.get_item_bbox();
        if( bbox ) {
            dx *= (*bbox).width();
            dy *= (*bbox).height();
        }
    }

    Geom::Affine trans = item_opt.get_matrix_user2pb();
    int device_scale = slot.get_drawing_options().device_scale;

    Geom::Point deviation(dx * trans.expansionX() * device_scale,
                          dy * trans.expansionY() * device_scale);
    Geom::Point old = size;

    downsampleForQuality(slot.get_drawing_options().blurquality, size, deviation);

    std::shared_ptr<Surface> dest = out;

    // Scaling allow for a lower quality but much faster blur
    auto tr = Geom::Scale(size[Geom::X] / old[Geom::X], size[Geom::Y] / old[Geom::Y]);
    if (tr != Geom::identity()) {
        dest = out->similar(size);
        auto context = Context(*dest);
        context.transform(tr);
        context.setSource(*out);
        context.set_operator(Cairo::Context::Operator::SOURCE);
        context.paint();
    }

    dest->run_pixel_filter<PixelAccessEdgeMode::ZERO>(PixelFilter::GaussianBlur(deviation));

    if (tr != Geom::identity()) {
        // Scale the slightly blured image back
        auto context = Context(*out);
        context.transform(tr.inverse());
        context.setSource(*dest);
        context.set_operator(Cairo::Context::Operator::SOURCE);
        context.paint();
    }

    return out;
}

void GaussianBlur::area_enlarge(Geom::IntRect &area, Geom::Affine const &trans) const
{
    int area_x = _effect_area_scr(_deviation_x * trans.expansionX());
    int area_y = _effect_area_scr(_deviation_y * trans.expansionY());
    // maximum is used because rotations can mix up these directions
    // TODO: calculate a more tight-fitting rendering area
    int area_max = std::max(area_x, area_y);
    area.expandBy(area_max);
}

bool GaussianBlur::can_handle_affine(Geom::Affine const &) const
{
    // Previously we tried to be smart and return true for rotations.
    // However, the transform passed here is NOT the total transform
    // from filter user space to screen.
    // TODO: fix this, or replace can_handle_affine() with isotropic().
    return false;
}

double GaussianBlur::complexity(Geom::Affine const &trans) const
{
    int area_x = _effect_area_scr(_deviation_x * trans.expansionX());
    int area_y = _effect_area_scr(_deviation_y * trans.expansionY());
    return 2.0 * area_x * area_y;
}

void GaussianBlur::set_deviation(double deviation)
{
    if (std::isfinite(deviation) && deviation >= 0) {
        _deviation_x = _deviation_y = deviation;
    }
}

void GaussianBlur::set_deviation(double x, double y)
{
    if (std::isfinite(x) && x >= 0 && std::isfinite(y) && y >= 0) {
        _deviation_x = x;
        _deviation_y = y;
    }
}

void GaussianBlur::downsampleForQuality(BlurQuality quality, Geom::IntPoint &size, Geom::Point &deviation)
{
    Geom::Point step(1 << _effect_subsample_step_log2(deviation[Geom::X], quality),
                     1 << _effect_subsample_step_log2(deviation[Geom::Y], quality));
    if (step[Geom::X] > 1) {
        size[Geom::X] = ceil((double)size[Geom::X] / step[Geom::X]) + 1;
        deviation[Geom::X] /= step[Geom::X];
    }
    if (step[Geom::Y] > 1) {
        size[Geom::Y] = ceil((double)size[Geom::Y] / step[Geom::Y]) + 1;
        deviation[Geom::Y] /= step[Geom::Y];
    }
}   

} // namespace Inkscape::Renderer::DrawingFilter

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
