// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <image> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Edward Flick (EAF)
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2005 Authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-image.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <giomm/error.h>
#include <glib/gstdio.h>
#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <2geom/rect.h>
#include <2geom/transforms.h>

// Added for preserveAspectRatio support -- EAF
#include "attributes.h"
#include "colors/document-cms.h"
#include "document.h"
#include "object/uri.h"
#include "path/path-curve.h"
#include "preferences.h"
#include "print.h"
#include "renderer/surface-image.h"
#include "renderer/drawing/svg-renderer.h"
#include "renderer/drawing-forward.h"
#include "snap-candidate.h"
#include "snap-preferences.h"
#include "xml/href-attribute-helper.h"
#include "xml/quote.h"

//#define DEBUG_LCMS
#ifdef DEBUG_LCMS
#define DEBUG_MESSAGE(key, ...)\
{\
    g_message( __VA_ARGS__ );\
}
#include <gtk/gtk.h>
#else
#define DEBUG_MESSAGE(key, ...)
#endif // DEBUG_LCMS
/*
 * SPImage
 */

// TODO: give these constants better names:
#define MAGIC_EPSILON 1e-9
#define MAGIC_EPSILON_TOO 1e-18
// TODO: also check if it is correct to be using two different epsilon values

static void sp_image_set_curve(SPImage *image);
static void sp_image_update_arenaitem (SPImage *img, Inkscape::Renderer::DrawingImage *ai);
static void sp_image_update_canvas_image (SPImage *image);

#ifdef DEBUG_LCMS
extern guint update_in_progress;
#define DEBUG_MESSAGE_SCISLAC(key, ...) \
{\
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();\
    bool dump = prefs->getBool("/options/scislac/" #key);\
    bool dumpD = prefs->getBool("/options/scislac/" #key "D");\
    bool dumpD2 = prefs->getBool("/options/scislac/" #key "D2");\
    dumpD &&= ( (update_in_progress == 0) || dumpD2 );\
    if ( dump )\
    {\
        g_message( __VA_ARGS__ );\
\
    }\
    if ( dumpD )\
    {\
        GtkWidget *dialog = gtk_message_dialog_new(NULL,\
                                                   GTK_DIALOG_DESTROY_WITH_PARENT, \
                                                   GTK_MESSAGE_INFO,    \
                                                   GTK_BUTTONS_OK,      \
                                                   __VA_ARGS__          \
                                                   );\
        g_signal_connect_swapped(dialog, "response",\
                                 G_CALLBACK(gtk_widget_destroy),        \
                                 dialog);                               \
        gtk_widget_set_visible(dialog, true);\
    }\
}
#else // DEBUG_LCMS
#define DEBUG_MESSAGE_SCISLAC(key, ...)
#endif // DEBUG_LCMS

SPImage::SPImage() : SPItem(), SPViewBox() {

    this->x.unset();
    this->y.unset();
    this->width.unset();
    this->height.unset();
    this->clipbox = Geom::Rect();
    this->sx = this->sy = 1.0;
    this->ox = this->oy = 0.0;
    this->dpi = 96.00;
    this->prev_width = 0.0;
    this->prev_height = 0.0;

    this->href = nullptr;
    this->color_profile = nullptr;
}

SPImage::~SPImage() = default;

void SPImage::build(SPDocument *document, Inkscape::XML::Node *repr) {
    SPItem::build(document, repr);

    this->readAttr(SPAttr::XLINK_HREF);
    this->readAttr(SPAttr::X);
    this->readAttr(SPAttr::Y);
    this->readAttr(SPAttr::WIDTH);
    this->readAttr(SPAttr::HEIGHT);
    this->readAttr(SPAttr::SVG_DPI);
    this->readAttr(SPAttr::PRESERVEASPECTRATIO);
    this->readAttr(SPAttr::COLOR_PROFILE);

    /* Register */
    document->addResource("image", this);
}

void SPImage::release() {
    if (this->document) {
        // Unregister ourselves
        this->document->removeResource("image", this);
    }

    if (this->href) {
        g_free (this->href);
        this->href = nullptr;
    }

    image.reset();

    if (this->color_profile) {
        g_free (this->color_profile);
        this->color_profile = nullptr;
    }

    curve.reset();

    SPItem::release();
}

void SPImage::set(SPAttr key, const gchar* value) {
    switch (key) {
        case SPAttr::XLINK_HREF:
            g_free (this->href);
            this->href = (value) ? g_strdup (value) : nullptr;
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_IMAGE_HREF_MODIFIED_FLAG);
            break;

        case SPAttr::X:
            /* ex, em not handled correctly. */
            if (!this->x.read(value)) {
                this->x.unset();
            }

            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::Y:
            /* ex, em not handled correctly. */
            if (!this->y.read(value)) {
                this->y.unset();
            }

            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::WIDTH:
            /* ex, em not handled correctly. */
            if (!this->width.read(value)) {
                this->width.unset();
            }

            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::HEIGHT:
            /* ex, em not handled correctly. */
            if (!this->height.read(value)) {
                this->height.unset();
            }

            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::SVG_DPI:
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_IMAGE_HREF_MODIFIED_FLAG);
            break;

        case SPAttr::PRESERVEASPECTRATIO:
            set_preserveAspectRatio( value );
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG);
            break;

        case SPAttr::COLOR_PROFILE:
            if ( this->color_profile ) {
                g_free (this->color_profile);
            }

            this->color_profile = (value) ? g_strdup (value) : nullptr;

            if ( value ) {
                DEBUG_MESSAGE( lcmsFour, "<this> color-profile set to '%s'", value );
            } else {
                DEBUG_MESSAGE( lcmsFour, "<this> color-profile cleared" );
            }

            // TODO check on this HREF_MODIFIED flag
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_IMAGE_HREF_MODIFIED_FLAG);
            break;


        default:
            SPItem::set(key, value);
            break;
    }

    sp_image_set_curve(this); //creates a curve at the image's boundary for snapping
}

