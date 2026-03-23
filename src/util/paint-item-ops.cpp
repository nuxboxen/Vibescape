// SPDX-License-Identifier: GPL-2.0-or-later

#include "paint-item-ops.h"

#include <optional>
#include <boost/smart_ptr/intrusive_ptr.hpp>

#include "document-undo.h"
#include "gradient-chemistry.h"
#include "pattern-manipulation.h"
#include "object/sp-gradient.h"
#include "object/sp-hatch.h"
#include "object/sp-item.h"
#include "object/sp-pattern.h"
#include "style.h"
#include "ui/widget/paint-attribute.h"
#include "ui/widget/stroke-style.h"
#include "filter-chemistry.h"
#include "preferences.h"
#include "util/style-utils.h"
#include "util/units.h"
#include "util/variant-visitor.h"
#include "xml/sp-css-attr.h"

namespace {

std::optional<Inkscape::Colors::Color> get_item_color(SPItem* item, bool fill) {
    if (!item || !item->style) return {};

    auto paint = item->style->getFillOrStroke(fill);
    return paint && paint->isColor() ? std::optional(paint->getColor()) : std::nullopt;
}

} // namespace

namespace Inkscape::Util {

using UI::Widget::calc_scale_line_width;
using UI::Widget::set_scaled_dash;
using UI::EditOperation;

boost::intrusive_ptr<SPCSSAttr> new_css_attr() {
    return boost::intrusive_ptr(sp_repr_css_attr_new(), false);
}

void set_item_style(SPItem* item, SPCSSAttr* css) {
    double scale = item->i2doc_affine().descrim();
    if (scale != 0 && scale != 1) {
        sp_css_attr_scale(css, 1 / scale);
    }
    item->changeCSS(css, "style");
}

void set_stroke_width(SPItem* item, double width_typed, bool hairline, const Util::Unit* unit) {
    auto css = new_css_attr();
    if (hairline) {
        // For renderers that don't understand -inkscape-stroke:hairline, fall back to 1px non-scaling
        width_typed = 1;
        sp_repr_css_set_property(css.get(), "vector-effect", "non-scaling-stroke");
        sp_repr_css_set_property(css.get(), "-inkscape-stroke", "hairline");
    }
    else {
        sp_repr_css_unset_property(css.get(), "vector-effect");
        sp_repr_css_unset_property(css.get(), "-inkscape-stroke");
    }

    double width = calc_scale_line_width(width_typed, item, unit);
    sp_repr_css_set_property_double(css.get(), "stroke-width", width);

    if (Preferences::get()->getBool("/options/dash/scale", true)) {
        // This will read the old stroke-width to unscale the pattern.
        auto [dash, offset] = getDashFromStyle(item->style);
        set_scaled_dash(css.get(), dash.size(), dash.data(), offset, width);
    }
    // item->style->stroke_dasharray.values = ;
    set_item_style(item, css.get());
}

SPGradient* swatch_operation(SPItem* item, SPGradient* vector, SPDesktop* desktop, bool fill,
                              EditOperation op, SPGradient* replacement,
                              std::optional<Colors::Color> color, Glib::ustring label,
                              unsigned int tag)
{
    auto kind = fill ? FILL : STROKE;

    switch (op) {
    case EditOperation::New:
        // try to find an existing swatch with matching color definition:
        if (auto clr = get_item_color(item, fill)) {
            vector = sp_find_matching_swatch(item->document, *clr);
        }
        else {
            // create a new swatch
            vector = nullptr;
        }
        vector = sp_item_apply_gradient(item, vector, desktop, SP_GRADIENT_TYPE_LINEAR, true, kind);
        DocumentUndo::done(item->document,
            fill ? RC_("Undo", "Set swatch on fill") : RC_("Undo", "Set swatch on stroke"),
            "dialog-fill-and-stroke", tag);
        return vector;
    case EditOperation::Change:
        if (color.has_value()) {
            sp_change_swatch_color(vector, *color);
            DocumentUndo::maybeDone(item->document, "swatch-color",
                RC_("Undo", "Change swatch color"), "dialog-fill-and-stroke", tag);
            return vector;
        }
        else {
            vector = sp_item_apply_gradient(item, vector, desktop, SP_GRADIENT_TYPE_LINEAR, true, kind);
            DocumentUndo::maybeDone(item->document,
                fill ? "fill-swatch-change" : "stroke-swatch-change",
                fill ? RC_("Undo", "Set swatch on fill") : RC_("Undo", "Set swatch on stroke"),
                "dialog-fill-and-stroke", tag);
            return vector;
        }
    case EditOperation::Delete:
        sp_delete_item_swatch(item, kind, vector, replacement);
        DocumentUndo::done(item->document, RC_("Undo", "Delete swatch"),
            "dialog-fill-and-stroke", tag);
        return replacement;
    case EditOperation::Rename:
        vector->setLabel(label.c_str());
        DocumentUndo::maybeDone(item->document, "swatch-rename",
            RC_("Undo", "Rename swatch"), "dialog-fill-and-stroke", tag);
        return vector;
    default:
        return nullptr;
    }
}

SPPaintServer* apply_paint_op_to_item(SPItem* item, const PaintEditDelegate::Op& op,
                                       SPDesktop* desktop, unsigned int tag)
{
    return std::visit(VariantVisitor{
        [item, tag](const PaintEditDelegate::CssOp& o) -> SPPaintServer* {
            set_item_style(item, o.css.get());
            item->requestModified(SP_OBJECT_MODIFIED_FLAG | tag);
            return nullptr;
        },
        [item, tag](const PaintEditDelegate::StrokeWidthOp& o) -> SPPaintServer* {
            set_stroke_width(item, o.width, o.hairline, o.unit);
            item->requestModified(SP_OBJECT_MODIFIED_FLAG | tag);
            return nullptr;
        },
        [item, tag](const PaintEditDelegate::DashOp& o) -> SPPaintServer* {
            double scale = item->i2doc_affine().descrim();
            if (Preferences::get()->getBool("/options/dash/scale", true)) {
                scale = item->style->stroke_width.computed * scale;
            }
            auto css = new_css_attr();
            set_scaled_dash(css.get(), o.dash.size(), o.dash.data(), o.offset, scale);
            set_item_style(item, css.get());
            item->requestModified(SP_OBJECT_MODIFIED_FLAG | tag);
            return nullptr;
        },
        [item, tag](const PaintEditDelegate::OpacityOp& o) -> SPPaintServer* {
            if (o.clear) item->style->opacity.clear();
            else item->style->opacity.set_double(o.opacity);
            item->updateRepr();
            item->requestModified(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | tag);
            item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            return nullptr;
        },
        [item](const PaintEditDelegate::BlendModeOp& o) -> SPPaintServer* {
            if (o.clear) ::clear_blend_mode(item);
            else ::set_blend_mode(item, o.mode);
            return nullptr;
        },
        [item](const PaintEditDelegate::VisibilityOp& o) -> SPPaintServer* {
            item->setExplicitlyHidden(o.hidden);
            return nullptr;
        },
        [item, desktop](const PaintEditDelegate::GradientOp& o) -> SPPaintServer* {
            return sp_item_apply_gradient(item, o.vector, desktop, o.type, false,
                                          o.fill ? FILL : STROKE);
        },
        [item](const PaintEditDelegate::PatternOp& o) -> SPPaintServer* {
            return sp_item_apply_pattern(item, o.pattern, o.fill ? FILL : STROKE,
                                         o.color, o.label, o.transform, o.offset,
                                         o.uniform_scale, o.gap);
        },
        [item](const PaintEditDelegate::HatchOp& o) -> SPPaintServer* {
            return sp_item_apply_hatch(item, o.hatch, o.fill ? FILL : STROKE,
                                       o.color, o.label, o.transform, o.offset,
                                       o.pitch, o.rotation, o.thickness);
        },
        [item](const PaintEditDelegate::MeshOp& o) -> SPPaintServer* {
            return sp_item_apply_mesh(item, o.mesh, item->document, o.fill ? FILL : STROKE);
        },
        [item, desktop, tag](const PaintEditDelegate::SwatchOp& o) -> SPPaintServer* {
            return swatch_operation(item, o.vector, desktop, o.fill, o.op,
                                    o.replacement, o.color, o.label, tag);
        },
    }, op);
}

} // namespace Inkscape::Util
