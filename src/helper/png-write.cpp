// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PNG file format utilities
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Whoever wrote this example in libpng documentation
 *   Peter Bostrom
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2002 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#include <2geom/rect.h>
#include <2geom/transforms.h>

#include <png.h>

#include "colors/color.h"
#include "document.h"
#include "png-write.h"
#include "rdf.h"

#include "renderer/context.h"
#include "renderer/drawing/drawing.h"
#include "renderer/drawing/svg-renderer.h"

#include "io/sys.h"

#include "object/sp-defs.h"
#include "object/sp-item.h"
#include "object/sp-root.h"

#include "ui/interface.h"
#include <glibmm/convert.h>
#include <glibmm/miscutils.h>

struct SPEBP {
    unsigned long int width, height, sheight;
    std::optional<Inkscape::Colors::Color> background;
    Cairo::RefPtr<Cairo::ImageSurface> surface;
    guchar *px;
    unsigned (*status)(float, void *);
    void *data;
};

/* write a png file */

struct SPPNGBD {
    guchar const *px;
    int rowstride;
};

/**
 * A simple wrapper to list png_text.
 */
class PngTextList {
public:
    PngTextList() : count(0), textItems(nullptr) {}
    ~PngTextList();

    void add(gchar const* key, gchar const* text);
    gint getCount() {return count;}
    png_text* getPtext() {return textItems;}

private:
    gint count;
    png_text* textItems;
};

PngTextList::~PngTextList() {
    for (gint i = 0; i < count; i++) {
        if (textItems[i].key) {
            g_free(textItems[i].key);
        }
        if (textItems[i].text) {
            g_free(textItems[i].text);
        }
    }
}

void PngTextList::add(gchar const* key, gchar const* text)
{
    if (count < 0) {
        count = 0;
        textItems = nullptr;
    }
    png_text* tmp = (count > 0) ? g_try_renew(png_text, textItems, count + 1): g_try_new(png_text, 1);
    if (tmp) {
        textItems = tmp;
        count++;

        png_text* item = &(textItems[count - 1]);
        item->compression = PNG_TEXT_COMPRESSION_NONE;
        item->key = g_strdup(key);
        item->text = g_strdup(text);
        item->text_length = 0;
#ifdef PNG_iTXt_SUPPORTED
        item->itxt_length = 0;
        item->lang = nullptr;
        item->lang_key = nullptr;
#endif // PNG_iTXt_SUPPORTED
    } else {
        g_warning("Unable to allocate array for %d PNG text data.", count);
        textItems = nullptr;
        count = 0;
    }
}

/**
 * Write to PNG.
 * 
 * @arg filename Filename and path. Value is in UTF8 encoding.
 */
