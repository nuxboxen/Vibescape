// SPDX-License-Identifier: GPL-2.0-or-later

#include "renderer/pixel-filters/color-space.h"
#include "pixel-access-testbase.h"
#include "color-testbase.h"

#include "../test-utils.h"

using namespace Inkscape::Renderer::PixelFilter;

TEST(PixelColorSpaceStepTest, spaceToProfileFast)
{
    TestCairoSurface<3> sb1{2000, 2000}; // Speed testing
    TestCairoSurface<3> sb2{2000, 2000};

    sb1.rect(5, 5, 20, 20, {0.415, 0.514, 0.565, 0.8});
    ColorSpaceTransform(hsl, rgb).transform_space_profile<true>(*sb1._d, *sb2._d);
    // The output to this function is NOT alpha premultiplied, delibrately.
    EXPECT_TRUE(ColorIs(*sb2._d, 10, 10, {0.341, 0.789, 0.561, 0.8}, false));
}

TEST(PixelColorSpaceStepTest, spaceToProfileSlow)
{
    TestCairoSurface<4> sb3{1000, 1000}; // Speed testing
    TestCairoSurface<3> sb4{1000, 1000};

    sb3.rect(5, 5, 20, 20, {1.0, 0.0, 0.0, 0.1, 0.8});
    ColorSpaceTransform(cmyk_cpp, rgb).transform_space_profile<true>(*sb3._d, *sb4._d);
    // The output to this function is NOT alpha premultiplied, delibrately.
    EXPECT_TRUE(ColorIs(*sb4._d, 10, 10, {0.0, 0.9, 0.9, 0.8}, false));
}

TEST(PixelColorSpaceStepTest, profileToProfile)
{
    for (auto is_alpha_premultiplied = 0; is_alpha_premultiplied < 2; is_alpha_premultiplied++) {
        TestCairoSurface<4> cmyk_surface{600, 600}; // Speed testing

        auto div = is_alpha_premultiplied ? 1.0 : 0.5;
        cmyk_surface.rect(5, 5, 20, 20, {0.5 / div, 0.0, 0.0, 0.5 / div, 0.5});
        auto cmyk_copy = cmyk_surface._d->createContiguousCopy();
        auto cmyk_out = cmyk_surface._d->createContiguousEmpty();

        ColorSpaceTransform(cmyk_icc, cmyk_icc).transform_lcms(cmyk_copy, cmyk_out, is_alpha_premultiplied);
        // The output to this function is NOT alpha premultiplied, via lcms2 restriction.
        EXPECT_TRUE(ColorIs(cmyk_out, 10, 10, {0.777, 0.418, 0.316, 0.0435, 0.5}, false));
    }
}

TEST(PixelColorSpaceStepTest, profileToSpaceFast)
{
    TestCairoSurface<3> sb1{2000, 2000}; // Speed testing
    TestCairoSurface<3> sb2{2000, 2000};

    // The input to this function is NOT alpha premultiplied, but cairo will
    // premultiply them on painting the rect so we preunpremultiply to test
    sb1.rect(5, 5, 20, 20, {0.341 / 0.8, 0.789 / 0.8, 0.561 / 0.8, 0.8});
    ColorSpaceTransform(rgb, hsl).transform_space_profile<false>(*sb1._d, *sb2._d);
    EXPECT_TRUE(ColorIs(*sb2._d, 10, 10, {0.415, 0.514, 0.565, 0.8}, true));
}

TEST(PixelColorSpaceStepTest, profileToSpaceSlow)
{
    TestCairoSurface<4> sb3{1000, 1000}; // Speed testing
    TestCairoSurface<3> sb2{1000, 1000};

    sb2.rect(5, 5, 20, 20, {0.0, 0.5 / 0.8, 0.5 / 0.8, 0.8});
    ColorSpaceTransform(rgb, cmyk_cpp).transform_space_profile<false>(*sb2._d, *sb3._d);
    EXPECT_TRUE(ColorIs(*sb3._d, 10, 10, {1.0, 0.0, 0.0, 0.5, 0.8}, true));
}

/* ===== A to B conversion test ===== */

