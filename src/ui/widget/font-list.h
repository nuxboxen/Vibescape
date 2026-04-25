// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Font browser and selector
 *
 * Copyright (C) 2022-2025 Michael Kowalski
 */

#ifndef INKSCAPE_UI_WIDGET_FONT_LIST_H
#define INKSCAPE_UI_WIDGET_FONT_LIST_H

#include <giomm/liststore.h>
#include <gtkmm/builder.h>
#include <gtkmm/grid.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/gridview.h>
#include <gtkmm/listbox.h>
#include <gtkmm/listview.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/singleselection.h>

#include "character-viewer.h"
#include "ui/widget/font-variations.h"
#include "util/font-discovery.h"
#include "util/font-tags.h"
#include "font-selector-interface.h"
#include "generic/number-combo-box.h"
#include "generic/popover-menu.h"
#include "ui/text_filter.h"

namespace Inkscape::UI::Widget {

class FontList : public Gtk::Box, public FontSelectorInterface {
public:
    static std::unique_ptr<FontSelectorInterface> create_font_list(Glib::ustring pref_path);

    FontList(Glib::ustring preferences_path);

    // get font selected in this FontList, if any
    Glib::ustring get_fontspec() const override;
    double get_fontsize() const override;

    // show requested font in a FontList
    void set_current_font(const Glib::ustring& family, const Glib::ustring& face) override;
    // 
    void set_current_size(double size) override;

    sigc::signal<void ()>& signal_changed() override { return _signal_changed; }
    sigc::signal<void ()>& signal_apply() override { return _signal_apply; }
    sigc::signal<void (const Glib::ustring&)>& signal_insert_text() override { return _signal_insert_text; }

    Gtk::Widget* box() override { return this; }

    ~FontList() override = default;

    // no op, not used
    void set_model() override {};
    void unset_model() override {};

private:
    void on_map() override;
    void sort_fonts(FontOrder order);
    void set_sort_icon();
    void apply_filters(bool all_filters = true);
    void rebuild_ui();
    void rebuild_store();
    void populate_font_store(bool by_family);
    void apply_filters_keep_selection(bool text_only = false);
    void add_font(const Glib::ustring& fontspec, bool select);
    bool select_font(const Glib::ustring& fontspec);
    void update_font_count();
    void add_categories();
    void update_categories(const std::string& tag, bool select);
    void update_filterbar();
    Gtk::Box* create_pill_box(const Glib::ustring& display_name, const Glib::ustring& tag, bool tags);
    void sync_font_tag(const FontTag* ftag, bool selected);
    void scroll_to_row(int index);
    void set_font_size_layout(bool top);
    Glib::RefPtr<Glib::ObjectBase> get_selected_font() const;
    Glib::RefPtr<Glib::ObjectBase> get_nth_font(int index) const;
    int find_font(const Glib::ustring& fontspec, int from = 0, int count = -1) const;
    void switch_view_mode(bool show_list);

    sigc::signal<void ()> _signal_changed;
    sigc::signal<void ()> _signal_apply;
    sigc::signal<void (const Glib::ustring&)> _signal_insert_text;
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Grid& _main_grid;
    Gtk::GridView& _font_grid;
    Gtk::ListView& _font_list;
    Gtk::SearchEntry2& _search;
    Gtk::Box& _tag_box;
    Gtk::Box& _info_box;
    Gtk::Box& _progress_box;
    Gtk::Entry& _grid_sample_entry;
    Gtk::Entry& _list_sample_entry;
    Gtk::Scale& _preview_size_scale;
    Gtk::Scale& _grid_size_scale;
    Gtk::ScrolledWindow& _var_axes;
    Gtk::ListBox& _tag_list;
    Inkscape::FontTags& _font_tags;
    std::vector<FontInfo> _fonts;
    std::vector<std::vector<FontInfo>> _font_families;
    Glib::RefPtr<Gio::ListStoreBase> _font_store;
    Glib::RefPtr<Gtk::BoolFilter> _text_filter;
    Glib::RefPtr<Gtk::BoolFilter> _font_filter;
    Glib::RefPtr<Gtk::BoolFilter> _family_filter;
    Glib::RefPtr<Gtk::SingleSelection> _list_selection;
    Glib::RefPtr<Gtk::SingleSelection> _grid_selection;
    bool _list_visible = true;
    FontOrder _order = FontOrder::ByFamily;
    Glib::ustring _filter;
    NumberComboBox& _font_size;
    Gtk::Scale& _font_size_scale;
    Glib::ustring _current_fspec;
    double _current_fsize = 0.0;
    bool _show_font_names = true;
    Glib::ustring _sample_text;
    Glib::ustring _grid_sample_text;
    int _sample_font_size = 200;
    int _grid_font_size = 300;
    Glib::ustring _search_term;
    OperationBlocker _update;
    FontVariations _font_variations;
    Glib::ustring _prefs;
    sigc::scoped_connection _font_stream;
    std::size_t _initializing = 0;
    sigc::scoped_connection _font_collections_update;
    sigc::scoped_connection _font_collections_selection;
    Gtk::Popover _charmap_popover;
    CharacterViewer _charmap;
    std::shared_ptr<FontInstance> _current_font_instance; // for charmap only
    PopoverMenuItem* _sort_by_family = nullptr;
};

} // namespaces

#endif // INKSCAPE_UI_WIDGET_FONT_LIST_H
