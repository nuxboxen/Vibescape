// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <2geom/svg-path-parser.h>

#include "renderer/context.h"
#include "renderer/context-pattern.h"
#include "renderer/context-pattern-complex.h"
#include "renderer/surface.h"

#include "color-testbase.h"
#include "surface-testbase.h"

using namespace Inkscape::Renderer;
using namespace Inkscape::Colors;

class RenderContextPatternTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        size = {21, 21};
        bounds = {0, 0, 21, 21};
        scale = {1, 1};

        surface = std::make_shared<Surface>(size, 1, cmyk_cpp);
        context = std::make_unique<Context>(*surface, bounds.min(), scale);
    }

    auto get_default_pattern(int count = 3) {
        auto pattern = std::make_unique<LinearGradientPattern>(cmyk_cpp, 0.0, 0.0, 21.0, 21.0);
        pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
        pattern->addColorStop(0.0, Colors::Color(cmyk_cpp, {1.0, 0.0, 0.0, 0.3, 1.0}));
        if (count > 1)
            pattern->addColorStop(0.5, Colors::Color(cmyk_cpp, {0.0, 1.0, 0.0, 0.4, 1.0}));
        if (count > 2)
            pattern->addColorStop(1.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 1.0, 0.5, 0.0}));
        return pattern;
    }

    Geom::IntPoint size = {21, 21};
    Geom::IntRect bounds;
    Geom::Scale scale;
    std::shared_ptr<Surface> surface;
    std::unique_ptr<Context> context;
};

TEST_F(RenderContextPatternTest, SetPatternSolidColor)
{

    auto pattern = std::make_unique<SolidColorPattern>(Colors::Color(cmyk_cpp, {0.0, 1.0, 0.0, 0.6, 1.0}));

    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    " nnnnn "
                    "       ");
}

TEST_F(RenderContextPatternTest, SetPatternSolidColorOpacity)
{
    auto pattern = std::make_unique<SolidColorPattern>(Colors::Color(rgb, {0.0, 0.0, 0.0, 0.6}));

    auto s2 = std::make_shared<TestSurface>(size, 1, rgb);
    {
        context = std::make_unique<Context>(*s2, bounds.min(), scale);
        context->setSource(*pattern);
        context->rectangle(Geom::Rect(3, 3, 18, 18));
        context->fill();
    }

    EXPECT_TRUE(VectorIsNear(s2->get_pixel(5, 5), {0, 0, 0, 0.6}, 0.01));

    EXPECT_IMAGE_IS(*s2,
                    "       "
                    " ..... "
                    " ..... "
                    " ..... "
                    " ..... "
                    " ..... "
                    "       ");
}

TEST_F(RenderContextPatternTest, SetPatternSurface)
{
    auto image_s = std::make_shared<Surface>(Geom::IntPoint(9, 9), 1, cmyk_cpp);
    auto image_ct = std::make_unique<Context>(*image_s, bounds.min(), scale);

    image_ct->setSource(Color(cmyk_cpp, {0.7, 0, 0.7, 0.2, 0.7}));
    image_ct->setLineWidth(1);
    image_ct->moveTo({0, 0});
    image_ct->lineTo({9, 9});
    image_ct->stroke();

    auto pattern = std::make_unique<Pattern>(*image_s);
    pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " R  R  "
                    "  RR   "
                    "  RR   "
                    " R  R  "
                    "     R "
                    "       ");
}

TEST_F(RenderContextPatternTest, GradientStopData)
{
    auto pattern = get_default_pattern();
    EXPECT_EQ(pattern->numColorStops(), 3);
    EXPECT_EQ(get_default_pattern(1)->numColorStops(), 1);

    auto [o1, c1] = pattern->getColorStop(0);
    auto [o2, c2] = pattern->getColorStop(1);
    auto [o3, c3] = pattern->getColorStop(2);

    EXPECT_EQ(o1, 0.0);
    EXPECT_EQ(o2, 0.5);
    EXPECT_EQ(o3, 1.0);
    EXPECT_EQ(c1.getValues()[3], 0.3);
    EXPECT_EQ(c2.getValues()[3], 0.4);
    EXPECT_EQ(c3.getValues()[3], 0.5);
}

