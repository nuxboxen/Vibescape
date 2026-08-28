// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Convert between color spaces
 *
 * This surface level color space conversion is based on the same logic
 * as that used by the Color::AnySpace converter for single colors, but
 * instead of using expensive std::vectors and feeding pixels into lcms
 * one at a time we convert the entire surface in each of the three passes.
 *
 * 1. SpaceToProfile which runs non-lcms2 code to convert from things like HSL
 *    and OkLab into their icc profile spaces such as sRGB and Lab.
 * 2. ProfileToProfile which is a regular lcms2 conversion, with the complexity
 *    that because we are working with cairo surfaces, the input and output
 *    might not be contiguous and so have to be transformed a bit more to make
 *    four channel spaces "fit" into a single memory surface for lcms to process
 * 3. ProfileToSpace which runs another non-lcms2 converter on the output to put
 *    the values into their final form.
 *
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_PIXEL_FILTER_COLOR_SPACE_H
#define INKSCAPE_RENDERER_PIXEL_FILTER_COLOR_SPACE_H

#include <iostream>
#include <optional>
#include <vector>

#include "colors/cms/transform-surface.h"
#include "colors/spaces/base.h"
#include "colors/spaces/convert-static.h"
#include "colors/manager.h"
#include "renderer/pixel-access.h"


namespace Inkscape::Renderer::PixelFilter {

struct ColorSpaceTransform
{
    // We expect to get transfer functions in the correct order for the input color space
    ColorSpaceTransform(std::shared_ptr<Colors::Space::AnySpace> from, std::shared_ptr<Colors::Space::AnySpace> to)
        : _from(from ? from : Colors::Manager::get().find(Colors::Space::Type::RGB))
        , _to(to ? to : Colors::Manager::get().find(Colors::Space::Type::RGB))
        , _needs_space_to_profile(std::dynamic_pointer_cast<Colors::Space::ProfileSpace<false>>(_from))
        , _needs_profile_to_profile(*_from->getProfile() != *_to->getProfile())
        , _needs_profile_to_space(std::dynamic_pointer_cast<Colors::Space::ProfileSpace<false>>(_to))
    {}

    std::shared_ptr<Colors::Space::AnySpace> _from;
    std::shared_ptr<Colors::Space::AnySpace> _to;

    bool _needs_space_to_profile;
    bool _needs_profile_to_profile;
    bool _needs_profile_to_space;

    template <class AccessDst, class AccessSrc>
    void filter(AccessDst &dst, AccessSrc const &src) const
    {
        if constexpr (std::is_same<AccessSrc, AccessDst>::value) {
            if (_to == _from && &dst == &src) {
                return; // Nothing to do.
            }
        }
        if (AccessDst::channel_total != _to->getComponentCount() + 1) {
            throw PixelAccessError("Target surface for color conversion doesn't have the right number of channels. Needs " + std::to_string(_to->getComponentCount() + 1) + " but destination has " + std::to_string(AccessDst::channel_total) + " channels.");
        }
        
        // lcms2 doesn't like converting from INT8 to FLOAT and our static functions take
        // decimals, so the conversion would be very paintfully slow, so we refuse.
        if constexpr (AccessDst::is_integer || AccessSrc::is_integer) {
            if (_needs_space_to_profile || _needs_profile_to_space) {
                throw PixelAccessError("Integer format not supported for color space conversions.");
            }
            transform_lcms(src, dst, true);
        } else {
            bool needs_contiguous = _needs_profile_to_profile && AccessSrc::has_more_channels;

            if (_needs_space_to_profile) {
                // Use the existing memory in destination if the same size as output
                // AND the next stage doesn't need the source to be re-written anyway
                if (_from->getComponentCount() + 1 == AccessDst::channel_total
                        && !(_needs_profile_to_profile && AccessSrc::has_more_channels)) {
                    transform_space_profile<true>(src, dst);
                    stage2(dst, dst, false);
                    return;
                }
                // Build a new surface memory for the output, we only support the number
                // of channels being used in ConvertableSpace::spaceToProfile functions.
                switch(_from->getProfile()->getSize()) {
                    case 3:
                        {
                            auto out = dst.template createContiguousEmpty<MEMORY_FORMAT_RGBA128F>();
                            transform_space_profile<true>(src, out);
                            stage2(out, dst, false);
                        }
                        break;
                    default:
                        throw PixelAccessError("Unsupported channel count conversion in space-to-profile stage.");
                }
            } else if (needs_contiguous) {
                // Source surface isn't contiguous so copy it into a new memory buffer for lcms2
                auto in = src.createContiguousCopy();
                stage2(in, dst, true);
            } else {
                stage2(src, dst, true);
            }
        }
    }

