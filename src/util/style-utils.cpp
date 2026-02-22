// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#include "style-utils.h"

#include <functional>
#include <string>
#include "object/sp-item.h"
#include "object/sp-item-group.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-radial-gradient.h"
#include "object/sp-mesh-gradient.h"
#include "object/sp-pattern.h"
#include "object/sp-hatch.h"
#include "object/sp-text.h"
#include "object/sp-flowtext.h"
#include "style-internal.h"
#include "style.h"
#include "ui/widget/paint-enums.h"

namespace Inkscape {

namespace {

using namespace UI::Widget;

// Classify a paint into a PaintProp, examining all paint combinations
// and setting the appropriate mode and associated data.
PaintProp classify_paint(const SPIPaint& paint) {
    PaintProp p;
    if (auto* server = paint.isPaintserver() ? paint.href->getObject() : nullptr) {
        if (auto* grad = cast<SPGradient>(server)) {
            auto* vec = grad->getVector();
            if (vec && vec->isSwatch()) {
                p.mode   = PaintMode::Swatch;
                p.swatch = vec;
            } else if (auto* lg = cast<SPLinearGradient>(server)) {
                p.mode   = PaintMode::Gradient;
                p.linear = lg;
            } else if (auto* rg = cast<SPRadialGradient>(server)) {
                p.mode   = PaintMode::Gradient;
                p.radial = rg;
            }
#ifdef WITH_MESH
            else if (auto* mg = cast<SPMeshGradient>(server)) {
                p.mode = PaintMode::Mesh;
                p.mesh = mg;
            }
#endif
        } else if (auto* pat = cast<SPPattern>(server)) {
            p.mode    = PaintMode::Pattern;
            p.pattern = pat;
        } else if (auto* hatch = cast<SPHatch>(server)) {
            p.mode  = PaintMode::Hatch;
            p.hatch = hatch;
        }
    } else if (paint.isColor() && paint.paintSource == SP_CSS_PAINT_ORIGIN_NORMAL) {
        p.mode  = PaintMode::Solid;
        p.color = paint.getColor();
    } else if (paint.isNone()) {
        p.mode = PaintMode::None;
    } else {
        p.mode = PaintMode::Derived;
        if (auto dm = get_inherited_paint_mode(paint)) {
            p.derived_mode = *dm;
        }
    }
    return p;
}

// Update a simple scalar property field: on a first call, set the value and state to Single;
// on subsequent calls, compare the value and set state to Mixed if different.
template<typename Field, typename T>
void merge_scalar(Field& field, T value, bool first) {
    if (first) {
        field.value = value;
        field.state = StylePropState::Single;
    } else if (field.state != StylePropState::Mixed && field.value != value) {
        field.state = StylePropState::Mixed;
    }
}

// Visit items recursively, invoking fn for each leaf item (no temp allocation).
void visit_items(SPItem* item, StyleQueryFlags flags, const std::function<void(SPItem*)>& fn) {
    if (!item) return;

    bool enter_group = (flags & StyleQueryFlags::EnterGroups) && is<SPGroup>(item);
    bool enter_text  = (flags & StyleQueryFlags::EnterText)
                       && (is<SPText>(item) || is<SPFlowtext>(item));

    if (enter_group || enter_text) {
        for (auto* child = item->firstChild(); child; child = child->getNext()) {
            if (auto* child_item = cast<SPItem>(child)) {
                visit_items(child_item, flags, fn);
            }
        }
    } else {
        fn(item);
    }
}

// Merge one item's style into props. first==true initialises all fields.
void merge_item_style(StyleProperties& props, SPItem* item, bool first) {
    auto* style = item->style;
    if (!style) return;

    // --- fill ---
    {
        auto cur = classify_paint(style->fill);
        if (first) {
            props.fill = cur;
            props.fill.state = StylePropState::Single;
        } else if (props.fill.state != StylePropState::Mixed && props.fill != cur) {
            props.fill.state = StylePropState::Mixed;
        }
    }

    // --- fill-opacity ---
    merge_scalar(props.fill_opacity, static_cast<double>(style->fill_opacity), first);

    // --- fill-rule ---
    merge_scalar(props.fill_rule, static_cast<int>(style->fill_rule.computed), first);

    // --- stroke ---
    {
        auto cur = classify_paint(style->stroke);
        if (first) {
            props.stroke = cur;
            props.stroke.state = StylePropState::Single;
        } else if (props.stroke.state != StylePropState::Mixed && props.stroke != cur) {
            props.stroke.state = StylePropState::Mixed;
        }
    }

    // --- stroke-opacity ---
    merge_scalar(props.stroke_opacity, static_cast<double>(style->stroke_opacity), first);

    // --- stroke-width (in px, before item transform) ---
    {
        bool hairline = style->stroke_extensions.hairline;
        double width = hairline ? 0.0 : style->stroke_width.computed;
        if (first) {
            props.stroke_width.value   = width;
            props.stroke_width.hairline = hairline;
            props.stroke_width.state   = StylePropState::Single;
        } else if (props.stroke_width.state != StylePropState::Mixed) {
            if (props.stroke_width.hairline != hairline || props.stroke_width.value != width) {
                props.stroke_width.state = StylePropState::Mixed;
            }
        }
    }

    // --- stroke-linecap ---
    merge_scalar(props.stroke_linecap, static_cast<int>(style->stroke_linecap.value), first);

    // --- stroke-linejoin ---
    merge_scalar(props.stroke_linejoin, static_cast<int>(style->stroke_linejoin.value), first);

    // --- stroke-miterlimit ---
    merge_scalar(props.stroke_miterlimit, style->stroke_miterlimit.value, first);

    // --- stroke-dasharray + dashoffset ---
    {
        std::vector<double> dashes;
        dashes.reserve(style->stroke_dasharray.values.size());
        for (auto& d : style->stroke_dasharray.values) {
            dashes.push_back(d.computed);
        }
        double offset = style->stroke_dashoffset.computed;
        if (first) {
            props.stroke_dash.dashes = std::move(dashes);
            props.stroke_dash.offset = offset;
            props.stroke_dash.state  = StylePropState::Single;
        } else if (props.stroke_dash.state != StylePropState::Mixed) {
            if (props.stroke_dash.dashes != dashes || props.stroke_dash.offset != offset) {
                props.stroke_dash.state = StylePropState::Mixed;
            }
        }
    }

    // --- markers ---
    auto merge_marker = [&](auto& field, const SPIString& marker_prop) {
        std::string uri = marker_prop.value() ? marker_prop.value() : "";
        if (first) {
            field.uri   = uri;
            field.state = StylePropState::Single;
        } else if (field.state != StylePropState::Mixed && field.uri != uri) {
            field.state = StylePropState::Mixed;
        }
    };
    merge_marker(props.marker_start, style->marker_start);
    merge_marker(props.marker_mid,   style->marker_mid);
    merge_marker(props.marker_end,   style->marker_end);

    // --- paint-order ---
    {
        std::string order_str = style->paint_order.set ? style->paint_order.value : "normal";
        if (first) {
            props.paint_order.value = order_str;
            props.paint_order.state = StylePropState::Single;
        } else if (props.paint_order.state != StylePropState::Mixed && props.paint_order.value != order_str) {
            props.paint_order.state = StylePropState::Mixed;
        }
    }

    // --- opacity ---
    merge_scalar(props.opacity, static_cast<double>(style->opacity), first);

    // --- blend mode ---
    {
        int blend = style->mix_blend_mode.set
            ? static_cast<int>(style->mix_blend_mode.value)
            : static_cast<int>(SP_CSS_BLEND_NORMAL);
        merge_scalar(props.blend_mode, blend, first);
    }

    // --- visibility ---
    {
        bool hidden = item->isExplicitlyHidden();
        if (first) {
            props.visibility.hidden = hidden;
            props.visibility.state  = StylePropState::Single;
        } else if (props.visibility.state != StylePropState::Mixed && props.visibility.hidden != hidden) {
            props.visibility.state = StylePropState::Mixed;
        }
    }
}

} // anonymous namespace

namespace detail {

void query_style_impl(StyleProperties& props, bool& first, SPItem* item, StyleQueryFlags flags) {
    visit_items(item, flags, [&](SPItem* leaf) {
        merge_item_style(props, leaf, first);
        first = false;
    });
}

} // namespace detail

} // namespace Inkscape