TEST_F(RenderContextPatternTest, GradientStopCopy)
{
    auto source = get_default_pattern();
    auto pattern = std::make_unique<LinearGradientPattern>(cmyk_cpp, 21.0, 0.0, 0.0, 0.0);
    pattern->copyColorStops(*source);
    EXPECT_EQ(pattern->numColorStops(), 3);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fillPreserve();
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
                    "       "
                    " :O$&& "
                    " :O$&& "
                    " :O$&& "
                    " :O$&& "
                    " :O$&& "
                    "       ");
}

TEST_F(RenderContextPatternTest, LinearGradient)
{
    auto pattern = get_default_pattern();
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(0, 0, 21, 21));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "2228888"
                    "2288888"
                    "2888888"
                    "8888888"
                    "888888P"
                    "88888PP"
                    "8888PPP");
}

TEST_F(RenderContextPatternTest, EasedGradient)
{
    // This is CSS easing function
    auto steps = CubicBezierEasingSteps({0.25, 0.1}, {0.25, 1});
    auto pattern = std::make_unique<LinearGradientPattern>(cmyk_cpp, 0.0, 0.0, 0.0, 21.0);
    pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
    pattern->addColorStop(0.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 1.0}));
    pattern->addColorStop(1.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 0.0}), steps);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(0, 0, 21, 21));
    context->fill();

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
                    "XXXXXXX"
                    "ooooooo"
                    ":::::::"
                    "......."
                    "......."
                    "       "
                    "       ");
}

TEST_F(RenderContextPatternTest, UneasedGradient)
{
    // This is CSS easing function
    auto steps = CubicBezierEasingSteps({0.25, 0.1}, {0.25, 1});
    steps.invert();

    auto pattern = std::make_unique<LinearGradientPattern>(cmyk_cpp, 0.0, 0.0, 0.0, 21.0);
    pattern->setExtend(Cairo::Pattern::Extend::REFLECT);
    pattern->addColorStop(0.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 1.0}));
    pattern->addColorStop(1.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 0.0}), steps);
    context->setSource(*pattern);
    context->rectangle(Geom::Rect(0, 0, 21, 21));
    context->fill();

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
                    "$$$$$$$"
                    "XXXXXXX"
                    "xxxxxxx"
                    "OOOOOOO"
                    "======="
                    ":::::::"
                    ".......");
}

TEST_F(RenderContextPatternTest, PatternMatrix)
{
    auto pattern = get_default_pattern();
    context->setSource(*pattern);
    pattern->setMatrix(Geom::Rotate(45));
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS(*surface,
                    "       "
                    " 22888 "
                    " 22888 "
                    " 22988 "
                    " 22688 "
                    " 22288 "
                    "       ");
}

TEST_F(RenderContextPatternTest, PatternMatrixBox)
{
    auto pattern = LinearGradientPattern(cmyk_cpp, 0.0, 0.0, 1.0, 1.0);
    pattern.setExtend(Cairo::Pattern::Extend::REFLECT);
    pattern.addColorStop(0.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 1.0}));
    pattern.addColorStop(1.0, Colors::Color(cmyk_cpp, {1.0, 1.0, 1.0, 1.0, 1.0}));
    pattern.setMatrix(Geom::identity(), Geom::Rect(0, 0, 21, 21));
    context->setSource(pattern);
    context->rectangle(Geom::Rect(0, 0, 21, 21));
    context->fill();

    EXPECT_IMAGE_IS<PixelPatch::Method::LIGHT>(*surface,
                    "   ...:"
                    "  ...::"
                    " ...::-"
                    "...::-+"
                    "..::-+="
                    ".::-+=o"
                    "::-+=oO");
}

