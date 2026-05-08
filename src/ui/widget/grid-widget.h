// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GridWidget — per-grid settings row shown in the Grids panel.
 */
/*
 * Authors:
 *   Michael Kowalski
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_GRID_WIDGET_H
#define INKSCAPE_UI_WIDGET_GRID_WIDGET_H

#include <gtkmm/box.h>
#include <gtkmm/sizegroup.h>

#include "object/sp-grid.h"
#include "ui/operation-blocker.h"
#include "ui/widget/unit-tracker.h"

#include <sigc++/signal.h>

namespace Inkscape::XML { class Node; }
namespace Inkscape::Colors { class Color; }

namespace Gtk {
class Button;
class Grid;
class Label;
class MenuButton;
class Switch;
class ToggleButton;
class Builder;
class Popover;
class Entry;
class CheckButton;
class SizeGroup;
class Widget;
}

namespace Inkscape::UI::Widget {

class AlignmentSelector;
class ColorPicker;
class InkSpinButton;
class IconComboBox;
class DefocusTarget;

/**
 * Grid widget row for the grids panel.
 */
class GridWidget : public Gtk::Box {
public:
    GridWidget(SPGrid* grid, void* tag = nullptr, DefocusTarget* target = nullptr);
    ~GridWidget() override = default;

    // reset this GridWidget to accept and watch a new grid; expects valid pointer
    void set_grid(SPGrid* grid, void* tag = nullptr);

    SPGrid* get_grid() const { return _grid; }
    void* get_tag() const { return _tag; }

private:
    void update();
    void watch_grid();
    void connect_signals(DefocusTarget* target);
    void setup_popovers();
    void setup_subordinate_widgets();
    void update_subordinate_widgets(bool enabled);
    void update_spin_units();
    // Helper method to convert and set spin button values
    void set_spin_button_value(InkSpinButton& spin, double value_px);
    void setup_alignment_selector();
    XML::Node* repr() { return _grid->getRepr(); }

    Glib::RefPtr<Gtk::Builder> _builder;
    SPGrid* _grid = nullptr;
    void* _tag = nullptr;

    // Main widgets from UI file
    Gtk::Grid& _main;

    // Header widgets
    // Gtk::Box& _header_box;
    Gtk::Switch& _enabled_switch;
    Gtk::Label& _id_label;
    IconComboBox& _grid_type_dropdown;

    // Button widgets
    // Gtk::Box& _buttons_box;
    Gtk::ToggleButton& _visible_toggle;
    ColorPicker& _color_button;
    Gtk::Button& _delete_button;
    Gtk::MenuButton& _options_button;

    // Input widgets
    UnitTracker _tracker{UnitType::UNIT_TYPE_LINEAR};
    InkSpinButton& _origin_x_spin;
    InkSpinButton& _origin_y_spin;
    InkSpinButton& _spacing_x_spin;
    InkSpinButton& _spacing_y_spin;
    InkSpinButton& _gap_x_spin;
    InkSpinButton& _gap_y_spin;
    InkSpinButton& _margin_x_spin;
    InkSpinButton& _margin_y_spin;
    InkSpinButton& _angle_x_spin;
    InkSpinButton& _angle_z_spin;
    InkSpinButton& _no_of_lines_spin;

    // Labels
    Gtk::Label& _origin_label;
    Gtk::Label& _spacing_label;
    Gtk::Label& _gap_label;
    Gtk::Label& _margin_label;
    Gtk::Label& _angle_label;
    Gtk::Label& _no_of_lines_label;
    Gtk::Label& _type_label;
    Gtk::Label& _color_label;

    // Special widgets
    Gtk::MenuButton& _align_button;
    Gtk::MenuButton& _angle_popup_button;
    Gtk::MenuButton& _units_button;

    // Popover content
    // Gtk::Popover* _options_popover = nullptr;
    // Gtk::Popover* _align_popover = nullptr;
    Gtk::Popover* _angle_popover = nullptr;
    Gtk::Entry* _aspect_ratio_entry = nullptr;
    Gtk::CheckButton* _snap_visible_check = nullptr;
    Gtk::CheckButton* _dotted_check = nullptr;
    AlignmentSelector* _alignment_selector = nullptr;

    // Size groups
    Glib::RefPtr<Gtk::SizeGroup> _first_row_height;
    Glib::RefPtr<Gtk::SizeGroup> _homogenous_column = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);

    // Subordinate widgets (disabled when grid is disabled)
    std::vector<Gtk::Widget*> _subordinate_widgets;

    // Signal connections
    sigc::scoped_connection _modified_signal;

    // Update blocking
    OperationBlocker _update;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_GRID_WIDGET_H
