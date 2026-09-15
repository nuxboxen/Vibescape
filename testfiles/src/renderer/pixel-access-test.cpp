// SPDX-License-Identifier: GPL-2.0-or-later

#include "pixel-access-testbase.h"

using namespace Inkscape;

/***** TEST DATA *******/
constexpr inline double dbl(bool a)
{
    return a ? 1.0 : 0.0;
}

struct TestFilter
{
    template <class AccessDst, class AccessSrc>
    void filter(AccessDst &dst, AccessSrc const &src)
    {
        for (int y = 0; y < dst.height(); y++) {
            for (int x = 0; x < dst.width(); x++) {
                if (x / 3 == y / 3) {
                    auto c = src.colorAt(x, y);
                    dst.colorTo(x, y, c);
                }
            }
        }
    }
};

/**** TESTS *****/

TEST(PixelAccessTest, ColorIs)
{
    for (auto format : {CAIRO_FORMAT_ARGB32, CAIRO_FORMAT_RGBA128F}) {
        auto cobj = cairo_image_surface_create(format, 21, 21);
        auto s = Cairo::RefPtr<Cairo::ImageSurface>(new Cairo::ImageSurface(cobj, true));

        // Draw something here
        {
            auto c = cairo_create(cobj);
            for (auto channel = 0; channel < (format == CAIRO_FORMAT_A8 ? 1 : 3); channel++) {
                cairo_rectangle(c, 3 + channel * 6, 3 + channel * 6, 6, 6);
                cairo_set_source_rgba(c, channel == 0, channel == 1, channel == 2, 0.6);
                cairo_fill(c);
            }
            cairo_destroy(c);
        }
        s->flush();

        ASSERT_TRUE(ImageIs(s, "       "
                               " 22    "
                               " 22    "
                               "   88  "
                               "   88  "
                               "     PP"
                               "     PP"))
        << "Format: " << get_cairo_format_name(format) << "\n"
        << "Method: INTEGER COORDS\n";

        // Bilinear should be the same as Int (just slower)
        ASSERT_TRUE(ImageIs(s,
                            "       "
                            " 22    "
                            " 22    "
                            "   88  "
                            "   88  "
                            "     PP"
                            "     PP",
                            PixelPatch::Method::COLORS, true, true))
            << "Format: " << get_cairo_format_name(format) << "\n"
            << "Method: BILINEAR DECIMAL COORDS (Premult)\n";

        ASSERT_TRUE(ImageIs(s,
                            "       "
                            " 22    "
                            " 22    "
                            "   88  "
                            "   88  "
                            "     PP"
                            "     PP",
                            PixelPatch::Method::COLORS, false, true))
            << "Format: " << get_cairo_format_name(format) << "\n"
            << "Method: BILINEAR DECIMAL COORDS (Unpremult)\n";
    }
}

TEST(PixelAccessTest, AlphaIs)
{
    for (auto format : {CAIRO_FORMAT_A8, CAIRO_FORMAT_ARGB32, CAIRO_FORMAT_RGBA128F}) {
        auto cobj = cairo_image_surface_create(format, 21, 21);
        auto s = Cairo::RefPtr<Cairo::ImageSurface>(new Cairo::ImageSurface(cobj, true));

        // Draw something here
        {
            auto c = cairo_create(cobj);
            cairo_rectangle(c, 3, 3, 15, 15);
            cairo_set_source_rgba(c, 0.0, 0.0, 0.0, 1.0);
            cairo_fill(c);
            cairo_destroy(c);
        }
        s->flush();

        ASSERT_TRUE(ImageIs(s,
                            "       "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            "       ",
                            PixelPatch::Method::ALPHA))
            << "Format: " << get_cairo_format_name(format) << "\n"
            << "Method: INTEGER COORDS\n";

        ASSERT_TRUE(ImageIs(s,
                            "       "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            " &&&&& "
                            "       ",
                            PixelPatch::Method::ALPHA, false, true))
            << "Format: " << get_cairo_format_name(format) << "\n"
            << "Method: BILINEAR FLOAT COORDS\n";

        if (format == CAIRO_FORMAT_A8) {
            ASSERT_TRUE(ImageIs(s, "       "
                                   " ..... "
                                   " ..... "
                                   " ..... "
                                   " ..... "
                                   " ..... "
                                   "       "))
                << "Format: " << get_cairo_format_name(format) << "\n"
                << "Method: Color for Alpha\n";
        }
    }
}

TEST(PixelAccessTest, ColorInIntegerFormat)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(4, 4);
    src.rect(1, 1, 2, 2, {1.0, 0.0, 0.2, 0.5, 0.5});

    EXPECT_TRUE(ColorIs<int>(*src._d, 1, 1, {1.0, 0.0, 0.2, 0.5, 0.5}, true));
    auto ret1 = ColorIs<int, uint8_t>(*src._d, 1, 1, {255, 0, 51, 127, 127}, true);
    EXPECT_TRUE(ret1);
    auto ret2 = ColorIs<int, uint16_t>(*src._d, 1, 1, {65535, 0, 13107, 32767, 32767}, true);
    EXPECT_TRUE(ret2);
    EXPECT_TRUE(ColorIs<double>(*src._d, 1.0, 1.0, {1.0, 0.0, 0.2, 0.5, 0.5}, true));
    auto ret3 = ColorIs<double, uint8_t>(*src._d, 1.0, 1.0, {255, 0, 51, 127, 127}, true);
    EXPECT_TRUE(ret1);
    auto ret4 = ColorIs<double, uint16_t>(*src._d, 1.0, 1.0, {65535, 0, 13107, 32767, 32767}, true);
    EXPECT_TRUE(ret2);
}

