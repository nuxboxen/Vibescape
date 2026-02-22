// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#ifndef INKSCAPE_TEXT_UTILS_H
#define INKSCAPE_TEXT_UTILS_H

#include <glibmm/ustring.h>
#include <optional>
#include <pangomm/fontdescription.h>
#include <string>
#include <vector>

#include "colors/color.h"
#include "style-internal.h"
#include "util/units.h"
#include "mixed-property.h"

class SPCSSAttr;
class SPDesktop;
class SPItem;
class SPText;

namespace Inkscape::Util { class Unit; }

namespace Inkscape {

// Input:
// rtl - text direction right-to-left
// text_align - text alignment
// Output:
// index 0..3 of the button to highlight, where buttons are left, center, right, justify
//
int get_text_align_button_index(bool rtl, SPCSSTextAlign text_align);


// Value type for decoration thickness: value + auto/from-font flags.
struct DecorationThicknessProp {
    double value = 0;
    bool auto_val = true;
    bool from_font = false;
    bool operator==(const DecorationThicknessProp&) const = default;
};

// Resolved text property values with per-property mixed-state flags.
// For mixed properties, the value is from the first encountered style.
struct TextProperties {
    // numeric
    mixed_property<double> font_size{0}; // value in px
    mixed_property<double> line_height{0};
    mixed_property<int> line_height_unit{0};
    mixed_property<double> letter_spacing{0};
    mixed_property<double> word_spacing{0};
    // font identity
    mixed_property<Glib::ustring> font_family;
    mixed_property<Glib::ustring> font_style;
    mixed_property<SPIFontVariationSettings> font_variation;
    // enums
    mixed_property<int> text_align{-1};   // button index 0-3
    mixed_property<int> writing_mode{0};
    mixed_property<int> direction{0};
    mixed_property<int> text_orientation{0};
    // booleans / toggles
    mixed_property<bool> superscript;
    mixed_property<bool> subscript;
    mixed_property<bool> underline{false};
    mixed_property<bool> overline{false};
    mixed_property<bool> strikethrough{false};
    // decoration extras
    mixed_property<bool> decoration_spelling_error{false};
    mixed_property<int> decoration_style{0}; // 0=solid,1=double,2=dotted,3=dashed,4=wavy
    mixed_property<std::optional<Colors::Color>> decoration_color;
    mixed_property<DecorationThicknessProp> decoration_thickness;
};

// Query text properties from a list of items (tspans, flowparas, or text elements).
// First item's values are used as baseline; subsequent items flag Mixed if different.
TextProperties query_text_properties(const std::vector<SPItem*>& items);

namespace UI::Tools { class TextTool; }

// Resolve start/end text-align to left/right based on text direction.
SPCSSTextAlign text_align_to_side(SPCSSTextAlign align, SPCSSDirection direction);

// Apply text alignment to an SPText item: sets text-anchor + text-align CSS,
// adjusts the text anchor position to preserve the visual bounding box,
// and triggers a display update. Does NOT call DocumentUndo.
// Returns true if the text position was moved.
bool apply_text_alignment(SPText* text, int align_mode);

// Fill CSS attributes from a Pango font description: sets font-family, font-style,
// font-weight, font-stretch, font-variant and -inkscape-font-specification.
// Mirrors FontLister::fill_css but takes a FontDescription directly instead of going through FontLister.
void fill_css_from_font_description(SPCSSAttr* css, const Glib::ustring& family,
    const Pango::FontDescription& desc, const Glib::ustring& fontspec);

// Apply horizontal kerning (dx) at the text tool's cursor/selection position.
// Computes delta from current dx and calls sp_te_adjust_dx.
// Returns true if applied. Does NOT call DocumentUndo.
bool apply_text_dx(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_dx);

// Apply vertical kerning (dy) at the text tool's cursor/selection position.
// Computes delta from current dy and calls sp_te_adjust_dy.
// Returns true if applied. Does NOT call DocumentUndo.
bool apply_text_dy(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_dy);

// Query horizontal kerning (dx) at the text tool's cursor position.
// Returns the dx value, or nullopt if unavailable.
std::optional<double> query_text_dx(UI::Tools::TextTool* tool);

// Query vertical kerning (dy) at the text tool's cursor position.
// Returns the dy value, or nullopt if unavailable.
std::optional<double> query_text_dy(UI::Tools::TextTool* tool);

// Apply character rotation at the text tool's cursor/selection position.
// Computes delta from current rotation and calls sp_te_adjust_rotation.
// Returns true if rotation was applied. Does NOT call DocumentUndo.
bool apply_text_char_rotation(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_degrees);

// Query character rotation at the text tool's cursor position.
// Returns the rotation in degrees (-180..180), or nullopt if unavailable.
std::optional<double> query_text_char_rotation(UI::Tools::TextTool* tool);

// Apply CSS to text: if text tool has a subselection, apply to that range via sp_te_apply_style;
// otherwise apply recursively to the text item. Does NOT call DocumentUndo — caller is
// responsible for maybeDone with a per-property undo key.
void apply_text_css(SPItem* text_item, UI::Tools::TextTool* tool, SPCSSAttr* css);

// --- Unit helpers for font-size / line-height ---

// True if unit is relative (unitless/em/ex/%).
bool is_relative_unit(Util::Unit const *unit);
bool is_relative_unit(int css_unit);

// Convert a Unit abbreviation to SP_CSS_UNIT_xx.
int unit_to_css_unit(Util::Unit const *unit);

// Convert a line-height value between old and new units.
// avg_font_size is needed for relative ↔ absolute conversion (in px).
double convert_lineheight_between_units(double value, int old_css_unit,
                                        Util::Unit const *new_unit, double avg_font_size);

// Format a line-height value + unit as a CSS string suitable for sp_repr_css_set_property.
// Relative units emit "value + abbr"; absolute units convert to px.
std::string format_line_height_css(double value, Util::Unit const *unit);


// Return all text spans in a text object
std::vector<SPItem*> get_all_text_spans(SPItem* text);

} // namespace

#endif //INKSCAPE_TEXT_UTILS_H
