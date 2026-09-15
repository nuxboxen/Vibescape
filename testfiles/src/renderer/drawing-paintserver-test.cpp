// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawing-testbase.h"

using namespace Inkscape::Renderer;

class DrawingPaintServerTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        size = {21, 21};
        bounds = {0, 0, 21, 21};
        scale = {1, 1};
        cmyk = Colors::Manager::get().find(Colors::Space::Type::CMYK);

        surface = std::make_shared<Surface>(size, 1, cmyk);
        context = std::make_unique<Context>(*surface, bounds.min(), scale);
    }

    Geom::IntPoint size = {21, 21};
    Geom::IntRect bounds;
    Geom::Scale scale;
    std::shared_ptr<Colors::Space::AnySpace> cmyk;
    std::shared_ptr<Surface> surface;
    std::unique_ptr<Context> context;

    void testPaintServer(PaintServerMockSource data, std::string result, std::string to_png = "")
    {
        auto ps = create_drawing_paintserver(&data);
        ASSERT_TRUE(ps) << "No PaintServer generated from MockSource";

        auto pattern = ps->create_pattern(context.get(), bounds, 1.0);
        ASSERT_TRUE(pattern) << "No Pattern generated from PaintServer";
        context->setSource(*pattern);
        context->rectangle(Geom::Rect(3, 3, 18, 18));
        context->fill();
        if (!to_png.empty()) {
            surface->write_to_png(to_png);
        }
        EXPECT_IMAGE_IS(*surface, result);
    }
};

TEST_F(DrawingPaintServerTest, NoServer)
{
    ASSERT_FALSE(create_drawing_paintserver<PaintServerMockSource>(nullptr));

    PaintServerMockSource invalid_ps = {
        .type = PaintServerType::INVALID
    };
    ASSERT_FALSE(create_drawing_paintserver(&invalid_ps));

    PaintServerMockSource invalid_solid_color = {
        .type = PaintServerType::SOLID_COLOR,
        .is_valid = false
    };
    ASSERT_FALSE(create_drawing_paintserver(&invalid_solid_color));
}

TEST_F(DrawingPaintServerTest, SolidColor)
{
    testPaintServer(
        {
            .type = PaintServerType::SOLID_COLOR,
            .color = Colors::Color(cmyk, {0.7, 0, 0.7, 0.2, 0.7})
        }, "       "
           " RRRRR "
           " RRRRR "
           " RRRRR "
           " RRRRR "
           " RRRRR "
           "       ");
}

TEST_F(DrawingPaintServerTest, LinearGradient)
{
    testPaintServer(
        // Same data as RenderContextPatternTest::PatternMatrixAndLinearGradient
        {
            .type = PaintServerType::LINEAR_GRADIENT,
            .spread = SP_GRADIENT_SPREAD_REFLECT,
            .units = SP_GRADIENT_UNITS_USERSPACEONUSE,
            .vector = SPGradientVector({
                .stops = {
                    {Colors::Color(cmyk, {1.0, 0.0, 0.0, 0.0, 1.0}), 0.0},
                    {Colors::Color(cmyk, {0.0, 1.0, 0.0, 0.0, 1.0}), 0.5},
                    {Colors::Color(cmyk, {0.0, 0.0, 0.0, 1.0, 0.0}), 1.0}
                },
                .geom = {0.0, 0.0, 21.0, 21.0}
            })
        }, "       "
           " 28888 "
           " 88888 "
           " 88888 "
           " 88888 "
           " 8888f "
           "       ");
    testPaintServer(
        {
            .type = PaintServerType::LINEAR_GRADIENT,
            .spread = SP_GRADIENT_SPREAD_REFLECT,
            .units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX,
            .transform = Geom::Rotate(-45.0), // This is inverse to RenderContextPatternTest
            .vector = SPGradientVector({
                .stops = {
                    {Colors::Color(cmyk, {1.0, 0.0, 0.0, 0.0, 1.0}), 0.0},
                    {Colors::Color(cmyk, {0.0, 1.0, 0.0, 0.0, 1.0}), 0.5},
                    {Colors::Color(cmyk, {0.0, 0.0, 0.0, 1.0, 0.0}), 1.0}
                },
                .geom = {0.0, 0.0, 1.0, 1.0}
            })
        }, "       "
           " 22888 "
           " 22888 "
           " 22988 "
           " 22688 "
           " 22288 "
           "       ");
}

