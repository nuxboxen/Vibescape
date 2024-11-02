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

#include "font-lister.h"

#include <string>

#include <glibmm/markup.h>
#include <glibmm/regex.h>
#include <gtkmm/settings.h>
#include <gtkmm/flattenlistmodel.h>

#include "font-factory.h"
#include "desktop.h"
#include "desktop-style.h"
#include "document.h"
#include "inkscape.h"
#include "preferences.h"
#include "style-text.h"
#include "libnrtype/font-instance.h"
#include "libnrtype/font-utils.h"
#include "object/sp-object.h"
#include "object/sp-root.h"
#include "util/font-collections.h"
#include "util/recently-used-fonts.h"
#include "util/document-fonts.h"
#include "xml/repr.h"

//#define DEBUG_FONT

// CSS dictates that font family names are case insensitive.
// This should really implement full Unicode case unfolding.
bool familyNamesAreEqual(const Glib::ustring &a, const Glib::ustring &b)
{
    return a.casefold() == b.casefold();
}

namespace Inkscape {

FontLister::FontLister()
{
    // Create default styles for use when font-family is unknown on system.
    default_styles = std::make_shared<Styles>(Styles{
        {"Normal"},
        {"Italic"},
        {"Bold"},
        {"Bold Italic"}
    });

    pango_family_map = FontFactory::get().GetUIFamilies();
    font_list_store = Gio::ListStore<WrapAsGObject<FontListItem>>::create();
    init_font_families();

    style_list_store = Gio::ListStore<WrapAsGObject<StyleNames>>::create();
    init_default_styles();

    // Watch gtk for the fonts-changed signal and refresh our pango configuration
    if (auto settings = Gtk::Settings::get_default()) {
        settings->property_gtk_fontconfig_timestamp().signal_changed().connect([this] {
            FontFactory::get().refreshConfig();
            pango_family_map = FontFactory::get().GetUIFamilies();
            init_font_families();
        });
    }
}

FontLister::~FontLister() = default;

bool FontLister::font_installed_on_system(Glib::ustring const &font) const
{
    return pango_family_map.find(font) != pango_family_map.end();
}

void FontLister::init_font_families()
{
    font_list_store->remove_all();
    font_list_store->freeze_notify();

    // Traverse through the family names and set up the list store
    for (auto const &[family_str, pango_family] : pango_family_map) {
        if (!family_str.empty()) {
#if __APPLE__
            // on macOS hide fonts with names starting with a dot; those are internal system fonts
            if (family_str[0] == '.') continue;
#endif
            font_list_store->append(WrapAsGObject<FontListItem>::create(FontListItem{
                .family = family_str,
                .on_system = true,
                .pango_family = pango_family,
            }));
        }
    }

    font_list_store->thaw_notify();
}

void FontLister::init_default_styles()
{
    // Initialize style store with defaults
    std::vector<Glib::RefPtr<WrapAsGObject<StyleNames>>> items;
    for (auto const &style : *default_styles) {
        items.push_back(WrapAsGObject<StyleNames>::create(style));
    }
    style_list_store->splice(0, style_list_store->get_n_items(), items);
    update_signal.emit();
}

std::pair<bool, std::string> FontLister::get_font_count_label() const
{
    std::string label;
    bool all_fonts = false;

    int size = font_list_store->get_n_items();
    int total_families = get_font_families_size();

    if (size >= total_families) {
        label += _("All Fonts");
        all_fonts = true;
    } else {
        label += _("Fonts ");
        label += std::to_string(size);
        label += "/";
        label += std::to_string(total_families);
    }

    return std::make_pair(all_fonts, label);
}

FontLister *FontLister::get_instance()
{
    static FontLister instance;
    return &instance;
}

/// Try to find in the Haystack the Needle - ignore case
static bool find_string_case_insensitive(std::string const &text, std::string const &pat)
{
    auto it = std::search(
        text.begin(), text.end(),
        pat.begin(),   pat.end(),
        [] (unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );

    return it != text.end();
}

void FontLister::show_results(Glib::ustring const &search_text)
{
    // Clear currently selected collections.
    Inkscape::FontCollections::get()->clear_selected_collections();

    if (search_text.empty()) {
        init_font_families();
        init_default_styles();
        return;
    }

    // Clear the list store.
    // Fixme: Font collections manager should not be recreating global font list to show search results.
    font_list_store->freeze_notify();
    font_list_store->remove_all();

    // Start iterating over the families.
    // Take advantage of sorted families to speed up the search.
    for (auto const &[family_str, pango_family] : pango_family_map) {
        if (find_string_case_insensitive(family_str, search_text)) {
            font_list_store->append(WrapAsGObject<FontListItem>::create(FontListItem{
                .family = family_str,
                .on_system = true,
                .pango_family = pango_family,
            }));
        }
    }

    // selected_fonts_count = count;
    font_list_store->thaw_notify();
    init_default_styles();

    // To update the count of fonts in the label.
    // update_signal.emit ();
}

void FontLister::apply_collections(std::set<Glib::ustring> &selected_collections)
{
    // Get the master set of fonts present in all the selected collections.
    std::set <Glib::ustring> fonts;

    FontCollections *font_collections = Inkscape::FontCollections::get();

    for (auto const &col : selected_collections) {
        if (col.raw() == Inkscape::DOCUMENT_FONTS) {
            auto const &document_fonts = SP_ACTIVE_DOCUMENT->getDocumentFonts();
            for (auto const &[family, _] : document_fonts.getMap()) {
                fonts.insert(family);
            }
        } else if (col.raw() == Inkscape::RECENTLY_USED_FONTS) {
            RecentlyUsedFonts *recently_used = Inkscape::RecentlyUsedFonts::get();
            for (auto const &font : recently_used->get_fonts()) {
                fonts.insert(font);
            }
        } else {
            for (auto const &font : font_collections->get_fonts(col)) {
                fonts.insert(std::move(font));
            }
        }
    }

    // Freeze the font list.
    font_list_store->freeze_notify();
    font_list_store->remove_all();

    if (fonts.empty()) {
        // Re-initialize the font list if
        // initialize_font_list();
        init_font_families();
        init_default_styles();
        return;
    }

    for (auto const &f : fonts) {
        font_list_store->append(WrapAsGObject<FontListItem>::create(FontListItem{
            .family = f,
            .on_system = true,
            .pango_family = pango_family_map[f],
        }));
    }

    font_list_store->thaw_notify();
    init_default_styles();

    // To update the count of fonts in the label.
    update_signal.emit();
}

// Ensures the style list for a particular family has been created.
void FontLister::FontListItem::ensureStyles()
{
    if (styles) {
        return;
    }

    if (pango_family) {
        styles = std::make_shared<Styles>(FontFactory::get().GetUIStyles(pango_family));
    } else {
        styles = FontLister::get_instance()->default_styles;
    }
}

/*
// Previously only called on typing a new font into the font combobox in the text toolbar.
Glib::RefPtr<WrapAsGObject<FontLister::FontListItem>> FontLister::insert_font_family(Glib::ustring const &new_family)
{
    auto styles = default_styles;

    // Shouldn't be necessary as to be handled by DocumentFonts style list generation.
    // In case this is a fallback list, check if first font-family on system.
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(",", new_family);
    if (!tokens.empty() && !tokens[0].empty()) {
        auto const size = font_list_store->get_n_items();
        for (unsigned i = 0; i < size; i++) {
            auto &row = font_list_store->get_item(i)->data;
            if (row.on_system && familyNamesAreEqual(tokens[0], row.family)) {
                styles = row.getStyles();
                break;
            }
        }
    }

    // Shouldn't be necessary as now in document fonts.
    auto result = WrapAsGObject<FontListItem>::create(FontListItem{
        .family = new_family,
        .styles = styles,
        .on_system = false,
        .pango_family = nullptr,
    });

    // Shouldn't be necessary as the only caller does the same later on.
    current_family = new_family;
    current_family_row = 0;
    current_style = "Normal";

    emit_update();

    return result;
}

int FontListeradd_document_fonts_at_top(SPDocument *document)
{
    // Remaining logic from this function is the merging of style lists between document and system fonts.

    // Insert the document's font families into the store.
    for (auto const &[data_family, data_styleset] : font_data) {
        // Ensure the font family is non-empty, and get the part up to the first comma.
        auto const i = data_family.find_first_of(',');
        if (i == 0) {
            continue;
        }
        auto const fam = data_family.substr(0, i);

        // Return the system font matching the given family name.
        auto find_matching_system_font = [this] (Glib::ustring const &fam) -> unsigned {
            auto const size = font_list_store->get_n_items();
            for (unsigned j = 0; j < size; j++) {
                auto &row = font_list_store->get_item(j)->data;
                if (row.on_system && familyNamesAreEqual(fam, row.family)) {
                    return j;
                }
            }
            return GTK_INVALID_LIST_POSITION;
        };

        // Initialise an empty Styles for this font.
        Styles data_styles;

        // Populate with the styles of the matching system font, if any.
        if (auto const iter = find_matching_system_font(fam); iter != GTK_INVALID_LIST_POSITION) {
            auto &row = font_list_store->get_item(iter)->data;
            data_styles = *row.getStyles();
        }

        // Add additional styles from 'font-variation-settings'; these may not be included in the system font's styles.
        for (auto const &data_style : data_styleset) {
            // std::cout << "  Inserting: " << j << std::endl;

            bool exists = false;
            for (auto const &style : data_styles) {
                if (style.css_name.compare(data_style) == 0) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                data_styles.emplace_back(data_style, data_style);
            }
        }

        font_list_store->insert(0, WrapAsGObject<FontListItem>::create(FontListItem{
            .family = data_family,
            .styles = std::make_shared<Styles>(std::move(data_styles)),
            .on_system = false, // false if document font
            .pango_family = nullptr, // CHECK ME (set to pango_family if on system?)
        }));
    }
}
*/

void FontLister::emit_update()
{
    if (block) return;

    block = true;
    update_signal.emit();
    block = false;
}

FamilyStyle ui_from_fontspec(Glib::ustring const &fontspec)
{
    auto descr = Pango::FontDescription{fontspec};
    auto family = descr.get_family();
    if (family.empty()) {
        family = "sans-serif";
    }

    // PANGO BUG...
    //   A font spec of Delicious, 500 Italic should result in a family of 'Delicious'
    //   and a style of 'Medium Italic'. It results instead with: a family of
    //   'Delicious, 500' with a style of 'Medium Italic'. We chop off any weight numbers
    //   at the end of the family:  match ",[1-9]00^".
    static auto const weight = Glib::Regex::create(",[1-9]00$", Glib::Regex::CompileFlags::OPTIMIZE);
    family = weight->replace(family, 0, "", Glib::Regex::MatchFlags::PARTIAL);

    // Pango canonized strings remove space after comma between family names. Put it back.
    size_t i = 0;
    while ((i = family.find(",", i)) != std::string::npos) {
        family.replace(i, 1, ", ");
        i += 2;
    }

    descr.unset_fields(Pango::FontMask::FAMILY);

    return {std::move(family), descr.to_string()};
}

// Not sure if there's some code here with functionality that's nowhere else. Particularly middle bit.
/*
// TODO: use to determine font-selector best style
// TODO: create new function new_font_family(Gtk::TreeModel::iterator iter)
std::pair<Glib::ustring, Glib::ustring> FontLister::new_font_family(Glib::ustring const &new_family, bool)
{
    // No need to do anything if new family is same as old family.
    if (familyNamesAreEqual(new_family, current_family)) {
        return std::make_pair(current_family, current_style);
    }

    // We need to do two things:
    // 1. Update style list for new family.
    // 2. Select best valid style match to old style.

    // For finding style list, use list of first family in font-family list.
    std::shared_ptr<Styles> styles = default_styles;
    auto const size = font_list_store->get_n_items();
    for (unsigned i = 0; i < size; i++) {
        auto &row = font_list_store->get_item(i)->data;
        if (familyNamesAreEqual(new_family, row.family)) {
            styles = row.getStyles();
            break;
        }
    }

    // TODO: if font-family is list, check if first family in list is on system
    // and set style accordingly.

    // Update style list.
    std::vector<Glib::RefPtr<WrapAsGObject<StyleNames>>> items;
    for (auto const &style : *styles) {
        items.push_back(WrapAsGObject<StyleNames>::create(style));
    }
    style_list_store->splice(0, style_list_store->get_n_items(), items);

    // Find best match to the style from the old font-family to the
    // styles available with the new font.
    // TODO: Maybe check if an exact match exists before using Pango.
    Glib::ustring best_style = closestStyle(get_item_for_font(new_family), current_style);

    return std::make_pair(new_family, best_style);
}
*/

void FontLister::set_dragging_family(const Glib::ustring &new_family)
{
    dragging_family = new_family;
}

// We do this ourselves as we can't rely on FontFactory.
void fill_css_from_fontspec(SPCSSAttr *css, Glib::ustring const &fontspec)
{
    auto family = Glib::ustring{std::move(ui_from_fontspec(fontspec).family)};

    // Font spec is single quoted... for the moment
    Glib::ustring fontspec_quoted(fontspec);
    css_quote(fontspec_quoted);
    sp_repr_css_set_property(css, "-inkscape-font-specification", fontspec_quoted.c_str());

    // Font families needs to be properly quoted in CSS (used unquoted in font-lister)
    css_font_family_quote(family);
    sp_repr_css_set_property(css, "font-family", family.c_str());

    PangoFontDescription *desc = pango_font_description_from_string(fontspec.c_str());
    PangoWeight weight = pango_font_description_get_weight(desc);
    switch (weight) {
        case PANGO_WEIGHT_THIN:
            sp_repr_css_set_property(css, "font-weight", "100");
            break;
        case PANGO_WEIGHT_ULTRALIGHT:
            sp_repr_css_set_property(css, "font-weight", "200");
            break;
        case PANGO_WEIGHT_LIGHT:
            sp_repr_css_set_property(css, "font-weight", "300");
            break;
        case PANGO_WEIGHT_SEMILIGHT:
            sp_repr_css_set_property(css, "font-weight", "350");
            break;
        case PANGO_WEIGHT_BOOK:
            sp_repr_css_set_property(css, "font-weight", "380");
            break;
        case PANGO_WEIGHT_NORMAL:
            sp_repr_css_set_property(css, "font-weight", "normal");
            break;
        case PANGO_WEIGHT_MEDIUM:
            sp_repr_css_set_property(css, "font-weight", "500");
            break;
        case PANGO_WEIGHT_SEMIBOLD:
            sp_repr_css_set_property(css, "font-weight", "600");
            break;
        case PANGO_WEIGHT_BOLD:
            sp_repr_css_set_property(css, "font-weight", "bold");
            break;
        case PANGO_WEIGHT_ULTRABOLD:
            sp_repr_css_set_property(css, "font-weight", "800");
            break;
        case PANGO_WEIGHT_HEAVY:
            sp_repr_css_set_property(css, "font-weight", "900");
            break;
        case PANGO_WEIGHT_ULTRAHEAVY:
            sp_repr_css_set_property(css, "font-weight", "1000");
            break;
        default:
            // Pango can report arbitrary numeric weights, not just those values
            // with corresponding convenience enums
            if (weight > 0 && weight < 1000) {
                sp_repr_css_set_property(css, "font-weight", std::to_string(weight).c_str());
            }
            else {
                g_message("Pango reported font weight of %d ignored (font: '%s').", weight, fontspec.c_str());
            }
            break;
    }

    PangoStyle style = pango_font_description_get_style(desc);
    switch (style) {
        case PANGO_STYLE_NORMAL:
            sp_repr_css_set_property(css, "font-style", "normal");
            break;
        case PANGO_STYLE_OBLIQUE:
            sp_repr_css_set_property(css, "font-style", "oblique");
            break;
        case PANGO_STYLE_ITALIC:
            sp_repr_css_set_property(css, "font-style", "italic");
            break;
    }

    PangoStretch stretch = pango_font_description_get_stretch(desc);
    switch (stretch) {
        case PANGO_STRETCH_ULTRA_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "ultra-condensed");
            break;
        case PANGO_STRETCH_EXTRA_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "extra-condensed");
            break;
        case PANGO_STRETCH_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "condensed");
            break;
        case PANGO_STRETCH_SEMI_CONDENSED:
            sp_repr_css_set_property(css, "font-stretch", "semi-condensed");
            break;
        case PANGO_STRETCH_NORMAL:
            sp_repr_css_set_property(css, "font-stretch", "normal");
            break;
        case PANGO_STRETCH_SEMI_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "semi-expanded");
            break;
        case PANGO_STRETCH_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "expanded");
            break;
        case PANGO_STRETCH_EXTRA_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "extra-expanded");
            break;
        case PANGO_STRETCH_ULTRA_EXPANDED:
            sp_repr_css_set_property(css, "font-stretch", "ultra-expanded");
            break;
    }

    PangoVariant variant = pango_font_description_get_variant(desc);
    switch (variant) {
        case PANGO_VARIANT_NORMAL:
            sp_repr_css_set_property(css, "font-variant", "normal");
            break;
        case PANGO_VARIANT_SMALL_CAPS:
            sp_repr_css_set_property(css, "font-variant", "small-caps");
            break;
    }

    // Convert Pango variations string to CSS format
    const char* str = pango_font_description_get_variations(desc);

    std::string variations;

    if (str) {
        auto variations_map = parse_variations(str);
        for (auto [tag, value] : variations_map) {

            // Per CSS Fonts Level 4, favor higher level properties over 'font-variation-settings'
            if (tag == "wght") {
                sp_repr_css_set_property(css, "font-weight", value.c_str());
         // } else if (tag == "wdth") {      Need to support font-stretch % values before enabling.
         //     sp_repr_css_set_property(css, "font-stretch", value.c_str());
         // } else if (tag == "slnt") {      Need to support arbitrary oblique angles before enabling.
         // Ranges from -90deg to 90deg. If not value given, defaults to 14deg.
         //     sp_repr_css_set_property(css, "font-style", "oblique" + std::to_string(value));
            } else if (tag == "ital") {
                // A font should not have both a 'slnt' and an 'ital' axis. The 'ital' axis ranges
                // between 0 and 1 but CSS only allows it to be either on or off.
                sp_repr_css_set_property(css, "font-style", (value == "1" ? "italic" : "normal"));
            } else {
                variations += "'";
                variations += tag;
                variations += "' ";
                variations += value;
                variations += ", ";
            }
        }
        if (variations.length() >= 2) { // Remove last comma/space
            variations.pop_back();
            variations.pop_back();
        }
    }

    if (!variations.empty()) {
        sp_repr_css_set_property(css, "font-variation-settings", variations.c_str());
    } else {
        sp_repr_css_unset_property(css,  "font-variation-settings" );
    }
    pango_font_description_free(desc);
}