// BLIP
void SPImage::update(SPCtx *ctx, unsigned int flags) {
    SPItem::update(ctx, flags);

    if (flags & SP_IMAGE_HREF_MODIFIED_FLAG) {
        image.reset();
        if (href) {
            std::shared_ptr<Renderer::Image> pb = nullptr;
            double svgdpi = 96;
            if (getRepr()->attribute("inkscape:svg-dpi")) {
                svgdpi = g_ascii_strtod(getRepr()->attribute("inkscape:svg-dpi"), nullptr);
            }
            dpi = svgdpi;
            image = readImage(Inkscape::getHrefAttribute(*getRepr()).second,
                              getRepr()->attribute("sodipodi:absref"),
                              document->getDocumentBase(), svgdpi);
        }
    }

    SPItemCtx *ictx = (SPItemCtx *) ctx;

    if (!x._set) {
        x.unit = SVGLength::PX;
        x.computed = 0;
    }
    if (!y._set) {
        y.unit = SVGLength::PX;
        y.computed = 0;
    }

    if (image) {
        if (!this->width._set) {
            this->width.unit = SVGLength::PX;
            this->width.computed = this->image->width();
        }

        if (!this->height._set) {
            this->height.unit = SVGLength::PX;
            this->height.computed = this->image->height();
        }
    }

    // Calculate x, y, width, height from parent/initial viewport, see sp-root.cpp
    this->calcDimsFromParentViewport(ictx);

    // Image creates a new viewport
    ictx->viewport = Geom::Rect::from_xywh(this->x.computed, this->y.computed,
                                           this->width.computed, this->height.computed);

    this->clipbox = ictx->viewport;

    // When we have a broken image, we feed it the width/height of the request viewport
    auto w = image ? image->width() : width.computed;
    auto h = image ? image->height() : height.computed;

    // Viewbox is either from SVG or dimensions of image (PNG, JPG)
    viewBox = Geom::Rect::from_xywh(0, 0, w, h);
    viewBox_set = true;

    get_rctx(ictx);
    ox = c2p[4];
    oy = c2p[5];
    sx = c2p[0];
    sy = c2p[3];

    // TODO: eliminate ox, oy, sx, sy

    sp_image_update_canvas_image ((SPImage *) this);

    double proportion_img = h / (double)w;
    double proportion_sp = this->height.computed / (double)this->width.computed;
    if (this->prev_width && (this->prev_width != w || this->prev_height != h)) {
        if (std::abs(this->prev_width - w) > std::abs(this->prev_height - h)) {
            proportion_img = w / (double)h;
            proportion_sp = this->width.computed / (double)this->height.computed;
            if (proportion_sp != proportion_img) {
                double new_height = this->height.computed * proportion_img;
                this->getRepr()->setAttributeSvgDouble("width", new_height);
            }
        }
        else {
            if (proportion_sp != proportion_img) {
                double new_width = this->width.computed * proportion_img;
                this->getRepr()->setAttributeSvgDouble("height", new_width);
            }
        }
    }
    this->prev_width = w;
    this->prev_height = h;
}

