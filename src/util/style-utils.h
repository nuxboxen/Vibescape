// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#ifndef INKSCAPE_STYLE_UTILS_H
#define INKSCAPE_STYLE_UTILS_H

#include <optional>
#include <ranges>
#include <string>
#include <vector>
#include "colors/color.h"
#include "style-enums.h"
#include "ui/widget/paint-enums.h"

class SPItem;
class SPGradient;
class SPLinearGradient;
class SPRadialGradient;
class SPMeshGradient;
class SPPattern;
class SPHatch;

// This file defines a helper function for querying style properties from either
// a range of SPItem* or a single SPItem*.
// Only a subset of styles is queried and returned in the StyleProperties struct.
// The function can detect if styles are the same or mixed, and reports them accordingly.

namespace Inkscape {

// Reuse PropState from text-utils — same semantics.
// Unset: no items visited yet; Single: all items agree; Mixed: items differ.
enum class StylePropState { Unset, Single, Mixed };

// Paint state for a single fill or stroke attribute, with all the
// possible combinations of paint types.
struct PaintProp {
    UI::Widget::PaintMode mode = UI::Widget::PaintMode::None;

    // Solid: flat color value.
    std::optional<Colors::Color> color;

    // Gradient: exactly one of these is non-null.
    SPLinearGradient* linear = nullptr;
    SPRadialGradient* radial = nullptr;

    // Swatch: the swatch vector gradient.
    SPGradient* swatch = nullptr;

    // Mesh: the mesh gradient array.
    SPMeshGradient* mesh = nullptr;

    // Pattern or Hatch: the selected server (cast as needed by caller).
    SPPattern* pattern = nullptr;
    SPHatch*   hatch   = nullptr;

    // Derived: how the paint is inherited.
    UI::Widget::PaintDerivedMode derived_mode = UI::Widget::PaintDerivedMode::Unset;

    StylePropState state = StylePropState::Unset;

    bool operator == (const PaintProp& o) const {
        return mode            == o.mode
            && color           == o.color
            && linear          == o.linear
            && radial          == o.radial
            && swatch          == o.swatch
            && mesh            == o.mesh
            && pattern         == o.pattern
            && hatch           == o.hatch
            && derived_mode    == o.derived_mode;
    }
    bool operator != (const PaintProp& o) const { return !(*this == o); }
};

// Resolved style property values across one or more items.
// For mixed properties, the value field holds the first encountered value.
struct StyleProperties {
    // --- fill ---
    PaintProp fill;
    struct { double value = 1.0; StylePropState state = StylePropState::Unset; } fill_opacity;
    struct { int value = 0; StylePropState state = StylePropState::Unset; } fill_rule; // SP_WIND_RULE_*

    // --- stroke ---
    PaintProp stroke;
    struct { double value = 1.0; StylePropState state = StylePropState::Unset; } stroke_opacity;

    // --- stroke geometry ---
    struct { double value = 1.0; bool hairline = false; StylePropState state = StylePropState::Unset; } stroke_width; // in px (document units, before item transform)
    struct { int value = SP_STROKE_LINECAP_BUTT; StylePropState state = StylePropState::Unset; } stroke_linecap;
    struct { int value = SP_STROKE_LINEJOIN_MITER; StylePropState state = StylePropState::Unset; } stroke_linejoin;
    struct { double value = 4.0; StylePropState state = StylePropState::Unset; } stroke_miterlimit;
    struct { std::vector<double> dashes; double offset = 0.0; StylePropState state = StylePropState::Unset; } stroke_dash;

    // --- markers ---
    struct { std::string uri; StylePropState state = StylePropState::Unset; } marker_start;
    struct { std::string uri; StylePropState state = StylePropState::Unset; } marker_mid;
    struct { std::string uri; StylePropState state = StylePropState::Unset; } marker_end;

    // --- paint order ---
    struct { std::string value; StylePropState state = StylePropState::Unset; } paint_order; // CSS string e.g. "stroke fill markers"

    // --- opacity & blend ---
    struct { double value = 1.0; StylePropState state = StylePropState::Unset; } opacity;
    struct { int value = SP_CSS_BLEND_NORMAL; StylePropState state = StylePropState::Unset; } blend_mode;

    // --- visibility ---
    struct { bool hidden = false; StylePropState state = StylePropState::Unset; } visibility;
};

// Flags controlling which items are visited during iteration.
enum class StyleQueryFlags : unsigned {
    None         = 0,   // only each item itself
    EnterGroups  = 1 << 0,  // recurse into SPGroup children
    EnterText    = 1 << 1,  // recurse into SPText/SPFlowtext children (tspans etc.)
};

inline StyleQueryFlags operator|(StyleQueryFlags a, StyleQueryFlags b) {
    return static_cast<StyleQueryFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
inline bool operator&(StyleQueryFlags a, StyleQueryFlags b) {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

namespace detail {
// Implemented in style-utils.cpp; visits leaf items and merges their style into props.
void query_style_impl(StyleProperties& props, bool& first,
                      SPItem* item, StyleQueryFlags flags);
} // namespace detail

// Query style properties from a single item.
inline StyleProperties query_style_properties(SPItem* item,
                                              StyleQueryFlags flags = StyleQueryFlags::None) {
    StyleProperties props;
    bool first = true;
    detail::query_style_impl(props, first, item, flags);
    return props;
}

// Query style properties from any range of SPItem* (e.g. ObjectSet::items(), a vector, a span).
template<std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, SPItem*>
StyleProperties query_style_properties(Range&& items,
                                       StyleQueryFlags flags = StyleQueryFlags::None) {
    StyleProperties props;
    bool first = true;
    for (SPItem* item : items) {
        detail::query_style_impl(props, first, item, flags);
    }
    return props;
}

} // namespace Inkscape

#endif // INKSCAPE_STYLE_UTILS_H
