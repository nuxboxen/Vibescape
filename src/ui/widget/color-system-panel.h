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

#ifndef INKSCAPE_UI_WIDGET_COLOR_SYSTEM_PANEL_H
#define INKSCAPE_UI_WIDGET_COLOR_SYSTEM_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/grid.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/label.h>
#include <sigc++/scoped_connection.h>

class SPDocument;

namespace Inkscape::UI::Widget {

/**
 * Passive panel for ICC color profile management.
 *
 * update(SPDocument*) refreshes the linked-profiles list.
 * The available-profiles combo is lazy-loaded on first reveal via
 * populate_available_profiles(), which callers should connect to an
 * appropriate "page shown" signal.
 */
class ColorSystemPanel : public Gtk::Box {
public:
    ColorSystemPanel();
    ~ColorSystemPanel() override = default;

    void set_document(SPDocument* document);
    void update(SPDocument* document);
    void populate_available_profiles(bool rebuild);

private:
    void link_selected_profile();
    void remove_selected_profile();
    void on_profile_select_row();
    bool available_profiles_separator(Glib::RefPtr<Gtk::TreeModel> const& model,
                                      Gtk::TreeModel::const_iterator const& iter);

    Glib::RefPtr<Gtk::Builder> _builder;
    SPDocument* _document = nullptr;

    Gtk::Label& _linked_label;
    Gtk::ScrolledWindow& _linked_scroller;
    Gtk::TreeView& _linked_view;
    Gtk::Label& _available_label;
    Gtk::ComboBox& _avail_combo;
    Gtk::Button& _unlink_btn;

    // Available profiles
    class AvailableColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        AvailableColumns() { add(file); add(name); add(is_separator); }
        Gtk::TreeModelColumn<Glib::ustring> file;
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<bool>          is_separator;
    };
    AvailableColumns                _avail_cols;
    Glib::RefPtr<Gtk::ListStore>    _avail_store;

    // Linked profiles
    class LinkedColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        LinkedColumns() { add(name); }
        Gtk::TreeModelColumn<Glib::ustring> name;
    };
    LinkedColumns                   _linked_cols;
    Glib::RefPtr<Gtk::ListStore>    _linked_store;

    sigc::scoped_connection _cms_connection;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_COLOR_SYSTEM_PANEL_H