void SPImage::modified(unsigned int flags) {
//  SPItem::onModified(flags);

    if (flags & SP_OBJECT_STYLE_MODIFIED_FLAG) {
        for (auto &v : views) {
            auto img = cast<Inkscape::Renderer::DrawingImage>(v.drawingitem.get());
            img->setStyle(style);
        }
    }
}

Inkscape::XML::Node *SPImage::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags ) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:image");
    }

    Inkscape::setHrefAttribute(*repr, this->href);

    /* fixme: Reset attribute if needed (Lauris) */
    if (this->x._set) {
        repr->setAttributeSvgDouble("x", this->x.computed);
    }

    if (this->y._set) {
        repr->setAttributeSvgDouble("y", this->y.computed);
    }

    if (this->width._set) {
        repr->setAttributeSvgDouble("width", this->width.computed);
    }

    if (this->height._set) {
        repr->setAttributeSvgDouble("height", this->height.computed);
    }
    repr->setAttribute("inkscape:svg-dpi", this->getRepr()->attribute("inkscape:svg-dpi"));

    this->write_preserveAspectRatio(repr);

    if (this->color_profile) {
        repr->setAttribute("color-profile", this->color_profile);
    }

    SPItem::write(xml_doc, repr, flags);

    return repr;
}

Geom::OptRect SPImage::bbox(Geom::Affine const &transform, SPItem::BBoxType /*type*/) const {
    Geom::OptRect bbox;

    if ((this->width.computed > 0.0) && (this->height.computed > 0.0)) {
        bbox = Geom::Rect::from_xywh(this->x.computed, this->y.computed, this->width.computed, this->height.computed);
        *bbox *= transform;
    }

    return bbox;
}

void SPImage::print(SPPrintContext *ctx) {
    if (image && width.computed > 0.0 && height.computed > 0.0) {
        //image.ensurePixelFormat(Renderer::Surface::PF_GDK);

        /*
        guchar *px = image.pixels();
        int w = pb.width();
        int h = pb.height();
        int rs = pb.rowstride();

        double vx = this->ox;
        double vy = this->oy;

        Geom::Affine t;
        Geom::Translate tp(vx, vy);
        Geom::Scale s(this->sx, this->sy);
        t = s * tp;
        ctx->image_R8G8B8A8_N(px, w, h, rs, t, this->style);
        */
    }
}

const char* SPImage::typeName() const {
    return "image";
}

const char* SPImage::displayName() const {
    return _("Image");
}

/**
 * Return this image's href as a URI object.
 */
Inkscape::URI SPImage::getURI() const
{
    return Inkscape::URI::from_href_and_basedir(href, document->getDocumentBase());
}