TEST(PixelAccessTest, BilinearInterpolation)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(4, 4);
    src.rect(1, 1, 2, 2, {1.0, 0.0, 1.0, 1.0});

    EXPECT_NEAR(src._d->alphaAt(0.5, 0.5), 0.25, 0.001);
    EXPECT_NEAR(src._d->alphaAt(2.5, 2.5), 0.25, 0.001);
    EXPECT_NEAR(src._d->alphaAt(0.5, 2.5), 0.25, 0.001);
    EXPECT_NEAR(src._d->alphaAt(2.5, 0.5), 0.25, 0.001);
    EXPECT_NEAR(src._d->alphaAt(1.5, 0.5), 0.50, 0.001);
    EXPECT_NEAR(src._d->alphaAt(0.5, 1.5), 0.50, 0.001);
    EXPECT_NEAR(src._d->alphaAt(0.3, 1.3), 0.6999, 0.001);

    EXPECT_TRUE(ColorIs<double>(*src._d, 0.5, 0.5, {1.0, 0.0, 1.0, 0.25}, true));
    EXPECT_TRUE(ColorIs<double>(*src._d, 0.5, 1.5, {1.0, 0.0, 1.0, 0.50}, true));
    EXPECT_TRUE(ColorIs<double>(*src._d, 0.3, 1.3, {1.0, 0.0, 1.0, 0.7}, true));
    EXPECT_TRUE(ColorIs<double>(*src._d, 0.5, 0.5, {0.25, 0.0, 0.25, 0.25}, false));
    EXPECT_TRUE(ColorIs<double>(*src._d, 0.5, 1.5, {0.5, 0.0, 0.5, 0.50}, false));
    EXPECT_TRUE(ColorIs<double>(*src._d, 0.3, 1.3, {0.7, 0.0, 0.7, 0.7}, false));
}

TEST(PixelAccessTest, CopyObject)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(4, 4);
    src.rect(1, 1, 2, 2, {1.0, 0.0, 1.0, 1.0});
    auto &access = *src._d;
    EXPECT_TRUE(ColorIs(access, 1, 1, {1.0, 0.0, 1.0, 1.0}));
    {
        auto access2 = access;
        EXPECT_TRUE(ColorIs(access, 1, 1, {1.0, 0.0, 1.0, 1.0}));
        EXPECT_TRUE(ColorIs(access2, 1, 1, {1.0, 0.0, 1.0, 1.0}));
        EXPECT_EQ(access.memory(), access2.memory());
    }
    EXPECT_TRUE(ColorIs(access, 1, 1, {1.0, 0.0, 1.0, 1.0}));
}

TEST(PixelAccessTest, UnmultiplyColor)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(4, 4);
    src.rect(1, 1, 2, 2, {1.0, 0.0, 1.0, 0.5});

    ASSERT_TRUE(ColorIs(*src._d, 1, 1, {0.5, 0.0, 0.5, 0.5}, false));
    ASSERT_TRUE(ColorWillBe(*src._d, 2, 2, {0.5, 0.5, 0.5, 0.5}, false));

    ASSERT_TRUE(ColorIs(*src._d, 1, 1, {1.0, 0.0, 1.0, 0.5}, true));
    ASSERT_TRUE(ColorWillBe(*src._d, 3, 3, {0.5, 0.5, 0.5, 0.5}, true));

    auto src2 = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(4, 4);
    src2.rect(1, 1, 2, 2, {1.0, 0.0, 1.0, 0.4, 0.5});

    ASSERT_TRUE(ColorIs(*src2._d, 1, 1, {0.5, 0.0, 0.5, 0.2, 0.5}, false));
    ASSERT_TRUE(ColorWillBe(*src2._d, 2, 2, {0.5, 0.5, 0.5, 0.5, 0.5}, false));

    ASSERT_TRUE(ColorIs(*src2._d, 1, 1, {1.0, 0.0, 1.0, 0.4, 0.5}, true));
    ASSERT_TRUE(ColorWillBe(*src2._d, 3, 3, {0.5, 0.5, 0.5, 0.5, 0.5}, true));
}

template <PixelAccessEdgeMode edge_mode>
PixelAccess<MEMORY_FORMAT_CMYA_KA256F, edge_mode> getEdgeModeSurface(int width, int height)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F, edge_mode>(width, height);

    // Draw a box of 4 channels, one edge per channel, overlaps at the corners
    for (int x = 0; x < (int)width; x++) {
        for (int y = 0; y < (int)height; y++) {
            src._d->colorTo(x, y, {dbl(y == 0), dbl(y == height - 1), dbl(x == 0), dbl(x == width - 1), 1.0});
        }
    }
    return *src._d;
}

