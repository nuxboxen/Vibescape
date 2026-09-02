// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Render out an SVG into a raster surface.
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef RENDERER_SURFACE_FACTORY_H
#define RENDERER_SURFACE_FACTORY_H

#include <2geom/rect.h>

#include "colors/color.h"
#include "renderer/surface.h"
#include "renderer/enums.h"

class SPDocument;
class SPItem;
class SPObject;

namespace Inkscape::Renderer {

/**
 * Generate a rendered svg from an svg document. Set up a factory and all its
 * attending settings before running the render function.
 *
 * Use the convience function to use all default settings.
 */
class SvgRenderer
{
public:
    /**
     * Render the given document to a raster using the factory settings.
     */
    std::shared_ptr<Surface> render(SPDocument *doc) const;

    /**
     * Render the given document to an existing surface object. Used by SurfaceImage.
     */
    void render(Surface &surface, SPDocument *document) const;

    /**
     * Render the given object in the best way possible, making decisions about how to
     * present an object with the settings and the type of object it is.
     */
    std::shared_ptr<Surface> render(SPObject const *object) const;

    /**
     * The area to render in document units.
     */
    void set_area(Geom::Rect const &area) {
        _area = area.roundOutwards();
    }

    /**
     * The resolution of the final output.
     */
    void set_dpi(double dpi) { _xdpi = dpi; _ydpi = dpi; }

    /**
     * The resolution of the final output.
     */
    void set_dpi(double xdpi, double ydpi) { _xdpi = xdpi; _ydpi = ydpi; }

    /**
     * The device scale to render to.
     */
    void set_device_scale(int scale) { _device_scale = scale; }

    /**
     * The viewbox scale to render to.
     */
    void set_viewbox_scale(double scale) { _viewbox_xscale = scale; _viewbox_yscale = scale; }

    /**
     * The viewbox scale to render to.
     */
    void set_viewbox_scale(double xscale, double yscale) { _viewbox_xscale = xscale; _viewbox_yscale = yscale; }

    /**
     * Set a list of items to restrict to.
     */
    void set_item_limit(std::vector<SPItem const *> items) { _items = std::move(items); }
    void set_opaque(bool opaque) { _is_opaque = opaque; }

    /**
     * Render everything onto a solid color.
     */
    void set_background(Colors::Color color)
    {
        _background_color = std::move(color);
    }

    /**
     * Render everything on a checkboard pattern with these colors.
     */
    void set_checkerboard(Colors::Color color, std::optional<Colors::Color> color2 = {})
    {
        _checkerboard_color = std::move(color);
        _checkerboard_color2 = color2;
    }

    /**
     * Debug the rendering by outputting compilable test code.
     */
    void set_code_build(bool code_build) { _code_build = code_build; }

    /**
     * Override the color space used by the document's final rendering.
     */
    void set_final_color_space(std::shared_ptr<Colors::Space::AnySpace> const &space)
    {
        _color_space = space;
    }

    /**
     * Set the antialiasing settings for this rendering.
     */
    void set_antialiasing(Antialiasing antialias)
    {
        _antialiasing_override = antialias;
    }

    /**
     * Static method to render an svg with default settings.
     */
    static auto render_svg(SPDocument *doc)
    {
        return SvgRenderer().render(doc);
    }

    /**
     * Return the size of the document we expect to render. Used by SurfaceImage
     */
    std::optional<Geom::IntPoint> get_dimensions(SPDocument *document) const;
    Geom::IntPoint get_dimensions(Geom::Rect const &area) const;
private:
    double get_xscale() const;
    double get_yscale() const;
    Geom::OptRect get_area(Geom::OptRect const &def) const;

    bool _is_opaque = false;
    bool _code_build = false;

    double _xdpi = 96.0;
    double _ydpi = 96.0;
    double _viewbox_xscale = 0;
    double _viewbox_yscale = 0;
    int _device_scale = 1;
    Geom::OptRect _area;
    std::vector<SPItem const *> _items;

    std::optional<Colors::Color> _background_color;
    std::optional<Colors::Color> _checkerboard_color;
    std::optional<Colors::Color> _checkerboard_color2;
    std::shared_ptr<Colors::Space::AnySpace> _color_space;
    std::optional<Antialiasing> _antialiasing_override;
};

} // namespace Inkscape::Renderer

#endif // RENDERER_SURFACE_FACTORY_H

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