struct tr : traced_data
{
    const std::shared_ptr<Space::AnySpace> from;
    const std::shared_ptr<Space::AnySpace> to;
    const std::vector<double> in;
    const double epsilon = 0.01;
};
void PrintTo(const tr &obj, std::ostream *oo)
{
    auto id = [](std::string name) {
        std::stringstream ss(name);
        std::string out;
        std::string t;
        while (getline(ss, t, '-')) out += (out.empty() ? "" : "_") + t;
        while (getline(ss, t, ' ')) out += (out.empty() ? "" : "_") + t;
        return out;
    };
    *oo << id(obj.from->getName()) << "_to_" << id(obj.to->getName());
}

class testConversions : public testing::TestWithParam<tr>
{
public:
    ::testing::AssertionResult testTr(tr &data)
    {
        auto a = data.in;
        if (a.size() == 4) {
            return t1<4>(data, {a[0], a[1], a[2], a[3]}, data.epsilon);
        } else if (a.size() == 5) {
            return t1<5>(data, {a[0], a[1], a[2], a[3], a[4]}, data.epsilon);
        }
        return ::testing::AssertionFailure() << "Unknown input color size (" << a.size() << ")";
    }
    template <int SrcSize>
    ::testing::AssertionResult t1(tr &data, std::array<double, SrcSize> in, double epsilon)
    {
        auto color = Inkscape::Colors::Color(data.from, data.in);
        auto b = color.converted(data.to)->getValues();
        if (b.size() == 4) {
            return t2<SrcSize, 4>(data, in, {b[0], b[1], b[2], b[3]}, epsilon);
        } else if (b.size() == 5) {
            return t2<SrcSize, 5>(data, in, {b[0], b[1], b[2], b[3], b[4]}, epsilon);
        }
        return ::testing::AssertionFailure() << "Unknown output color size (" << b.size() << ")";
    }
    template <int SrcSize, int DstSize>
    ::testing::AssertionResult t2(tr &data, std::array<double, SrcSize> in, std::array<double, DstSize> out, double epsilon)
    {
        TestCairoSurface<SrcSize-1> s1{1, 1};
        TestCairoSurface<DstSize-1> s2{1, 1};
        s1.rect(0, 0, 52, 52, in);
        ColorSpaceTransform(data.from, data.to).filter(*s2._d, *s1._d);
        return ColorIs(*s2._d, 0, 0, out, true, epsilon);
    }
};

TEST_P(testConversions, convert)
{
    auto data = GetParam();
    auto scope = data.enable_scope();
    EXPECT_TRUE(testTr(data));
}