Glib::ustring fontspec_from_style(SPStyle const *style)
{
    return ink_font_description_from_style(style).to_string();
}

static int compute_distance(Pango::FontDescription const &a, Pango::FontDescription const &b)
{
    // Weight: multiples of 100
    int distance = std::abs(a.get_weight() - b.get_weight());

    distance += 10000 * std::abs(static_cast<int>(a.get_stretch()) - static_cast<int>(b.get_stretch()));

    auto const style_a = a.get_style();
    auto const style_b = b.get_style();
    if (style_a != style_b) {
        if ((style_a == Pango::Style::OBLIQUE && style_b == Pango::Style::ITALIC) ||
            (style_b == Pango::Style::OBLIQUE && style_a == Pango::Style::ITALIC)) {
            distance += 1000; // Oblique and italic are almost the same
        } else {
            distance += 100000; // Normal vs oblique/italic, not so similar
        }
    }

    // Normal vs small-caps
    distance += 1000000 * (a.get_variant() != b.get_variant());

    return distance;
}

// void
// font_description_dump( PangoFontDescription* target ) {
//   std::cout << "  Font:      " << pango_font_description_to_string( target ) << std::endl;
//   std::cout << "    style:   " << pango_font_description_get_style(   target ) << std::endl;
//   std::cout << "    weight:  " << pango_font_description_get_weight(  target ) << std::endl;
//   std::cout << "    variant: " << pango_font_description_get_variant( target ) << std::endl;
//   std::cout << "    stretch: " << pango_font_description_get_stretch( target ) << std::endl;
//   std::cout << "    gravity: " << pango_font_description_get_gravity( target ) << std::endl;
// }

