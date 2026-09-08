// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * A simple gradient preview
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2001-2002 Lauris Kaplinski
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "gradient-image.h"

#include <gdkmm/pixbuf.h>

#include "object/sp-gradient.h"
#include "object/sp-stop.h"
#include "renderer/context.h"
#include "renderer/surface-texture.h"

static Renderer::CheckerboardPattern make_gradient_checkerboard()
{
    return Renderer::CheckerboardPattern(Colors::Color(0xc4c4c400), 6);
}

static void sp_gradient_draw(SPGradient *gr, int width, int height, Cairo::RefPtr<Cairo::Context> const &cr)
{
    if (!gr) {
        return;
    }

    auto ctx = Renderer::Context(cr);

    ctx.setSource(make_gradient_checkerboard());
    ctx.paint();

    ctx.setSource(*gr->createPreviewPattern(width));
    ctx.paint();
}

namespace Inkscape::UI::Widget {

GradientImage::GradientImage(SPGradient *gradient)
{
    set_name("GradientImage");
    set_draw_func(sigc::mem_fun(*this, &GradientImage::draw_func));
    set_gradient(gradient);
}

void GradientImage::draw_func(Cairo::RefPtr<Cairo::Context> const &cr, int width, int height)
{
    sp_gradient_draw(_gradient, width, height, cr);
}

void GradientImage::set_gradient(SPGradient *gradient)
{
    if (_gradient == gradient) {
        return;
    }

    if (_gradient) {
        _release_connection.disconnect();
        _modified_connection.disconnect();
    }

    _gradient = gradient;

    if (gradient) {
        _release_connection = gradient->connectRelease([this] (auto) {
            set_gradient(nullptr);
        });
        _modified_connection = gradient->connectModified([this] (auto, auto) {
            queue_draw();
        });
    }

    queue_draw();
}

} // namespace Inkscape::UI::Widget

Glib::RefPtr<Gdk::Texture> sp_gradient_to_texture(SPGradient *gr, int width, int height)
{
    return Renderer::build_texture(sp_gradient_to_surface(gr, width, height));
}

Cairo::RefPtr<Cairo::ImageSurface> sp_gradient_to_surface(SPGradient* gr, int width, int height) {
    auto surface = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, width, height);
    auto ctx = Cairo::Context::create(surface);
    sp_gradient_draw(gr, width, height, ctx);
    surface->flush();

    return surface;
}

Cairo::RefPtr<Cairo::ImageSurface> sp_gradstop_to_surface(SPStop *stop, int width, int height)
{
    auto surface = Cairo::ImageSurface::create(Cairo::ImageSurface::Format::ARGB32, width, height);
    auto cairo_ctx = Cairo::Context::create(surface);
    auto ctx = Renderer::Context(cairo_ctx);

    /* Checkerboard background */
    auto check = make_gradient_checkerboard();
    ctx.rectangle(0, 0, width, height);
    ctx.setSource(check);
    ctx.fillPreserve();

    if (stop) {
        /* Alpha area */
        ctx.rectangle(0, 0, width/2, height);
        ctx.setSource(stop->getColor());
        ctx.fill();

        /* Solid area */
        auto no_alpha = stop->getColor();
        no_alpha.enableOpacity(false);
        ctx.rectangle(width/2, 0, width, height);
        ctx.setSource(no_alpha);
        ctx.fill();
    }

    surface->flush();

    return surface;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
