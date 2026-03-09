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

#include "grid-widget.h"

#include <array>
#include <cmath>
#include <giomm/themedicon.h>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/colorbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/popover.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/switch.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/widget.h>

#include "document.h"
#include "document-undo.h"
#include "object/sp-grid.h"
#include "snapper.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/util.h"
#include "ui/widget/alignment-selector.h"
#include "ui/widget/color-picker.h"
#include "ui/widget/generic/icon-combobox.h"
#include "ui/widget/generic/spin-button.h"
#include "util/expression-evaluator.h"
#include "util-string/context-string.h"
#include "util/units.h"
#include "xml/node.h"

using Inkscape::UI::create_builder;
using Inkscape::UI::get_derived_widget;

namespace Inkscape::UI::Widget {

namespace {

// Grid widget limits
constexpr double GRID_WIDGET_MIN_VALUE = -1e6;
constexpr double GRID_WIDGET_MAX_VALUE = 1e6;

static const auto grid_types = std::to_array({std::tuple
    {C_("Grid", "Rectangular"), GridType::RECTANGULAR, "grid-rectangular"},
    {C_("Grid", "Axonometric"), GridType::AXONOMETRIC, "grid-axonometric"},
    {C_("Grid", "Modular"),     GridType::MODULAR,     "grid-modular"}
});

// Helper functions for widget operations
void set_widget_color(Gtk::ColorButton& button, const Colors::Color& color) {
    auto rgba = color.toRGBA();
    Gdk::RGBA gdk_color;
    gdk_color.set_red(SP_RGBA32_R_F(rgba));
    gdk_color.set_green(SP_RGBA32_G_F(rgba));
    gdk_color.set_blue(SP_RGBA32_B_F(rgba));
    gdk_color.set_alpha(SP_RGBA32_A_F(rgba));
    button.set_rgba(gdk_color);
}

Colors::Color get_widget_color(const Gtk::ColorButton& button) {
    auto rgba = button.get_rgba();
    // Create an RGB color from the GdkRGBA values
    std::vector<double> rgb_values = {
        rgba.get_red(), rgba.get_green(), rgba.get_blue(), rgba.get_alpha()
    };
    return Colors::Color(Colors::Space::Type::RGB, rgb_values);
}

// Grid property helpers
void set_grid_property(XML::Node* repr, const char* name, const char* value) {
    repr->setAttribute(name, value);
}

void set_grid_property_bool(XML::Node* repr, const char* name, bool value) {
    repr->setAttributeBoolean(name, value);
}

void set_grid_property_double(XML::Node* repr, const char* name, double value) {
    repr->setAttributeCssDouble(name, value);
}

const char* get_grid_property(XML::Node* repr, const char* name) {
    return repr->attribute(name);
}

} // anonymous namespace

GridWidget::GridWidget(SPGrid* grid, void* tag, DefocusTarget* target)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , _builder(create_builder("grid-widget.ui"))
    , _grid(grid)
    , _tag(tag)

    // Main widgets from UI file
    , _main(get_widget<Gtk::Grid>(_builder, "main"))

    // Header widgets
    , _enabled_switch(get_widget<Gtk::Switch>(_builder, "enabled-switch"))
    , _id_label(get_widget<Gtk::Label>(_builder, "id-label"))
    , _grid_type_dropdown(get_derived_widget<IconComboBox>(_builder, "grid-type-dropdown"))

    // Button widgets
    , _visible_toggle(get_widget<Gtk::ToggleButton>(_builder, "visible-toggle"))
    , _color_button(get_derived_widget<ColorPicker>(_builder, "color-button", _("Grid color")))
    , _delete_button(get_widget<Gtk::Button>(_builder, "delete-button"))
    , _options_button(get_widget<Gtk::MenuButton>(_builder, "options-button"))

