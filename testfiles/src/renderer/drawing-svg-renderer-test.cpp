// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "inkscape.h"
#include "document.h"
#include "object/sp-item.h"

#include "color-testbase.h"
#include "surface-testbase.h"

#include "renderer/drawing/svg-renderer.h"

// DEBUG

#include "extension/db.h"
#include "extension/init.h"
#include "extension/output.h"
#include "extension/system.h"

using namespace Inkscape::Renderer;
using namespace Inkscape::Colors;
using namespace std::literals;

class SurfaceSvgFactory : public ::testing::Test
{
public:
    static void SetUpTestCase() {
        Inkscape::Application::create(false);
    }

    void SetUp() override
    {
        constexpr auto docString = R"A(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg version="1.1" width="21" height="21" xmlns="http://www.w3.org/2000/svg">
    <g id="g1">
      <rect style="fill:#00ff00;" width="6" height="6" x="12" y="3" />
      <circle style="fill:#ff0000;" cx="6" cy="6" r="3" />
    </g>
    <g id="g2">
      <rect style="fill:#0000ff;" width="6" height="6" x="3" y="12" />
      <path style="fill:#000000;" d="m 12,18 3,-6 3,6 z"/>
    </g>
</svg>)A"sv;

        _doc = SPDocument::createNewDocFromMem(docString);

        ASSERT_TRUE(_doc);
        ASSERT_TRUE(_doc->getRoot());
    }

    std::unique_ptr<SPDocument> _doc;
};

TEST_F(SurfaceSvgFactory, RenderWholeCanvas)
{
    auto surface = SvgRenderer().render(_doc.get());
    EXPECT_EQ(surface->width(), 21);
    EXPECT_EQ(surface->height(), 21);
    // The svg will generate an integer based surface
    EXPECT_EQ(surface->format(), CAIRO_FORMAT_ARGB32);
    EXPECT_FALSE(surface->getColorSpace());

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        " ** && "
        " *O && "
        "       "
        " && .. "
        " && OO "
        "       "
    );
}

TEST_F(SurfaceSvgFactory, RenderSmallArea)
{
    auto factory = SvgRenderer();
    factory.set_device_scale(3);
    factory.set_area(Geom::Rect(6, 6, 15, 15));
    auto surface = factory.render(_doc.get());
    EXPECT_EQ(surface->width(), 9);
    EXPECT_EQ(surface->height(), 9);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "&&X   &&&"
        "&&+   &&&"
        "X+    &&&"
        "         "
        "         "
        "         "
        "&&&     ."
        "&&&     O"
        "&&&    .&"
    );
}

TEST_F(SurfaceSvgFactory, RenderOneItemWholeCanvas)
{
    auto item = cast<SPItem>(_doc->getObjectById("g1"));
    auto factory = SvgRenderer();
    factory.set_item_limit({item});
    auto surface = factory.render(_doc.get());
    EXPECT_EQ(surface->width(), 21);
    EXPECT_EQ(surface->height(), 21);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        " ** && "
        " *O && "
        "       "
        "       "
        "       "
        "       "
    );
}

TEST_F(SurfaceSvgFactory, RenderOneItemArea)
{
    _doc->ensureUpToDate(); // Allow item bounds to be known
    auto item = cast<SPItem>(_doc->getObjectById("g1"));
    auto factory = SvgRenderer();
    factory.set_device_scale(3);
    auto surface = factory.render(item);
    ASSERT_TRUE(surface);
    EXPECT_EQ(surface->width(), 15);
    EXPECT_EQ(surface->height(), 6);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        " +XX+    &&&&&&"
        "+&&&&+   &&&&&&"
        "X&&&&X   &&&&&&"
        "X&&&&X   &&&&&&"
        "+&&&&+   &&&&&&"
        " +XX+    &&&&&&"
    );
}

TEST_F(SurfaceSvgFactory, RenderDoubleSize)
{
    auto factory = SvgRenderer();
    factory.set_dpi(96 * 2);
    auto surface = factory.render(_doc.get());

    EXPECT_EQ(surface->width(), 42);
    EXPECT_EQ(surface->height(), 42);

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "              "
        "              "
        "  .XX.  &&&&  "
        "  X&&X  &&&&  "
        "  X&&X  &&&&  "
        "  .XX.  &&&&  "
        "              "
        "              "
        "  &&&&   ..   "
        "  &&&&   OO   "
        "  &&&&  .&&.  "
        "  &&&&  O&&O  "
        "              "
        "              ");
}

TEST_F(SurfaceSvgFactory, RenderRGBColorSpace)
{
    auto factory = SvgRenderer();
    factory.set_final_color_space(rgb);

    auto surface = factory.render(_doc.get());
    EXPECT_EQ(surface->format(), CAIRO_FORMAT_RGBA128F);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        " ** && "
        " *O && "
        "       "
        " && .. "
        " && OO "
        "       ");
}

TEST_F(SurfaceSvgFactory, RenderCMYKIccColorSpace)
{
    ASSERT_TRUE(_doc->setColorSpace(cmyk_icc));

    auto surface = SvgRenderer().render(_doc.get());
    EXPECT_EQ(surface->format(), CAIRO_FORMAT_RGBA128F);
    EXPECT_EQ(surface->getColorSpace()->getName(), "Artifex-CMYK-SWOP-Profile");

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        " ** && "
        " *O && "
        "       "
        " && .. "
        " && OO "
        "       ");
}

TEST_F(SurfaceSvgFactory, RenderCMYKColorSpace)
{
    auto factory = SvgRenderer();
    factory.set_final_color_space(cmyk_icc);

    auto surface = factory.render(_doc.get());

    EXPECT_EQ(surface->format(), CAIRO_FORMAT_RGBA128F);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        " ** && "
        " *O && "
        "       "
        " && .. "
        " && OO "
        "       ");
}

TEST_F(SurfaceSvgFactory, RenderCheckerboard)
{
    auto factory = SvgRenderer();
    factory.set_checkerboard(Colors::Color(0x000000ff), Colors::Color(0xffffffff));
    auto surface = factory.render(_doc.get());
    EXPECT_IMAGE_IS<PixelPatch::Method::COLORS>(*surface,
        "Z.Z.Z.Z"
        ".22Z88."
        "Z22.88Z"
        ".Z.Z.Z."
        "ZPP.Z.Z"
        ".PPZ..."
        "Z.Z.Z.Z");
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
