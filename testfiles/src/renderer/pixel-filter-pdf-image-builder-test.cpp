// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "renderer/pixel-filters/pdf-image-builder.h"
#include "pixel-access-testbase.h"

using namespace Inkscape::Renderer::PixelFilter;

TEST(PixelPdfBuildImageTest, DataFormatInt)
{
    TestSurface<MEMORY_FORMAT_CMYA_KA256F> surface{3, 3};
    surface.rect(1, 1, 1, 1, {0.0, 1.0, 0.5, 0.1, 0.5});
    
    EXPECT_TRUE(VectorIsNear(surface._d->colorAt((unsigned)0, 0, true), {0, 0,   0,   0, 0.0}, 0.001));
    EXPECT_TRUE(VectorIsNear(surface._d->colorAt((unsigned)1, 1, true), {0, 1, 0.5, 0.1, 0.5}, 0.001));

    auto [pixels, alpha] = BuildPdfImageData<uint8_t>().filter(*surface._d);

    std::vector<uint8_t> exp0 = {
        0, 0, 0, 0,    0,   0,   0,  0,    0, 0, 0, 0,
        0, 0, 0, 0,    0, 255, 127, 25,    0, 0, 0, 0,
        0, 0, 0, 0,    0,   0,   0,  0,    0, 0, 0, 0,
    };
    EXPECT_EQ(pixels, exp0);

    std::vector<uint8_t> exp1 = {
        0,   0, 0,
        0, 127, 0,
        0,   0, 0};
    EXPECT_EQ(alpha, exp1);
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