// This is inspired by pango_font_description_better_match, but that routine
// always returns false if variant or stretch are different. This means, for
// example, that PT Sans Narrow with style Bold Condensed is never matched
// to another font-family with Bold style.
// TODO: Remove or turn into function to be used by new_font_family.
std::string closestStyle(FontLister::FontListItem *item, std::string const &target_style)
{
    if (!item) {
        return target_style;
    }

    auto const target = Pango::FontDescription{item->family + ", " + target_style};

    struct Best
    {
        Pango::FontDescription descr;
        int distance;
    };
    std::optional<Best> best;

    for (auto const &style : *item->getStyles()) {
        auto const descr = Pango::FontDescription(item->family + ", " + style.css_name);
        auto const distance = compute_distance(target, descr);
        if (!best || distance < best->distance) {
            best = Best{
                .descr = std::move(descr),
                .distance = distance,
            };
        }
    }

    if (!best) {
        return target_style;
    }

    best->descr.unset_fields(Pango::FontMask::FAMILY);
    return best->descr.to_string();
}

LocalFontLister::LocalFontLister()
{
    all_font_models = Gio::ListStore<Gio::ListModel>::create();
    all_font_models->append(FontLister::get_instance()->get_font_list());
    all_fonts_flat = Gtk::FlattenListModel::create(all_font_models);

    styles_store = Gio::ListStore<WrapAsGObject<StyleNames>>::create();
    _setStyles(*FontLister::get_instance()->getDefaultStyles());
}

