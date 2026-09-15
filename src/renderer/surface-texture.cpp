// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Convert Gdk::Textures to and from Renderer::Surfaces
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdkmm/texture.h>

#include "surface-texture.h"
#include "surface.h"

namespace Inkscape::Renderer {

Glib::RefPtr<Gdk::Texture> build_texture(std::shared_ptr<Surface> const &surface)
{
    // Don't bother building a texture for null data
    if (!surface || !surface->ready()) {
        return {};
    }
    if (surface->getSurfaceCount() != 1) {
        throw TextureCreationError("Surface must not have more than 3 channels (i.e. CMYK) when building a screen texture.");
    }

    return build_texture(surface->getCairoSurfaces()[0]);
}

Glib::RefPtr<Gdk::Texture> build_texture(Cairo::RefPtr<Cairo::Surface> const &surface)
{
    // Bad conversion results in null object which is safely returned below
    return build_texture(std::dynamic_pointer_cast<Cairo::ImageSurface>(surface));
}

Glib::RefPtr<Gdk::Texture> build_texture(Cairo::RefPtr<Cairo::ImageSurface> const &img)
{
    if (!img) {
        return {};
    }

    auto memory_format = GDK_MEMORY_DEFAULT;
    if (cairo_image_surface_get_format(img->cobj()) == CAIRO_FORMAT_RGBA128F) {
        memory_format = GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED;
    }

    auto bytes = g_bytes_new_with_free_func(img->get_data(),
                                            img->get_stride() * img->get_height(),
                                            (GDestroyNotify)cairo_surface_destroy,
                                            cairo_surface_reference(img->cobj()));

    // TODO: We can not properly set the color profile for this texture here, which limits
    // the display gamut. If this can be properly controlled then we'll be able to push
    // the output gamut to a wider than sRGB space.
    auto texture = gdk_memory_texture_new(img->get_width(),
                                          img->get_height(),
                                          memory_format,
                                          bytes,
                                          img->get_stride());

    g_bytes_unref(bytes);
    return Glib::wrap(texture);
}

} // namespace Inkscape::Renderer

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