gchar* SPImage::description() const {
    char *href_desc;

    if (this->href) {
        href_desc = (strncmp(this->href, "data:", 5) == 0)
            ? g_strdup(_("embedded"))
            : xml_quote_strdup(this->href);
    } else {
        g_warning("Attempting to call strncmp() with a null pointer.");
        href_desc = g_strdup("(null_pointer)"); // we call g_free() on href_desc
    }

    char *ret = ( !image
                  ? g_strdup_printf(_("[bad reference]: %s"), href_desc)
                  : g_strdup_printf(_("%d &#215; %d: %s"),
                                    image->width(),
                                    image->height(),
                                    href_desc) );

    g_free(href_desc);
    return ret;
}

Inkscape::Renderer::DrawingItem* SPImage::show(Inkscape::Renderer::Drawing &drawing, unsigned int /*key*/, unsigned int /*flags*/) {
    Inkscape::Renderer::DrawingImage *ai = new Inkscape::Renderer::DrawingImage(drawing);

    sp_image_update_arenaitem(this, ai);

    return ai;
}


std::shared_ptr<Renderer::Image> SPImage::readImage(gchar const *href, gchar const *absref, gchar const *base, double svgdpi)
{
    if (!href) {
        return {};
    }
    auto svg_factory = std::make_shared<Renderer::SvgRenderer>();
    svg_factory->set_dpi(svgdpi);

    if (g_ascii_strncasecmp(href, "data:", 5) == 0) {
        /* data URI - embedded image */
        std::string_view view(href + 5);
        return std::make_shared<Renderer::Image>(view, svg_factory);
    }

    auto url = Inkscape::URI::from_href_and_basedir(href, base);
    try {
        // handle non-data URIs with GVfs
        auto file = Gio::File::create_for_uri(url.str());
        return std::make_shared<Renderer::Image>(file, svg_factory);
    } catch (Renderer::Image::ImageError const &e) {
        g_warning("readImage: %s", e.what());
    } catch (Glib::ConvertError const &e) {
        g_warning("readImage: %s", e.what());
    }
    return {};
}

/* We assert that realimage is either NULL or identical size to image */
static void
sp_image_update_arenaitem (SPImage *image, Inkscape::Renderer::DrawingImage *ai)
{
    ai->setStyle(image->style);
    ai->setImage(image->image);
    ai->setOrigin(Geom::Point(image->ox, image->oy));
    ai->setScale(image->sx, image->sy);
    ai->setClipbox(image->clipbox);
}

static void sp_image_update_canvas_image(SPImage *image)
{
    for (auto &v : image->views) {
        sp_image_update_arenaitem(image, cast<Inkscape::Renderer::DrawingImage>(v.drawingitem.get()));
    }
}

void SPImage::snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) const {
    /* An image doesn't have any nodes to snap, but still we want to be able snap one image
    to another. Therefore we will create some snappoints at the corner, similar to a rect. If
    the image is rotated, then the snappoints will rotate with it. Again, just like a rect.
    */

    if (this->getClipObject()) {
        //We are looking at a clipped image: do not return any snappoints, as these might be
        //far far away from the visible part from the clipped image
        //TODO Do return snappoints, but only when within visual bounding box
    } else {
        if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_IMG_CORNER)) {
            // The image has not been clipped: return its corners, which might be rotated for example
            double const x0 = this->x.computed;
            double const y0 = this->y.computed;
            double const x1 = x0 + this->width.computed;
            double const y1 = y0 + this->height.computed;

            Geom::Affine const i2d (this->i2dt_affine ());

            p.emplace_back(Geom::Point(x0, y0) * i2d, Inkscape::SNAPSOURCE_IMG_CORNER, Inkscape::SNAPTARGET_IMG_CORNER);
            p.emplace_back(Geom::Point(x0, y1) * i2d, Inkscape::SNAPSOURCE_IMG_CORNER, Inkscape::SNAPTARGET_IMG_CORNER);
            p.emplace_back(Geom::Point(x1, y1) * i2d, Inkscape::SNAPSOURCE_IMG_CORNER, Inkscape::SNAPTARGET_IMG_CORNER);
            p.emplace_back(Geom::Point(x1, y0) * i2d, Inkscape::SNAPSOURCE_IMG_CORNER, Inkscape::SNAPTARGET_IMG_CORNER);
        }
    }
}

