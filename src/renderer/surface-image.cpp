// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Load and save raster image formats using glycin.
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "surface-image.h"

#include <cairomm/surface.h>

#include "colors/manager.h"
#include "colors/cms/profile.h"
#include "colors/spaces/cms.h"

#include "renderer/surface-factory.h"

#include "util/uri.h"

namespace Inkscape::Renderer {

static auto const srgb = Colors::Manager::get().find(Colors::Space::Type::RGB);

/* ======== STATIC FUNCTIONS ======== */

static GlyMemoryFormat get_memory_format(Cairo::RefPtr<Cairo::ImageSurface> cairo_surface)
{
    switch (cairo_image_surface_get_format(cairo_surface->cobj())) {
        case CAIRO_FORMAT_A8:
            return  GLY_MEMORY_G8;
        case CAIRO_FORMAT_RGB24:
            return GLY_MEMORY_B8G8R8;
        case CAIRO_FORMAT_ARGB32:
            return GLY_MEMORY_B8G8R8A8_PREMULTIPLIED;
        case CAIRO_FORMAT_RGBA128F:
            return GLY_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED;
        default:
            throw Image::ImageError("Unsupported memory format for raster image.");
    }
}

/**
 * Process the given GlyLoader into a bundle of useful raster information
 * making sure the memory format of the output is floating point.
 *
 * @arg loader - The GlyLoader to load an image from.
 *
 * @returns The GlyImage pointer
 */
static GlyImage *get_image_from_loader(GlyLoader *loader)
{
    gly_loader_set_accepted_memory_formats(loader, GLY_MEMORY_SELECTION_R32G32B32A32_FLOAT_PREMULTIPLIED);
    gly_loader_set_apply_transformations(loader, false);
    gly_loader_set_color_convert_icc_srgb(loader, false);
    return gly_loader_load(loader, nullptr);
}
/**
 * Load any image from a file as referenced by a Gio::File.
 *
 * @arg file - The file, which may be a uri to the image in question
 *
 * @returns A varient of either a GlyImage pair or an SPDocument depending on
 *          the detected type.
 */
static ImageType load_from_file(Glib::RefPtr<Gio::File> const &file)
{
    if (!file->query_exists() || file->query_file_type() != Gio::FileType::REGULAR) {
        throw Image::ImageError("Image file does not exist or is not a regular file.");
    }
    if (Glib::str_has_suffix(file->get_basename(), ".svg")) {
        if (auto doc = SPDocument::createNewDoc(file->get_path().c_str()); doc && doc->getRoot()) {
            return std::move(doc);
        }
        return std::monostate{};
    }
    // We load through glycin as a filename to take advantage of the security
    auto loader = gly_loader_new(file->gobj());
    if (!loader) {
        throw Image::ImageError("Couldn't make loader for the filename using Glycin.");
    }
    auto image = get_image_from_loader(loader);
    if (!image) {
        throw Image::ImageError("Couldn't get raster image from loader using Glycin.");
    }
    return GlyLoad(loader, image);
}

/**
 * Load any image from a set of bytes, you must keep the Bytes object alive for the GlyLoader
 *
 * @arg bytes - The bytes to load into an image.
 *
 * @returns A varient of either a GlyImage pair or an SPDocument.
 */
static ImageType load_from_bytes(Glib::RefPtr<Glib::Bytes> bytes, std::optional<bool> is_svg = {})
{
    gsize length;
    const char *byte_ptr = (const char*)bytes->get_data(length);

    if (!is_svg) { // We don't know if it's svg or not, do some detectoring.
        static const char *svg_tag = (const char *)"<svg";
        auto it = std::search(byte_ptr, byte_ptr + std::min((gsize)200, length), svg_tag, svg_tag + 4);
        is_svg = it < byte_ptr + 200; // Found an svg tag in the first 200 bytes
    }
    if (*is_svg) {
        if (auto doc = SPDocument::createNewDocFromMem({byte_ptr, length}); doc && doc->getRoot()) {
            return std::move(doc);
        }
        return std::monostate{};
    }
    // Keep these bytes around until we have the image loaded
    auto loader = gly_loader_new_for_bytes(bytes->gobj());
    return GlyLoad(loader, get_image_from_loader(loader));
}

/**
 * Load an image from a base64 encoded string. Bytes are passed to load_from_bytes.
 *
 * @arg uri - A base64 encoded uri string. This is NOT a url.
 *
 * @returns See load_from_bytes.
 */
static ImageType load_from_base64(std::string_view const &uri)
{
    auto [data, type] = extract_uri_data(uri.data());
    gsize decoded_len = 0;
    unsigned char *decoded = g_base64_decode(data, &decoded_len);
    // This makes a copy, very annoying.
    auto bytes = Glib::wrap(g_bytes_new(decoded, decoded_len));
    return load_from_bytes(bytes, type == Base64Data::SVG);
}

/**
 * Get the image size regardless of detected type. This is fed into
 * the surface constructor for consistant immutability.
 */
static Geom::IntPoint get_image_size(ImageType &image, SvgFactory const &svg_factory)
{
    if (std::holds_alternative<GlyLoad>(image)) {
        auto gi = std::get<GlyLoad>(image).second;
        return Geom::IntPoint(gly_image_get_width(gi), gly_image_get_height(gi));
    } else if (std::holds_alternative<std::unique_ptr<SPDocument>>(image)) {
        if (auto pt = svg_factory->get_dimensions(std::get<std::unique_ptr<SPDocument>>(image).get())) {
            return *pt;
        }
    }
    return Geom::IntPoint(0, 0);
}

static SvgFactory default_svg_factory()
{
    return std::make_shared<SurfaceFactory>();
}

/* ======== OBJECT FUNCTIONS ======== */

Image::Image(std::string_view const &uri, SvgFactory const &svg_factory)
    : Image(load_from_base64(uri), svg_factory ? std::move(svg_factory) : default_svg_factory())
{}
Image::Image(Glib::RefPtr<Gio::File> const &file, SvgFactory const &svg_factory)
    : Image(load_from_file(file), svg_factory ? std::move(svg_factory) : default_svg_factory())
{}
Image::Image(Glib::RefPtr<Glib::Bytes> bytes, SvgFactory const &svg_factory)
    : Image(load_from_bytes(bytes), svg_factory ? std::move(svg_factory) : default_svg_factory(), bytes)
{}

// Note: This is private and we expect svg_factory will have had a default set by
// the time this code runs. It's a bug if the svg_factory is empty here.
Image::Image(ImageType image, SvgFactory const &svg_factory, Glib::RefPtr<Glib::Bytes> bytes)
    : Surface(get_image_size(image, svg_factory), 1.0)
    , _svg_factory(std::move(svg_factory))
    , _byte_cache(bytes)
{
    if (std::holds_alternative<GlyLoad>(image)) {
        auto pair = std::get<GlyLoad>(image);
        _loader = pair.first;
        _image = pair.second;
        if (auto mime = gly_image_get_mime_type(_image)) {
            _mime_type = mime;
        }
    } else if (std::holds_alternative<std::unique_ptr<SPDocument>>(image)) {
        _doc = std::move(std::get<std::unique_ptr<SPDocument>>(image));
        _mime_type = "image/svg+xml";
    }
}

Image::~Image()
{
    if (_frame) {
        _surfaces.clear();
        g_object_unref(_frame);
    }
    if (_image) {
        g_object_unref(_image);
    }
    if (_loader) {
        g_object_unref(_loader);
    }
}

std::vector<Cairo::RefPtr<Cairo::ImageSurface>> const &Image::getCairoSurfaces() const
{
    if (ready()) {
        // Already loaded
        return Surface::getCairoSurfaces();
    } else if (_image) {
        // Moving some parts of this to the loader requires more metadata about the file
        // such as it's width and height, prior to being fully loaded. Not available in glycin.
        if ((_frame = gly_image_next_frame(_image, nullptr))) {

            // There's a future here were we support 4 channel CMYK but Glycin
            // today can't do it. If upstream supports it, we'd load in two memory
            // frames for CMYA and K--A into cairo here.
            GBytes *bytes = gly_frame_get_buf_bytes(_frame);
            gsize bytes_size;
            unsigned char *bytes_src = (unsigned char*)g_bytes_get_data(bytes, &bytes_size);

            auto cobj = cairo_image_surface_create_for_data(
                bytes_src,             // Bytes are owned by GlyFrame
                CAIRO_FORMAT_RGBA128F, // <-> GLY_MEMORY_SELECTION_R32G32B32A32*
                gly_frame_get_width(_frame),
                gly_frame_get_height(_frame),
                gly_frame_get_stride(_frame)
            );
            cairo_surface_set_device_scale(cobj, getDeviceScale(), getDeviceScale());
            _surfaces.push_back(Cairo::RefPtr<Cairo::ImageSurface>(new Cairo::ImageSurface(cobj, true)));

            switch (gly_frame_get_color_mode(_frame)) {
                case GLY_COLOR_MODE_SRGB:
                    _color_space = srgb;
                    break;
                case GLY_COLOR_MODE_CICP:
                    std::cerr << "Can't handle CICP color space definition (video format?)\n";
                    break;
                case GLY_COLOR_MODE_ICC_PROFILE:
                    auto icc_profile = Colors::CMS::Profile::create_from_data(Glib::wrap(gly_frame_get_color_icc_profile(_frame)));
                    _color_space = std::make_shared<Colors::Space::CMS>(icc_profile);
                    break;
            }
        } else {
            throw Image::ImageError("Glycin: No frame to load.");
        }
    } else if (_doc) {
        // Rendering into the surfaces at get time does require a bit of const shinanigans.
        _svg_factory->render(*const_cast<Image *>(this), _doc.get());
    } else {
        throw Image::ImageError("Glycin: No image to load.");
    }
    return Surface::getCairoSurfaces();
}

Glib::RefPtr<Glib::Bytes> Image::encode_as_bytes(std::optional<std::string> mime_type)
{
    GError *error;
    auto mime = mime_type ? *mime_type : _mime_type;

    if (mime == "image/svg+xml") {
        throw Image::ImageError("Can't export a purely raster image as an svg");
    } else if (_surfaces.empty()) {
//        if (_mime_type == mime_type && _image) {
//            Future optimisation: Just ask for the image bytes we already have
        throw Image::ImageError("There is no image ready for image-encoding.");
    } else if (_surfaces.size() > 1) {
        throw Image::ImageError("Can not save more than 3 channels of raster data yet.");
    } else if (GlyCreator *creator = gly_creator_new(mime.c_str(), NULL)) {
        auto memory_strd = _surfaces[0]->get_stride();
        auto memory_fmt = get_memory_format(_surfaces[0]);
        auto memory = _surfaces[0]->get_data();
        GBytes *unencoded = g_bytes_new(memory, memory_strd * height());

        if (!gly_creator_add_frame_with_stride(creator,
            width(), height(), memory_strd,
            memory_fmt, unencoded, &error)) {
            throw Image::ImageError("Failed to create image: " + std::string(error->message));
        }

        auto image = gly_creator_create(creator, &error);
        if (!image) {
            throw Image::ImageError("Failed to encode image: " + std::string(error->message));
        }

        auto encoded = gly_encoded_image_get_data(image);
        g_object_unref(creator);

        return Glib::wrap(encoded);
    } else {
        throw Image::ImageError("Failed to load Glycin Creator for mime type '"+mime+"'. " + (error ? std::string(error->message) : ""));
    }
}

std::string const Image::encode_as_base64(std::optional<std::string> mime_type)
{
    auto mime = mime_type ? *mime_type : _mime_type;

    if (auto bytes = encode_as_bytes(mime_type)) {
        gsize length;
        auto byte_ptr = bytes->get_data(length);
        auto encoded = g_base64_encode((const unsigned char*)byte_ptr, length);
        std::string ret = std::string("data:") + mime + ";base64," + encoded;
        g_free(encoded);
        return ret;
    }
    return "";
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