TEST(PixelAccessTest, EdgeModeError)
{
    std::array<double, 5> c;
    auto d = getEdgeModeSurface<PixelAccessEdgeMode::ERROR>(4, 4);
    for (int x = -1; x < 5; x++) {
        for (int y = -1; y < 5; y++) {
            if (x < 0 || x > 3 || y < 0 || y > 3) {
                EXPECT_THROW(d.colorAt(x, y), std::exception);
                EXPECT_THROW(d.colorTo(x, y, c), std::exception);
            } else {
                // Checking for no error, and confirming the test-suite
                ASSERT_TRUE(ColorIs(d, x, y, {dbl(y == 0), dbl(y == 3), dbl(x == 0), dbl(x == 3), 1.0}, false));
                ASSERT_TRUE(ColorIs(d, x, y, {dbl(y == 0), dbl(y == 3), dbl(x == 0), dbl(x == 3), 1.0}, true));
                ASSERT_TRUE(ColorWillBe(d, x, y, {0.5, 0.5, 0.5, 0.5, 0.5}));
            }
        }
    }
}

TEST(PixelAccessTest, EdgeModeExtend)
{
    auto d = getEdgeModeSurface<PixelAccessEdgeMode::EXTEND>(4, 4);

    for (int x = -1; x < 5; x++) {
        for (int y = -1; y < 5; y++) {
            ASSERT_TRUE(ColorIs(d, x, y, {dbl(y <= 0), dbl(y >= 3), dbl(x <= 0), dbl(x >= 3), 1.0}, false));
            ASSERT_TRUE(ColorIs(d, x, y, {dbl(y <= 0), dbl(y >= 3), dbl(x <= 0), dbl(x >= 3), 1.0}, true));
            ASSERT_TRUE(
                ColorWillBe(d, x, y, {0.5, 0.5, 0.5, 0.5, 0.5}, true, std::clamp(x, 0, 3), std::clamp(y, 0, 3)));
        }
    }
}

TEST(PixelAccessTest, EdgeModeWrap)
{
    auto d = getEdgeModeSurface<PixelAccessEdgeMode::WRAP>(4, 4);

    for (int x = -1; x < 5; x++) {
        for (int y = -1; y < 5; y++) {
            ASSERT_TRUE(ColorIs(
                d, x, y,
                {dbl(y == 0 || y == 4), dbl(y == -1 || y == 3), dbl(x == 0 || x == 4), dbl(x == -1 || x == 3), 1.0},
                false));
            ASSERT_TRUE(ColorIs(
                d, x, y,
                {dbl(y == 0 || y == 4), dbl(y == -1 || y == 3), dbl(x == 0 || x == 4), dbl(x == -1 || x == 3), 1.0},
                true));
            ASSERT_TRUE(ColorWillBe(d, x, y, {0.5, 0.5, 0.5, 0.5, 0.5}, true, x % 4, y % 4));
        }
    }
}

TEST(PixelAccessTest, EdgeModeNone)
{
    auto d = getEdgeModeSurface<PixelAccessEdgeMode::ZERO>(4, 4);

    for (int x = -1; x < 5; x++) {
        for (int y = -1; y < 5; y++) {
            if (x < 0 || x > 3 || y < 0 || y > 3) {
                ASSERT_TRUE(ColorIs(d, x, y, {0.0, 0.0, 0.0, 0.0, 0.0}, false));
                ASSERT_TRUE(ColorIs(d, x, y, {0.0, 0.0, 0.0, 0.0, 0.0}, true));
                ASSERT_FALSE(ColorWillBe(d, x, y, {0.5, 0.5, 0.5, 0.5, 0.5}));
            }
        }
    }
}

template <MemoryFormat F>
void TestColorTo()
{
    auto d = PixelAccess<F>(21, 21);

    for (int x = 0; x < 21; x++) {
        for (int y = 0; y < 21; y++) {
            // Build a red X in the image surface
            if (x == y || 20 - x == y) {
                d.colorTo(x, y, {1.0, 0.0, 0.0, 1.0}, true);
                // Build blue square outline
            } else if (x == 0 || x == 20 || y == 0 || y == 20) {
                d.colorTo(x, y, {0.0, 0.0, 0.5, 0.5}, false);
                // Build a green cross (unpremultiplied)
            } else if (x == 10 || y == 10) {
                d.colorTo(x, y, {0.0, 0.7, 0.0, 0.7}, true);
            }
        }
    }

    // Premultiplied test ignores semi-transparent:
    // blue - because it's value of 1.0 is READ as 0.5 premultiplied
    // green - because it's value of 0.7 is WRITTEN as 0.49 premultiplied
    ASSERT_TRUE(ImageIs(d,
                             "1  .  1"
                             " 1   1 "
                             "  1 1  "
                             ".  1  ."
                             "  1 1  "
                             " 1   1 "
                             "1  .  1",
                             PixelPatch::Method::COLORS, false))
        << get_memory_format_name(F);

    // Unpremultiplied includes semi-transparent blue and green.
    ASSERT_TRUE(ImageIs(d,
                             "A@@@@@A"
                             "@1 4 1@"
                             "@ 141 @"
                             "@44544@"
                             "@ 141 @"
                             "@1 4 1@"
                             "A@@@@@A",
                             PixelPatch::Method::COLORS, true))
        << get_memory_format_name(F);
}