void LocalFontLister::unsetDocument()
{
    assert(document_fonts);
    all_font_models->remove(0); // remove document fonts
    document_fonts = {};
}

void LocalFontLister::setDocument(SPDocument *document)
{
    assert(!document_fonts);
    document_fonts = document->getDocumentFonts().getFamilies();
    all_font_models->insert(0, document_fonts); // insert document fonts
}

void LocalFontLister::setFontFamily(unsigned pos)
{
    auto &item = getItem(pos);
    family = item.family;
    _setStyles(*item.getStyles());
    style = closestStyle(&item, style);
}

void LocalFontLister::setFontFamily(Glib::ustring new_family)
{
    family = std::move(new_family);
    if (auto const item = getItemForFont(family)) {
        _setStyles(*item->getStyles());
        style = closestStyle(item, style);
    } else {
        _setStyles(*FontLister::get_instance()->getDefaultStyles());
    }
}

void LocalFontLister::setFontStyle(Glib::ustring new_style)
{
    // TODO: Validate input using Pango. If Pango doesn't recognize a style it will
    // attach the "invalid" style to the font-family.
    style = std::move(new_style);
}

void LocalFontLister::selectionUpdate(SPDesktop *desktop)
{
    // Get fontspec from a selection, preferences, or thin air.

    // Directly from stored font specification.
    auto query = SPStyle{desktop->getDocument()};
    int result = sp_desktop_query_style(desktop, &query, QUERY_STYLE_PROPERTY_FONT_SPECIFICATION);

    Glib::ustring fontspec;

    //std::cout << "  Attempting selected style" << std::endl;
    if (result != QUERY_STYLE_NOTHING && query.font_specification.set) {
        fontspec = query.font_specification.value();
        //std::cout << "   fontspec from query   :" << fontspec << ":" << std::endl;
    }

    // From style
    if (fontspec.empty()) {
        //std::cout << "  Attempting desktop style" << std::endl;
        int rfamily = sp_desktop_query_style(desktop, &query, QUERY_STYLE_PROPERTY_FONTFAMILY);
        int rstyle = sp_desktop_query_style(desktop, &query, QUERY_STYLE_PROPERTY_FONTSTYLE);

        // Must have text in selection
        if (rfamily != QUERY_STYLE_NOTHING && rstyle != QUERY_STYLE_NOTHING) {
            fontspec = fontspec_from_style(&query);
        }
        //std::cout << "   fontspec from style   :" << fontspec << ":" << std::endl;
    }

    // From preferences
    if (fontspec.empty()) {
        auto prefs = Inkscape::Preferences::get();
        if (prefs->getBool("/tools/text/usecurrent")) {
            query.mergeCSS(desktop->getCurrentOrToolStyle("/tools/text", true));
        } else {
            query.readFromPrefs("/tools/text");
        }
        fontspec = fontspec_from_style(&query);
    }

    // From thin air
    if (fontspec.empty()) {
        //std::cout << "  Attempting thin air" << std::endl;
        fontspec = family + ", " + style;
        //std::cout << "   fontspec from thin air   :" << fontspec << ":" << std::endl;
    }

    auto fs = ui_from_fontspec(fontspec);

    family = std::move(fs.family);
    style = std::move(fs.style);
}

