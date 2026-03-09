// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Grids panel — list of document grids with incremental add/remove.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_GRIDS_PANEL_H
#define INKSCAPE_UI_WIDGET_GRIDS_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/sizegroup.h>

class SPGrid;
class SPNamedView;

namespace Inkscape::XML { class Node; }

namespace Inkscape::UI::Widget {
class DefocusTarget;

/**
 * Passive panel that displays a list of document grids.
 *
 * Grids are added and removed incrementally via add_grid() and remove_grid()
 * rather than rebuilt in bulk.  set_namedview() must be called (once) so that
 * newly-created GridWidget rows can bind themselves to the named view.
 *
 * The "New Grid" button uses the doc.new-grid action so no desktop pointer
 * is needed here.
 */
class GridsPanel : public Gtk::Box {
public:
    GridsPanel(DefocusTarget* target = nullptr);
    ~GridsPanel() override = default;

    // pass NamedView; it will only be used to create a new grid
    void set_namedview(SPNamedView* namedview);

    // refresh UI
    void update(SPNamedView* namedview);

private:
    void update_placeholder();
    void scroll_to_bottom();
    void add_grid(SPGrid* grid);

    Glib::RefPtr<Gtk::Builder> _builder;
    SPNamedView* _namedview = nullptr;
    sigc::scoped_connection _on_idle_scroll;
    Gtk::Button& _new_grid;
    Gtk::Label& _no_grids;
    Gtk::ScrolledWindow& _scroller;
    Gtk::ListBox& _list;
    Glib::RefPtr<Gtk::SizeGroup> _size_group = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    DefocusTarget* _target = nullptr;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_GRIDS_PANEL_H