TEST_F(RenderContextPatternTest, PatternMatrixRadial)
{
    auto pattern = RadialGradientPattern(cmyk_cpp, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0);
    pattern.setExtend(Cairo::Pattern::Extend::REFLECT);
    pattern.addColorStop(0.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 1.0}));
    pattern.addColorStop(1.0, Colors::Color(cmyk_cpp, {1.0, 1.0, 1.0, 1.0, 1.0}));
    pattern.setMatrix(Geom::Translate(-1, 0), Geom::Rect(0, 0, 21, 21));
    context->setSource(pattern);
    context->rectangle(Geom::Rect(0, 0, 21, 21));
    context->fill();

    EXPECT_IMAGE_IS<PixelPatch::Method::LIGHT>(*surface,
                    "OO*Oo=+"
                    "=ooO*O="
                    "-+=oO*o"
                    "::-+oOO"
                    "..:-=o*"
                    "...:+oO"
                    " ..:-=O");
}

TEST_F(RenderContextPatternTest, RadialGradient)
{
    auto pattern = std::make_unique<RadialGradientPattern>(cmyk_cpp, 10.0, 10.0, 9.0, 10.0, 10.0, 0.0);
    pattern->addColorStop(0.0, Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 0.0}));
    pattern->addColorStop(1.0, Colors::Color(cmyk_cpp, {1.0, 0.0, 0.0, 0.0, 1.0}));

    context->setSource(*pattern);
    context->rectangle(Geom::Rect(3, 3, 18, 18));
    context->fill();

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
                    "       "
                    "  .:.  "
                    " .=o-. "
                    " :ox=. "
                    " .-=:  "
                    "  ..   "
                    "       ");
}

TEST_F(RenderContextPatternTest, Checkerboard)
{
    auto white = Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 0.0, 1.0});
    auto black = Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 1.0, 1.0});

    context->paint(CheckerboardPattern(white, black, 3));
    EXPECT_IMAGE_IS(*surface,
                    "f.f.f.f"
                    ".f.f.f."
                    "f.f.f.f"
                    ".f.f.f."
                    "f.f.f.f"
                    ".f.f.f."
                    "f.f.f.f");

    context->paint(CheckerboardPattern(white, black, 6));
    EXPECT_IMAGE_IS(*surface,
                    "ff..ff."
                    "ff..ff."
                    "..ff..f"
                    "..ff..f"
                    "ff..ff."
                    "ff..ff."
                    "..ff..f"
            );
}

TEST_F(RenderContextPatternTest, Stripes)
{
    auto black = Colors::Color(cmyk_cpp, {0.0, 0.0, 0.0, 1.0, 1.0});
    context->paint(StripesPattern(black));
    EXPECT_IMAGE_IS(*surface,
                    "  ffp p"
                    " ffp pf"
                    "ffp pff"
                    "fp pff "
                    "p pff  "
                    " pff  f"
                    "pff  ff");
}

TEST_F(RenderContextPatternTest, GradientShadow)
{
    auto image_s = std::make_shared<Surface>(Geom::IntPoint(35, 35), 1, cmyk_cpp);
    {
        auto image_ct = Context(*image_s, bounds.min(), scale);
        auto color = Colors::Color(cmyk_cpp, {0, 0, 0, 0, 1.0});
        auto rect = Geom::Rect(Geom::Point{5, 5}, {15, 15});
        auto comp = GradientShadow(rect, 25, {15, 15}, color);
        image_ct.paint(comp);
    }

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*image_s,
         ".::::::::.."
         ":O+++x*o+::"
         ":+   $xo+::"
         ":+   $xo+::"
         ":+   $xo+::"
         ":x$$$X*o+::"
         ":*xxx*O=+::"
         ":ooooo=+-:."
         ":++++++-::."
         ".::::::::.."
         ".::::::....");
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
