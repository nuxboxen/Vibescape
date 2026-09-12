// SPDX-License-Identifier: GPL-2.0-or-later

#include "renderer/pixel-filters/composite.h"
#include "pixel-access-testbase.h"

using namespace Inkscape::Renderer::PixelFilter;

TEST(PixelFilterCompositeTest, CompositeArithmetic)
{
    EXPECT_TRUE(FilterColors(CompositeArithmetic(0.5, 0.5, 0.2, 0.5), {0.7, 1.0, 0.5, 1.0},
                                {1.0, 0.0, 0.0, 1.0},     // input1
                                {{0.0, 1.0, 0.0, 1.0}})); // input2
    EXPECT_TRUE(FilterColors(CompositeArithmetic(0.2, 0.2, 0.2, 0.8), {0.8, 1.0, 1.0, 1.0},
                                {0.0, 0.0, 1.0, 1.0},
                                {{0.0, 1.0, 0.0, 1.0}}));
    EXPECT_TRUE(FilterColors(CompositeArithmetic(1.5, 0.5, 0.5, 0.0), {0.131, 0.297, 0.731, 0.875},
                                {0.2, 0.4, 0.8, 0.5},
                                {{0.2, 0.4, 0.8, 0.5}}));
}

TEST(PixelFilterCompositeTest, CompositeArithmeticInt)
{
    EXPECT_TRUE(FilterColors<MEMORY_FORMAT_ARGB32>(CompositeArithmetic(0.5, 0.5, 0.2, 0.5), {0.698, 1, 0.498, 1.0},
                                {1.0, 0.0, 0.0, 1.0},     // input1
                                {{0.0, 1.0, 0.0, 1.0}})); // input2
    EXPECT_TRUE(FilterColors<MEMORY_FORMAT_ARGB32>(CompositeArithmetic(0.2, 0.2, 0.2, 0.8), {0.8, 1.0, 1.0, 1.0},
                                {0.0, 0.0, 1.0, 1.0},
                                {{0.0, 1.0, 0.0, 1.0}}));
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
