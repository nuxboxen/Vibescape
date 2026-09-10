// SPDX-License-Identifier: GPL-2.0-or-later

#include "renderer/pixel-filters/displacement-map.h"
#include "pixel-access-testbase.h"

using namespace Inkscape::Renderer::PixelFilter;

TEST(PixelFilterDisplacementTest, DisplacementMap)
{
    auto texture = TestSurface<MEMORY_FORMAT_CMYA_KA256F, PixelAccessEdgeMode::ZERO>(21, 21);
    texture.rect(3, 3, 15, 15, {0.5, 0.0, 0.0, 1.0, 1.0});

    // This map splits off the top, bottom, left and right rows and moves them out
    auto map = TestSurface<MEMORY_FORMAT_RGBA128F>(21, 21);
    for (auto x = 0; x < 21; x++) {
        for (auto y = 0; y < 21; y++) {
            auto x1 = x / 3;
            auto y1 = y / 3;
            map._d->colorTo(x, y,
                            {x1 == 0 || x1 == 5 ? 1.0 : (x1 == 1 || x1 == 6 ? 0.0 : 0.5),
                             y1 == 0 || y1 == 5 ? 1.0 : (y1 == 1 || y1 == 6 ? 0.0 : 0.5), 0.0, 1.0},
                            true);
        }
    }

    auto f = DisplacementMap(0, 1, 6, 6);
    auto dst = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    f.filter(*dst._d, *texture._d, *map._d);

    ASSERT_TRUE(ImageIs(*dst._d,
                        "h hhh h"
                        "       "
                        "h hhh h"
                        "h hhh h"
                        "h hhh h"
                        "       "
                        "h hhh h",
                        PixelPatch::Method::COLORS, true));
}

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
