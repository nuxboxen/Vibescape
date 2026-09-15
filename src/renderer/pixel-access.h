// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Access the memory of a surface of pixels in a predictable way these can be either
 * cairo surface memory formats (CAIRO_FORMAT_*) when supplied with Cairo::Surface
 * objects. Or maybe simple arrays for conversion and export by a supplying std::vector<T>.
 *//*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2025-2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_PIXEL_ACCESS_H
#define INKSCAPE_RENDERER_PIXEL_ACCESS_H

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <type_traits>
#include <cairomm/surface.h>
#include <glib.h>

#include "helper/mathfns.h"
#include "threading.h"

/**
 * Terms:
 *
 *  Color - Is a collection of channels, plus an alpha of an inkscape color space.
 *  Channel - Is one of those color space double values where alpha is always the last
 *            item. For example in CMYKA, C is channel 0, M is 1 and A is 4
 *  Surface - Is a collection of Cairo pixels in a 2d grid with a specific stride.
 *  Pixel - A collection of Primaries packed into this
 *          surface grid. These may be floats or integers of various scales.
 *  Primary - One of the values packed into a pixel. These get turned into channels
 *            through unpacking of specific memory locations.
 *  Coordinates - Or Coords, are a pair of X,Y values within the surface image.
 *  Position - A single memory address offset which a coordinate can be transformed
 *             into to locate the pixel or primary in the surface memory.
 */
namespace Inkscape::Renderer {

class PixelAccessError : public std::exception
{
public:
    PixelAccessError(std::string msg)
        : std::exception()
        , msgstr(std::move(msg))
    {}
    const char* what() const noexcept override {
        return msgstr.c_str();
    }
protected:
    std::string msgstr;
};

/**
 * What to do when a x/y coordinate is outside the width and height. This happens
 * when filters are asking for small grids of pixels.
 */
enum class PixelAccessEdgeMode
{
    NO_CHECK = 0, // No edge checking needed, crash if out of bounds
    ERROR,  // Raise an error
    EXTEND, // Clamp the x,y to 0,0,w,h
    WRAP,   // Treat surface as a spherical space
    ZERO,   // Return zero for getter, and ignore OOB setter
};

enum MemoryFormat
{
    MEMORY_FORMAT_INVALID  = CAIRO_FORMAT_INVALID,
    MEMORY_FORMAT_A8       = CAIRO_FORMAT_A8,
    MEMORY_FORMAT_RGB24    = CAIRO_FORMAT_RGB24,
    MEMORY_FORMAT_ARGB32   = CAIRO_FORMAT_ARGB32,
    MEMORY_FORMAT_RGBA128F = CAIRO_FORMAT_RGBA128F,
    MEMORY_FORMAT_RGB96F   = CAIRO_FORMAT_RGB96F,

    MEMORY_FORMAT_NOTCAIRO = 1000, // Split between cairo and non-cairo memory

    MEMORY_FORMAT_G8,
    MEMORY_FORMAT_GA16,
    MEMORY_FORMAT_G16,
    MEMORY_FORMAT_GA32,
    MEMORY_FORMAT_RGBA64,
    MEMORY_FORMAT_RGBA128,
    MEMORY_FORMAT_CMYA_KA256F, // CMYK split into two RGBA128F
    MEMORY_FORMAT_CMYKA160F,
};

/**
 * Get the cairo format as a printable name. Used in tests, errors and debugging.
 */
inline std::string get_memory_format_name(MemoryFormat format)
{
    static const std::map<MemoryFormat, std::string> map = {
        {MEMORY_FORMAT_INVALID,  "INVALID"},
        {MEMORY_FORMAT_A8,       "A8"},
        {MEMORY_FORMAT_RGB24,    "RGB24"},
        {MEMORY_FORMAT_ARGB32,   "ARGB32"},
        {MEMORY_FORMAT_RGBA128F, "RGBA128F"},
        {MEMORY_FORMAT_RGB96F,   "RGB96F"},

        {MEMORY_FORMAT_G8,       "*G8"},
        {MEMORY_FORMAT_GA16,     "*GA16"},
        {MEMORY_FORMAT_G16,      "*G16"},
        {MEMORY_FORMAT_GA32,     "*GA32"},
        {MEMORY_FORMAT_RGBA64,   "*RGBA64"},
        {MEMORY_FORMAT_RGBA128,  "*RGBA128"},
        {MEMORY_FORMAT_CMYA_KA256F, "*CMYA_KA256F"},
        {MEMORY_FORMAT_CMYKA160F,   "*CMYKA160F"},
    };
    return map.at(format);
}
inline std::string get_cairo_format_name(cairo_format_t format)
{
    return get_memory_format_name((MemoryFormat)format);
}


/**
 * Scale numbers between two number types by scaling them
 *
 * From T0 to T1 use: value * get_format_scale<...>();
 * From T1 to T0 use: value / get_format_scale<...>();
 */
template <typename T0, typename T1>
constexpr static inline double get_format_scale()
{
    using namespace std;
    return is_integral_v<T1>
        ? (is_integral_v<T0>
          ? (double)numeric_limits<T1>::max() / numeric_limits<T0>::max() // T0=char|int  T1=int|char
          : (double)numeric_limits<T1>::max())                            // T0=float     T1=int|char
        : (is_integral_v<T0>
          ? (double)1.0 / numeric_limits<T0>::max()                       // T0=char|int  T1=float
          : (double)1.0);                                                 // T0=float     T1=float
}

/**
 * Convert *back* from an inkscape memory format to a cairo memory format for checking and converting.
 */
constexpr static inline cairo_format_t memory_to_cairo_format(MemoryFormat format)
{
    if (format < MEMORY_FORMAT_NOTCAIRO) {
        return (cairo_format_t)format;
    }
    if (format == MEMORY_FORMAT_CMYA_KA256F) {
        return CAIRO_FORMAT_RGBA128F; // Map
    }
    return CAIRO_FORMAT_INVALID;
}

/**
 * Image surface memory access for different types which can span multiple surfaces.
 *
 * @template_arg format           - The cairo type for this pixel access.
 * @template_arg edge_mode        - Set the edge checking and how out of range x,y coordinates treated
 */
template <MemoryFormat _format, PixelAccessEdgeMode edge_mode = PixelAccessEdgeMode::NO_CHECK>
class PixelAccess
{
public:
    constexpr static MemoryFormat format = _format;