static bool
sp_png_write_rgba_striped(SPDocument *doc,
                          gchar const *filename, unsigned long int width, unsigned long int height, double xdpi, double ydpi,
                          int (* get_rows)(guchar const **rows, void **to_free, int row, int num_rows, void *data, int color_type, int bit_depth),
                          void *data, bool interlace, int color_type, int bit_depth, int zlib)
{
    g_return_val_if_fail(filename != nullptr, false);
    g_return_val_if_fail(data != nullptr, false);

    struct SPEBP *ebp = (struct SPEBP *) data;
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_color_8 sig_bit;
    png_uint_32 r;

    /* open the file */

    Inkscape::IO::dump_fopen_call(filename, "M");
    fp = Inkscape::IO::fopen_utf8name(filename, "wb");
    if(fp == nullptr) return false;

    /* Create and initialize the png_struct with the desired error handler
     * functions.  If you want to use the default stderr and longjump method,
     * you can supply NULL for the last three parameters.  We also check that
     * the library version is compatible with the one used at compile time,
     * in case we are using dynamically linked libraries.  REQUIRED.
     */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (png_ptr == nullptr) {
        fclose(fp);
        return false;
    }

    /* Allocate/initialize the image information data.  REQUIRED */
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        fclose(fp);
        png_destroy_write_struct(&png_ptr, nullptr);
        return false;
    }

    /* Set error handling.  REQUIRED if you aren't supplying your own
     * error handling functions in the png_create_write_struct() call.
     */
    if (setjmp(png_jmpbuf(png_ptr))) {
        // If we get here, we had a problem reading the file
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    /* set up the output control if you are using standard C streams */
    png_init_io(png_ptr, fp);

    /* Set the image information here.  Width and height are up to 2^31,
     * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
     * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
     * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
     * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
     * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
     * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
     */

    png_set_compression_level(png_ptr, zlib);

    png_set_IHDR(png_ptr, info_ptr,
                 width,
                 height,
                 bit_depth,
                 color_type,
                 interlace ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    if ((color_type&2) && bit_depth == 16) {
        // otherwise, if we are dealing with a color image then
        sig_bit.red = 8;
        sig_bit.green = 8;
        sig_bit.blue = 8;
        // if the image has an alpha channel then
        if (color_type&4)
            sig_bit.alpha = 8;
        png_set_sBIT(png_ptr, info_ptr, &sig_bit);
    }

    PngTextList textList;

    textList.add("Software", "www.inkscape.org"); // Made by Inkscape comment
    {
        const gchar* pngToDc[] = {"Title", "title",
                               "Author", "creator",
                               "Description", "description",
                               //"Copyright", "",
                               "Creation Time", "date",
                               //"Disclaimer", "",
                               //"Warning", "",
                               "Source", "source"
                               //"Comment", ""
        };
        for (size_t i = 0; i < G_N_ELEMENTS(pngToDc); i += 2) {
            struct rdf_work_entity_t * entity = rdf_find_entity ( pngToDc[i + 1] );
            if (entity) {
                gchar const* data = rdf_get_work_entity(doc, entity);
                if (data && *data) {
                    textList.add(pngToDc[i], data);
                }
            } else {
                g_warning("Unable to find entity [%s]", pngToDc[i + 1]);
            }
        }


        struct rdf_license_t *license =  rdf_get_license(doc, true);
        if (license) {
            if (license->name && license->uri) {
                gchar* tmp = g_strdup_printf("%s %s", license->name, license->uri);
                textList.add("Copyright", tmp);
                g_free(tmp);
            } else if (license->name) {
                textList.add("Copyright", license->name);
            } else if (license->uri) {
                textList.add("Copyright", license->uri);
            }
        }
    }
    if (textList.getCount() > 0) {
        png_set_text(png_ptr, info_ptr, textList.getPtext(), textList.getCount());
    }

    /* other optional chunks like cHRM, bKGD, tRNS, tIME, oFFs, pHYs, */
    /* note that if sRGB is present the cHRM chunk must be ignored
     * on read and must be written in accordance with the sRGB profile */
    if(xdpi < 0.0254 ) xdpi = 0.0255;
    if(ydpi < 0.0254 ) ydpi = 0.0255;

    png_set_pHYs(png_ptr, info_ptr, unsigned(xdpi / 0.0254 ), unsigned(ydpi / 0.0254 ), PNG_RESOLUTION_METER);

    /* Write the file header information.  REQUIRED */
    png_write_info(png_ptr, info_ptr);

    /* Once we write out the header, the compression type on the text
     * chunks gets changed to PNG_TEXT_COMPRESSION_NONE_WR or
     * PNG_TEXT_COMPRESSION_zTXt_WR, so it doesn't get written out again
     * at the end.
     */

    /* set up the transformations you want.  Note that these are
     * all optional.  Only call them if you want them.
     */

    /* --- CUT --- */

    /* The easiest way to write the image (you may have a different memory
     * layout, however, so choose what fits your needs best).  You need to
     * use the first method if you aren't handling interlacing yourself.
     */

    png_bytep* row_pointers = new png_bytep[ebp->sheight];
    int number_of_passes = interlace ? png_set_interlace_handling(png_ptr) : 1;

    for(int i=0;i<number_of_passes; ++i){
        r = 0;
        while (r < static_cast<png_uint_32>(height)) {
            void *to_free;
            int n = get_rows((unsigned char const **) row_pointers, &to_free, r, height-r, data, color_type, bit_depth);
            if (!n) break;
            png_write_rows(png_ptr, row_pointers, n);
            g_free(to_free);
            r += n;
        }
    }

    delete[] row_pointers;

    /* You can write optional chunks like tEXt, zTXt, and tIME at the end
     * as well.
     */

    /* It is REQUIRED to call this to finish writing the rest of the file */
    png_write_end(png_ptr, info_ptr);

    /* if you allocated any text comments, free them here */

    /* clean up after the write, and free any memory allocated */
    png_destroy_write_struct(&png_ptr, &info_ptr);

    /* close the file */
    fclose(fp);

    /* that's it */
    return true;
}

static constexpr uint16_t get_luminance(uint32_t r, uint32_t g, uint32_t b)
{
    return ((1063 * r + 3576 * g + 361 * b) * 257 + 2500) / 5000;
}

G_GNUC_CONST static inline guint32
unpremul_alpha(const guint32 color, const guint32 alpha)
{
    if (color >= alpha)
        return 0xff;
    return (255 * color + alpha/2) / alpha;
}

/**
 * Converts a pixbuf to a PNG data structure.
 * For 8-but RGBA png, this is like copying.
 *
 */
static guchar *
pixbuf_to_png(guchar const**rows, guchar* px, int num_rows, int num_cols, int stride, int color_type, int bit_depth)
{
    int n_fields = 1 + (color_type&2) + (color_type&4)/4;
    guchar* new_data = (guchar*)malloc(((n_fields * bit_depth * num_cols + 7)/8) * num_rows);
    char* ptr = (char*) new_data;
    // Used when we write image data smaller than one byte (for instance in
    // black and white images where 1px = 1bit). Only possible with greyscale.
    int pad = 0;
    for (int row = 0; row < num_rows; ++row) {
        rows[row] = (const guchar*)ptr;
        for (int col = 0; col < num_cols; ++col) {
            guint32 *pixel = reinterpret_cast<guint32*>(px + row*stride)+col;

            guint64 pix3 = (*pixel & 0xff000000) >> 24;
            guint64 pix2 = (*pixel & 0x00ff0000) >> 16;
            guint64 pix1 = (*pixel & 0x0000ff00) >> 8;
            guint64 pix0 = (*pixel & 0x000000ff);

            uint64_t a, r, g, b;
            if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
                a = pix3;
                b = pix2;
                g = pix1;
                r = pix0;
            } else {
                r = pix3;
                g = pix2;
                b = pix1;
                a = pix0;
            }

            // One of possible rgb to greyscale formulas. This one is called "luminance", "luminosity" or "luma"
            uint16_t const gray = get_luminance(r, g, b);

            if (color_type & 2) { // RGB or RGBA
                // for 8bit->16bit transition, I take the FF -> FFFF convention (multiplication by 0x101).
                // If you prefer FF -> FF00 (multiplication by 0x100), remove the <<8, <<24, <<40 and <<56
                // for little-endian, and remove the <<0, <<16, <<32 and <<48 for big-endian.
                if (color_type & 4) { // RGBA
                    if (bit_depth == 8)
                        *((guint32*)ptr) = *pixel;
                    else
                        // This uses the samples in the order they appear in pixel rather than
                        // normalised to abgr or rgba in order to make it endian agnostic,
                        // exploiting the symmetry of the expression (0x101 is the same in both
                        // endiannesses and each sample is multiplied by that).
                        *((guint64*)ptr) = (guint64)((pix3<<56)+(pix3<<48)+(pix2<<40)+(pix2<<32)+(pix1<<24)+(pix1<<16)+(pix0<<8)+(pix0));
                } else { // RGB
                    if (bit_depth == 8) {
                        *ptr = r;
                        *(ptr+1) = g;
                        *(ptr+2) = b;
                    } else {
                        *((guint16*)ptr) = (r<<8)+r;
                        *((guint16*)(ptr+2)) = (g<<8)+g;
                        *((guint16*)(ptr+4)) = (b<<8)+b;
                    }
                }
            } else { // Grayscale
                if (bit_depth == 16) {
                    if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
                        *(guint16*)ptr = ((gray & 0xff00)>>8) + ((gray & 0x00ff)<<8);
                    } else {
                        *(guint16*)ptr = gray;
                    }
                    // For 8bit->16bit this mirrors RGB(A), multiplying by
                    // 0x101; if you prefer multiplying by 0x100, remove the
                    // <<8 for little-endian, and remove the unshifted value
                    // for big-endian.
                    if (color_type & 4) // Alpha channel
                        *((guint16*)(ptr+2)) = a + (a<<8);
                } else if (bit_depth == 8) {
                    *ptr = guint8(gray >> 8);
                    if (color_type & 4) // Alpha channel
                        *((guint8*)(ptr+1)) = a;
                } else {
                    if (!pad) *ptr=0;
                    // In PNG numbers are stored left to right, but in most significant bits first, so the first one processed is the ``big'' mask, etc.
                    int realpad = 8 - bit_depth - pad;
                    *ptr += guint8((gray >> (16-bit_depth))<<realpad); // Note the "+="
                    if (color_type & 4) // Alpha channel
                        *(ptr+1) += guint8((a >> (8-bit_depth))<<(bit_depth + realpad));
                }
            }

            pad += bit_depth*n_fields;
            ptr += pad/8;
            pad %= 8;
        }
        // Align bytes on rows
        if (pad) {
            pad = 0;
            ptr++;
        }
    }
    return new_data;
}

