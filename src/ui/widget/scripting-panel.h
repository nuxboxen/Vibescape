// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Scripting panel — external and embedded SVG scripts.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_SCRIPTING_PANEL_H
#define INKSCAPE_UI_WIDGET_SCRIPTING_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stack.h>
#include <gtkmm/textview.h>
#include <gtkmm/treeview.h>
#include <gtkmm/label.h>
#include "ui/widget/generic/tab-strip.h"

#include "xml/helper-observer.h"

class SPDocument;
class SPDesktop;

namespace Inkscape::UI::Widget {

/**
 * Passive panel for external and embedded SVG script management.
 *
 * update(SPDocument*) reloads the script lists from the document.
 */
class ScriptingPanel : public Gtk::Box {
public:
    ScriptingPanel();
    ~ScriptingPanel() override;

    // desktop to use for file chooser only
    void set_desktop(SPDesktop* desktop);

    void update(SPDocument* document);

private:
    void populate_lists(SPDocument* document);

    void add_external_script();
    void remove_external_script();
    void add_embedded_script();
    void remove_embedded_script();
    void on_external_script_selected();
    void on_embedded_script_selected();
    void on_embedded_cursor_changed();
    void edit_embedded_script();
    void browse_external_script();

    Glib::RefPtr<Gtk::Builder> _builder;
    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;

    Inkscape::UI::Widget::TabStrip& _tab_strip;
    Gtk::Stack& _stack;
    Gtk::Box& _external_page;
    Gtk::Box& _embedded_page;

    // External scripts tab widgets
    Gtk::Label& _external_label;
    Gtk::ScrolledWindow& _external_scroller;
    Gtk::TreeView& _external_view;
    Gtk::Entry& _external_entry;
    Gtk::Button& _external_add_btn;
    Gtk::Button& _external_remove_btn;

    // Embedded scripts tab widgets
    Gtk::Label& _embedded_label;
    Gtk::ScrolledWindow& _embedded_scroller;
    Gtk::TreeView& _embedded_view;
    Gtk::TextView& _content_view;
    Gtk::ScrolledWindow& _content_scroller;
    Gtk::Button& _embedded_new_btn;
    Gtk::Button& _embedded_remove_btn;

    // External scripts tab
    class ExternalColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ExternalColumns() { add(filename); }
        Gtk::TreeModelColumn<Glib::ustring> filename;
    };
    ExternalColumns              _ext_cols;
    Glib::RefPtr<Gtk::ListStore> _ext_store;

    // Embedded scripts tab
    class EmbeddedColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        EmbeddedColumns() { add(id); }
        Gtk::TreeModelColumn<Glib::ustring> id;
    };
    EmbeddedColumns              _emb_cols;
    Glib::RefPtr<Gtk::ListStore> _emb_store;

    Inkscape::XML::SignalObserver _scripts_observer;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_SCRIPTING_PANEL_H