TEST(PixelAccessTest, colorTo)
{
    {
        auto cobj = cairo_image_surface_create(CAIRO_FORMAT_A8, 21, 21);
        auto s = Cairo::RefPtr<Cairo::ImageSurface>(new Cairo::ImageSurface(cobj, true));
        auto d = PixelAccess<MEMORY_FORMAT_A8>(s);

        for (unsigned x = 0; x < 21; x++) {
            for (unsigned y = 0; y < 21; y++) {
                // Build a cross in the image surface
                if (x == y || 20 - x == y) {
                    d.colorTo(x, y, {1.0});
                }
            }
        }

        ASSERT_TRUE(ImageIs(s,
                                 ".     ."
                                 " .   . "
                                 "  . .  "
                                 "   +   "
                                 "  . .  "
                                 " .   . "
                                 ".     .",
                                 PixelPatch::Method::ALPHA));
    }

    TestColorTo<MEMORY_FORMAT_RGBA128F>();
    TestColorTo<MEMORY_FORMAT_ARGB32>();
    TestColorTo<MEMORY_FORMAT_RGBA64>();
    TestColorTo<MEMORY_FORMAT_RGBA128>();
}

TEST(PixelAccessTest, colorEndian)
{
    auto int32 = PixelAccess<MEMORY_FORMAT_ARGB32>(1, 1);
    auto mem32 = int32.memory();
    int32.colorTo(0, 0, {0.2, 0.3, 0.4, 1.0});

    // LITTLE ENDIAN
    if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        EXPECT_EQ(mem32[0], 102); // BLUE
        EXPECT_EQ(mem32[1], 76);  // GREEN
        EXPECT_EQ(mem32[2], 51);  // RED
        EXPECT_EQ(mem32[3], 255); // ALPHA
    }
    EXPECT_TRUE(VectorIsNear(int32.colorAt(0, 0), {0.2, 0.3, 0.4, 1.0}, 0.005));

    auto int64 = PixelAccess<MEMORY_FORMAT_RGBA64>(1, 1);
    auto mem64 = int64.memory();
    int64.colorTo(0, 0, {0.2, 0.3, 0.4, 1.0});

    EXPECT_EQ(mem64[0], 13107); // RED
    EXPECT_EQ(mem64[1], 19660); // GREEN
    EXPECT_EQ(mem64[2], 26214); // BLUE
    EXPECT_EQ(mem64[3], 65535); // ALPHA
    EXPECT_TRUE(VectorIsNear(int64.colorAt(0, 0), {0.2, 0.3, 0.4, 1.0}, 0.005));
}

TEST(PixelAccessTest, multiSpanChannels)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    {
        auto c1 = cairo_create(src._cobj[0]);
        auto c2 = cairo_create(src._cobj[1]);
        for (auto channel = 0; channel < 4; channel++) {
            cairo_rectangle(c1, channel * 5, channel * 5, 6, 6);
            cairo_set_source_rgba(c1, channel == 0, channel == 1, channel == 2, 1.0);
            cairo_fill(c1);

            cairo_rectangle(c2, channel * 5, channel * 5, 6, 6);
            cairo_set_source_rgba(c2, channel == 3, 0.0, 0.0, 1.0);
            cairo_fill(c2);
        }
        cairo_destroy(c1);
        cairo_destroy(c2);
    }

    EXPECT_TRUE(ImageIs(*src._d,
                        "&&     "
                        "&&..   "
                        " .&o   "
                        " .o*o. "
                        "   o&. "
                        "   ..&&"
                        "     &&",
                        PixelPatch::Method::ALPHA));

    ASSERT_TRUE(ColorIs(*src._d, 0, 0, {1.0, 0.0, 0.0, 0.0, 1.0}, true));
    ASSERT_TRUE(ColorIs(*src._d, 20, 20, {0.0, 0.0, 0.0, 1.0, 1.0}, true));

    ASSERT_TRUE(ImageIs(*src._d, "22     "
                                 "224    "
                                 " 488   "
                                 "  8DP  "
                                 "   PP@ "
                                 "    @ff"
                                 "     ff", PixelPatch::Method::COLORS, false, false));
    // Request by float coordinates instead
    ASSERT_TRUE(ImageIs(*src._d, "22     "
                                 "224    "
                                 " 488   "
                                 "  8DP  "
                                 "   PP@ "
                                 "    @ff"
                                 "     ff", PixelPatch::Method::COLORS, false, true));

for (auto x = 0; x < 21; x++) {
        for (auto y = 0; y < 21; y++) {
            src._d->colorTo(x, y, {1.0, 0.0, 0.0, 1.0, 1.0});
        }
    }

    ASSERT_TRUE(ImageIs(*src._d, "hhhhhhh"
                                      "hhhhhhh"
                                      "hhhhhhh"
                                      "hhhhhhh"
                                      "hhhhhhh"
                                      "hhhhhhh"
                                      "hhhhhhh"));
}

TEST(PixelAccessTest, Filter)
{
    auto src1 = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    src1.rect(3, 3, 15, 15, {1.0, 0.0, 1.0, 0.0, 0.5});

    ASSERT_TRUE(ImageIs(*src1._d, "       "
                                       " RRRRR "
                                       " RRRRR "
                                       " RRRRR "
                                       " RRRRR "
                                       " RRRRR "
                                       "       "));

    auto src2 = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    src2.rect(12, 12, 9, 9, {1.0, 0.5, 1.0, 0.5, 1.0});

    ASSERT_TRUE(ImageIs(*src2._d, "       "
                                       "       "
                                       "       "
                                       "       "
                                       "    FFF"
                                       "    FFF"
                                       "    FFF"));

    TestFilter().filter(*src1._d, *src2._d);

    ASSERT_TRUE(ImageIs(*src1._d, "       "
                                       "  RRRR "
                                       " R RRR "
                                       " RR RR "
                                       " RRRFR "
                                       " RRRRF "
                                       "      F"));
}

