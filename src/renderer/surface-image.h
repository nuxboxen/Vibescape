// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Load and save raster image formats using glycin.
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef RENDERER_SURFACE_IMAGE_H
#define RENDERER_SURFACE_IMAGE_H

#include <string>
#include <giomm.h>
#include <glycin.h>

#include "document.h"
#include "surface.h"

namespace Inkscape::Renderer {

using GlyLoad = std::pair<GlyLoader *, GlyImage *>;
using ImageType = std::variant<std::monostate, GlyLoad, std::unique_ptr<SPDocument>>;

class SvgRenderer; // Svg loading
using SvgFactory = std::shared_ptr<SvgRenderer>;

class Image : public Surface
{
public:
    class ImageError : public std::exception
    {
        std::string msgstr;
    public:
        ImageError(std::string msg) : std::exception(), msgstr(std::move(msg)) {}
        const char* what() const noexcept override { return msgstr.c_str(); }
    };
private:
    Image(ImageType image, SvgFactory const &svg_factory, Glib::RefPtr<Glib::Bytes> bytes = {});
public:
    ~Image();

    /**
     * Load an svg or raster image for a file.
     *
     * @arg file - The file object to load from disk.
     * @arg svg_factory - The optinal svg loader which may be set with rendering
     *                    settings such dpi, antialiasing, etc. See SvgRenderer
     */
    Image(Glib::RefPtr<Gio::File> const &file, SvgFactory const &svg_factory = {});

    /**
     * Load an svg or raster image from bytes, or from a base64 uri of bytes.
     *
     * @arg bytes - The bytes object to use, or the base64 encoded uri.
     * @arg svg_factory - The optinal svg loader which may be set with rendering
     *                    settings such dpi, antialiasing, etc. See SvgRenderer
     */
    Image(Glib::RefPtr<Glib::Bytes> bytes, SvgFactory const &svg_factory = {});
    Image(std::string_view const &base64_uri, SvgFactory const &svg_factory = {});

    /**
     * Output a raster surface as a set of encoded bytes in the format of the mime_type
     * specified. IF no mime type is specified then the original mime type is used.
     */
    Glib::RefPtr<Glib::Bytes> encode_as_bytes(std::optional<std::string> mime_type);
    std::string const encode_as_base64(std::optional<std::string> mime_type);

    /**
     * Internal function for getting the final raster images. Do not use this unless
     * you know exactly why you are overriding the normal Renderer API infrastructure.
     */
    std::vector<Cairo::RefPtr<Cairo::ImageSurface>> const &getCairoSurfaces() const override;

    /**
     * Some data is only available after loading, color space for images is just-in-time
     * rather than defined on construction.
     */
    std::shared_ptr<Colors::Space::AnySpace> getColorSpace() const {
        if (!ready()) {
            getCairoSurfaces(); // Load image
        }
        return _color_space;
    }

    void setEncodingQuality(int val) { _encoding_quality = val; }
    void setEncodingCompression(int val) { _encoding_compression = val; }
    void setPixelDensity(Geom::Point density) { _pixel_density = density; }
    void setMetadata(std::string key, std::string val) { _metadata[key] = val; }
    void setInterlacing(bool interlacing) { _interlacing = interlacing; }

private:
    SvgFactory const _svg_factory;
    std::unique_ptr<SPDocument> _doc;      // Used in getCairoSurface to render the svg
    GlyLoader *_loader = nullptr;          // Primary access when loading a GlyImage
    GlyImage *_image = nullptr;            // Used in getCairoSurface to render a raster image
    Glib::RefPtr<Glib::Bytes> _byte_cache; // Memory used by GlyImage until rendered
    std::string _mime_type;                // Remember opening mime type for saving

    mutable GlyFrame *_frame = nullptr;    // Shared memory with Cairo::Surface

    std::optional<int> _encoding_quality;
    std::optional<int> _encoding_compression;
    std::optional<bool> _interlacing;
    std::optional<Geom::Point> _pixel_density;
    std::map<std::string, std::string> _metadata;
};

} // namespace Inkscape::Renderer

#endif // RENDERER_SURFACE_IMAGE_H

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