    constexpr static bool is_integer =
        format != MEMORY_FORMAT_RGBA128F &&
        format != MEMORY_FORMAT_RGB96F &&
        format != MEMORY_FORMAT_CMYKA160F &&
        format != MEMORY_FORMAT_CMYA_KA256F;

    constexpr static bool has_alpha = 
        format != MEMORY_FORMAT_RGB24 &&
        format != MEMORY_FORMAT_RGB96F &&
        format != MEMORY_FORMAT_G8 &&
        format != MEMORY_FORMAT_G16;

    constexpr static bool little_endian = G_BYTE_ORDER == G_LITTLE_ENDIAN && (
        format == MEMORY_FORMAT_RGB24 ||
        format == MEMORY_FORMAT_ARGB32);

    // How many primaries are there in this format
    constexpr static int primary_count =
        format == MEMORY_FORMAT_CMYKA160F ? 4 : (
        format == MEMORY_FORMAT_A8 ? 0 : (
        format == MEMORY_FORMAT_G8 ||
        format == MEMORY_FORMAT_GA16 ||
        format == MEMORY_FORMAT_G16 ||
        format == MEMORY_FORMAT_GA32 ? 1 : 3));
    constexpr static int primary_total = primary_count + has_alpha; // Plus Alpha

    // The internal type used by each channel in the format
    using PrimaryType = std::conditional_t<!is_integer,                       float,
                          std::conditional_t<format == MEMORY_FORMAT_RGBA128, uint32_t,
                          std::conditional_t<format == MEMORY_FORMAT_RGBA64 ||
                                             format == MEMORY_FORMAT_G16 ||
                                             format == MEMORY_FORMAT_GA32,    uint16_t,
                                                                              uint8_t>>>;

    // Provides the size of the primary in memory as number of bytes
    constexpr static int primary_size = sizeof(PrimaryType);

    // Allows for compile time requires that edge mode be set
    constexpr static bool checks_edge = edge_mode != PixelAccessEdgeMode::NO_CHECK;

    // Scale of each primary to convert to a double used in Channels
    constexpr static double primary_scale = get_format_scale<double, PrimaryType>();
    constexpr static double primary_unscale = get_format_scale<PrimaryType, double>();

    // Position of the alpha primary in this format
    constexpr static int primary_alpha = (little_endian || !has_alpha) ? 0 : primary_count;
    constexpr static int primary_alpha_pos = little_endian ? primary_count - primary_alpha : primary_alpha;

    // Actual number of channels from both surfaces (if split onto two surfaces)
    constexpr static int channel_count = format == MEMORY_FORMAT_CMYA_KA256F ? 4 : primary_count;
    constexpr static int channel_total = channel_count + has_alpha;
    using Color = std::array<double, channel_total>;

    // Does this PixelAccess need two surfaces?
    constexpr static bool has_more_channels = channel_count > primary_count;

