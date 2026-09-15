// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Split pixel data from alpha data and return in a format suitable for PDF export
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_PIXEL_FILTER_PDF_IMAGE_BUILDER_H
#define INKSCAPE_RENDERER_PIXEL_FILTER_PDF_IMAGE_BUILDER_H

#include <stdint.h>
#include <vector>

namespace Inkscape::Renderer::PixelFilter {

template <typename T0> requires (std::same_as<T0, uint8_t> || std::same_as<T0, uint16_t>)
struct BuildPdfImageData
{
    template <class AccessSrc>
    std::pair<std::vector<T0>, std::vector<T0>> filter(AccessSrc const &src) const
    {   
        static constexpr int channel_count = AccessSrc::channel_total - 1;
        std::vector<T0> pixels;
        std::vector<T0> alpha;

        auto width = src.width();
        pixels.resize(width * src.height() * channel_count);
        alpha.resize(width * src.height());

        src.template forEachPixelColor<T0, true>([width, &pixels, &alpha](int x, int y, std::array<T0, AccessSrc::channel_total> const &color) {
            auto pos = y * width + x;
            auto pos_c = pos * channel_count;
            for (auto c = 0; c < channel_count; c++) {
                pixels[pos_c + c] = color[c];
            }
            alpha[pos] = color[channel_count];
        });
        return {pixels, alpha};
    }   
};

} // namespace Inkscape::Renderer::PixelFilter

#endif // INKSCAPE_RENDERER_PIXEL_FILTER_PDF_IMAGE_BUILDER_H

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
