// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Font selection widgets
 */
/*
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   See Git history
 *
 * Copyright (C) 1999-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2013 Tavmjong Bah
 * Copyright (C) 2018+ Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef FONT_LISTER_H
#define FONT_LISTER_H

#include <memory>
#include <set>
#include <vector>
#include <map>

#include <giomm/liststore.h>

#include <glibmm/ustring.h>
#include <glibmm/stringutils.h> // For strescape()

#include <pangomm/fontfamily.h>

#include "libnrtype/font-utils.h" // canonize_fontspec
#include "libnrtype/font-factory.h" // StyleNames
#include "ui/item-factories.h"

class SPObject;
class SPDesktop;
class SPDocument;
class SPCSSAttr;
class SPStyle;

namespace Inkscape {

/**
 *  This class enumerates fonts using libnrtype into reusable data stores and
 *  allows for random access to the font-family list and the font-style list.
 *  Setting the font-family updates the font-style list. "Style" in this case
 *  refers to everything but family and size (e.g. italic/oblique, weight).
 *
 *  This class handles font-family lists and fonts that are not on the system,
 *  where there is not an entry in the fontInstanceMap.
 *
 *  This class uses the idea of "font_spec". This is a plain text string as used by
 *  Pango. It is similar to the CSS font shorthand except that font-family comes
 *  first and in this class the font-size is not used.
 *
 *  This class uses the FontFactory class to get a list of system fonts
 *  and to find best matches via Pango. The Pango interface is only setup
 *  to deal with fonts that are on the system so care must be taken. For
 *  example, best matches should only be done with the first font-family
 *  in a font-family list. If the first font-family is not on the system
 *  then a generic font-family should be used (sans-serif -> Sans).
 *
 *  This class is used by the UI interface (text-toolbar, font-select, etc.).
 *  Those items can change the selected font family and style here. When that
 *  happens. this class emits a signal for those items to update their displayed
 *  values.
 *
 *  This class is a singleton (one instance per Inkscape session). Since fonts
 *  used in a document are added to the list, there really should be one
 *  instance per document.
 *
 *  "Font" includes family and style. It should not be used when one
 *  means font-family.
 */

/**
 * Gets font-family and style from fontspec.
 */
struct FamilyStyle
{
    Glib::ustring family;
    Glib::ustring style;
};
FamilyStyle ui_from_fontspec(Glib::ustring const &fontspec);

/**
 * Fill css using given fontspec (doesn't need to be member function).
 */
void fill_css_from_fontspec(SPCSSAttr *css, Glib::ustring const &fontspec);

Glib::ustring fontspec_from_style(SPStyle const *style);

class FontLister
{
public:
    using Styles = std::vector<StyleNames>;

    /// Data for each item in the font list.
    struct FontListItem
    {
        /// Family name
        Glib::ustring family;

        /// Styles for each family name. May be lazy-loaded.
        std::shared_ptr<Styles> styles;

        /// Whether font is on system
        bool on_system = false;

        /// Used for lazy-loading %styles.
        PangoFontFamily *pango_family = nullptr;

        /// Ensure styles is not null, loading it if necessary.
        void ensureStyles();

        /// Return styles, loading it if necessary.
        auto &getStyles() { ensureStyles(); return styles; }
    };

    /**
     * The list of fonts, sorted by the order they will appear in the UI.
     * Also used to give log-time access to each font's PangoFontFamily, owned by the FontFactory.
     */
    std::map<std::string, PangoFontFamily *> pango_family_map;

    /** 
     * @return the ListStore with the family names
     *
     * The ListStore is ready to be used after class instantiation
     * and should not be modified.
     */
    Glib::RefPtr<Gio::ListStore<WrapAsGObject<FontListItem>>> const &get_font_list() const { return font_list_store; }

    /**
     * @return the ListStore with the styles
     */
    Glib::RefPtr<Gio::ListStore<WrapAsGObject<StyleNames>>> const &get_style_list() const { return style_list_store; }

    static Inkscape::FontLister *get_instance();

    /**
     * Functions to display the search results in the font list.
     */
    void show_results(Glib::ustring const &search_text);
    void apply_collections(std::set <Glib::ustring> &selected_collections);
    void set_dragging_family(Glib::ustring const &new_family);

    Glib::ustring const &get_dragging_family() const
    {
        return dragging_family;
    }

    /**
     * Let users of FontLister know to update GUI.
     * This is to allow synchronization of changes across multiple widgets.
     * Handlers should block signals.
     * Input is fontspec to set.
     */
    sigc::connection connectUpdate(sigc::slot<void ()> slot) {
        return update_signal.connect(slot);
    }

    bool blocked() const { return block; }

    int get_font_families_size() const { return pango_family_map.size(); }
    bool font_installed_on_system(Glib::ustring const &font) const;

    void init_font_families();
    void init_default_styles();
    std::pair<bool, std::string> get_font_count_label() const;

    auto const &getDefaultStyles() const { return default_styles; }

private:
    FontLister();
    ~FontLister();

    Glib::RefPtr<Gio::ListStore<WrapAsGObject<FontListItem>>> font_list_store;
    Glib::RefPtr<Gio::ListStore<WrapAsGObject<StyleNames>>> style_list_store;

    Glib::ustring dragging_family;

    /**
     * If a font-family is not on system, this list of styles is used.
     */
    std::shared_ptr<Styles> default_styles;

    bool block = false;
    void emit_update();
    sigc::signal<void ()> update_signal;
};

class LocalFontLister
{
public:
    LocalFontLister();

    void unsetDocument();
    void setDocument(SPDocument *document);

    auto const &getFonts() const { return all_fonts_flat; }
    auto const &getStyles() const { return styles_store; }

    // Only used to determine section headers.
    auto firstDocumentFont() const { return document_fonts ? document_fonts->get_item(0) : nullptr; }

    /**
     * Info for currently selected font (what is shown in the UI).
     *  May include font-family lists and fonts not on system.
     */
    std::string family = "sans-serif";
    std::string style = "Normal";

    Glib::ustring getFontspec() const { return canonize_fontspec(family + ", " + style); }

    /**
     * Sets font-family, updating style list and attempting
     *  to find closest style to old current_style.
     *  New font-family and style returned.
     *  Updates current_family and current_style.
     *  Calls new_font_family().
     *  (For use in text-toolbar where update is immediate.)
     */
    void setFontFamily(unsigned pos);
    void setFontFamily(Glib::ustring new_family);
    void setFontStyle(Glib::ustring style);
    void selectionUpdate(SPDesktop *desktop);
    void setFontspec(Glib::ustring const &new_fontspec); // Note: Used to have an optional check arg, used to assume style is valid.

    FontLister::FontListItem &getItem(unsigned pos);
    unsigned getPosForFont(Glib::ustring const &family);
    FontLister::FontListItem *getItemForFont(Glib::ustring const &family);

private:
    Glib::RefPtr<Gio::ListStore<WrapAsGObject<FontLister::FontListItem>>> document_fonts;
    Glib::RefPtr<Gio::ListStore<Gio::ListModel>> all_font_models;
    Glib::RefPtr<Gio::ListModel> all_fonts_flat;

    Glib::RefPtr<Gio::ListStore<WrapAsGObject<StyleNames>>> styles_store;

    void _setStyles(FontLister::Styles const &styles);
};

/**
 * Return best style match for new font given style for old font.
 */
std::string closestStyle(FontLister::FontListItem *item, std::string const &target_style);

Glib::ustring font_lister_get_markup(FontLister::FontListItem const &item);

} // namespace Inkscape

#endif // FONT_LISTER_H

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