    /**
     * Create a pixel access object for the given cairo surface(s).
     *
     * @arg cairo_surface - The Cairo Surface to gain memory access to.
     * @arg next_surface  - Optionally add another surface to handle color interpolation
     *                      in MEMORY_FORMAT_CMYA_KA256F.
     */
    explicit PixelAccess(Cairo::RefPtr<Cairo::ImageSurface> cairo_surface,
                         Cairo::RefPtr<Cairo::ImageSurface> next_surface = {})
        requires(channel_count <= primary_count * (has_more_channels + 1))
        : _width(cairo_surface->get_width())
        , _height(cairo_surface->get_height())
        , _stride(cairo_surface->get_stride() / primary_size)
        , _size(_height * _stride)
        , _memory(reinterpret_cast<PrimaryType *>(cairo_surface->get_data()))
        , _cairo_surface(cairo_surface)
        , _next_surface(next_surface)
    {
        if (cairo_image_surface_get_format(cairo_surface->cobj()) != memory_to_cairo_format(format)) {
            throw PixelAccessError("format of the cairo surface doesn't match the PixelAccess type.");
        }
        _cairo_surface->flush(); // This pairs with mark_dirty in ~PixelAccess

        if constexpr (has_more_channels) {
            if (_width != _next_surface->get_width() || _height != _next_surface->get_height() ||
                _stride != _next_surface->get_stride() / primary_size ||
                cairo_image_surface_get_format(_next_surface->cobj()) != memory_to_cairo_format(format)) {
                throw PixelAccessError("Pixel Access Next Surface must be the same formats.");
            }
            _next_memory = reinterpret_cast<PrimaryType *>(_next_surface->get_data());
            _next_surface->flush();
        }
    }

    ~PixelAccess()
    {
        // TODO: Find a way to do this only for changed surfaces
        if (_cairo_surface) {
            _cairo_surface->mark_dirty();
        }
        if constexpr (has_more_channels) {
            _next_surface->mark_dirty();
        }
    }

    /**
     * Create access to a patch of memory which isn't part of a cairo surface. This can be used
     * to do color convertions using lcms2 and run filters on the same memory without needing
     * to convert to cairo formats first.
     */
    PixelAccess(std::vector<PrimaryType> memory, int width, int height)
        : _local_memory(std::move(memory))
        , _width(width)
        , _height(height)
        , _stride(width * primary_total)
        , _size(_height * _stride)
        , _memory(_local_memory.data())
    {}

    /**
     * Create a pixel access and the memory for the given type.
     */
    PixelAccess(int width, int height)
        : PixelAccess(
            std::vector<PrimaryType>(width * height * channel_total, 0.0), width, height)
    {}

    /**
     * Get a color from the surface at the given coordinates.
     *
     * @arg x - The pixel x coordinate to get
     * @arg y - The pixel y coordinate to get
     * @arg unmultiply_alpha - Remove premultiplied alpha if true
     *
     * @return - The pre-sized memory for the returned color space including alpha.
     *           We use the same memory so we don't have to re-allocate for every pixel in a filter.
     */
    template <typename T0 = double>
    inline std::array<T0, channel_total> colorAt(int x, int y, bool unmultiply_alpha = false) const
    {
        std::array<T0, channel_total> ret;
        colorAt<T0>(x, y, ret, unmultiply_alpha);
        return ret;
    }
    template <typename T0 = double>
    inline void colorAt(int x, int y, std::array<T0, channel_total> &ret, bool unmultiply_alpha = false) const
    {
        int pos = _pixel_pos(x, y);
        double alpha = _get_alpha(pos);
        double alpha_mult = unmultiply_alpha ? _mult(alpha) : 1.0;
        for (int c = 0; c < channel_count; c++) {
            ret[c] = _get_channel<T0>(pos, c, alpha_mult);
        }
        // We have to re-request the alpha if we're asking for a different output type
        ret[channel_count] = std::same_as<T0, double> ? alpha : _get_channel<T0>(pos, channel_count, 1.0);
    }

