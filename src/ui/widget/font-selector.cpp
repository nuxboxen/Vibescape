// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2018 Tavmong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "font-selector.h"

#include <gtkmm/dragsource.h>
#include <gtkmm/grid.h>
#include <glibmm/i18n.h>
#include <glibmm/markup.h>

#include "libnrtype/font-factory.h"
#include "libnrtype/font-lister.h"

#include "desktop.h"
#include "preferences.h"
#include "util-string/ustring-format.h"

namespace Inkscape::UI::Widget {

std::unique_ptr<FontSelectorInterface> FontSelector::create_font_selector() {
    return std::make_unique<FontSelector>();
}

FontSelector::FontSelector(bool with_size, bool with_variations)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , family_frame(_("Font family"))
    , style_frame(C_("Font selector", "Style"))
    , size_label(_("Font size"))
    , signal_block(false)
{
    // Font family
    family_selection = Gtk::SingleSelection::create();
    family_listview.set_model(family_selection);
    family_listview.set_factory(create_label_factory<FontLister::FontListItem>(font_lister_get_markup, true));
    family_listview.set_name("FontSelector: Family");

    family_scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    family_scroll.set_child(family_listview);

    family_frame.set_hexpand(true);
    family_frame.set_vexpand(true);
    family_frame.set_child(family_scroll);

    // Style
    auto col_css = Gtk::ColumnViewColumn::create(_("CSS"), create_label_factory<StyleNames>([] (auto &s) { return s.css_name; }));
    style_columnview.append_column(col_css);
    col_css->set_resizable(true);

    auto col_face = Gtk::ColumnViewColumn::create(_("Face"), create_label_factory<StyleNames>([this] (auto &s) { return get_style_markup(s); }, true));
    style_columnview.append_column(col_face);
    col_face->set_expand(true);

    style_selection = Gtk::SingleSelection::create();
    style_columnview.set_model(style_selection);
    style_columnview.set_name("FontSelectorStyle");
    style_columnview.add_css_class("data-table");

    style_scroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    style_scroll.set_child(style_columnview);

    style_frame.set_hexpand(true);
    style_frame.set_vexpand(true);
    style_frame.set_child(style_scroll);

    // Size
    size_selector.setSize(sp_style_css_size_px_to_units(18, size_selector.getUnit()));

    // Font Variations
    font_variations.set_vexpand(true);
    font_variations_scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    font_variations_scroll.set_child(font_variations);
    font_variations_scroll.set_vexpand(false);

    // Grid
    set_name ("FontSelectorGrid");
    set_spacing(4);

    auto const grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_column_homogeneous(true);
    grid->set_column_spacing(4);
    constexpr int extra = 1;
    grid->attach(family_frame, 0, 0, 1 + extra, 1);
    grid->attach(style_frame, 1 + extra, 0, 1, 1);
    append(*grid);

    if (with_size) { // Glyph panel does not use size.
        auto const size_grid = Gtk::make_managed<Gtk::Grid>();
        size_grid->set_column_spacing(4);
        size_grid->attach(size_label, 0, 0, 1, 1);
        size_grid->attach(size_selector, 1, 0, 1, 1);
        append(*size_grid);
    }
    if (with_variations) { // Glyphs panel does not use variations.
        append(font_variations_scroll);
    }

    // update_variations(font_lister->get_fontspec());

    // For drag and drop.
    auto const drag = Gtk::DragSource::create();
    drag->signal_prepare().connect(sigc::mem_fun(*this, &FontSelector::on_drag_prepare), false); // before
    drag->signal_drag_begin().connect([this, &drag = *drag](auto &&...args) { on_drag_begin(drag, args...); });
    family_listview.add_controller(drag);

    // Add signals
    family_selection->signal_selection_changed().connect([this] (auto, auto) { on_family_changed(); });
    style_selection->signal_selection_changed().connect([this] (auto...) { on_style_changed(); });
    font_variations.connectChanged(sigc::mem_fun(*this, &FontSelector::on_variations_changed));
}

void FontSelector::hide_others()
{
    style_frame.set_visible(false);
    size_label.set_visible(false);
    size_selector.set_visible(false);
    font_variations_scroll.set_visible(false);
    font_variations_scroll.set_vexpand(false);
}

// TODO: Dropping doesnʼt seem to be implemented anywhere
void FontSelector::on_drag_begin(Gtk::DragSource &source,
                                 Glib::RefPtr<Gdk::Drag> const &drag)
{
    // Get the current collection.
    // path = family_selection->selection();
    // auto paintable = family_listview.create_row_drag_icon(path);
    // source.set_icon(paintable, 0, 0);
}

Glib::RefPtr<Gdk::ContentProvider> FontSelector::on_drag_prepare(double, double)
{
    auto font_lister = Inkscape::FontLister::get_instance();
    Glib::ustring family_name = font_lister->get_dragging_family(); // Fixme: WTF.

    Glib::Value<Glib::ustring> value;
    value.init(G_TYPE_STRING);
    value.set(family_name);
    return Gdk::ContentProvider::create(value);
}

static std::pair<Glib::RefPtr<Gio::ListStore<WrapAsGObject<StyleNames>>>, unsigned> create_styles_store(FontLister::Styles const &styles, Glib::ustring const &search_css_name)
{
    unsigned pos = 0;
    unsigned match = GTK_INVALID_LIST_POSITION;
    std::vector<Glib::RefPtr<WrapAsGObject<StyleNames>>> items;
    for (auto const &style : styles) {
        items.push_back(WrapAsGObject<StyleNames>::create(style));
        if (style.css_name == search_css_name) {
            match = pos;
        }
        pos++;
    }

    auto store = Gio::ListStore<WrapAsGObject<StyleNames>>::create();
    store->splice(0, 0, items);

    return {std::move(store), match};
}

// Update GUI.
// We keep a private copy of the style list as the font-family in widget is only temporary
// until the "Apply" button is set so the style list can be different from that in FontLister.
void FontSelector::update_font()
{
    signal_block = true;

    Glib::ustring family = _localfontlister->family;
    Glib::ustring style  = _localfontlister->style;

    // Set font family
    auto const path = _localfontlister->getPosForFont(family);
    if (path == GTK_INVALID_LIST_POSITION) {
        std::cerr << "FontSelector::update_font: Couldn't find row for font-family: "
                  << family.raw() << std::endl;
    }

    auto const pos = family_selection->get_selected();
    if (pos == GTK_INVALID_LIST_POSITION || _localfontlister->getItem(pos).family.casefold() != family.casefold()) {
        family_listview.scroll_to(path, Gtk::ListScrollFlags::FOCUS | Gtk::ListScrollFlags::SELECT);
    }

    // Get font-lister style list for selected family
    auto &row = _localfontlister->getItem(path);

    // Copy font-lister style list to private list store, searching for match.
    auto [local_style_list_store, match] = create_styles_store(*row.getStyles(), style);

    // Attach store to column view and select row.
    style_selection->set_model(local_style_list_store);
    if (match != GTK_INVALID_LIST_POSITION) {
        style_selection->set_selected(match);
    }

    update_variations(_localfontlister->getFontspec());

    signal_block = false;
}

void FontSelector::unset_model()
{
    _localfontlister = nullptr;
    family_selection->set_model({});
    style_selection->set_model({});
}

void FontSelector::set_model(LocalFontLister &localfontlister)
{
    _localfontlister = &localfontlister;
    family_selection->set_model(_localfontlister->getFonts());
    style_selection->set_model(_localfontlister->getStyles());
}

// If use_variations is true (default), we get variation values from variations widget otherwise we
// get values from CSS widget (we need to be able to keep the two widgets synchronized both ways).
Glib::ustring
FontSelector::get_fontspec(bool use_variations) {

    // Build new fontspec from GUI settings
    Glib::ustring family = "Sans";  // Default...family list may not have been constructed.
    auto const iter = dynamic_cast<WrapAsGObject<FontLister::FontListItem> const *>(family_selection->get_selected_item().get());
    if (iter) {
        family = iter->data.family;
    }

    Glib::ustring style = "Normal";
    if (auto const selected_style = style_selection->get_selected_item()) {
        style = dynamic_cast<WrapAsGObject<StyleNames> const &>(*selected_style).data.css_name;
    }

    if (family.empty()) {
        std::cerr << "FontSelector::get_fontspec: empty family!" << std::endl;
    }

    if (style.empty()) {
        std::cerr << "FontSelector::get_fontspec: empty style!" << std::endl;
    }

    Glib::ustring fontspec = family + ", ";

    if (use_variations) {
        // Clip any font_variation data in 'style' as we'll replace it.
        auto pos = style.find('@');
        if (pos != Glib::ustring::npos) {
            style.erase (pos, style.length()-1);
        }

        Glib::ustring variations = font_variations.get_pango_string();

        if (variations.empty()) {
            fontspec += style;
        } else {
            fontspec += style;
            fontspec += " ";
            fontspec += variations;
        }
    } else {
        fontspec += style;
    }

    return fontspec;
}

Glib::ustring FontSelector::get_style_markup(StyleNames const &stylenames)
{
    Glib::ustring family = "Sans";  // Default...family list may not have been constructed.
    auto const iter_family = dynamic_cast<WrapAsGObject<FontLister::FontListItem> const *>(family_selection->get_selected_item().get());
    if (iter_family) {
        family = iter_family->data.family;
    }

    Glib::ustring style_escaped  = Glib::Markup::escape_text(stylenames.display_name);
    Glib::ustring font_desc = Glib::Markup::escape_text(family + ", " + stylenames.css_name);
    Glib::ustring markup;

    return "<span font='" + font_desc + "'>" + style_escaped + "</span>";
}

// Callbacks

// Need to update style list
void
FontSelector::on_family_changed() {

    if (signal_block) return;
    signal_block = true;

    Glib::RefPtr<Gtk::TreeModel> model;
    auto const iter = family_selection->get_selected();

    if (iter == GTK_INVALID_LIST_POSITION) {
        // This can happen just after the family list is recreated.
        signal_block = false;
        return;
    }

    auto &row = _localfontlister->getItem(iter);

    FontLister::get_instance()->set_dragging_family(row.family);

    // Find best style match for selected family with current style (e.g. of selected text).
    Glib::ustring best = closestStyle(&row, _localfontlister->style);

    // Create our own store of styles for selected font-family (the font-family selected
    // in the dialog may not be the same as stored in the font-lister class until the
    // "Apply" button is triggered).
    auto [local_style_list_store, match] = create_styles_store(*row.getStyles(), best);

    // Attach store to tree view and select row.
    style_selection->set_model(local_style_list_store);
    if (match != GTK_INVALID_LIST_POSITION) {
        style_selection->set_selected(match);
    }

    // variation sliders are refreshed immediately,
    // rather than waiting for an Apply.
    update_variations(get_fontspec(false));

    signal_block = false;

    // Let world know
    changed_emit();
}

void
FontSelector::on_style_changed() {
    if (signal_block) return;

    // Update variations widget if new style selected from style widget.
    signal_block = true;
    Glib::ustring fontspec = get_fontspec( false );
    update_variations(fontspec);
    signal_block = false;

    // Let world know
    changed_emit();
}

void
FontSelector::on_size_changed(double size, int unit) {

    if (signal_block) return;

    changed_emit();
}

void
FontSelector::on_variations_changed() {

    if (signal_block) return;

    // Let world know
    changed_emit();
}

void FontSelector::changed_emit() {
    signal_block = true;
    _signal_set_default.emit();
    signal_block = false;
}

void FontSelector::update_variations(const Glib::ustring& fontspec) {
    font_variations.update(fontspec);

    // Check if there are any variations available; if not, don't expand font_variations_scroll
    bool hasContent = font_variations.variations_present();
    font_variations_scroll.set_visible(hasContent);
}

} // namespace Inkscape::UI::Widget

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