TEST(PixelAccessTest, nonCairoMemoryAccess)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYKA160F>(21, 21);
    src.rect(6, 6, 9, 9, {1.0, 0.0, 1.0, 0.5, 1.0});

    EXPECT_TRUE(ImageIs(*src._d,
                             "       "
                             "       "
                             "  &&&  "
                             "  &&&  "
                             "  &&&  "
                             "       "
                             "       ",
                             PixelPatch::Method::ALPHA));
}

TEST(PixelAccessTest, createContiguousEmpty)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    src._d->colorTo(11, 11, {1.0, 0.5, 0.7, 0.1, 0.5});

    auto empty_cmyk = src._d->createContiguousEmpty();
    EXPECT_EQ(empty_cmyk.local_memory().size(), 21 * 21 * 5);
    EXPECT_TRUE(VectorIsNear(empty_cmyk.colorAt(11, 11), {0.0, 0.0, 0.0, 0.0, 0.0}, 0.005));

    auto empty_gray = src._d->template createContiguousEmpty<MEMORY_FORMAT_GA16>();
    EXPECT_EQ(empty_gray.local_memory().size(), 21 * 21 * 2);
    EXPECT_TRUE(VectorIsNear(empty_gray.colorAt(11, 11), {0.0, 0.0}, 0.005));

    //auto empty4 = src._d->createContiguousEmpty();
}

TEST(PixelAccessTest, createContiguousCMYK)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(21, 21);
    src._d->colorTo(11, 11, {1.0, 0.5, 0.7, 0.1, 0.5});
    EXPECT_TRUE(src._d->has_more_channels);

    auto copy1 = src._d->createContiguousCopy<false>();
    EXPECT_FALSE(copy1.has_more_channels);
    EXPECT_TRUE(VectorIsNear(copy1.colorAt(11, 11), src._d->colorAt(11, 11, false), 0.005));

    auto copy2 = src._d->createContiguousCopy<true>();
    EXPECT_FALSE(copy2.has_more_channels);
    EXPECT_TRUE(VectorIsNear(copy2.colorAt(11, 11), src._d->colorAt(11, 11, true), 0.005));

    auto copy3 = copy2.createContiguousCopy();
    EXPECT_TRUE(VectorIsNear(copy3.colorAt(11, 11), copy2.colorAt(11, 11), 0.005));
}

TEST(PixelAccessTest, createContiguousAlphaUnPremultiply)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(21, 21);
    src._d->colorTo(11, 11, {0.1, 0.2, 0.3, 0.5});

    auto copy = src._d->createContiguousCopy<true>();
    EXPECT_TRUE(VectorIsNear(copy.colorAt(11, 11), src._d->colorAt(11, 11, true), 0.005));

}

TEST(PixelAccessTest, createContiguousWithType)
{
    int w = 3, h = 3;
    int x = 1, y = 1;
    int pixloc = 4; // ((y * w) + x)

    auto src_float = TestSurface<MEMORY_FORMAT_RGBA128F, PixelAccessEdgeMode::NO_CHECK>(w, h);
    src_float._d->colorTo(x, y, {0.1, 0.2, 0.3, 0.5});
    EXPECT_NEAR(src_float._d->memory<0>()[pixloc * 4], 0.1, 0.05);

    auto copy_uint8 = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_ARGB32>();
    EXPECT_TRUE(VectorIsNear(copy_uint8.colorAt(x, y), src_float._d->colorAt(x, y, true), 0.005));
    if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        // NOTE: ARGB32 is endien dependent
        EXPECT_TRUE(MemoryArrayIsNear(copy_uint8.memory() + (pixloc * 4), {153, 102,  51, 127}, 0.05));
    }

    auto copy_uint16 = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_RGBA64>();
    EXPECT_TRUE(VectorIsNear(copy_uint16.colorAt(x, y), src_float._d->colorAt(x, y, true), 0.005));
    EXPECT_TRUE(MemoryArrayIsNear(copy_uint16.memory() + (pixloc * 4), {13107, 26214, 39321, 32767}, 0.05));

    auto copy_uint32 = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_RGBA128>();
    EXPECT_TRUE(VectorIsNear(copy_uint32.colorAt(x, y), src_float._d->colorAt(x, y, true), 0.005));
    EXPECT_TRUE(MemoryArrayIsNear(copy_uint32.memory() + (pixloc * 4), {858993471, 1717986943, 2576980479, 2147483647}, 0.05));
}