TEST_F(DrawingPaintServerTest, RadialGradient)
{
    testPaintServer(
        // Same data as RenderContextPatternTest::RadialGradient
        {
            .type = PaintServerType::RADIAL_GRADIENT,
            .spread = SP_GRADIENT_SPREAD_REFLECT,
            .units = SP_GRADIENT_UNITS_USERSPACEONUSE,
            .vector = SPGradientVector({
                .stops = {
                    {Colors::Color(cmyk, {0.0, 0.0, 0.0, 0.0, 0.0}), 0.0},
                    {Colors::Color(cmyk, {1.0, 0.0, 0.0, 0.0, 1.0}), 1.0},
                },
                .geom = {10.0, 10.0, 0.0, 10.0, 10.0, 9.0}
            })
        }, "       "
           "  ..   "
           " .221  "
           " .222  "
           "  121  "
           "       "
           "       ");
}

TEST_F(DrawingPaintServerTest, MeshGradient)
{
    auto white = Colors::Color(cmyk, {0.0, 0.0, 0.0, 0.0, 1.0});
    auto black = Colors::Color(cmyk, {0.0, 0.0, 0.0, 1.0, 1.0});
    auto red = Colors::Color(cmyk, {0.0, 1.0, 1.0, 0.0, 1.0});
    auto green = Colors::Color(cmyk, {1.0, 0.0, 1.0, 0.0, 1.0});
    auto blue = Colors::Color(cmyk, {1.0, 1.0, 0.0, 0.0, 1.0});
    auto rblack = Colors::Color(cmyk, {1.0, 1.0, 1.0, 0.0, 1.0});

    testPaintServer(
        {
            .type = PaintServerType::MESH_GRADIENT,
            .units = SP_GRADIENT_UNITS_USERSPACEONUSE,
            .mesh = SPGradientMesh({
                .rows = 2,
                .cols = 2,


                .patches = {
                    {
                        SPGradientPatch({
                            .points = {
                                {{ 0,    0  }, { 3.5,  0  }, { 7,    0  }, {10.5,  0  }},
                                {{10.5,  0  }, {10.5,  3.5}, {10.5,  7  }, {10.5, 10.5}},
                                {{10.5, 10.5}, { 7,   10.5}, { 3.5, 10.5}, {0,    10.5}},
                                {{ 0,   10.5}, { 0,    7  }, { 0,    3.5}, {0,     0  }}
                            },
                            .pathtype = {'c', 'c', 'c', 'c'},
                            .color = {red, white, black, white}
                        }),
                        SPGradientPatch({
                            .points = {
                                {{10.5,  0  }, {14,    0  }, {17.5,  0  }, {21,    0  }},
                                {{21,    0  }, {21,    3.5}, {21,    7  }, {21,   10.5}},
                                {{21,   10.5}, {17.5, 10.5}, {14,   10.5}, {10.5, 10.5}},
                                {{10.5, 10.5}, {10.5,  7  }, {10.5,  3.5}, {10.5,  0  }}
                            },
                            .pathtype = {'c', 'c', 'c', 'c'},
                            .color = {white, rblack, white, black}
                        }),
                    },
                    {
                        SPGradientPatch({
                            .points = {
                                {{ 0,   10.5}, { 3.5, 10.5}, {7,    10.5}, {10.5, 10.5}},
                                {{10.5, 10.5}, {10.5, 14  }, {10.5, 17.5}, {10.5, 21  }},
                                {{10.5, 21  }, { 7,   21  }, {3.5,  21  }, { 0,   21  }},
                                {{ 0,   21  }, { 0,   17.5}, {0,    14  }, { 0,   10.5}}
                            },
                            .pathtype = {'c', 'c', 'c', 'c'},
                            .color = {white, black, white, blue}
                        }),
                        SPGradientPatch({
                            .points = {
                                {{10.5, 10.5}, {14,   10.5}, {17.5, 10.5}, {21,   10.5}},
                                {{21,   10.5}, {21,   14  }, {21,   17.5}, {21,   21  }},
                                {{21,   21  }, {17.5, 21  }, {14,   21  }, {10.5, 21  }},
                                {{10.5, 21  }, {10.5, 17.5}, {10.5, 14  }, {10.5, 10.5}}
                            },
                            .pathtype = {'c', 'c', 'c', 'c'},
                            .color = {black, white, green, white}
                        }),
                    },
                }
            })
        }, "       "
           " ..... "
           " .fff. "
           " .fff. "
           " .ffp. "
           " ..... "
           "       ");
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