    /**
     * Using bilinear interpolation get the effective pixel at the given coordinates.
     * Note: Bilinear interpolation is two linear interpolations across 4 pixels
     *
     * @arg x - The fractional position in the x coordinate to get.
     * @arg y - The fractional position in the y coordinate to get.
     * @arg unmultiply_alpha - Remove premultiplied alpha if true
     *
     * @return_arg - The pre-sized memory for the returned color space including alpha.
     *               We use the same memory so we don't have to re-allocate for every pixel in a filter.
     */
    template <typename T0 = double>
    inline std::array<T0, channel_total> colorAt(double x, double y, bool unmultiply_alpha = false) const
    {
        constexpr static double scale = get_format_scale<double, T0>();

        int fx = floor(x), fy = floor(y);
        int cx = ceil(x), cy = ceil(y);
        double weight_x = x - fx, weight_y = y - fy;
        double alpha_mult = 1.0;

        std::array<T0, channel_total> ret;
        for (int c = channel_count; c >= 0; c--) {
            double val = _bilinear_interpolate(
                _get_channel(_pixel_pos(fx, fy), c, 1.0), _get_channel(_pixel_pos(cx, fy), c, 1.0),
                _get_channel(_pixel_pos(fx, cy), c, 1.0), _get_channel(_pixel_pos(cx, cy), c, 1.0), weight_x, weight_y);

            ret[c] = val * scale * alpha_mult;
            if (unmultiply_alpha && c == channel_count) {
                alpha_mult = _mult(val);
            }
        }
        return ret;
    }

    /**
     * Set the given pixel to the color values, apply premultiplication of alpha if neccessary to
     * keep the surface in a premultiplied state for further drawing operations.
     *
     * @arg x - The x coordinate to set
     * @arg y - The y coordinate to set
     * @arg values - The color values to set
     * @arg not_premultiplied - If true, values are premultiplied before saving
     *
     * @arg values - A set of doubles to apply to the pixel data
     */
    void colorTo(int x, int y, Color const &values, bool unmultiply_alpha = false)
    {
        _set_primaries_recursively(_pixel_pos(x, y), values[channel_count], values, unmultiply_alpha);
    }

    /**
     * Return the alpha compnent only.
     *
     * @arg x - The x coordinate to get
     * @arg y - The y coordinate to get
     *
     * @returns The alpha channel at the given coordinates
     */
    double alphaAt(int x, int y) const { return _get_alpha(_pixel_pos(x, y)); }

    /**
     * Use bilinear interpolation to get an alpha channel value inbetween pixels.
     */
    double alphaAt(double x, double y) const
    {
        int fx = floor(x), fy = floor(y);
        int cx = ceil(x), cy = ceil(y);
        double weight_x = x - fx, weight_y = y - fy;

        return _bilinear_interpolate(_get_alpha(_pixel_pos(fx, fy)), _get_alpha(_pixel_pos(cx, fy)),
                                     _get_alpha(_pixel_pos(fx, cy)), _get_alpha(_pixel_pos(cx, cy)), weight_x,
                                     weight_y);
    }

    /**
     * Set the alpha channel
     *
     * @arg x - The x coordinate to set
     * @arg y - The y coordinate to set
     */
    void alphaTo(int x, int y, double value) { return _set_alpha(_pixel_pos(x, y), value); }

    /**
     * Get the width of the surface image
     */
    int width() const { return _width; }

    /**
     * Get the height of the surface image
     */
    int height() const { return _height; }

    /**
     * Get the calculated stride for the surface
     */
    int stride() const { return _stride; }

    /**
     * Get the number of output channels minus alpha
     */
    static int getOutputChannels() { return channel_count; }

    /**
     * Get access to the memory directly
     *
     * @template_arg - ForChannel provides access to the memory of the second buffer.
     */
    template<int ForChannel = -1>
    PrimaryType *memory()
        requires(!has_more_channels || ForChannel >= 0)
    {
        if constexpr (has_more_channels && ForChannel >= primary_count && ForChannel != channel_count) {
            return _next_memory;
        }
        return _memory;
    }
    template<int ForChannel = -1>
    PrimaryType const *memory() const
        requires(!has_more_channels || ForChannel >= 0)
    {
        if constexpr (has_more_channels && ForChannel >= primary_count && ForChannel != channel_count) {
            return _next_memory;
        }
        return _memory;
    }

    /**
     * Get access to the contiguous memory, if any
     */
    auto const &local_memory() const { return _local_memory; }