void LocalFontLister::setFontspec(Glib::ustring const &new_fontspec)
{
    auto fs = ui_from_fontspec(new_fontspec);
    family = std::move(fs.family);
    style = std::move(fs.style);
}

FontLister::FontListItem &LocalFontLister::getItem(unsigned pos)
{
    return all_fonts_flat->get_typed_object<WrapAsGObject<FontLister::FontListItem>>(pos)->data;
}

unsigned LocalFontLister::getPosForFont(Glib::ustring const &family)
{
    unsigned const size = all_fonts_flat->get_n_items();
    for (unsigned i = 0; i < size; i++) {
        if (familyNamesAreEqual(family, getItem(i).family)) {
            return i;
        }
    }
    return GTK_INVALID_LIST_POSITION;
}

FontLister::FontListItem *LocalFontLister::getItemForFont(Glib::ustring const &family)
{
    auto const pos = getPosForFont(family);
    if (pos == GTK_INVALID_LIST_POSITION) {
        return nullptr;
    }
    return &getItem(pos);
}

void LocalFontLister::_setStyles(FontLister::Styles const &styles)
{
    std::vector<Glib::RefPtr<WrapAsGObject<StyleNames>>> items;
    for (auto const &style : styles) {
        items.push_back(WrapAsGObject<StyleNames>::create(style));
    }
    styles_store->splice(0, styles_store->get_n_items(), items);
}