TEST(PixelAccessTest, createContiguousWithGray)
{
    int w = 3, h = 3;
    int x = 1, y = 1;
    int pixloc = 4; // ((y * w) + x)

    auto src_float = TestSurface<MEMORY_FORMAT_RGBA128F, PixelAccessEdgeMode::NO_CHECK>(w, h);
    src_float._d->colorTo(x, y, {0.1, 0.2, 0.3, 0.5});

    auto copy_gray_alpha16 = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_GA16>();
    // Scan a channel before and after to make sure there's no overlap writes
    EXPECT_TRUE(MemoryArrayIsNear(copy_gray_alpha16.memory() + pixloc * 2 - 1, {0, 51, 127, 0}, 0.05));
}

TEST(PixelAccessTest, createContiguousWithNoAlpha)
{
    int w = 3, h = 3;
    int x = 1, y = 1;
    int pixloc = 4; // ((y * w) + x)

    auto src_float = TestSurface<MEMORY_FORMAT_RGBA128F, PixelAccessEdgeMode::NO_CHECK>(w, h);
    src_float._d->colorTo(x, y, {0.1, 0.2, 0.3, 0.5});

    auto no_alpha = src_float._d->createContiguousCopy<false, MEMORY_FORMAT_RGB96F>();
    EXPECT_TRUE(MemoryArrayIsNear(no_alpha.memory() + pixloc * 3, {0.1, 0.2, 0.3}, 0.05));

    if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        // NOTE: RGB24 is endien dependent
        auto copy_rgb = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_RGB24>();
        EXPECT_TRUE(MemoryArrayIsNear(copy_rgb.memory() + pixloc * 3, {153, 102, 51}, 0.05));
    }

    auto copy_gray8 = src_float._d->createContiguousCopy<true, MEMORY_FORMAT_G8>();
    EXPECT_TRUE(MemoryArrayIsNear(copy_gray8.memory() + pixloc - 1, {0, 51, 0}, 0.05));
}

TEST(PixelAccessTest, createContiguousCopySpeed)
{
    TestSurface<MEMORY_FORMAT_CMYA_KA256F> sb3{1000, 1000}; // Speed testing
    sb3._d->colorTo(11, 11, {1.0, 0.5, 0.7, 0.1, 0.5});
    auto copy = sb3._d->createContiguousCopy();
    auto val = copy.colorAt(11, 11);
    EXPECT_TRUE(VectorIsNear(val, {1.0, 0.5, 0.7, 0.1, 0.5}, 0.005));
}

TEST(PixelAccessTest, createRegularCopySpeed)
{
    TestSurface<MEMORY_FORMAT_RGBA128F> sb3{1000, 1000}; // Speed testing
    sb3._d->colorTo(11, 11, {1.0, 0.5, 0.7, 0.5});
    auto copy = sb3._d->createContiguousCopy();
    auto val = copy.colorAt(11, 11);
    EXPECT_TRUE(VectorIsNear(val, {1.0, 0.5, 0.7, 0.5}, 0.005));
}

template <int write = 3, int read = 1, bool is_column = false, typename T0 = float, typename Access>
void testPixelAccess(Access &access, std::string result)
{
    {
        T0 val = std::is_integral_v<T0> ? std::numeric_limits<T0>::max() : 1.0;
        auto line_alpha = access.template getLineAccess<is_column, write, T0>();
        auto line_color = std::as_const(access).template getLineAccess<is_column, read, T0>();
        for (auto y = 0; y < 7; y+=2) {
            line_alpha.gotoLine(y);
            line_color.gotoLine(y);
            for (auto x = 0; x < 7; x+=2) {
                *(line_alpha.pixels + x * line_alpha.next) = val;
                if (x > 0 && x < 6 && y > 0 && y < 6) {
                    EXPECT_EQ(*(line_color.pixels + x * line_color.next), val);
                } else {
                    EXPECT_EQ(*(line_color.pixels + x * line_color.next), 0);
                }
            }
            *(line_alpha.pixels + line_alpha.next) = val;
        }
    }
    EXPECT_TRUE(ImageIs(access, result, PixelPatch::Method::COLORS, false, false, 1));
}

TEST(PixelAccessLineTest, FloatSingleSurfaceHorz)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0});
    testPixelAccess(*src._d,
                    "... . ."
                    "       "
                    "..888 ."
                    "  888  "
                    "..888 ."
                    "       "
                    "... . .");
}

TEST(PixelAccessLineTest, FloatSingleSurfaceVert)
{
    auto src = TestSurface<MEMORY_FORMAT_RGBA128F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0});
    testPixelAccess<3, 1, true>(*src._d,
                    ". . . ."
                    ". . . ."
                    ". 888 ."
                    "  888  "
                    ". 888 ."
                    "       "
                    ". . . .");
}

TEST(PixelAccessLineTest, IntSingleSurfaceHorz)
{
    auto src = TestSurface<MEMORY_FORMAT_ARGB32, PixelAccessEdgeMode::NO_CHECK>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0});
    testPixelAccess(*src._d,
                    "... . ."
                    "       "
                    "..888 ."
                    "  888  "
                    "..888 ."
                    "       "
                    "... . .");
    testPixelAccess<3, 1, false, unsigned char>(*src._d,
                    "... . ."
                    "       "
                    "..888 ."
                    "  888  "
                    "..888 ."
                    "       "
                    "... . .");
}

TEST(PixelAccessLineTest, IntSingleSurfaceVert)
{
    auto src = TestSurface<MEMORY_FORMAT_ARGB32, PixelAccessEdgeMode::NO_CHECK>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0});
    testPixelAccess<3, 1, true>(*src._d,
                    ". . . ."
                    ". . . ."
                    ". 888 ."
                    "  888  "
                    ". 888 ."
                    "       "
                    ". . . .");
}

