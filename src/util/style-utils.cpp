// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#include "style-utils.h"

#include <functional>
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

// Merge one item's style into props.
void merge_item_style(StyleProperties& props, SPItem* item) {
    auto* style = item->style;
    if (!style) return;

    // --- fill ---
    if (!props.fill.is_mixed()) {
        props.fill.merge(classify_paint(style->fill));
    }

    props.fill_opacity.merge(static_cast<double>(style->fill_opacity));
    props.fill_rule.merge(static_cast<int>(style->fill_rule.computed));

    // --- stroke ---
    if (!props.stroke.is_mixed()) {
        props.stroke.merge(classify_paint(style->stroke));
    }

    props.stroke_opacity.merge(static_cast<double>(style->stroke_opacity));

    // --- stroke-width ---
    {
        bool hairline = style->stroke_extensions.hairline;
        double width = hairline ? 0.0 : style->stroke_width.computed;
        props.stroke_width.merge(StrokeWidthProp{width, hairline});
    }

    props.stroke_linecap.merge(static_cast<int>(style->stroke_linecap.value));
    props.stroke_linejoin.merge(static_cast<int>(style->stroke_linejoin.value));
    props.stroke_miterlimit.merge(style->stroke_miterlimit.value);

    // --- stroke-dasharray + dashoffset ---
    if (!props.stroke_dash.is_mixed()) {
        StrokeDashProp sd;
        sd.dashes.reserve(style->stroke_dasharray.values.size());
        for (auto& d : style->stroke_dasharray.values) {
            sd.dashes.push_back(d.computed);
        }
        sd.offset = style->stroke_dashoffset.computed;
        props.stroke_dash.merge(std::move(sd));
    }

    // --- markers ---
    props.marker_start.merge(style->marker_start.value() ? style->marker_start.value() : "");
    props.marker_mid.merge(style->marker_mid.value()     ? style->marker_mid.value()   : "");
    props.marker_end.merge(style->marker_end.value()     ? style->marker_end.value()   : "");

    // --- paint-order ---
    props.paint_order.merge(style->paint_order.set ? style->paint_order.value : "normal");

    props.opacity.merge(static_cast<double>(style->opacity));

    // --- blend mode ---
    {
        int blend = style->mix_blend_mode.set
            ? static_cast<int>(style->mix_blend_mode.value)
            : static_cast<int>(SP_CSS_BLEND_NORMAL);
        props.blend_mode.merge(blend);
    }

    props.visibility.merge(item->isExplicitlyHidden());
}

} // anonymous namespace

namespace detail {

void query_style_impl(StyleProperties& props, SPItem* item, StyleQueryFlags flags) {
    visit_items(item, flags, [&](SPItem* leaf) {
        merge_item_style(props, leaf);
    });
}

} // namespace detail

} // namespace Inkscape
