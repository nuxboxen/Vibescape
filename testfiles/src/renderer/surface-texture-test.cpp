// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <gdkmm/texture.h>
#include <gtkmm/init.h>

#include "color-testbase.h"
#include "surface-testbase.h"

#include "renderer/surface-texture.h"

TEST(SurfaceToTextureTest, RGB16SurfaceTexture)
{
    Gtk::init_gtkmm_internals();

    auto surface = std::make_shared<Renderer::Surface>(INKSCAPE_TESTS_DIR "/data/renderer/transform-source-16.png");
    auto texture = Renderer::build_texture(surface);
    ASSERT_TRUE(texture) << "Texture building";
    EXPECT_EQ(texture->get_width(), 422);
    EXPECT_EQ(texture->get_height(), 423);
}

TEST(SurfaceToTextureTest, MemoryReferences)
{
    Gtk::init_gtkmm_internals();

    auto surface = std::make_shared<Renderer::Surface>(INKSCAPE_TESTS_DIR "/data/renderer/transform-source-16.png");
    auto cobj = cairo_surface_reference(surface->getCairoSurfaces()[0]->cobj());
    auto texture = Renderer::build_texture(surface);
    EXPECT_EQ(cairo_surface_get_reference_count(cobj), 3); // shared ptr, texture, and our manual ref
    surface.reset();
    EXPECT_EQ(cairo_surface_get_reference_count(cobj), 2);
    texture.reset();
    EXPECT_EQ(cairo_surface_get_reference_count(cobj), 1);
    cairo_surface_destroy(cobj);
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