    /*
     * Construct a NON-CAIRO memory buffer of the given format.
     *
     * @template_arg format - The new memory format to use for the contigous buffer. The default
     *                        is the format of this pixel access as a contigious format.
     *
     * @returns A pixel access object which unlike regular objects, owns it's
     *          memory and will unallocate the temporary surface on destruction.
     */
    template <MemoryFormat new_format = format>
    auto createContiguousEmpty() const
    {
        return createContiguousEmpty<new_format>(_width, _height);
    }
    template <MemoryFormat new_format = format>
    static PixelAccess<new_format == MEMORY_FORMAT_CMYA_KA256F ? MEMORY_FORMAT_CMYKA160F : new_format, edge_mode> createContiguousEmpty(int width, int height)
    {
        if constexpr (new_format == MEMORY_FORMAT_CMYA_KA256F) {
            return PixelAccess<MEMORY_FORMAT_CMYKA160F, edge_mode>(width, height);
        } else {
            return PixelAccess<new_format, edge_mode>(width, height);
        }
    }
    /**
     * Creates a non-cairo memory buffer and copies the data from this pixel access into it.
     *
     * @template_arg UnpremultiplyAlpha - Remove alpha premultiplication from the source.
     * @template_arg new_format - The new format of the memory buffer. If this is narrower
     *                            than the source in either bit size or number of channels
     *                            the data will be reduced without careful consideration.
     */
    template <bool UnpremultiplyAlpha = false, MemoryFormat new_format = format>
    auto createContiguousCopy() const
    {
        auto dst = createContiguousEmpty<new_format>(_width, _height);
        createContiguousCopy<UnpremultiplyAlpha>(dst);
        return dst;
    }
    template <bool UnpremultiplyAlpha = false, typename AccessDst>
    auto createContiguousCopy(AccessDst &dst) const
        requires (!AccessDst::has_more_channels)
    {
        if constexpr (!std::is_same<typename AccessDst::PrimaryType, PrimaryType>::value || UnpremultiplyAlpha || channel_total != AccessDst::channel_total) {
            // SLOWEST ARM, converting memory TYPEs
            forEachLine(dst, [](PrimaryType const *src1, PrimaryType const *src2, PrimaryType const *end, AccessDst::PrimaryType *dst1, AccessDst::PrimaryType *) {
                constexpr static double scale = get_format_scale<PrimaryType, typename AccessDst::PrimaryType>();
                //constexpr static int remaining_primaries = AccessDst::channel_count - primary_count;
                constexpr static bool reend = little_endian != AccessDst::little_endian;

                // Ignoring dst2 because contigous means dest is always one surface only.
                for (; src1 < end; src1+=primary_total, src2+=primary_total, dst1+=AccessDst::primary_total) {
                    double alpha = (has_alpha ? *(src1 + primary_alpha_pos) * primary_unscale : 1.0);
                    double mult = (UnpremultiplyAlpha ? (alpha > 0.0 ? 1.0 / alpha : 0.0) : 1.0);

                    auto src = src1;
                    for (int c = 0, s = (reend ? primary_count - 1 : 0); c < AccessDst::channel_count; c++) {
                        *(dst1 + c) = *(src + s) * mult * scale;

                        if constexpr (reend) {
                            if (s > 0) {
                                s--;
                            } else if (has_more_channels && src == src1) {
                                s = primary_count - 1;
                                src = src2;
                            }
                        } else {
                            if (s < primary_count - 1) {
                                s++;
                            } else if (has_more_channels && src == src1) {
                                s = 0;
                                src = src2;
                            }
                        }
                    }
                    if constexpr (AccessDst::has_alpha) {
                        *(dst1 + AccessDst::primary_alpha_pos) = alpha * AccessDst::primary_scale;
                    }   
                }   
            }); 
        } else if constexpr (has_more_channels) {
            // SLOW ARM 6000x6000 -> 515ms
            forEachLine(dst, [](PrimaryType const *src1, PrimaryType const *src2, PrimaryType const *end, AccessDst::PrimaryType *dst1, AccessDst::PrimaryType *dst2) {
                for (;src1 < end; src1+=primary_total, src2+=primary_total, dst1+=channel_total, dst2+=channel_total) {
                    std::memcpy(dst1, src1, primary_count * sizeof(PrimaryType)); // CMY
                    dst2[0] = src2[0]; // K
                    dst1[channel_count] = src1[primary_count]; // A
                }
            });
        } else {
            // FAST ARM 6000x6000 -> 393ms
            // It's already contiguous, so just make a copy
            auto total_size = _width * _height * channel_total;
            std::memcpy(dst.memory(), _memory, total_size * sizeof(PrimaryType));
        }
        return dst;
    }

    /**
     * Simple multi-thread enabled loop for all the pixels in this raster.
     */
    void forEachPixel(std::function<void(int, int)> &&function)
    {
        auto const pool = get_global_dispatch_pool();
        bool const limit = width() * height() > POOL_THRESHOLD;

        pool->dispatch_threshold(height(), limit, [&](int y, int) {
            for (int x = 0; x < width(); x++) {
                function(x, y);
            }
        });
    }

