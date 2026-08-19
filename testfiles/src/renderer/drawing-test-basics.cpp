// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawing-testbase.h"

TEST_F(DrawingTest, TreeOfObjects)
{
    prepare_test({21, 21}, cmyk_cpp);

    auto draw = Drawing();
    auto item = make_drawingitem<DrawingShape>(draw);
    //auto image = make_drawingitem<DrawingImage>(draw);
    auto group = make_drawingitem<DrawingGroup>(draw);
    draw.setRoot(&*group);
    //group->appendChild(&*item);
    //group->appendChild(&*image);

    draw.render(*context, {0, 0, 21, 21}, 0);
}

// RENDERER-FIXME - need setStyle() versions for StyleMockSource, but templating that ends up pushing the mock
//                  through CodeBuilder, which then hits issue of building calls to the mock.
#if 0
TEST_F(DrawingTest, PartialRender)
{
    prepare_test({21, 21}, cmyk_cpp);

    auto draw = Drawing();
    auto group = make_drawingitem<DrawingGroup>(draw);
    auto item = make_drawingitem<DrawingShape>(draw);

    StyleMockSource style_g = {
        .color_interpolation = cmyk_cpp
    };
    group->setStyle(&style_g);

    auto pv5 = std::make_shared<Geom::PathVector>();
    {
        auto path5 = std::make_shared<Geom::Path>();
        path5->append(Geom::LineSegment(Geom::Point(0, 0), {21, 0}));
        path5->append(Geom::LineSegment(Geom::Point(21, 0), {21, 21}));
        path5->append(Geom::LineSegment(Geom::Point(21, 21), {0, 21}));
        path5->append(Geom::LineSegment(Geom::Point(0, 21), {0, 0}));
        pv5->push_back(*path5);
    }
    item->setPath(pv5);
    StyleMockSource style = {
        .fill = { .color = Colors::Color(cmyk_cpp, {1, 0, 0, 0}) },
        .fill_opacity = 0.1,
        .color_interpolation = cmyk_cpp
    };
    item->setStyle(&style);
    group->appendChild(&*item);
    draw.setRoot(&*group);

    // Render the drawing in strips of 3 pixels
    for (auto y = 0; y < 21; y += 3) {
        draw.update({0, y, 21, y+3}, Geom::Affine(1, 0, 0, 1, 0, 0), 31, 0);
        draw.render(*context, {0, y, 21, y+3}, 0);
    }
    for (int xy = 0; xy < 21; xy++) {
        EXPECT_TRUE(VectorIsNear(get_pixel(xy, xy), {1, 0, 0, 0, 0.1}, 0.01));
    }
}
#endif

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