// Draw system fonts in dark blue, missing fonts with red strikeout.
// Used by both FontSelector and Text toolbar.
Glib::ustring font_lister_get_markup(FontLister::FontListItem const &item)
{
    auto prefs = Inkscape::Preferences::get();
    auto font_lister = FontLister::get_instance();

    auto family_escaped = Glib::Markup::escape_text(item.family);
    bool dark = prefs->getBool("/theme/darkTheme", false);
    Glib::ustring markup;

    if (!item.on_system) {
        markup = "<span font-weight='bold'>";

        // See if font-family on system.
        auto tokens = Glib::Regex::split_simple("\\s*,\\s*", item.family);

        for (auto const &token : tokens) {
            if (font_lister->font_installed_on_system(token)) {
                markup += Glib::Markup::escape_text(token);
                markup += ", ";
            } else {
                if (dark) {
                    markup += "<span strikethrough='true' strikethrough_color='salmon'>";
                } else {
                    markup += "<span strikethrough='true' strikethrough_color='red'>";
                }
                markup += Glib::Markup::escape_text(token);
                markup += "</span>";
                markup += ", ";
            }
        }
        // Remove extra comma and space from end.
        if (markup.size() >= 2) {
            markup.resize(markup.size() - 2);
        }
        markup += "</span>";
    } else {
        markup = family_escaped;
    }

    int show_sample = prefs->getInt("/tools/text/show_sample_in_list", 1);
    if (show_sample) {
        auto const sample = prefs->getString("/tools/text/font_sample");
        markup += " <span alpha='55%";
        // Set small line height to avoid semi-hidden fonts (one line height rendering overlap without padding).
#if PANGO_VERSION_CHECK(1, 50, 0)
        markup += "' font-size='100%' line-height='0.6' font_family='";
#else
        markup += "' font_family='";
#endif
        markup += family_escaped;
        markup += "'>";
        markup += Glib::Markup::escape_text(sample);
        markup += "</span>";
    }

    return markup;
}

} // namespace Inkscape

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