/*
 * Initially we'll do:
 * Transform x, y, set x, y, clear translation
 */

Geom::Affine SPImage::set_transform(Geom::Affine const &xform) {
    /* Calculate position in parent coords. */
    Geom::Point pos( Geom::Point(this->x.computed, this->y.computed) * xform );

    /* This function takes care of translation and scaling, we return whatever parts we can't
       handle. */
    Geom::Affine ret(Geom::Affine(xform).withoutTranslation());
    Geom::Point const scale(hypot(ret[0], ret[1]),
                            hypot(ret[2], ret[3]));

    if ( scale[Geom::X] > MAGIC_EPSILON ) {
        ret[0] /= scale[Geom::X];
        ret[1] /= scale[Geom::X];
    } else {
        ret[0] = 1.0;
        ret[1] = 0.0;
    }

    if ( scale[Geom::Y] > MAGIC_EPSILON ) {
        ret[2] /= scale[Geom::Y];
        ret[3] /= scale[Geom::Y];
    } else {
        ret[2] = 0.0;
        ret[3] = 1.0;
    }

    this->width = this->width.computed * scale[Geom::X];
    this->height = this->height.computed * scale[Geom::Y];

    /* Find position in item coords */
    pos = pos * ret.inverse();
    this->x = pos[Geom::X];
    this->y = pos[Geom::Y];

    return ret;
}

static void sp_image_set_curve( SPImage *image )
{
    //create a curve at the image's boundary for snapping
    if ((image->height.computed < MAGIC_EPSILON_TOO) || (image->width.computed < MAGIC_EPSILON_TOO) || (image->getClipObject())) {
    } else {
        Geom::OptRect rect = image->bbox(Geom::identity(), SPItem::VISUAL_BBOX);

        if (rect->isFinite()) {
            image->curve = rect_to_open_path(*rect);
        }
    }
}

/**
 * Return a borrowed pointer to curve (if any exists) or NULL if there is no curve
 */
Geom::PathVector const *SPImage::get_curve() const
{
    return curve ? &*curve : nullptr;
}

void sp_embed_image(Inkscape::XML::Node *image_node, std::shared_ptr<Renderer::Image> pb)
{
    // check whether the image has MIME data
    auto [data_mimetype, data] = pb->getMimeData("image/png");

    // Save base64 encoded data in image node
    // this formula taken from Glib docs
    gsize needed_size = data.size() * 4 / 3 + data.size() * 4 / (3 * 72) + 7;
    needed_size += 5 + 8 + data_mimetype.size(); // 5 bytes for data: + 8 for ;base64,

    gchar *buffer = (gchar *) g_malloc(needed_size);
    gchar *buf_work = buffer;
    buf_work += g_sprintf(buffer, "data:%s;base64,", data_mimetype.c_str());

    gint state = 0;
    gint save = 0;
    gsize written = 0;
    written += g_base64_encode_step((const guchar*)data.c_str(), data.size(), TRUE, buf_work, &state, &save);
    written += g_base64_encode_close(TRUE, buf_work + written, &state, &save);
    buf_work[written] = 0; // null terminate

    // TODO: this is very wasteful memory-wise.
    // It would be better to only keep the binary data around,
    // and base64 encode on the fly when saving the XML.
    Inkscape::setHrefAttribute(*image_node, buffer);

    g_free(buffer);
}

