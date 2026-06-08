// SPDX-License-Identifier: GPL-2.0-or-later
/***** SHARED TEST TOOLS FOR SURFACE TESTS *******/

#ifndef INKSCAPE_TEST_RENDERER_DRAWING_TESTBASE_H
#define INKSCAPE_TEST_RENDERER_DRAWING_TESTBASE_H

#include <memory>
#include <2geom/elliptical-arc.h>
#include <glibmm/init.h>

#include "color-testbase.h"
#include "drawing-style-testbase.h"
#include "surface-testbase.h"

#include "renderer/drawing/drawing.h"
#include "renderer/drawing/drawing-group.h"
#include "renderer/drawing/drawing-item.h"
#include "renderer/drawing/drawing-item-ptr.h"
#include "renderer/drawing/drawing-image.h"
#include "renderer/drawing/drawing-pattern.h"
#include "renderer/drawing/drawing-shape.h"
#include "renderer/drawing/drawing-text.h"

#include "libnrtype/font-factory.h"
#include "libnrtype/font-instance.h"

using namespace Inkscape::Renderer;

class DrawingTest : public ::testing::Test
{
protected:
    DrawingTest()
    {
        // Used for Text drawing (finding fonts via FontConfig/Pango)
        Glib::init();
    }
    ~DrawingTest() {}

    // Paired with Glib::init so FontFactory is available
    Inkscape::Util::Statics statics;
public:
    std::shared_ptr<FontInstance> make_fontinstance(std::string descr)
    {
        auto pd = pango_font_description_from_string(descr.c_str());
        auto ret = FontFactory::get().Face(pd);
        pango_font_description_free(pd);
        return ret;
    }

    void prepare_test(Geom::IntPoint const &size, std::shared_ptr<Colors::Space::AnySpace> const &space = {})
    {
        Geom::IntRect bounds = {{0, 0}, size};
        Geom::Scale scale = {1, 1};
        surface = std::make_shared<Surface>(size, 1, space ? space : rgb);
        context = std::make_unique<Context>(surface, bounds.min(), scale);
    }

    void assert_drawing(std::string const &result, std::optional<std::string> fail_png = {}, unsigned threshold = 10)
    {
        auto size = surface->dimensions();
        auto [patch_x, patch_y] = PixelPatch::get_patch_scale(size[Geom::X], size[Geom::Y]);
        auto patch = surface->run_pixel_filter(PixelPatch{
            ._method = PixelPatch::Method::COLORS,
            ._patch_x = (unsigned)patch_x,
            ._patch_y = (unsigned)patch_y,
        });
        auto expected = PatchResult(result, patch._stride);
        auto diff = patch.diff(expected);
        EXPECT_LT(diff.delta, threshold) << patch << expected << diff;
        if (diff.delta > threshold && fail_png) {
            // Output png on failure
            surface->write_to_png(*fail_png);
        }
    }

    auto get_pixel(int x, int y)
    {
        return surface->run_pixel_filter(SampleColor(x, y));
    }

    std::shared_ptr<Surface> surface;
    std::unique_ptr<Context> context;

};

#endif // INKSCAPE_TEST_RENDERER_DRAWING_TESTBASE_H

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