    template <class AccessSrc, class AccessDst>
    void stage2(AccessSrc const &src, AccessDst &dst, bool is_alpha_premultiplied) const
    {
        // If the alpha channel is still premultiplied, then we're going to cheat and
        // ask lcms2 to unmultiply it for us by converting between the same profiles
        if (_needs_profile_to_profile || is_alpha_premultiplied) {
            // Use the existing memory in destination if the same size as output
            // AND that destination is a contiguous memory surface.
            if (_to->getProfile()->getSize() + 1 == AccessDst::channel_total
                && !AccessDst::has_more_channels) {
                transform_lcms_or_premultiply(src, dst, is_alpha_premultiplied);
                stage3(dst, dst);
                return;
            }
            // We support each of the sizes of lcms2 profiles in CMS::Profile
            // which at the moment is a limited set in order to allow static
            // code to function like this.
            switch(_to->getProfile()->getSize()) {
                case 1:
                    {
                        // TODO: Upgrade this to floating point
                        auto out = dst.template createContiguousEmpty<MEMORY_FORMAT_A8>();
                        transform_lcms_or_premultiply(src, out, is_alpha_premultiplied);
                        stage3(out, dst);
                        return;
                    }
                case 3:
                    {
                        auto out = dst.template createContiguousEmpty<MEMORY_FORMAT_RGBA128F>();
                        transform_lcms_or_premultiply(src, out, is_alpha_premultiplied);
                        stage3(out, dst);
                        return;
                    }
                case 4:
                    {
                        auto out = dst.template createContiguousEmpty<MEMORY_FORMAT_CMYKA160F>();
                        transform_lcms_or_premultiply(src, out, is_alpha_premultiplied);
                        stage3(out, dst);
                        return;
                    }
                default:
                    throw PixelAccessError("Unsupported channel count conversion in profile conversion stage.");
            }
        } else {
            stage3(src, dst);
        }
    }

    template <class AccessSrc, class AccessDst>
    void stage3(AccessSrc const &src, AccessDst &dst) const
    {
        if (_needs_profile_to_space) {
            if (_to->getComponentCount() + 1 != AccessDst::channel_total) {
                throw PixelAccessError("Unsupported channel count conversion in profile to space stage.");
            }
            // Use the existing memory in destination if the same size as output
            transform_space_profile<false>(src, dst);
        } else {
            if constexpr (AccessSrc::channel_total == AccessDst::channel_total) {
                premultiply_alpha<true>(src, dst);
            } else {
                throw PixelAccessError("Final conversion step MUST have the same source and destination sizes when not converting to the profile space.");
            }
        }
    }

    template <bool Premultiply, class AccessSrc, class AccessDst>
    void premultiply_alpha(AccessSrc const &src, AccessDst &dst) const
        requires (AccessSrc::channel_total == AccessDst::channel_total)
    {
        // The colors will be alpha unmultiplied, and may even be on a contiguous set of pixels now.
        // So we have to undo and make sure the pixels end up on the dst correctly.

        if constexpr (!AccessSrc::has_more_channels && !AccessDst::has_more_channels) {
            src.forEachLine(dst, [](AccessDst::PrimaryType const *src, AccessDst::PrimaryType const *end, AccessDst::PrimaryType *dst) {
                for (;src < end; src+=AccessDst::primary_total, dst+=AccessDst::channel_total) {
                    auto alpha = src[AccessSrc::primary_count];
                    for (auto c = 0; c < AccessSrc::primary_count; c++) {
                        dst[c] = Premultiply ? src[c] * alpha : src[c] / alpha;
                    }
                    dst[AccessDst::primary_count] = alpha;
                }
            });
        } else {
            // Get access to second memory buffer
            src.forEachLine(dst, [](float const *src1, float const *src2, float const *end, float *dst1, float *dst2) {
                for (;src1 < end; src1+=AccessSrc::primary_total, src2+=AccessSrc::primary_total,
                                  dst1+=AccessDst::primary_total, dst2+=AccessDst::primary_total) {
                    auto alpha = src1[AccessSrc::primary_count];
                    for (auto c = 0; c < AccessSrc::primary_count && c < AccessDst::primary_count; c++) {
                        dst1[c] = Premultiply ? src1[c] * alpha : src1[c] / alpha;
                    }
                    if (src2) {
                        dst2[0] = Premultiply ? src2[0] * alpha : src2[0] / alpha;
                    }
                    dst1[AccessDst::primary_count] = alpha;
                    if constexpr (AccessDst::has_more_channels) {
                        dst2[AccessDst::primary_count] = alpha;
                    }
                }
            });
        }
    }