void sp_embed_svg(Inkscape::XML::Node *image_node, std::string const &fn)
{
    if (!g_file_test(fn.c_str(), G_FILE_TEST_EXISTS)) {
        return;
    }
    GStatBuf stdir;
    int val = g_stat(fn.c_str(), &stdir);
    if (val == 0 && stdir.st_mode & S_IFDIR){
        return;
    }

    // we need to load the entire file into memory,
    // since we'll store it as MIME data
    gchar *data = nullptr;
    gsize len = 0;
    GError *error = nullptr;

    if (g_file_get_contents(fn.c_str(), &data, &len, &error)) {

        if (error != nullptr) {
            std::cerr << "Pixbuf::create_from_file: " << error->message << std::endl;
            std::cerr << "   (" << fn << ")" << std::endl;
            return;
        }

        std::string data_mimetype = "image/svg+xml";


        // Save base64 encoded data in image node
        // this formula taken from Glib docs
        gsize needed_size = len * 4 / 3 + len * 4 / (3 * 72) + 7;
        needed_size += 5 + 8 + data_mimetype.size(); // 5 bytes for data: + 8 for ;base64,

        gchar *buffer = (gchar *) g_malloc(needed_size);
        gchar *buf_work = buffer;
        buf_work += g_sprintf(buffer, "data:%s;base64,", data_mimetype.c_str());

        gint state = 0;
        gint save = 0;
        gsize written = 0;
        written += g_base64_encode_step(reinterpret_cast<guchar *>(data), len, TRUE, buf_work, &state, &save);
        written += g_base64_encode_close(TRUE, buf_work + written, &state, &save);
        buf_work[written] = 0; // null terminate

        // TODO: this is very wasteful memory-wise.
        // It would be better to only keep the binary data around,
        // and base64 encode on the fly when saving the XML.
        Inkscape::setHrefAttribute(*image_node, buffer);

        g_free(buffer);
        g_free(data);
    }
}

void SPImage::refresh_if_outdated()
{
    /*
    if ( href && image && image->modificationTime()) {
        // It *might* change

        GStatBuf st;
        memset(&st, 0, sizeof(st));
        int val = 0;
        if (g_file_test(image->originalPath().c_str(), G_FILE_TEST_EXISTS)) {
            val = g_stat(image->originalPath().c_str(), &st);
        }
        if ( !val ) {
            // stat call worked. Check time now
            if ( st.st_mtime != image->modificationTime() ) {
                requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_IMAGE_HREF_MODIFIED_FLAG);
            }
        }
    }
    */
}

/**
 * Crop the image (remove pixels) based on the area rectangle
 * and translate image to componsate for movement.
 *
 * @param area - Rectangle in document units
 *
 * @returns true if any pixels were removed.
 */
bool SPImage::cropToArea(Geom::Rect area)
{
    area *= i2doc_affine().inverse();

    // Apply the image's viewbox and scal to get us image pixels
    area *= Geom::Translate(-x.computed, -y.computed);
    area *= Geom::Scale(image->width() / width.computed, image->height() / height.computed);

    // Any precision problems and we choose to retain more pixels (roundOut)
    return cropToArea(area.roundOutwards());
}

/**
 * Crop to the actual pixel area of the image, and adjusting the
 * image's coordinates to compensate for the changes.
 *
 * @param area - Rectangle in image pixel units
 *
 * @returns true if any pixels were removed.
 */
bool SPImage::cropToArea(const Geom::IntRect &area)
{
    // Contrain requested area to the available pixels.
    auto px = Geom::IntRect::from_xywh(0.0, 0.0, image->width(), image->height());
    auto px_area = area & px;
    if (!px_area)
        return false;

    if (false) {  // TODO auto pb = image->cropTo(*px_area)) {
        // Crop ended up with bad pixels, this should rarely happen.
        //if (pb->width() <= 0 || pb->height() <= 0)
            return false;

        // Cropping is done, now embed this image back into image tag.
        //sp_embed_image(getRepr(), pb);

        // Our new image has new sizes, so adjust image tag's internal viewbox
        auto repr = getRepr();
        auto scale_x = px.width() / width.computed;
        auto scale_y = px.height() / height.computed;
        repr->setAttributeSvgDouble("x", this->x.computed + (px_area->left() / scale_x));
        repr->setAttributeSvgDouble("y", this->y.computed + (px_area->top() / scale_y));
        repr->setAttributeSvgDouble("width", px_area->width() / scale_x);
        repr->setAttributeSvgDouble("height", px_area->height() / scale_y);

        return true;
    }
    return false;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
