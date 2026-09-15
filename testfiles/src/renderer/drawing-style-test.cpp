// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawing-testbase.h"

using namespace Inkscape::Renderer;

TEST(DrawingStyleTest, DefaultStyles)
{
    auto source = StyleMockSource();
    auto style = DrawingStyle(&source);

    ASSERT_EQ(style.fill.type, DrawingStyle::PaintType::NONE);
    ASSERT_EQ(style.stroke.type, DrawingStyle::PaintType::NONE);
}

TEST(DrawingStyleTest, StyleEnums)
{
    StyleMockSource source = {
        .paint_order = {
            .layer = {SP_CSS_PAINT_ORDER_STROKE, SP_CSS_PAINT_ORDER_MARKER, SP_CSS_PAINT_ORDER_FILL},
        },
        .stroke_linejoin = SP_STROKE_LINEJOIN_ROUND,
        .stroke_linecap = SP_STROKE_LINECAP_ROUND,
        .fill_rule = SP_WIND_RULE_EVENODD,
    };
    auto style = DrawingStyle(&source);

    ASSERT_EQ(style.paint_order_layer[0], DrawingStyle::PAINT_ORDER_STROKE);
    ASSERT_EQ(style.paint_order_layer[1], DrawingStyle::PAINT_ORDER_MARKER);
    ASSERT_EQ(style.paint_order_layer[2], DrawingStyle::PAINT_ORDER_FILL);
    ASSERT_EQ(style.fill_rule, SP_WIND_RULE_EVENODD);
    ASSERT_EQ(style.line_cap, SP_STROKE_LINECAP_ROUND);
    ASSERT_EQ(style.line_join, SP_STROKE_LINEJOIN_ROUND);
}

TEST(DrawingStyleTest, SolidColor)
{
    StyleMockSource source = {
        .fill = { .color = Colors::Color(0xff0000ff) },
        .fill_opacity = 0.5
    };
    auto style = DrawingStyle(&source);

    ASSERT_EQ(style.fill.type, DrawingStyle::PaintType::COLOR);
    ASSERT_TRUE(style.fill.color);
    ASSERT_EQ(style.fill.color->toString(), "#ff0000ff");
    ASSERT_EQ(style.fill.opacity, 0.5);
}

TEST(DrawingStyleTest, PaintServer)
{
    auto cmyk = Colors::Manager::get().find(Colors::Space::Type::CMYK);
    auto ps = PaintServerMockSource({
        .type = PaintServerType::LINEAR_GRADIENT,
    });
    auto ps2 = std::make_unique<PaintServerMockSource>(ps);
    auto href = std::make_unique<StyleMockSource::PaintMockSource::Href>(std::move(ps2));
    StyleMockSource source = {
        .fill = {
            .href = std::move(href),
        },
    };
    auto style = DrawingStyle(&source);

    ASSERT_EQ(style.fill.type, DrawingStyle::PaintType::SERVER);
    ASSERT_TRUE(style.fill.server);
    // Actual values are tested in `drawing-paintserver-test.cpp`
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
