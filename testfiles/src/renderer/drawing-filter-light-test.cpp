// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawing-filter-testbase.h"

using namespace Inkscape::Renderer;

TEST(DrawingLightTest, LightDiffuse)
{
    auto dl = std::make_unique<DrawingFilter::DiffuseLighting>();

    dl->light_type = DrawingFilter::POINT_LIGHT;
    dl->light.point.x = 9;
    dl->light.point.y = 40;
    dl->light.point.z = 33;

    dl->setInterpolationSpace(rgb);
    dl->lighting_color = Colors::Color(rgb, {1.0, 1.0, 1.0, 1.0});
    dl->diffuseConstant = 1.0;
    dl->surfaceScale = 1.0;

    EXPECT_PRIMITIVE_IS<PixelPatch::Method::LIGHT>(std::move(dl),
                        "          "
                        "          "
                        "          "
                        "          "
                        "          "
                        " .        "
                        " ..       "
                        "  ..      "
                        "   ..     "
                        "    ..    "
    );
}

// Same as above, but with an int surface
TEST(DrawingLightTest, LightDiffuseWithIntColorSpace)
{
    auto dl = std::make_unique<DrawingFilter::DiffuseLighting>();

    dl->light_type = DrawingFilter::POINT_LIGHT;
    dl->light.point.x = 9;
    dl->light.point.y = 40;
    dl->light.point.z = 33;
    dl->lighting_color = Colors::Color(rgb, {1.0, 1.0, 1.0, 1.0});
    dl->diffuseConstant = 1.0;
    dl->surfaceScale = 1.0;

    auto image = std::make_shared<Renderer::Surface>(get_transformed_input()->convertedToInt());

    EXPECT_PRIMITIVE_IS<PixelPatch::Method::LIGHT>(std::move(dl),
                        "          "
                        "          "
                        "          "
                        "          "
                        "          "
                        " .        "
                        " ..       "
                        "  ..      "
                        "   ..     "
                        "    ..    ",
                        {},
                        image
    );
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