/**
 * Convert one pixel from ARGB to pixbuf format.
 *
 * @param c ARGB color
 * @param bgcolor Color to use if c.alpha is zero (bgcolor.alpha is ignored)
 */
static guint32
pixbuf_from_argb32(guint32 c, guint32 bgcolor)
{
    guint32 a = (c & 0xff000000) >> 24;
    if (a == 0) {
        assert(c == 0);
        c = bgcolor;
    }

    // extract color components
    guint32 r = (c & 0x00ff0000) >> 16;
    guint32 g = (c & 0x0000ff00) >> 8;
    guint32 b = (c & 0x000000ff);

    if (a != 0) {
        r = unpremul_alpha(r, a);
        g = unpremul_alpha(g, a);
        b = unpremul_alpha(b, a);
    }

    // combine into output
    if constexpr (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        return r | (g << 8) | (b << 16) | (a << 24);
    } else {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }
}

static void
convert_pixels_argb32_to_pixbuf(guchar *data, int w, int h, int stride, guint32 bgcolor)
{
    if (!data || w < 1 || h < 1 || stride < 1) {
        return;
    }
    for (size_t i = 0; i < h; ++i) {
        guint32 *px = reinterpret_cast<guint32*>(data + i*stride);
        for (size_t j = 0; j < w; ++j) {
            *px = pixbuf_from_argb32(*px, bgcolor);
            ++px;
        }
    }
}

