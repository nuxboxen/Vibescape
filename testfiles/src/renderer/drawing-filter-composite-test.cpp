// SPDX-License-Identifier: GPL-2.0-or-later

#include "drawing-filter-testbase.h"

using namespace Inkscape::Renderer;

TEST(DrawingCompositeTest, CompositeIN)
{
    auto cp = std::make_unique<DrawingFilter::Composite>();
    cp->set_output(0);
    cp->set_input(0, DrawingFilter::SLOT_BACKGROUND_IMAGE);
    cp->set_input(1, DrawingFilter::SLOT_SOURCE_IMAGE);
    cp->set_operator(CompositeOperator::IN);

    // This is much slower in linearRGB which is the default
    auto rgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    cp->setInterpolationSpace(rgb);

    EXPECT_PRIMITIVE_IS(std::move(cp),
                        "          "
                        "     P    "
                        "    PPP   "
                        "   XXXXX  "
                        "  HHHHHH8 "
                        " 88888888 "
                        "  8999994 "
                        "   ::::5  "
                        "    ::5   "
                        "          ");
}

TEST(DrawingCompositeTest, CompositeArithmetic)
{
    auto cp = std::make_unique<DrawingFilter::Composite>();
    cp->set_output(0);
    cp->set_input(0, DrawingFilter::SLOT_BACKGROUND_IMAGE);
    cp->set_input(1, DrawingFilter::SLOT_SOURCE_IMAGE);
    cp->set_operator(CompositeOperator::ARITHMETIC);
    cp->set_arithmetic(0.0, 0.0, 2.0, 0.0);

    // This is much slower in linearRGB which is the default
    auto rgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    cp->setInterpolationSpace(rgb);

    EXPECT_PRIMITIVE_IS<PixelPatch::Method::COLORS>(std::move(cp),
         "          "
         "     8    "
         "    888   "
         "   88888  "
         "  8888888 "
         " 88888888 "
         "  8888884 "
         "   88884  "
         "    884   "
         "          ");
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