    // Input widgets
    // , _units_dropdown(get_widget<Gtk::DropDown>(_builder, "units_dropdown"))
    , _origin_x_spin(get_widget<InkSpinButton>(_builder, "origin-x-spin"))
    , _origin_y_spin(get_widget<InkSpinButton>(_builder, "origin-y-spin"))
    , _spacing_x_spin(get_widget<InkSpinButton>(_builder, "spacing-x-spin"))
    , _spacing_y_spin(get_widget<InkSpinButton>(_builder, "spacing-y-spin"))
    , _gap_x_spin(get_widget<InkSpinButton>(_builder, "gap-x-spin"))
    , _gap_y_spin(get_widget<InkSpinButton>(_builder, "gap-y-spin"))
    , _margin_x_spin(get_widget<InkSpinButton>(_builder, "margin-x-spin"))
    , _margin_y_spin(get_widget<InkSpinButton>(_builder, "margin-y-spin"))
    , _angle_x_spin(get_widget<InkSpinButton>(_builder, "angle-x-spin"))
    , _angle_z_spin(get_widget<InkSpinButton>(_builder, "angle-z-spin"))
    , _no_of_lines_spin(get_widget<InkSpinButton>(_builder, "no-of-lines-spin"))

    // Labels
    , _origin_label(get_widget<Gtk::Label>(_builder, "origin-label"))
    , _spacing_label(get_widget<Gtk::Label>(_builder, "spacing-label"))
    , _gap_label(get_widget<Gtk::Label>(_builder, "gap-label"))
    , _margin_label(get_widget<Gtk::Label>(_builder, "margin-label"))
    , _angle_label(get_widget<Gtk::Label>(_builder, "angle-label"))
    , _no_of_lines_label(get_widget<Gtk::Label>(_builder, "no-of-lines-label"))
    , _type_label(get_widget<Gtk::Label>(_builder, "type-label"))
    , _color_label(get_widget<Gtk::Label>(_builder, "color-label"))

    // Special widgets
    , _align_button(get_widget<Gtk::MenuButton>(_builder, "align-button"))
    , _angle_popup_button(get_widget<Gtk::MenuButton>(_builder, "angle-popup-button"))
    , _units_button(get_widget<Gtk::MenuButton>(_builder, "units-button"))
{
    append(_main);

    _first_row_height = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::VERTICAL);

    setup_popovers();
    setup_subordinate_widgets();
    connect_signals(target);
    update();
}

void GridWidget::setup_popovers() {
    _snap_visible_check = &get_widget<Gtk::CheckButton>(_builder, "snap-visible-check");
    _dotted_check = &get_widget<Gtk::CheckButton>(_builder, "dotted-check");

    // Load alignment selector from UI file
    _alignment_selector = &get_derived_widget<AlignmentSelector>(_builder, "alignment-selector");

    // Load angle popover from UI file
    _angle_popover = &get_widget<Gtk::Popover>(_builder, "angle-popover");
    _aspect_ratio_entry = &get_widget<Gtk::Entry>(_builder, "aspect-ratio-entry");
    auto apply_button = &get_widget<Gtk::Button>(_builder, "apply-angle-button");

    // Connect popover signals
    apply_button->signal_clicked().connect([this]() {
        try {
            auto const result = Inkscape::Util::ExpressionEvaluator{_aspect_ratio_entry->get_text().c_str()}.evaluate().value;
            if (!std::isfinite(result) || result <= 0) return;
            auto ang = Geom::deg_from_rad(std::atan(1.0 / result));
            if (ang > 0.0 && ang < 90.0) {
                _angle_x_spin.set_value(ang);
                _angle_z_spin.set_value(ang);
                if (_grid && _grid->document) {
                    DocumentUndo::done(_grid->document, RC_("Undo", "Change grid dimensions"), "grid-angle");
                }
            }
        }
        catch (Inkscape::Util::EvaluatorException&) {
            // Ignore invalid expressions
        }
    });

    _angle_popover->signal_show().connect([this]() {
        if (!_grid) return;

        auto ax = _angle_x_spin.get_value();
        auto az = _angle_z_spin.get_value();
        if (az == ax) {
            auto ratio = std::tan(Geom::rad_from_deg(ax));
            if (ratio > 0) {
                _aspect_ratio_entry->set_text(ratio > 1.0 ?
                    Glib::ustring::format("1 : ", ratio) :
                    Glib::ustring::format(1.0 / ratio, " : 1"));
            }
        }
    });
}

