// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Color system panel — ICC/CMS color profile linking.
 */
/*
 * Authors:
 *
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-system-panel.h"

#include <glibmm/i18n.h>

#include "colors/cms/profile.h"
#include "colors/cms/system.h"
#include "colors/document-cms.h"
#include "document.h"
#include "document-undo.h"
#include "object/color-profile.h"
#include "ui/builder-utils.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace Inkscape::UI::Widget {

namespace {

void sanitize_name(std::string& str)
{
    if (str.empty()) return;
    auto val = str.at(0);
    if ((val < 'A' || val > 'Z') && (val < 'a' || val > 'z') && val != '_' && val != ':')
        str.insert(0, "_");
    for (std::size_t i = 1; i < str.size(); i++) {
        val = str.at(i);
        if ((val < 'A' || val > 'Z') && (val < 'a' || val > 'z') &&
            (val < '0' || val > '9') &&
            val != '_' && val != ':' && val != '-' && val != '.') {
            if (str.at(i - 1) == '-') { str.erase(i, 1); i--; }
            else str.replace(i, 1, "-");
        }
    }
    if (str.at(str.size() - 1) == '-') str.pop_back();
}

} // namespace

ColorSystemPanel::ColorSystemPanel()
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , _builder(create_builder("color-system-panel.ui"))
    , _linked_label(get_widget<Gtk::Label>(_builder, "linked_label"))
    , _linked_scroller(get_widget<Gtk::ScrolledWindow>(_builder, "linked_scroller"))
    , _linked_view(get_widget<Gtk::TreeView>(_builder, "linked_view"))
    , _available_label(get_widget<Gtk::Label>(_builder, "available_label"))
    , _avail_combo(get_widget<Gtk::ComboBox>(_builder, "avail_combo"))
    , _unlink_btn(get_widget<Gtk::Button>(_builder, "unlink_btn"))
{
    _avail_store = Gtk::ListStore::create(_avail_cols);
    _avail_combo.set_model(_avail_store);
    _avail_combo.pack_start(_avail_cols.name);
    _avail_combo.set_row_separator_func(
        sigc::mem_fun(*this, &ColorSystemPanel::available_profiles_separator));
    _avail_combo.signal_changed().connect(
        sigc::mem_fun(*this, &ColorSystemPanel::link_selected_profile));
    auto cell = _avail_combo.get_cells()[0];
    if (auto text_cell = dynamic_cast<Gtk::CellRendererText*>(cell)) {
        text_cell->property_ellipsize() = Pango::EllipsizeMode::END;
        text_cell->property_width_chars() = 20; // Max characters before ellipsis
    }

    _linked_store = Gtk::ListStore::create(_linked_cols);
    _linked_view.set_model(_linked_store);
    // Create a custom column with ellipsis for long text
    auto column = Gtk::make_managed<Gtk::TreeViewColumn>();
    auto cell_renderer = Gtk::make_managed<Gtk::CellRendererText>();
    cell_renderer->property_ellipsize() = Pango::EllipsizeMode::END;
    cell_renderer->property_width_chars() = 25; // Max characters before ellipsis
    column->pack_start(*cell_renderer, true);
    column->add_attribute(cell_renderer->property_text(), _linked_cols.name);
    column->set_title(_("Profile Name"));
    _linked_view.append_column(*column);
    _linked_view.set_headers_visible(false);
    _linked_view.set_enable_search(false);

    // Set TreeView as child of ScrolledWindow
    _linked_scroller.set_child(_linked_view);

    _unlink_btn.set_sensitive(false);
    _unlink_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &ColorSystemPanel::remove_selected_profile));

    _linked_view.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &ColorSystemPanel::on_profile_select_row));

    auto& grid = get_widget<Gtk::Grid>(_builder, "color_system_grid");
    append(grid);

    set_spacing(4);
    set_margin(8);
}

void ColorSystemPanel::set_document(SPDocument* document) {
    _document = document;
    update(document);
}

void ColorSystemPanel::update(SPDocument* document) {
    _document = document;
    _linked_store->clear();
    if (!document) return;

    std::vector<SPObject*> current = document->getResourceList("iccprofile");
    std::set<ColorProfile*> profiles;
    for (auto obj : current) {
        if (auto p = dynamic_cast<ColorProfile*>(obj))
            profiles.insert(p);
    }
    for (auto p : profiles) {
        auto row = *(_linked_store->append());
        row[_linked_cols.name] = p->getName();
    }
    on_profile_select_row();
}

void ColorSystemPanel::populate_available_profiles() {
    if (!_avail_store->children().empty()) return;

    _avail_store->clear();

    bool home = true;
    bool first = true;
    auto& cms = Colors::CMS::System::get();
    cms.refreshProfiles();
    for (auto const& profile : cms.getProfiles()) {
        if (!first && profile->inHome() != home) {
            auto row = *(_avail_store->append());
            row[_avail_cols.file]         = "<separator>";
            row[_avail_cols.name]         = "<separator>";
            row[_avail_cols.is_separator] = true;
        }
        home  = profile->inHome();
        first = false;

        auto row = *(_avail_store->append());
        row[_avail_cols.file]         = profile->getPath();
        row[_avail_cols.name]         = profile->getName();
        row[_avail_cols.is_separator] = false;
    }
}

void ColorSystemPanel::link_selected_profile() {
    if (!_document) return;

    auto iter = _avail_combo.get_active();
    if (!iter) return;

    Glib::ustring file = (*iter)[_avail_cols.file];
    _document->getDocumentCMS().attachProfileToDoc(
        file, ColorProfileStorage::HREF_FILE, Colors::RenderingIntent::AUTO);
    DocumentUndo::done(_document, RC_("Undo", "Link Color Profile"), "");
    update(_document);
}

void ColorSystemPanel::remove_selected_profile() {
    if (!_document) return;

    Glib::ustring name;
    if (auto sel = _linked_view.get_selection()) {
        auto it = sel->get_selected();
        if (it) name = (*it)[_linked_cols.name];
    }
    if (name.empty()) return;

    if (auto cp = _document->getDocumentCMS().getColorProfileForSpace(name)) {
        cp->deleteObject(true, false);
        DocumentUndo::done(_document, RC_("Undo", "Remove linked color profile"), "");
    }
    update(_document);
    on_profile_select_row();
}

void ColorSystemPanel::on_profile_select_row() {
    if (auto sel = _linked_view.get_selection())
        _unlink_btn.set_sensitive(sel->count_selected_rows() > 0);
}

bool ColorSystemPanel::available_profiles_separator(
    Glib::RefPtr<Gtk::TreeModel> const& /*model*/,
    Gtk::TreeModel::const_iterator const& iter)
{
    return (*iter)[_avail_cols.is_separator];
}

} // namespace Inkscape::UI::Widget