TEST(PixelAccessLineTest, A8SingleSurfaceHorz)
{
    auto src = TestSurface<MEMORY_FORMAT_A8>(7, 7);
    src.rect(2, 2, 3, 3, {1.0});
    testPixelAccess<0, 0>(*src._d,
                          "... . ."
                          "       "
                          "..... ."
                          "  ...  "
                          "..... ."
                          "       "
                          "... . .");
}

TEST(PixelAccessLineTest, A8SingleSurfaceVert)
{
    auto src = TestSurface<MEMORY_FORMAT_A8>(7, 7);
    src.rect(2, 2, 3, 3, {1.0});
    testPixelAccess<0, 0, true>(*src._d,
                          ". . . ."
                          ". . . ."
                          ". ... ."
                          "  ...  "
                          ". ... ."
                          "       "
                          ". . . .");
}
TEST(PixelAccessLineTest, FloatDoubleSurfaceHorz)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0, 1.0});
    testPixelAccess<4, 1>(*src._d,
                          "... . ."
                          "       "
                          "..nnn ."
                          "  nnn  "
                          "..nnn ."
                          "       "
                          "... . .");
    testPixelAccess<3, 1>(*src._d,
                          "fff f f"
                          "       "
                          "ffnnn f"
                          "  nnn  "
                          "ffnnn f"
                          "       "
                          "fff f f");
}

TEST(PixelAccessLineTest, FloatDoubleSurfaceIntRead)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 0.2, 1.0});

    auto const &s = *src._d;

    {
        auto line_channel = s.template getLineAccess<false, 1, uint16_t>(4);
        EXPECT_EQ(line_channel.size, 7);
        EXPECT_EQ(line_channel.next, 1);
        EXPECT_EQ(line_channel.pixels[0], 0);
        EXPECT_EQ(line_channel.pixels[3], 65535);
    }
    {
        auto line_channel = s.template getLineAccess<false, 3, uint16_t>(4);
        EXPECT_EQ(line_channel.size, 7);
        EXPECT_EQ(line_channel.next, 1);
        EXPECT_EQ(line_channel.pixels[0], 0);
        EXPECT_EQ(line_channel.pixels[3], 13107);
    }
    {
        auto line_channel = s.template getLineAccess<false, -1, uint16_t>(4);
        EXPECT_EQ(line_channel.size, 7);
        EXPECT_EQ(line_channel.next, 5);
        EXPECT_EQ(line_channel.pixels[1], 0);
        EXPECT_EQ(line_channel.pixels[3*5+1], 65535);
        EXPECT_EQ(line_channel.pixels[3*5+3], 13107);
    }
    {
        auto line_channel = s.template getLineAccess<false, -2, uint16_t>(4);
        EXPECT_EQ(line_channel.size, 7);
        EXPECT_EQ(line_channel.next, 4);
        EXPECT_EQ(line_channel.pixels[1], 0);
        EXPECT_EQ(line_channel.pixels[3*4+1], 65535);
        EXPECT_EQ(line_channel.pixels[3*4+3], 13107);
    }
}

TEST(PixelAccessLineTest, FloatDoubleSurfaceVert)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0, 1.0});
    testPixelAccess<4, 1, true>(*src._d,
                          ". . . ."
                          ". . . ."
                          ". nnn ."
                          "  nnn  "
                          ". nnn ."
                          "       "
                          ". . . .");
    testPixelAccess<3, 1, true>(*src._d,
                          "f f f f"
                          "f f f f"
                          "f nnn f"
                          "  nnn  "
                          "f nnn f"
                          "       "
                          "f f f f");
}

TEST(PixelAccessTest, forEachPixel)
{
    auto src = TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7);
    src.rect(2, 2, 3, 3, {0.0, 1.0, 0.0, 1.0, 1.0});
    src._d->forEachPixel([&src](int x, int y) {
        if (x == y || 6 - x == y) {
            auto c = src._d->colorAt(x, y);
            c[0] = 1.0;
            c[1] = 0.0;
            c[2] = 0.0;
            src._d->colorTo(x, y, c);
        }
    });
    EXPECT_TRUE(ImageIs(*src._d, 
                        "2     2"
                        " 2   2 "
                        "  hnh  "
                        "  nhn  "
                        "  hnh  "
                        " 2   2 "
                        "2     2"
    , PixelPatch::Method::COLORS, false, false, 1));
}

