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
#include "object/sp-paint-server.h"
#include "style-enums.h"
#include "ui/widget/paint-enums.h"
#include "mixed-property.h"

class SPItem;
class SPGradient;
class SPLinearGradient;
class SPRadialGradient;
class SPMeshGradient;
class SPPattern;
class SPHatch;
class SPPaintServer;

// This file defines a helper function for querying style properties from either
// a range of SPItem* or a single SPItem*.
// Only a subset of styles is queried and returned in the StyleProperties struct.
// The function can detect if styles are the same or mixed, and reports them accordingly.

namespace Inkscape {

// Paint value type for a single fill or stroke attribute, with all the
// possible combinations of paint types decoded.
struct PaintProp {
    UI::Widget::PaintMode mode = UI::Widget::PaintMode::None;

    // Solid: flat color value.
    std::optional<Colors::Color> color;

    // Paint server (gradient, mesh, pattern, hatch, etc.).
    // Below are decoded values, so one will be set.
    SPPaintServer* server = nullptr;

    // Gradient: linear or radial gradient.
    SPGradient* gradient = nullptr;
    // Possible selected stop of a gradient, comes from paint tag, not the server.
    SPStop* selected_stop = nullptr;

    // Swatch: the swatch vector gradient.
    SPGradient* swatch = nullptr;

    // Mesh: the mesh gradient array.
    SPMeshGradient* mesh = nullptr;

    // Pattern or Hatch
    SPPattern* pattern = nullptr;
    SPHatch*   hatch   = nullptr;

    // Derived: how the paint is inherited, if it is inherited
    std::optional<UI::Widget::PaintDerivedMode> derived_mode;

    bool operator == (const PaintProp&) const = default;
};

// Value type for stroke-width: width in px plus hairline flag.
struct StrokeWidthProp {
    double value    = 1.0;
    bool   hairline = false;
    bool operator == (const StrokeWidthProp&) const = default;
};

// Value type for stroke-dasharray + dashoffset.
struct StrokeDashProp {
    std::vector<double> dashes;
    double offset = 0.0;
    bool operator==(const StrokeDashProp&) const = default;
};

// Resolved style property values across one or more items.
// For mixed properties, the stored value is the first encountered value.
struct StyleProperties {
    // --- fill ---
    mixed_property<PaintProp>  fill;
    mixed_property<double>     fill_opacity{1.0};
    mixed_property<SPWindRule> fill_rule{SP_WIND_RULE_NONZERO};

    // --- stroke ---
    mixed_property<PaintProp> stroke;
    mixed_property<double>    stroke_opacity{1.0};

    // --- stroke geometry ---
    mixed_property<StrokeWidthProp> stroke_width; // in px (document units, before item transform)
    mixed_property<int>            stroke_linecap{SP_STROKE_LINECAP_BUTT};
    mixed_property<int>            stroke_linejoin{SP_STROKE_LINEJOIN_MITER};
    mixed_property<double>         stroke_miterlimit{4.0};
    mixed_property<StrokeDashProp>  stroke_dash;

    // --- markers ---
    mixed_property<std::string> marker_start;
    mixed_property<std::string> marker_mid;
    mixed_property<std::string> marker_end;

    // --- paint order ---
    mixed_property<std::string> paint_order;  // CSS string e.g. "stroke fill markers"

    // --- opacity & blend ---
    mixed_property<double> opacity{1.0};
    mixed_property<int>    blend_mode{SP_CSS_BLEND_NORMAL};

    // --- visibility ---
    mixed_property<bool>   visibility{false};  // true = hidden
};

// Flags controlling which items are visited during iteration.
enum class StyleQueryFlags : unsigned {
    None         = 0x00,   // only each item itself
    EnterGroups  = 0x01,   // recurse into SPGroup children
    EnterText    = 0x02,   // recurse into SPText/SPFlowtext children (TSpans etc.)
};

inline StyleQueryFlags operator|(StyleQueryFlags a, StyleQueryFlags b) {
    return static_cast<StyleQueryFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
inline bool operator&(StyleQueryFlags a, StyleQueryFlags b) {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

namespace detail {
// Implemented in style-utils.cpp; visits leaf items and merges their style into props.
void query_style_impl(StyleProperties& props, SPItem* item, StyleQueryFlags flags);
} // namespace detail

// Query style properties from a single item.
inline StyleProperties query_style_properties(SPItem* item,
                                              StyleQueryFlags flags = StyleQueryFlags::None) {
    StyleProperties props;
    detail::query_style_impl(props, item, flags);
    return props;
}

// Query style properties from any range of SPItem* (e.g. ObjectSet::items(), a vector, a span).
template<std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_value_t<Range>, SPItem*>
StyleProperties query_style_properties(Range&& items,
                                       StyleQueryFlags flags = StyleQueryFlags::None) {
    StyleProperties props;
    for (SPItem* item : items) {
        detail::query_style_impl(props, item, flags);
    }
    return props;
}

} // namespace Inkscape

#endif // INKSCAPE_STYLE_UTILS_H