    /**
     * Simple multi-thread enabled loop for all the pixels in this raster.
     */
    template <typename T0 = double, bool unmultiply = false>
    void forEachPixelColor(std::function<void(int, int, std::array<T0, channel_total> const &)> &&function) const
    {
        auto const pool = get_global_dispatch_pool();
        bool const limit = width() * height() > POOL_THRESHOLD;

        pool->dispatch_threshold(height(), limit, [&](int y, int) {
            std::array<T0, channel_total> color;
            for (int x = 0; x < width(); x++) {
                colorAt<T0>(x, y, color, unmultiply);
                function(x, y, color);
            }
        });
    }
    /**
     * Dispatch a thread for each of the lines in this and the other pixel surfaces at the
     * same time, this one as cost for reading and the other as mutable for writing.
     */
    template <typename OtherAccess>
    void forEachLine(OtherAccess &other, std::function<void(PrimaryType const *, PrimaryType const *,
                                                            typename OtherAccess::PrimaryType *)> &&function) const
        requires(!OtherAccess::has_more_channels && !has_more_channels)
    {
        auto const pool = get_global_dispatch_pool();
        bool const limit = width() * height() > POOL_THRESHOLD;

        pool->dispatch_threshold(height(), limit, [this, &other, function](int y, int) {
            function(get_line(y), get_line(y+1), other.get_line(y));
        });
    }

    template <typename OtherAccess>
    void forEachLine(OtherAccess &other, std::function<void(PrimaryType const *,
                                                            PrimaryType const *,
                                                            PrimaryType const *,
                                                            typename OtherAccess::PrimaryType *,
                                                            typename OtherAccess::PrimaryType *)> &&function) const
    {
        auto const pool = get_global_dispatch_pool();
        bool const limit = width() * height() > POOL_THRESHOLD;

        // The offset is always three for other memory where it exists
        pool->dispatch_threshold(height(), limit, [this, &other, function](int y, int) {
            function(
                get_line(y),
                get_other_line<3>(y),
                // The end doesn't use stride, as we want the end to indicate
                // the end of the line of pixels, not the end of the surface memory.
                get_line(y) + (_width * primary_total),
                other.get_line(y),
                other.template get_other_line<3>(y)
            );
        });
    }

    inline PrimaryType *get_line(int y) const {
        return _memory + (y * _stride);
    }

    template <int cutoff>
    inline PrimaryType *get_other_line(int y) const {
        return has_more_channels
               ? _next_memory + (y * _stride)
               : (cutoff < channel_total ? get_line(y) + cutoff : nullptr);
    }

#ifdef UNIT_TEST
    /**
     * Write the connected surface to a png file for debugging
     */
    void write_to_png(std::string const &filename) const
    {
        if (_cairo_surface && _next_surface) {
            _cairo_surface->write_to_png(filename + "-0.png");
            _next_surface->write_to_png(filename + "-1.png");
        } else if (_cairo_surface) {
            _cairo_surface->write_to_png(filename + ".png");
        } else {
            std::cerr << "Can't debug contiguous surface. '" << filename << "' skipped\n";
        }
    }
#endif

private:
    /*
     * Sets the Primaries from this Color.
     *
     * If the next access is set it will recursively set the next unused channels to the next
     * surface primaries until all are exhausted.
     *
     * @param pos - The typed memory Position in the surface to get a value from
     * @param alpha - The value from the alpha Channel in the Color.
     * @param values - The Color we're setting to this surface
     * @param offset - Internal recursive value off where in the Color we have gotten to.
     *
     */
    inline void _set_primaries_recursively(int pos, double alpha, Color const &values, bool unmultiply_alpha,
                                           int offset = 0)
    {
        if (_edge_check(pos)) {
            return;
        }
        if constexpr (has_alpha) {
            // Set alpha in the surface
            _memory[pos + primary_alpha_pos] = alpha * primary_scale;
        }
        auto mult = has_alpha && unmultiply_alpha ? alpha : 1.0;


        // Set the primaries in the surface
        for (int p = 0; p < primary_total && offset < values.size(); p++) {
            if (p != primary_alpha) {
                _memory[pos + _primary_pos(p)] = values[offset] * mult * primary_scale;
                offset++;
            }
        }

        // If we have more channels, keep setting them
        if constexpr (has_more_channels) {
            if constexpr (has_alpha) {
                // Alpha is always set in every surface
                _next_memory[pos + primary_alpha_pos] = alpha * primary_scale;
            }

            for (int p = 0; p < primary_total && offset < values.size() - has_alpha; p++) {
                if (p != primary_alpha) {
                    _next_memory[pos + _primary_pos(p)] = values[offset] * mult * primary_scale;
                    offset++;
                }
            }
        }
    }