template <typename AccessSrc, typename AccessDst>
void testForEachUncontiguousLine(AccessSrc &src, AccessDst &dst)
{
    // Draw something manually
    for (int x = 0; x < 7; x++)
        for (int y = 0; y < 7; y++)
            if (x == 0 || x == 6 || y == 0 || y == 6) {
                if constexpr (AccessSrc::channel_total == 5)
                    src.colorTo(x, y, {0.2, 0.5, 0.2, 0.5, 0.5});
                else
                    src.colorTo(x, y, {1.0, 0.0, 1.0, 0.5});
            }

    // This is a real world simulation of copying between contiguous and non-contiguous pixel memory
    src.forEachLine(dst, [](float const *src1, float const *src2, float const *end, float *dst1, float *dst2) {
        ASSERT_EQ(bool(src2), bool(dst2));
        for (auto x = 0;src1 < end; x++, src1+=AccessSrc::primary_total, dst1+=AccessDst::primary_total) {
            std::memcpy(dst1, src1, std::min(AccessDst::primary_count, AccessSrc::primary_count) * sizeof(float));
            dst1[AccessDst::primary_count] = src1[AccessSrc::primary_count];

            if (src2) {
                dst2[0] = src2[0];
                if (AccessDst::has_more_channels) { // Alpha test
                    dst2[AccessDst::primary_count] = src1[AccessSrc::primary_count];
                }
                src2+=AccessSrc::primary_total;
                dst2+=AccessDst::primary_total;
            }
        }
    });
    if constexpr (AccessDst::channel_total == 5) {
        EXPECT_TRUE(VectorIsNear(dst.colorAt(0, 0), {0.2, 0.5, 0.2, 0.5, 0.5}, 0.01));
        EXPECT_TRUE(ImageIs(dst,
                            "......."
                            ".     ."
                            ".     ."
                            ".     ."
                            ".     ."
                            ".     ."
                            "......."
        , PixelPatch::Method::COLORS, false, false, 1));
    } else {
        EXPECT_TRUE(VectorIsNear(dst.colorAt(0, 0), {1.0, 0.0, 1.0, 0.5}, 0.01));
        EXPECT_TRUE(ImageIs(dst,
                            "RRRRRRR"
                            "R     R"
                            "R     R"
                            "R     R"
                            "R     R"
                            "R     R"
                            "RRRRRRR"
        , PixelPatch::Method::COLORS, false, false, 1));
    }
}

TEST(PixelAccessTest, forEachLineUncontiguous)
{
    testForEachUncontiguousLine(*TestSurface<MEMORY_FORMAT_RGBA128F>(7, 7)._d, *TestSurface<MEMORY_FORMAT_RGBA128F>(7, 7)._d);
    testForEachUncontiguousLine(*TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7)._d, *TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7)._d);
    {
        auto dst = PixelAccess<MEMORY_FORMAT_CMYA_KA256F>::createContiguousEmpty(7, 7);
        testForEachUncontiguousLine(*TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7)._d, dst);
    }
    {
        auto src = PixelAccess<MEMORY_FORMAT_CMYA_KA256F>::createContiguousEmpty(7, 7);
        testForEachUncontiguousLine(src, *TestSurface<MEMORY_FORMAT_CMYA_KA256F>(7, 7)._d);
    }
    {
        auto dst = PixelAccess<MEMORY_FORMAT_CMYA_KA256F>::createContiguousEmpty(7, 7);
        auto src = PixelAccess<MEMORY_FORMAT_CMYA_KA256F>::createContiguousEmpty(7, 7);
        testForEachUncontiguousLine(src, dst);
    }
}

template <MemoryFormat src_format, MemoryFormat dst_format>
void testForEachContiguousLine(typename PixelAccess<src_format>::Color src_color,
                               typename PixelAccess<dst_format>::Color dst_color)
{
    auto src = PixelAccess<src_format>(7, 7);
    auto dst = PixelAccess<dst_format>(7, 7);

    // Draw something manually
    for (int x = 0; x < 7; x++)
        for (int y = 0; y < 7; y++)
            if (x == 0 || x == 6 || y == 0 || y == 6)
                src.colorTo(x, y, src_color);

    src.forEachLine(dst, [&src_color, &dst_color](float const *src, float const *end, float *dst) {
        for (auto x = 0;src < end; x++, src+=src_color.size(), dst+=dst_color.size()) {
            bool same = true;
            for (unsigned c = 0; c < src_color.size(); c++) {
                same = same && (src[c] == src_color[c]);
            }
            for (unsigned c = 0; (x == 3 || same) && c < dst_color.size(); c++) {
                dst[c] = dst_color[c];
            }
        }
    });
    EXPECT_TRUE(ImageIs(dst,
                        "RRRRRRR"
                        "R  R  R"
                        "R  R  R"
                        "R  R  R"
                        "R  R  R"
                        "R  R  R"
                        "RRRRRRR"
    , PixelPatch::Method::COLORS, false, false, 1));
}

TEST(PixelAccessTest, forEachLineContiguous)
{
    testForEachContiguousLine<MEMORY_FORMAT_RGBA128F,  MEMORY_FORMAT_RGBA128F> ({1.0, 0.0, 1.0,      1.0}, {1.0, 0.0, 1.0,      1.0});
    testForEachContiguousLine<MEMORY_FORMAT_RGBA128F,  MEMORY_FORMAT_CMYKA160F>({1.0, 0.0, 1.0,      1.0}, {1.0, 0.0, 1.0, 0.5, 1.0});
    testForEachContiguousLine<MEMORY_FORMAT_CMYKA160F, MEMORY_FORMAT_RGBA128F> ({1.0, 0.0, 1.0, 0.5, 1.0}, {1.0, 0.0, 1.0,      1.0});
    testForEachContiguousLine<MEMORY_FORMAT_CMYKA160F, MEMORY_FORMAT_CMYKA160F>({1.0, 0.0, 1.0, 0.5, 1.0}, {1.0, 0.0, 1.0, 0.5, 1.0});
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
