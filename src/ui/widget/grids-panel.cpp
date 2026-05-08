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

#include "grids-panel.h"

#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/widget.h>
#include <gtkmm/grid.h>

#include "document.h"
#include "document-undo.h"
#include "inkscape-window.h"
#include "object/sp-grid.h"
#include "object/sp-namedview.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/widget/grid-widget.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

namespace {

void create_new_grid(SPNamedView* namedview) {
    if (!namedview || !namedview->document) return;

    auto repr = namedview->getRepr();
    SPGrid::create_new(namedview->document, repr, GridType::RECTANGULAR);

    // flip global switch, so snapping to grid works
    namedview->newGridCreated();

    Inkscape::DocumentUndo::done(namedview->document, RC_("Undo", "Create new grid"), INKSCAPE_ICON("document-properties"));
}

} // anonymous namespace

namespace Inkscape::UI::Widget {

GridsPanel::GridsPanel(DefocusTarget* target)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , _builder(create_builder("grids-panel.ui"))
    , _new_grid(get_widget<Gtk::Button>(_builder, "new-grid"))
    , _no_grids(get_widget<Gtk::Label>(_builder, "no_grids"))
    , _scroller(get_widget<Gtk::ScrolledWindow>(_builder, "scroller"))
    , _list(get_widget<Gtk::ListBox>(_builder, "list"))
    , _target(target)
{
    auto& main_box = get_widget<Gtk::Grid>(_builder, "grids-panel");
    append(main_box);

    // Bind the new grid button click signal
    _new_grid.signal_clicked().connect([this]() {
        create_new_grid(_namedview);
        scroll_to_bottom();
    });

    update_placeholder();
}

void GridsPanel::set_namedview(SPNamedView* namedview) {
    _namedview = namedview;
}

// Update grids avoiding unnecessary UI rebuilds;
// Note: we do not update individual GridWidgets here; they watch their SPGrids already
void GridsPanel::update(SPNamedView* namedview) {
    if (!namedview) {
        // pathological case; namedview is gone
        for (auto w : _size_group->get_widgets()) {
            _size_group->remove_widget(*w);
        }
        _list.remove_all();
        return;
    }

    // Get first child row
    auto child_it = _list.get_first_child();
    size_t grid_index = 0;

    // Sync existing widgets or add new ones
    const auto& current_grids = namedview->grids;
    while (grid_index < current_grids.size()) {
        if (child_it) {
            // Update existing widget if it watches different grid
            auto& row = dynamic_cast<Gtk::ListBoxRow&>(*child_it);
            auto& widget = dynamic_cast<GridWidget&>(*row.get_child());
            if (widget.get_grid() != current_grids[grid_index]) {
                widget.set_grid(current_grids[grid_index]);
            }
            child_it = child_it->get_next_sibling();
        }
        else {
            // Add new widget
            add_grid(current_grids[grid_index]);
        }
        grid_index++;
    }

    // Remove excess widgets
    while (child_it) {
        auto next_child = child_it->get_next_sibling();
        auto& row = dynamic_cast<Gtk::ListBoxRow&>(*child_it);
        auto& widget = dynamic_cast<GridWidget&>(*row.get_child());
        _size_group->remove_widget(widget);
        _list.remove(row);
        child_it = next_child;
    }

    update_placeholder();
}

void GridsPanel::add_grid(SPGrid* grid) {
    if (!grid) return;

    auto widget = Gtk::make_managed<GridWidget>(grid);
    widget->set_margin_bottom(12);
    _list.append(*widget);
    _size_group->add_widget(*widget);

    // Remove activatable highlight from all rows
    int index = 0;
    for (auto row = _list.get_row_at_index(index); row;
         row = _list.get_row_at_index(++index)) {
        row->property_activatable() = false;
    }

    update_placeholder();
}

// void GridsPanel::remove_grid(Inkscape::XML::Node& node) {
//     int index = 0;
//     for (auto row = _list.get_row_at_index(index); row;
//          row = _list.get_row_at_index(++index)) {
//         if (auto widget = dynamic_cast<GridWidget*>(row->get_child())) {
//             if (&node == widget->get_grid()->getRepr()) {
//                 _size_group->remove_widget(*widget);
//                 _list.remove(*row);
//                 break;
//             }
//         }
//     }
//     update_placeholder();
// }

void GridsPanel::update_placeholder() {
    _no_grids.set_visible(_list.get_first_child() == nullptr);
}

void GridsPanel::scroll_to_bottom() {
    // scroll to the last (newly added) grid, so we can see it; postponed till idle time, since scrolling
    // range is not yet updated, despite new grid UI being in place already
    _on_idle_scroll = Glib::signal_idle().connect([this]() {
        if (auto adj = _scroller.get_vadjustment()) {
            adj->set_value(adj->get_upper());
        }
        return false;
    });
}

} // namespace Inkscape::UI::Widget
