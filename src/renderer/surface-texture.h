// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Convert Gdk::Textures to and from Renderer::Surfaces
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef RENDERER_SURFACE_TEXTURE_H
#define RENDERER_SURFACE_TEXTURE_H

#include <glibmm.h>
#include <cairomm/refptr.h>

namespace Gdk {
class Texture;
}
namespace Cairo {
class ImageSurface;
class Surface;
}

namespace Inkscape::Renderer {

class TextureCreationError : public std::exception
{   
    std::string msgstr;
public:
    TextureCreationError(std::string msg) : std::exception(), msgstr(std::move(msg)) {}
    const char* what() const noexcept override { return msgstr.c_str(); }
};

class Surface;

Glib::RefPtr<Gdk::Texture> build_texture(std::shared_ptr<Surface> const &surface);
Glib::RefPtr<Gdk::Texture> build_texture(Cairo::RefPtr<Cairo::Surface> const &surface);
Glib::RefPtr<Gdk::Texture> build_texture(Cairo::RefPtr<Cairo::ImageSurface> const &surface);

} // namespace Inkscape::Renderer

#endif // RENDERER_SURFACE_TEXTURE_H

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
