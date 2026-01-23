// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2026 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_DRAWING_FILTER_GAUSSIAN_BLUR_H
#define SEEN_INKSCAPE_RENDERER_DRAWING_FILTER_GAUSSIAN_BLUR_H

#include <2geom/forward.h>

#include "primitive.h"
#include "renderer/surface.h"

namespace Inkscape::Renderer::DrawingFilter {

class GaussianBlur : public Primitive
{
public:
    GaussianBlur() = default;
    ~GaussianBlur() override = default;

    GaussianBlur(Geom::Point deviation)
        : _deviation_x{deviation[Geom::X]}
        , _deviation_y{deviation[Geom::Y]}
    {}

    void render(Slot &slot) const override;
    void area_enlarge(Geom::IntRect &area, Geom::Affine const &m) const override;
    bool can_handle_affine(Geom::Affine const &m) const override;
    double complexity(Geom::Affine const &ctm) const override;

    /**
     * Set the standard deviation value for gaussian blur. Deviation along
     * both axis is set to the provided value.
     * Negative value, NaN and infinity are considered an error and no
     * changes to filter state are made. If not set, default value of zero
     * is used, which means the filter results in transparent black image.
     */
    void set_deviation(double deviation);

    /**
     * Set the standard deviation value for gaussian blur. First parameter
     * sets the deviation alogn x-axis, second along y-axis.
     * Negative value, NaN and infinity are considered an error and no
     * changes to filter state are made. If not set, default value of zero
     * is used, which means the filter results in transparent black image.
     */
    void set_deviation(double x, double y);

    Glib::ustring name() const override { return Glib::ustring("Gaussian Blur"); }

    /**
     * Instead of applying an expensive blur to a massive number of pixels, a lower quality
     * value estimates the blur by bluring a little bit and squashing and scaling in that
     * direction instead. It's not as good as bluring, but it's was faster for worse quality.
     */
    static void downsampleForQuality(BlurQuality quality, Geom::IntPoint &size, Geom::Point &deviation);

protected:
    std::shared_ptr<Surface> _render(Slot &slot, int input) const;

private:
    double _deviation_x = 0.0;
    double _deviation_y = 0.0;
};

} // namespace Inkscape::Renderer::DrawingFilter

#endif // SEEN_INKSCAPE_RENDERER_DRAWING_FILTER_GAUSSIAN_BLUR_H
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