INSTANTIATE_TEST_SUITE_P(PixelColorSpaceTest, testConversions, testing::Values(
      _P(tr, rgb,      rgb,      {0.5, 0.0, 0.25, 0.5})
    , _P(tr, rgb,      lrgb,     {0.5, 0.0, 0.25, 0.5})
    , _P(tr, rgb,      hsl,      {0.5, 0.0, 0.25, 0.5})
    , _P(tr, rgb,      oklab,    {0.5, 0.0, 0.25, 0.5})
    , _P(tr, rgb,      cmyk_cpp, {0.5, 0.0, 0.25, 0.5})
    , _P(tr, rgb,      cmyk_icc, {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     rgb,      {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     lrgb,     {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     hsl,      {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     oklab,    {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     cmyk_cpp, {0.5, 0.0, 0.25, 0.5})
    , _P(tr, lrgb,     cmyk_icc, {0.5, 0.0, 0.25, 0.5})
    , _P(tr, hsl,      rgb,      {0.5, 0.5, 0.25, 0.5})
    , _P(tr, hsl,      lrgb,     {0.5, 0.5, 0.25, 0.5})
    , _P(tr, hsl,      hsl,      {0.5, 0.5, 0.25, 0.5})
    , _P(tr, hsl,      oklab,    {0.5, 0.5, 0.25, 0.5})
    , _P(tr, hsl,      cmyk_cpp, {0.5, 0.5, 0.25, 0.5})
    , _P(tr, hsl,      cmyk_icc, {0.5, 0.5, 0.25, 0.5})
    , _P(tr, oklab,    rgb,      {0.5, 0.2, 0.25, 0.5})
    , _P(tr, oklab,    lrgb,     {0.5, 0.2, 0.25, 0.5})
    , _P(tr, oklab,    hsl,      {0.5, 0.2, 0.25, 0.5})
    , _P(tr, oklab,    oklab,    {0.5, 0.2, 0.25, 0.5})
    , _P(tr, oklab,    cmyk_cpp, {0.5, 0.2, 0.25, 0.5})
    , _P(tr, oklab,    cmyk_icc, {0.5, 0.2, 0.25, 0.5})
    , _P(tr, cmyk_cpp, rgb,      {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_cpp, lrgb,     {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_cpp, hsl,      {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_cpp, oklab,    {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_cpp, cmyk_cpp, {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_cpp, cmyk_icc, {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_icc, rgb,      {0.5, 0.0, 0.25, 0.1, 0.5})
    // There's a 2x varience between the lcms2 with alpha premultiplied and without
    // We don't know why this might be the case as it works for sRGB correctly
    , _P(tr, cmyk_icc, lrgb,     {0.5, 0.0, 0.25, 0.1, 0.5}, 0.02)
    , _P(tr, cmyk_icc, hsl,      {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_icc, oklab,    {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_icc, cmyk_cpp, {0.5, 0.0, 0.25, 0.1, 0.5})
    , _P(tr, cmyk_icc, cmyk_icc, {0.5, 0.0, 0.25, 0.1, 0.5})
), testing::PrintToStringParamName());

TEST(PixelColorSpaceTest, LuminosityToAlpha)
{
    TestCairoSurface<0> a1{21, 21};
    TestCairoSurface<3> c1{21, 21};

    c1.rect(0,  3,  21, 3,  {0.0, 0.9, 0.0, 0.5});
    c1.rect(15, 0,  3,  21, {0.5, 0.5, 0.5, 0.5});
    c1.rect(0,  15, 21, 3,  {0.9, 0.0, 0.0, 0.5});
    c1.rect(3,  0,  3,  21, {0.0, 0.0, 0.9, 0.5});

    ColorSpaceTransform(rgb, alpha).filter(*a1._d, *c1._d);

    EXPECT_TRUE(ImageIs(*a1._d,
                " .   - "
                "*.***+*"
                " .   - "
                " .   - "
                " .   - "
                ":.:::::"
                " .   - ",
                PixelPatch::Method::ALPHA, true));

}

TEST(PixelColorSpaceTest, RGBAToAlpha)
{
    TestCairoSurface<3> s1{60,60};
    TestCairoSurface<0> a1{60,60};

    s1.rect(6, 6, 20, 20, {1.0, 0.0, 0.75, 0.5});
    AlphaSpaceExtraction().filter(*a1._d, *s1._d);
    EXPECT_TRUE(ColorIs(*a1._d, 10, 10, {0.5}));
}

TEST(PixelColorSpaceTest, CMYKAToAlpha)
{
    TestCairoSurface<4> s3{60,60};
    TestCairoSurface<0> a1{60,60};

    s3.rect(6, 6, 20, 20, {1.0, 0.0, 0.4, 0.75, 0.5});
    AlphaSpaceExtraction().filter(*a1._d, *s3._d);
    EXPECT_TRUE(ColorIs(*a1._d, 10, 10, {0.5}));
}

TEST(PixelColorSpaceTest, AlphaToLuminosity)
{
    TestCairoSurface<0> a1{21, 21};
    TestCairoSurface<3> c1{21, 21};

    c1.rect(0,  3,  21, 3,  {0.0, 0.9, 0.0, 0.5});
    c1.rect(15, 0,  3,  21, {0.5, 0.5, 0.5, 0.5});
    c1.rect(0,  15, 21, 3,  {0.9, 0.0, 0.0, 0.5});
    c1.rect(3,  0,  3,  21, {0.0, 0.0, 0.9, 0.5});

    AlphaSpaceExtraction().filter(*a1._d, *c1._d);

    // Notice that this result is different from LuminosityToAlpha
    // despite the same c1 source. That extracts luminosity of the
    // color channels while this discards color and extracts alpha.
    EXPECT_TRUE(ImageIs(*a1._d,
                " -   - "
                "-O---O-"
                " -   - "
                " -   - "
                " -   - "
                "-O---O-"
                " -   - ",
                PixelPatch::Method::ALPHA, true));
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