    /**
     * Get the channel value from a specific memory position
     *
     * @arg pos        - The memory position in the surface (see _pixel_pos)
     * @arg channel    - Which channel to get, NOT the primary number.
     * @arg alpha_mult - If set will unpremultiply the channel, we do it here to preserve
     *                   as much precision before possible conversion to int.
     */
    template <typename T0 = double>
    inline T0 _get_channel(int pos, int channel, double alpha_mult) const
    {
        // Allow this function to output integer types of various sizes as well as floating point types
        constexpr static double scale = get_format_scale<PrimaryType, T0>();
        if (_edge_check(pos)) {
            return 0.0;
        }
        if constexpr (has_more_channels) {
            if (channel >= primary_count && channel != channel_count) {
                return _next_memory[pos + _channel_to_primary(channel)] * scale * alpha_mult;
            }
        }
        return _memory[pos + _channel_to_primary(channel)] * scale * alpha_mult;
    }

    /**
     * Set the primary position, like _get_channel
     */
    template <typename T0 = double>
    inline void _set_channel(int pos, int channel, T0 value) const
    {
        constexpr static double scale = get_format_scale<T0, PrimaryType>();
        if constexpr (has_more_channels) {
            if (channel >= primary_count && channel != channel_count) {
                _next_memory[pos + _channel_to_primary(channel)] = value * scale;
            }
        } else {
            _memory[pos + _channel_to_primary(channel)] = value * scale;
        }
    }

    /**
     * Return the primary position given the channel index.
     */
    inline int _channel_to_primary(int channel) const
    {
        if constexpr (has_more_channels) {
            if (channel >= primary_count && channel != channel_count) {
                return _primary_pos(channel - primary_count + little_endian);
            }
        }
        return _primary_pos(channel < channel_count ? channel + little_endian : primary_alpha);
    }

    /**
     * Get the alpha primary only
     */
    inline double _get_alpha(int pos) const
    {
        if constexpr (checks_edge) {
            if (_edge_check(pos)) {
                return 0.0;
            }
        }
        return _memory[pos + primary_alpha_pos] * primary_unscale;
    }

    /**
     * Set the alpha primary only.
     */
    inline void _set_alpha(int pos, double alpha)
    {
        if constexpr(checks_edge) {
            if (_edge_check(pos)) {
                return;
            }
        }
        _memory[pos + primary_alpha_pos] = alpha * primary_scale;
    }

    /**
     * Return true if pos is off the edge of the surface. Compiled out when not needed.
     */
    inline bool _edge_check(int pos) const {
        if constexpr(edge_mode != PixelAccessEdgeMode::NO_CHECK) {
            return pos < 0 || pos >= _size;
        }
        return false;
    }

    /**
     * Get the multiplication alpha for use in premultiplications
     */
    static inline double _mult(double alpha) { return alpha > 0 ? 1.0 / alpha : 0.0; }

    /**
     * Get the position in the memory of this pixel
     */
    inline int _pixel_pos(int x, int y) const
    {
        if constexpr (edge_mode != PixelAccessEdgeMode::NO_CHECK) {
            if (x < 0 || y < 0 || x >= _width || y >= _height) {
                switch (edge_mode) {
                    case PixelAccessEdgeMode::EXTEND:
                        x = std::clamp(x, (int)0, _width - 1);
                        y = std::clamp(y, (int)0, _height - 1);
                        break;
                    case PixelAccessEdgeMode::WRAP:
                        x = Util::safemod(x, _width);
                        y = Util::safemod(y, _height);
                        break;
                    case PixelAccessEdgeMode::ZERO:
                        // This means OOB to _get_channel, which will return zero
                        return -1;
                    case PixelAccessEdgeMode::ERROR:
                    default:
                        throw PixelAccessError("Filter went over the edge");
                }
            }
        }
        return y * _stride + x * primary_total;
    }

    /**
     * Convert the primary position into a memory location based on the endianness
     * of the uint32 Cairo stores things in. This might need adjusting for platforms.
     */
    static inline int _primary_pos(int p)
    {
        return little_endian ? primary_count - p : p;
    }

    /**
     * Standard bilinear interpolation
     */
    static inline double _bilinear_interpolate(double a, double b, double c, double d, double wx, double wy)
    {
        // This should only be useful for linearRGB color space, gamut curved colors such as sRGB and
        // periodic channels like HSL/HSV would give bad results. This equation is for premultiplied values.
        return (a * wx + b * (1 - wx)) * wy + (c * wx + d * (1 - wx)) * (1 - wy);
    }

    // This is used for temporary contiguous surfaces in color transformations.
    std::vector<PrimaryType> _local_memory;

    // Basic metrics for the surface
    int const _width;
    int const _height;
    int const _stride;
    int const _size;
    PrimaryType *_memory{};

    // Keep a copy of the cairo surface RefPtr to keep it alive while we exist (we don't use it directly)
    Cairo::RefPtr<Cairo::ImageSurface> _cairo_surface;

    // When the color space involves more channels than primaries available in one cairo surface
    PrimaryType *_next_memory = nullptr;
    Cairo::RefPtr<Cairo::ImageSurface> _next_surface;

public:

    template <bool is_column, int LA_channel, typename T0 = PrimaryType>
    auto getLineAccess(int line = -1) {
        return LineAccess<is_column, LA_channel, T0, false>(*this, line);
    }
    template <bool is_column, int LA_channel, typename T0 = PrimaryType>
    auto getLineAccess(int line = -1) const {
        return LineAccess<is_column, LA_channel, T0, true>(*this, line);
    }

    /**
     * Provides access to a line of bytes as floats. If the memory is already float, the
     * memory used is a pointer into the PixelAccess memory. If it's integer, then a copy
     * of the line is made and reformatted into float.
     *
     * @template_arg is_column  - If set, changes the direction of the line from row to column.
     * @template_arg LA_channel - Which channel this line should focus on.
     *                             -1 means "all channels"
     *                             -2 means "all except alpha", this forces memory per line copy.
     * @teamplte_arg is_const   - Read only access. TODO: Can this be done in a better way?

     * @arg access    - The PixelAccess we are trying to read or modify.
     * @arg index     - Which line to get access to.
     */
    template <bool is_column, int LA_channel, typename T0 = float, bool is_const = false>
    struct LineAccess
    {
        // Inputs
        using PixelAccessType = std::conditional_t<is_const, const PixelAccess<format, edge_mode>,
                                                                   PixelAccess<format, edge_mode>>;
        using DataAccessType = std::conditional_t<is_const, const T0, T0>;
        PixelAccessType *_access;

        // Calculated and stored
        int _line_num = -1;
        int _line_pos = 0;
        const int _next_col;
        const int _next_line;
        const int _primary_offset;
        std::vector<T0> _memory;
        static constexpr bool all_channels = LA_channel == -1;
        static constexpr bool skip_alpha = LA_channel == -2;
        static constexpr bool memory_in_use = (!std::is_same<T0, PrimaryType>::value)
            || skip_alpha || (has_more_channels && all_channels);
public:
        LineAccess(PixelAccessType &access, int index = -1)
            : _access(&access)
            , _next_col(access._pixel_pos(!is_column, is_column))
            , _next_line(access._pixel_pos(is_column, !is_column))
            , _primary_offset(_access->_channel_to_primary(LA_channel))
            , size(is_column ? _access->height() : _access->width())
        {
            if constexpr (memory_in_use) {
                // We reserve a buffer of this type for the pixels
                next = (all_channels || skip_alpha ? channel_total : 1) - skip_alpha;
                _memory.reserve(size * next);
                for (auto i = 0; i < size * next; i++) {
                    _memory.emplace_back(0);
                }
                pixels = _memory.data();
            } else {
                next = _next_col;
            }
            if (index >= 0) {
                gotoLine(index);
            }
        }

        void nextLine() { gotoLine(_line_num + 1); }
        void gotoLine(int line)
        {
            if constexpr (!is_const && memory_in_use) {
                commitLine();
            }
            _line_num = line;
            _line_pos = line * _next_line;

            if constexpr (memory_in_use) {
                for (auto i = 0; i < size; i++) {
                    if constexpr (LA_channel < 0) {
                        // Copy the whole line into a buffer, but in the correct format
                        for (auto c = 0; c < next; c++) {
                            _memory[(i*next) + c] = _access->template _get_channel<T0>(_line_pos + i * _next_col, c, 1.0);
                        }
                    } else {
                        // Make a copy of just this one channel of this line
                        _memory[i] = _access->template _get_channel<T0>(_line_pos + i * _next_col, LA_channel, 1.0);
                    }
                }
            } else {
                // Actually let's not copy, access directly
                pixels = _access->template memory<LA_channel>();
                pixels += _line_pos + _primary_offset;
            }
        }

        void commitLine() requires(!is_const && memory_in_use)
        {
            if (_line_num >= 0) {
                // Copy the data back into the PixelAccess
                for (auto i = 0; i < size; i++) {
                    if constexpr (LA_channel == -1) {
                        // Copy the whole line into a buffer, but in the correct format
                        for (auto c = 0; c < next; c++) {
                            _access->template _set_channel<T0>(_line_pos + i * _next_col, c, _memory[(i * next) + c]);
                        }
                    } else {
                        _access->template _set_channel<T0>(_line_pos + i * _next_col, LA_channel, _memory[i]);
                    }
                }
            }
        }

        ~LineAccess() = default;
        ~LineAccess() requires(!is_const && memory_in_use) { commitLine(); }

        int next;
        const int size;
        mutable DataAccessType *pixels = nullptr;
    };
};

} // namespace Inkscape::Renderer

#endif // INKSCAPE_RENDERER_PIXEL_ACCESS_H

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
