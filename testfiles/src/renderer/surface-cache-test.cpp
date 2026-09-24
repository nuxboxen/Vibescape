// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "color-testbase.h"
#include "surface-testbase.h"
#include "renderer/surface-cache.h"

using namespace Inkscape::Renderer;

TEST(DrawingSurfaceCacheTest, DefaultIsDirty)
{
    auto cache = SurfaceCache({0, 0, 100, 100}, 1, int_rgb);
    Geom::OptIntRect area = Geom::IntRect(0, 0, 100, 100);
    auto clean = cache.getCleanRegionWithin(area);

    ASSERT_TRUE(clean->empty());
    ASSERT_TRUE(area);
    EXPECT_EQ(area->dimensions()[Geom::X], 100);
    EXPECT_EQ(area->dimensions()[Geom::Y], 100);
}

TEST(DrawingSurfaceCacheTest, MarkWholeImage)
{
    auto cache = SurfaceCache({0, 0, 100, 100}, 1, int_rgb);

    Geom::OptIntRect area = Geom::IntRect(0, 0, 100, 100);
    cache.markClean(*area);

    auto clean = cache.getCleanRegionWithin(area);
    ASSERT_FALSE(clean->empty());
    ASSERT_FALSE(area);

    area = Geom::IntRect(0, 0, 100, 100);
    cache.markDirty(*area);

    clean = cache.getCleanRegionWithin(area);
    ASSERT_TRUE(clean->empty());
    ASSERT_TRUE(area);
}

TEST(DrawingSurfaceCacheTest, PaintWholeImage)
{
    Geom::OptIntRect area = Geom::IntRect(100, 100, 200, 200);
    auto cache = SurfaceCache(*area, 1, int_rgb);
    auto src = Surface({100, 100}, 1, int_rgb);

    {
        auto ctx = Context(src, area->min());
        ctx.rectangle(120, 120, 60, 60);
        ctx.fill();
    }

    cache.paintToCache(src, *area);
    auto clean = cache.getCleanRegionWithin(area);
    ASSERT_FALSE(clean->empty());
    ASSERT_FALSE(area);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(cache,
         "            "
         "            "
         "  .-------  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "            "
         "            ", 8);

    area = Geom::IntRect(100, 100, 200, 200);
    auto dst = Surface({100, 100}, 1, int_rgb);
    {
        auto ctx = Context(dst, area->min());
        cache.paintFromCache(ctx, area, false);
    }
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(dst,
         "            "
         "            "
         "  .-------  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "            "
         "            ", 8);
}

TEST(DrawingSurfaceCacheTest, PaintPartImage)
{
    Geom::OptIntRect area = Geom::IntRect(100, 100, 200, 200);
    auto cache = SurfaceCache(*area, 1, int_rgb);
    auto src = Surface({100, 100}, 1, int_rgb);

    {
        auto ctx = Context(src, area->min());
        ctx.rectangle(120, 120, 60, 60);
        ctx.fill();
    }

    // Painting a strip in the middle doesn't shrink the dirty box
    cache.paintToCache(src, Geom::IntRect(100, 120, 200, 150));
    auto clean = cache.getCleanRegionWithin(area);
    ASSERT_TRUE(clean->empty());

    // Painting a stop at the top edge, does shink the dirty box
    cache.paintToCache(src, Geom::IntRect(100, 100, 200, 150));
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(cache,
         "            "
         "            "
         "  .-------  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "   .......  "
         "            "
         "            "
         "            "
         "            "
         "            ", 8);
    clean = cache.getCleanRegionWithin(area);
    auto geoms = cairo_to_geom(*clean);
    ASSERT_EQ(geoms.size(), 1);
    EXPECT_EQ(geoms[0].left(), 100);
    EXPECT_EQ(geoms[0].top(), 100);
    EXPECT_EQ(geoms[0].width(), 100);
    EXPECT_EQ(geoms[0].height(), 50);
    EXPECT_TRUE(area);
    EXPECT_EQ(area->left(), 100);
    EXPECT_EQ(area->top(), 150);
    EXPECT_EQ(area->width(), 100);
    EXPECT_EQ(area->height(), 50);

    // Paint the remainder to complete the cache
    area = Geom::IntRect(100, 100, 200, 200);
    cache.paintToCache(src, Geom::IntRect(100, 149, 200, 200));
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(cache,
         "            "
         "            "
         "  .-------  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "  -&&&&&&&  "
         "            "
         "            ", 8);

    clean = cache.getCleanRegionWithin(area);
    geoms = cairo_to_geom(*clean);
    ASSERT_EQ(geoms.size(), 1);
    EXPECT_EQ(geoms[0].left(), 100);
    EXPECT_EQ(geoms[0].top(), 100);
    EXPECT_EQ(geoms[0].width(), 100);
    EXPECT_EQ(geoms[0].height(), 100);
    EXPECT_FALSE(area);
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
