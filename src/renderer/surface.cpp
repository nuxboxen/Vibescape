// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Contain multiple Cairo surfaces for rendering
 *//*
 * Authors:
 *  Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cairomm/surface.h>

#include "colors/spaces/components.h"
#include "pixel-filters/color-space.h"
#include "renderer/surface.h"

namespace Inkscape::Renderer {

/**
 * Create a rendering surface
 */
Surface::Surface(Geom::IntPoint const &dimensions, int device_scale, std::shared_ptr<Colors::Space::AnySpace> const &color_space)
    : _dimensions(dimensions)
    , _device_scale(device_scale)
    , _color_space(color_space)
{}

Surface::Surface(Cairo::RefPtr<Cairo::ImageSurface> const &argb32_source, bool convert)
    : _dimensions{argb32_source->get_width(), argb32_source->get_height()}
    , _device_scale{argb32_source->get_device_scale()}
{
    if (convert) {
        _color_space = Colors::Manager::get().find(Colors::Space::Type::RGB);
        auto pa = PixelAccess<MEMORY_FORMAT_RGBA128F>(getCairoSurfaces()[0]);
        PixelAccess<MEMORY_FORMAT_ARGB32>(argb32_source).createContiguousCopy(pa);
    } else {
        _surfaces.emplace_back(argb32_source);
    }
}

#ifdef UNIT_TEST
Surface::Surface(std::string const &filename)
        : _surfaces{Cairo::ImageSurface::create_from_png(filename)}
        , _dimensions{_surfaces.back()->get_width(), _surfaces.back()->get_height()}
        , _device_scale{1.0}
        , _color_space(Colors::Manager::get().find(Colors::Space::Type::RGB))
{
    // When loading 32bit PNGS, but not when loading 64bit PNGs

    auto format = cairo_image_surface_get_format(_surfaces[0]->cobj());
    if (format != CAIRO_FORMAT_RGBA128F) {
        auto surface = _surfaces.back();
        _surfaces.pop_back();
        auto pixels = PixelAccess<MEMORY_FORMAT_RGBA128F>(getCairoSurfaces()[0]);

        if (format == CAIRO_FORMAT_ARGB32) { // 32bit PNG
            PixelAccess<MEMORY_FORMAT_ARGB32>(surface).createContiguousCopy(pixels);
        } else if (format ==  CAIRO_FORMAT_RGB24) { // 24bit PNG
            PixelAccess<MEMORY_FORMAT_RGB24>(surface).createContiguousCopy(pixels);
        } else {
            throw SurfaceError(std::string("Wrong format returned from opening PNG file: '") + get_cairo_format_name(format) + "'");
        }
    }
}
#endif

Surface Surface::convertedToFloat() const
{
    // Always initalise the data because we don't want to return an empty Surface when it used to be an ImageSurface
    auto surfaces = getCairoSurfaces();
    if (_color_space) return *this; // already float

    // An int surface is ALWAYS in sRGB to the target color space is sRGB
    static auto rgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto ret = Surface(_dimensions, _device_scale, rgb);

    if (surfaces.empty()) {
        throw SurfaceError("No image data to convert to format.");
    }
    auto pa = PixelAccess<MEMORY_FORMAT_RGBA128F>(ret.getCairoSurfaces()[0]);
    PixelAccess<MEMORY_FORMAT_ARGB32>(surfaces[0]).createContiguousCopy(pa);

    ret._user_data = _user_data;
    return ret;
}

Surface Surface::convertedToInt() const
{
    // Always initalise the data because we don't want to return an empty Surface when it used to be an ImageSurface
    auto surfaces = getCairoSurfaces();
    if (!_color_space) return *this; // already int, makes a copy

    static auto rgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    if (_color_space != rgb) {
        // Integer spaces are ALWAYS sRGB, no exception so this now needs to be converted.
        return convertedToColorSpace(rgb)->convertedToInt();
    }

    auto ret = Surface(_dimensions, _device_scale);

    if (surfaces.empty()) {
        throw SurfaceError("No image data to convert to format.");
    }
    auto pa = PixelAccess<MEMORY_FORMAT_ARGB32>(ret.getCairoSurfaces()[0]);
    PixelAccess<MEMORY_FORMAT_RGBA128F>(surfaces[0]).createContiguousCopy(pa);

    ret._user_data = _user_data;
    return ret;
}

Surface Surface::convertedToCompatible(cairo_format_t target_format) const
{
    if (target_format == CAIRO_FORMAT_ARGB32 && format() == CAIRO_FORMAT_RGBA128F) {
        std::cerr << "Warning: Slowly converting surface to Integer.\n";
        return convertedToInt();
    } else if (target_format == CAIRO_FORMAT_RGBA128F && format() == CAIRO_FORMAT_ARGB32) {
        std::cerr << "Warning: Slowly converting surface to Floating Point.\n";
        return convertedToFloat();
    } else if (target_format != format() && target_format != CAIRO_FORMAT_INVALID) {
        std::ostringstream msg;
        msg << "Can not convert memory formats: " << get_cairo_format_name(format())
                                        << " to " << get_cairo_format_name(target_format);
        throw SurfaceError(msg.str());
    }
    return *this;
}

Cairo::RefPtr<Cairo::ImageSurface> Surface::exportToARGB32() const
{
    // Temporary object during conversion
    return convertedToInt()._surfaces[0];
}

inline std::pair<cairo_format_t, unsigned> Surface::getSurfaceFormat(std::shared_ptr<Colors::Space::AnySpace> const &space)
{
    // Backwards compatability for smaller memory footprint.
    auto format = CAIRO_FORMAT_ARGB32;
    unsigned count = 1;

    // Get enough surfaces to store all the channels in the format
    if (space) {
        auto size = space->getComponentCount();
        if (size == 0) { // Alpha channel, opacity only
            format = CAIRO_FORMAT_A8;
        } else {
            format = CAIRO_FORMAT_RGBA128F;
            count = std::ceil((double)size / 3);
        }
    }
    return {format, count};
}

/**
 * Create or return existing cairo surfaces.
 */
std::vector<Cairo::RefPtr<Cairo::ImageSurface>> const &Surface::getCairoSurfaces() const
{
    auto [format, count] = getSurfaceFormat(_color_space);

    // deferred allocation
    if (_surfaces.empty()) {
        for (unsigned i = 0; i < count; i++) {
            // Must be created in C as CairoMM doesn't support all the needed formats yet.
            auto cobj = cairo_image_surface_create(format,
                                                   _dimensions.x() * _device_scale,
                                                   _dimensions.y() * _device_scale);
            cairo_surface_set_device_scale(cobj, _device_scale, _device_scale);
            _surfaces.push_back(Cairo::RefPtr<Cairo::ImageSurface>(new Cairo::ImageSurface(cobj, true)));
        }
    }
    return _surfaces;
}

int Surface::components() const
{
    return _color_space ? _color_space->getComponentCount() : 3;
}

std::shared_ptr<Surface> Surface::similar(std::optional<Geom::IntPoint> dimensions) const
{
    return std::make_shared<Surface>(dimensions ? *dimensions : _dimensions, _device_scale, _color_space);
}

std::shared_ptr<Surface> Surface::similar(std::optional<Geom::IntPoint> dimensions, std::shared_ptr<Colors::Space::AnySpace> const &color_space) const
{
    return std::make_shared<Surface>(dimensions ? *dimensions : _dimensions, _device_scale, color_space);
}

void Surface::convertToColorSpace(std::shared_ptr<Colors::Space::AnySpace> const &color_space)
{
    if (color_space->getType() == Colors::Space::Type::Alpha) {
        throw SurfaceError("Refusing to convert to alpha in-place, make a copy instead");
    }
    if (!color_space || !_color_space) {
        throw SurfaceError("Refusing to convert to or from a legacy color space sRGB:RGBA32");
    }
    if (ready() && color_space != _color_space) {
        run_pixel_filter(PixelFilter::ColorSpaceTransform(_color_space, color_space), *this);
    }
    sanityCheckSurface(color_space);
    _color_space = color_space;
}

std::shared_ptr<Surface> Surface::convertedToColorSpace(std::shared_ptr<Colors::Space::AnySpace> const &color_space) const
{
    auto dest = similar(_dimensions, color_space);
    dest->_user_data = _user_data;
    if (ready()) {
        if ((color_space && color_space->getType() == Colors::Space::Type::Alpha)
            || (_color_space && _color_space->getType() == Colors::Space::Type::Alpha)) {
            dest->run_pixel_filter(PixelFilter::AlphaSpaceExtraction(), *this);
        } else {
            dest->run_pixel_filter(PixelFilter::ColorSpaceTransform(_color_space, color_space), *this);
        }
    }
    return dest;
}

static std::string csp_to_str(std::shared_ptr<Colors::Space::AnySpace> const &space)
{
    return space ? space->getName() : "{INTRGB}";
}

void Surface::sanityCheckSurface(std::shared_ptr<Colors::Space::AnySpace> const &space) const
{
    if (_surfaces.empty()) {
        // Generate cairo surfaces as this point.
        getCairoSurfaces();
    }

    if ((space && space->getType() == Colors::Space::Type::Alpha) || format() == CAIRO_FORMAT_A8) {
        return;
    }

    assert(sanityCheckColorSpace(space, _color_space));

    auto [fmt, count] = getSurfaceFormat(space);

    if (_surfaces.size() != count) {
        std::ostringstream msg;
        msg << "Wrong number of surfaces '" << _surfaces.size() << " for color space " << csp_to_str(space) << ", expected '" << count << "'";
        throw SurfaceError(msg.str());
    }

    if (format() != fmt) {
        std::ostringstream msg;
        msg << "Wrong surface memory, expected '" << get_cairo_format_name(format()) << "' but got '" << get_cairo_format_name(fmt) << "'\n";
        throw SurfaceError(msg.str());
    }
}

bool Surface::sanityCheckColorSpace(std::shared_ptr<Colors::Space::AnySpace> const &space,
                                    std::shared_ptr<Colors::Space::AnySpace> const &other)
{
    if (space != other) {
        std::ostringstream msg;
        msg << "Wrong target color space '" << csp_to_str(space) << "' for surface in '" << csp_to_str(other) << "'\n";
        throw SurfaceError(msg.str());
    }
    return true;
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