void GridWidget::setup_subordinate_widgets() {
    // Collect all widgets that should be disabled when the grid is disabled
    _subordinate_widgets = {
        // Input widgets
        &_origin_x_spin, &_origin_y_spin,
        &_spacing_x_spin, &_spacing_y_spin,
        &_gap_x_spin, &_gap_y_spin,
        &_margin_x_spin, &_margin_y_spin,
        &_angle_x_spin, &_angle_z_spin,
        &_no_of_lines_spin,

        // Labels
        &_origin_label, &_spacing_label, &_gap_label,
        &_margin_label, &_angle_label, &_no_of_lines_label,
        &_type_label, &_color_label,

        // Header widgets (except ID label which should remain visible)
        &_grid_type_dropdown,

        // Button widgets (except delete button)
        &_visible_toggle, &_color_button, &_options_button,
        &_align_button, &_angle_popup_button, &_units_button,

        // Special widgets
        _snap_visible_check, _dotted_check,
        _alignment_selector,
    };

    // Connect the enabled switch to control subordinate widgets
    _enabled_switch.property_active().signal_changed().connect([this]() {
        update_subordinate_widgets(_enabled_switch.get_active());
    });

    // Set initial state
    update_subordinate_widgets(_enabled_switch.get_active());
}

void GridWidget::update_subordinate_widgets(bool enabled) {
    for (auto* widget : _subordinate_widgets) {
        widget->set_sensitive(enabled);
    }
}