/**
 *
 */
static int
sp_export_get_rows(guchar const **rows, void **to_free, int row, int num_rows, void *data, int color_type, int bit_depth)
{
    struct SPEBP *ebp = (struct SPEBP *) data;

    if (ebp->status) {
        if (!ebp->status((float) row / ebp->height, ebp->data)) return 0;
    }
    if (!ebp->background) {
        return 0;
    }

    num_rows = MIN(num_rows, static_cast<int>(ebp->sheight));
    num_rows = MIN(num_rows, static_cast<int>(ebp->height - row));

    auto px = ebp->surface->get_data();
    auto stride = ebp->surface->get_stride();
    px += stride * row;

    // TODO: Rip out custom png code out and replace with Glycin once it supports all the output options we need.

    // PNG stores data as unpremultiplied big-endian RGBA, which is identical to the GdkPixbuf format.
    convert_pixels_argb32_to_pixbuf(px, ebp->width, num_rows, stride, ebp->background->toARGB());

    // If a custom bit depth or color type is asked, then convert rgb to grayscale, etc.
    *to_free = pixbuf_to_png(rows, px, num_rows, ebp->width, stride, color_type, bit_depth);

    return num_rows;
}

ExportResult sp_export_png_file(SPDocument *doc, gchar const *filename,
                                double x0, double y0, double x1, double y1,
                                unsigned long int width, unsigned long int height, double xdpi, double ydpi,
                                Inkscape::Colors::Color const &bgcolor,
                                unsigned int (*status) (float, void *),
                                void *data, bool force_overwrite,
                                const std::vector<SPItem const *> &items_only, bool interlace, int color_type, int bit_depth, int zlib, int antialiasing)
{
    return sp_export_png_file(doc, filename, Geom::Rect(Geom::Point(x0,y0),Geom::Point(x1,y1)),
                              width, height, xdpi, ydpi, bgcolor, status, data, force_overwrite, items_only, interlace, color_type, bit_depth, zlib, antialiasing);
}

/**
 * Export an area to a PNG file
 *
 * @param area Area in document coordinates
 * @param filename Filename and path. Value is UTF8 encoded.
 */
ExportResult sp_export_png_file(SPDocument *doc, gchar const *filename,
                                Geom::Rect const &area,
                                unsigned long width, unsigned long height, double xdpi, double ydpi,
                                Inkscape::Colors::Color const &bgcolor,
                                unsigned (*status)(float, void *),
                                void *data, bool force_overwrite,
                                const std::vector<SPItem const *> &items_only, bool interlace, int color_type, int bit_depth, int zlib, int antialiasing)
{
    g_return_val_if_fail(doc != nullptr, EXPORT_ERROR);
    g_return_val_if_fail(filename != nullptr, EXPORT_ERROR);
    g_return_val_if_fail(width >= 1, EXPORT_ERROR);
    g_return_val_if_fail(height >= 1, EXPORT_ERROR);
    g_return_val_if_fail(!area.hasZeroArea(), EXPORT_ERROR);

    if (!force_overwrite && !sp_ui_overwrite_file(Glib::filename_from_utf8(filename))) {
        // aborted overwrite
        return EXPORT_ABORTED;
    }

    Inkscape::Renderer::SvgRenderer renderer;
    renderer.set_area(area);
    renderer.set_dpi(xdpi, ydpi);
    renderer.set_viewbox_scale(width / area.width(), height / area.height());
    renderer.set_item_limit(items_only);
    renderer.set_background(bgcolor);
    renderer.set_antialiasing(static_cast<Inkscape::Renderer::Antialiasing>(antialiasing));
    auto cairo_surface = renderer.render(doc)->exportToARGB32();

    struct SPEBP ebp;
    ebp.width = width;
    ebp.height = height;
    ebp.background = bgcolor;
    ebp.surface = cairo_surface;
    ebp.status = status;
    ebp.data = data;
    ebp.sheight = 64;

    auto write_status = sp_png_write_rgba_striped(doc, filename, width, height, xdpi, ydpi, sp_export_get_rows, &ebp,
                                                  interlace, color_type, bit_depth, zlib);
    return write_status ? EXPORT_OK : EXPORT_ERROR;
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
