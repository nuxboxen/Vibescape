// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "inkscape.h"
#include "document.h"
#include "object/sp-image.h"

#include "renderer/surface-testbase.h"
#include "renderer/drawing/svg-renderer.h"

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

  <image href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAAJUlEQVQI1yXJoREAIAwEsPQQxWDZf86aRxAbYV8SVh0CA6H6Tz+kLwgyOkrwGwAAAABJRU5ErkJggg=="
    id="img" style="image-rendering:optimizeSpeed" x="0" y="0" width="21" height="21"/>

</svg>)A"sv;

        _doc = SPDocument::createNewDocFromMem(docString);
        _img = cast<SPImage>(_doc->getObjectById("img"));

        ASSERT_TRUE(_doc);
        ASSERT_TRUE(_doc->getRoot());
    }

    std::unique_ptr<SPDocument> _doc;
    SPImage *_img;
};

TEST_F(SurfaceSvgFactory, RenderImageBase64)
{
    auto surface = SvgRenderer().render(_doc.get());
    EXPECT_IMAGE_IS<PixelPatch::Method::COLORS>(*surface,
        "221.122"
        "221.122"
        "11...11"
        "......."
        "11...11"
        "221.122"
        "221.122");
}

TEST_F(SurfaceSvgFactory, RenderImageFile)
{
    static std::string filename = INKSCAPE_TESTS_DIR "/data/renderer/transform-source-16.png";
    _img->setAttribute("href", "file://" + filename);
    auto surface = SvgRenderer().render(_doc.get());
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(*surface,
        "       "
        "  .&.  "
        " .&&&. "
        " &&&&& "
        " .&&&. "
        "  .&.  "
        "       ");
}


TEST_F(SurfaceSvgFactory, RenderBroken)
{
    _img->setAttribute("href", "NOT KNOWN AT THIS ADDRESS");

    auto factory = SvgRenderer();
    factory.set_device_scale(2);
    auto surface = factory.render(_doc.get());
    EXPECT_IMAGE_IS<PixelPatch::Method::LIGHT>(*surface,
        ".............."
        ".oOOOOOOOOOOo."
        ".OOOOOooOOOOO."
        ".OOO+....+OOO."
        ".OO+......+OO."
        ".OO..O:.O..OO."
        ".Oo...OO...oO."
        ".Oo...OO:..oO."
        ".OO..O:.O..OO."
        ".OO+......+OO."
        ".OOO+....+OOO."
        ".OOOOOooOOOOO."
        ".oOOOOOOOOOOo."
        "..............");
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