void GridWidget::connect_signals(DefocusTarget* target) {
    // Grid type dropdown
    for (auto const& [label, type, icon] : grid_types) {
        _grid_type_dropdown.add_row(icon, label, static_cast<int>(type));
    }
    _grid_type_dropdown.refilter();

    _grid_type_dropdown.signal_changed().connect([this](int index) {
        if (_update.pending() || index < 0) return;

        _grid->setType(std::get<1>(grid_types.at(index)));
        update();
    });

    // Delete button
    _delete_button.signal_clicked().connect([this]() {
        auto doc = get_grid()->document;
        get_grid()->deleteObject();
        DocumentUndo::done(doc, RC_("Undo", "Remove grid"), INKSCAPE_ICON("document-properties"));
    });

    // Widget value changes
    _enabled_switch.property_active().signal_changed().connect([this]() {
        if (_update.pending()) return;

        bool enabled = _enabled_switch.get_active();
        set_grid_property_bool(repr(), "enabled", enabled);
        if (_grid && _grid->document) {
            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid enabled state"), "grid-enabled");
        }
    });

    _visible_toggle.signal_toggled().connect([this]() {
        if (_update.pending()) return;

        bool visible = _visible_toggle.get_active();
        set_grid_property_bool(repr(), "visible", visible);
        if (_grid && _grid->document) {
            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid visibility"), "grid-visible");
        }
    });

    // Spin button changes and init
    auto connect_spin = [this, target](InkSpinButton& spin, const char* prop, bool positive = false, bool unitless = false) {
        // Apply the same limits
        auto lower = positive ? 0 : GRID_WIDGET_MIN_VALUE;
        spin.set_range(lower, GRID_WIDGET_MAX_VALUE);
        spin.set_digits(unitless ? 0 : 6);
        _homogenous_column->add_widget(spin);

        spin.setDefocusTarget(target);

        spin.signal_value_changed().connect([this, &spin, prop, unitless](double value) {
            if (_update.pending() || !_grid) return;

            auto value_in_px = value;
            if (!unitless) {
                // Get the current unit and convert value to document units (px)
                auto current_unit = _tracker.getActiveUnit();
                if (current_unit) {
                    // Convert from current unit to document units (px)
                    value_in_px = Inkscape::Util::Quantity::convert(value, current_unit, "px");
                }
            }
            set_grid_property_double(repr(), prop, value_in_px);

            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid dimensions"), "grid-dimensions");
        });
    };

    connect_spin(_origin_x_spin, "originx");
    connect_spin(_origin_y_spin, "originy");
    connect_spin(_spacing_x_spin, "spacingx", true);
    connect_spin(_spacing_y_spin, "spacingy", true);
    connect_spin(_angle_x_spin, "gridanglex");
    connect_spin(_angle_z_spin, "gridanglez");
    connect_spin(_no_of_lines_spin, "empspacing", true, true);
    connect_spin(_gap_x_spin, "gapx");
    connect_spin(_gap_y_spin, "gapy");
    connect_spin(_margin_x_spin, "marginx");
    connect_spin(_margin_y_spin, "marginy");

    // Color button
    _color_button.connectChanged([this](Colors::Color const& color) {
        if (_update.pending()) return;

        // Set both empcolor/empopacity and color/opacity
        set_grid_property(repr(), "empcolor", color.toString(false).c_str());
        repr()->setAttributeCssDouble("empopacity", color.getOpacity());
        auto color_with_opacity = color;
        color_with_opacity.addOpacity(0.5);
        set_grid_property(repr(), "color", color_with_opacity.toString(false).c_str());
        repr()->setAttributeCssDouble("opacity", color_with_opacity.getOpacity());
        if (_grid && _grid->document) {
            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid color"), "grid-color");
        }
    });

    // Check buttons in options popover
    _snap_visible_check->signal_toggled().connect([this]() {
        if (_update.pending()) return;

        bool active = _snap_visible_check->get_active();
        set_grid_property_bool(repr(), "snapvisiblegridlinesonly", active);
        if (_grid && _grid->document) {
            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid snap settings"), "grid-snap");
        }
    });

    _dotted_check->signal_toggled().connect([this]() {
        if (_update.pending()) return;

        bool active = _dotted_check->get_active();
        set_grid_property_bool(repr(), "dotted", active);
        if (_grid && _grid->document) {
            DocumentUndo::done(_grid->document, RC_("Undo", "Change grid dotted setting"), "grid-dotted");
        }
    });

    // Alignment selector
    _alignment_selector->connectAlignmentClicked([this](int align) {
        if (_update.pending() || !_grid) return;

        auto dimensions = _grid->document->getDimensions();
        dimensions[Geom::X] *= align % 3 * 0.5;
        dimensions[Geom::Y] *= align / 3 * 0.5;
        dimensions *= _grid->document->doc2dt();
        dimensions *= _grid->document->getDocumentScale().inverse();
        _grid->setOrigin(dimensions);
        update();
    });

    auto& units_popover = get_widget<Gtk::PopoverMenu>(_builder, "units");
    auto units_menu = _tracker.create_popover_unit_menu(units_popover);
    units_popover.set_menu_model(units_menu.menu);

    // Add spin button adjustments to unit tracker
    _tracker.addAdjustment(_origin_x_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_origin_y_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_spacing_x_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_spacing_y_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_gap_x_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_gap_y_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_margin_x_spin.get_adjustment()->gobj());
    _tracker.addAdjustment(_margin_y_spin.get_adjustment()->gobj());

    // Units dropdown
    _tracker.signal_unit_changed().connect([this](const Unit *unit) {
        if (_update.pending()) return;
        // Set the grid's unit when user changes units
        if (_grid && unit) {
            _grid->setUnit(unit->abbr);
            update();
        }
        update_spin_units();
    });
    _tracker.setActiveUnitByAbbr("px");
    update_spin_units();
    set_degree_suffix(_angle_x_spin);
    set_degree_suffix(_angle_z_spin);

    watch_grid();
}

void GridWidget::watch_grid() {
    // Grid modification signal
    _modified_signal = _grid->connectModified([this](const SPObject*, unsigned) {
        if (!_update.pending()) {
            _modified_signal.block();
            update();
            _modified_signal.unblock();
        }
    });
}

void GridWidget::set_grid(SPGrid* grid, void* tag) {
    assert(grid);

    _grid = grid;
    _tag = tag;
    watch_grid();

    update();
}