    template <bool ToProfile, class AccessSrc, class AccessDst>
    void transform_space_profile(AccessSrc const &src, AccessDst &dst) const
    {
        auto space = ToProfile ? std::dynamic_pointer_cast<Colors::Space::ProfileSpace<false>>(_from)
                               : std::dynamic_pointer_cast<Colors::Space::ProfileSpace<false>>(_to);

        // If source AND dest is contiguous, use Line by line mechanism, if not, use pixel based colorAt instead
        if constexpr (AccessSrc::is_integer        || AccessDst::is_integer
                   || AccessSrc::has_more_channels || AccessDst::has_more_channels) {

            // SLOW ARM 6000x6000px -> 2103ms

            auto fn = ToProfile ? Colors::Space::spaceToProfile<double, false>(space)
                                : Colors::Space::profileToSpace<double, false>(space);

            dst.forEachPixel([&fn, &src, &dst](int x, int y) {
                typename AccessSrc::Color in;
                typename AccessDst::Color out;
                src.colorAt(x, y, in, ToProfile);
                if (in.back() > 0.0) {
                    fn(in.data(), out.data());
                    out.back() = in.back();
                    dst.colorTo(x, y, out, !ToProfile);
                }
            });
        } else {
            // FAST ARM 6000x6000px -> 370ms
            auto fn = ToProfile ? Colors::Space::spaceToProfile<float, true>(space)
                                : Colors::Space::profileToSpace<float, true>(space);

            src.forEachLine(dst, [&fn](float const *src, float const *end, float *dst) {
                for (;src < end; src+=AccessSrc::channel_total, dst+=AccessDst::channel_total) {
                    if (src[AccessSrc::channel_total-1] > 0.0) {
                        fn(src, dst);
                    }
                }
            });
        }
    }

    /**
     * If there is no profile to profile and we've falled through, we will still need to remove the
     * alpha premultiplication from the input surface, lcms2 will do this for us in those cases.
     */
    template <class AccessSrc, class AccessDst>
    void transform_lcms_or_premultiply(AccessSrc const &src, AccessDst &dst, bool is_alpha_premultiplied) const
    {
        if (_needs_profile_to_profile) {
            transform_lcms(src, dst, is_alpha_premultiplied);
        } else {
            if constexpr (AccessSrc::channel_total == AccessDst::channel_total) {
                premultiply_alpha<false>(src, dst);
            } else {
                throw PixelAccessError("Middle conversion step MUST have the same source and destination sizes when not converting via lcms2.");
            }
        }
    }
    /**
     * Transform the color space data between the two icc color profiles. Stage 2 of conversion.
     */
    template <class AccessSrc, class AccessDst>
    void transform_lcms(AccessSrc const &src, AccessDst &dst, bool is_alpha_premultiplied) const
    {
        if ( AccessDst::primary_count != _to->getComponentCount()
          && AccessSrc::primary_count != _from->getComponentCount()) {
            throw PixelAccessError(std::string("Output surface format doesn't match color space!  (")
                + std::to_string(AccessSrc::primary_count) + " != " + std::to_string(_from->getComponentCount())
                + " || "
                + std::to_string(AccessDst::primary_count) + " != " + std::to_string(_to->getComponentCount())
            );
        }

        if constexpr (!AccessSrc::has_more_channels && !AccessDst::has_more_channels) {
            // EXTREMELY SLOW ARM 6000x6000 -> 16,636ms

            // TODO: A caching key should be used to cache transforms, if possible.
            Colors::CMS::TransformSurface::Format in = {
                _from->getProfile(),
                AccessSrc::primary_size,
                AccessSrc::is_integer,
                is_alpha_premultiplied,
                // When the primary count is zero, this means one channel, alpha only and
                // since alpha is the primary, we don't need an extra alpha channel in transform.
                AccessSrc::primary_count > 0
            };
            Colors::CMS::TransformSurface::Format out = {
                _to->getProfile(),
                AccessDst::primary_size,
                AccessDst::is_integer,
                false, // lcms2 can not output premultiplied alpha
                AccessDst::primary_count > 0
            };

            // Error is printed by TransformContext error_handler
            if (auto transform = Colors::CMS::TransformSurface(in, out, _from->getBestIntent(_to))) {
                transform.do_transform(dst.width(), dst.height(), src.memory(), dst.memory(),
                // Access strides are by type size stride, but lcms2 is by byte stride.
                src.stride() * AccessSrc::primary_size,
                dst.stride() * AccessDst::primary_size);
            }
        } else {
            throw PixelAccessError("Image surface isn't a contiguous memory surface in lcms2 color conversion.");
        }
    }
};

struct AlphaSpaceExtraction
{
    template <class AccessDst, class AccessSrc>
    void filter(AccessDst &dst, AccessSrc const &src) const
        // requires (AccessDst::channel_total == 1)
    {
        dst.forEachPixel([&](int x, int y) {
            dst.alphaTo(x, y, src.alphaAt(x, y));
        });
    }
};

} // namespace Inkscape::Renderer::PixelFilter

#endif // INKSCAPE_RENDERER_PIXEL_FILTER_COLOR_SPACE_H

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
