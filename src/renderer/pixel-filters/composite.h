// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Raw filter functions for composite, most of the options are
 * handled directly by cairo, these is just the arithmetic function.
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_PIXEL_FILTER_COMPOSITE_H
#define INKSCAPE_RENDERER_PIXEL_FILTER_COMPOSITE_H

#include <algorithm>

namespace Inkscape::Renderer::PixelFilter {

struct CompositeArithmetic
{
    double _k1, _k2, _k3, _k4;

    CompositeArithmetic(double k1, double k2, double k3, double k4)
        : _k1(k1)
        , _k2(k2)
        , _k3(k3)
        , _k4(k4)
    {}

    template <class AccessDst, class AccessSrc>
    void filter(AccessDst &dst, AccessSrc const &src) const
    {
        src.forEachLine(dst, [k1=_k1, k2=_k2, k3=_k3, k4=_k4](AccessSrc::PrimaryType const *src1,
                                AccessSrc::PrimaryType const *src2,
                                AccessSrc::PrimaryType const *end,
                                AccessDst::PrimaryType *dst1,
                                AccessDst::PrimaryType *dst2) {
            while (src1 < end) {
                // NOTE: Nowhere in the SVG specification does it explain that this calulcation is done
                // on the pre-multiplied alpha directly and not on regular colors.
                double a1 = (AccessSrc::has_alpha ? (double)*(src1 + AccessSrc::primary_alpha_pos) * AccessSrc::primary_unscale : 1.0);
                double a2 = (AccessDst::has_alpha ? (double)*(dst1 + AccessDst::primary_alpha_pos) * AccessDst::primary_unscale : 1.0);
                double ao = std::clamp((k1*a1*a2 + k2*a1 + k3*a2 + k4), 0.0, 1.0);

                for (int c = 0; c < AccessSrc::primary_total; c++, src1++, dst1++) {
                    if (AccessSrc::has_alpha && c == AccessSrc::primary_alpha_pos) {

                        *dst1 = ao * AccessDst::primary_scale;
                        if constexpr (AccessSrc::has_more_channels && AccessDst::has_more_channels) {
                            *dst2 = ao * AccessDst::primary_scale;
                            src2++;
                            dst2++;
                        }
                    } else {
                        {
                            double b1 = a1 > 0.0 ? (double)*src1 * AccessSrc::primary_unscale : 0.0;
                            double b2 = a2 > 0.0 ? (double)*dst1 * AccessDst::primary_unscale : 0.0;
                            double bo = std::clamp((k1*b1*b2 + k2*b1 + k3*b2 + k4), 0.0, ao);
                            *dst1 = bo * AccessDst::primary_scale;
                        }
                        if constexpr (AccessSrc::has_more_channels && AccessDst::has_more_channels) {
                            auto b3 = a1 > 0.0 ? *src2 * AccessSrc::primary_unscale : 0.0;
                            auto b4 = a2 > 0.0 ? *dst2 * AccessDst::primary_unscale : 0.0;
                            *dst2 = std::clamp((k1*b3*b4 + k2*b3 + k3*b4 + k4), 0.0, ao) * AccessDst::primary_scale;
                            src2++;
                            dst2++;
                        }
                    }
                }
            }
        });
    }
};

} // namespace Inkscape::Renderer::PixelFilter

#endif // INKSCAPE_RENDERER_PIXEL_FILTER_COMPOSITE_H

/*
  ;Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