void GridWidget::update() {
    if (!_grid) return;

    auto block = _update.block();

    auto scale = _grid->document->getDocumentScale();

    // Set the active unit from the grid
    _tracker.setActiveUnit(_grid->getUnit());

    const auto modular     = _grid->getType() == GridType::MODULAR;
    const auto axonometric = _grid->getType() == GridType::AXONOMETRIC;
    const auto rectangular = _grid->getType() == GridType::RECTANGULAR;

    // Update grid type dropdown
    _grid_type_dropdown.set_active_by_id(static_cast<int>(_grid->getType()));

    // Update origin
    auto origin = _grid->getOrigin() * scale;
    set_spin_button_value(_origin_x_spin, origin[Geom::X]);
    set_spin_button_value(_origin_y_spin, origin[Geom::Y]);

    // Update spacing
    auto spacing = _grid->getSpacing() * scale;
    set_spin_button_value(_spacing_x_spin, spacing[Geom::X]);
    set_spin_button_value(_spacing_y_spin, spacing[Geom::Y]);

    // Update spacing label based on grid type
    _spacing_label.set_markup_with_mnemonic(modular ? _("Block size") : _("Spacing"));

    // Update visibility of widgets based on grid type
    _angle_x_spin.set_visible(axonometric);
    _angle_z_spin.set_visible(axonometric);
    _angle_label.set_visible(axonometric);
    _angle_popup_button.set_visible(axonometric);

    if (axonometric) {
        _angle_x_spin.set_value(_grid->getAngleX());
        _angle_z_spin.set_value(_grid->getAngleZ());
    }

    _gap_x_spin.set_visible(modular);
    _gap_y_spin.set_visible(modular);
    _gap_label.set_visible(modular);
    _margin_x_spin.set_visible(modular);
    _margin_y_spin.set_visible(modular);
    _margin_label.set_visible(modular);

    if (modular) {
        auto gap    = _grid->get_gap()    * scale;
        auto margin = _grid->get_margin() * scale;
        set_spin_button_value(_gap_x_spin, gap.x());
        set_spin_button_value(_gap_y_spin, gap.y());
        set_spin_button_value(_margin_x_spin, margin.x());
        set_spin_button_value(_margin_y_spin, margin.y());
    }

    // Update color
    _color_button.setColor(_grid->getMajorColor());

    _no_of_lines_spin.set_visible(!modular);
    _no_of_lines_label.set_visible(!modular);
    _no_of_lines_spin.set_value(_grid->getMajorLineInterval());

    // Update switches
    _enabled_switch.set_active(_grid->isEnabled());
    _visible_toggle.set_active(_grid->isVisible());

    if (_dotted_check) {
        _dotted_check->set_active(_grid->isDotted());
    }

    if (_snap_visible_check) {
        _snap_visible_check->set_active(_grid->getSnapToVisibleOnly());
    }

    // Update visibility based on grid type
    if (_dotted_check) {
        _dotted_check->set_visible(rectangular);
    }
    // _spacing_x_spin.set_visible(!axonometric);
    _spacing_y_spin.set_visible(!axonometric);

    // Update ID label
    auto id = _grid->getId() ? _grid->getId() : "-";
    _id_label.set_label(id);
    _id_label.set_tooltip_text(id);

}

void GridWidget::update_spin_units() {
    if (auto unit = _tracker.getActiveUnit()) {
        auto abbr = " " + unit->abbr;
        _spacing_x_spin.set_suffix(abbr, false);
        _spacing_y_spin.set_suffix(abbr, false);
        _gap_x_spin.set_suffix(abbr, false);
        _gap_y_spin.set_suffix(abbr, false);
        _margin_x_spin.set_suffix(abbr, false);
        _margin_y_spin.set_suffix(abbr, false);
        _origin_x_spin.set_suffix(abbr, false);
        _origin_y_spin.set_suffix(abbr, false);
    }
}

void GridWidget::set_spin_button_value(InkSpinButton& spin, double value_px) {
    auto current_unit = _tracker.getActiveUnit();
    if (current_unit) {
        // Convert from document units (px) to current display unit
        double value_display = Inkscape::Util::Quantity::convert(value_px, "px", current_unit);
        spin.set_value(value_display);
    } else {
        // Fallback: set raw values
        spin.set_value(value_px);
    }
}

} // namespace Inkscape::UI::Widget
