// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Style interactions with libnrtype
 *//*
 * Copyright (C) 2018-2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "style-text.h"

Pango::FontDescription ink_font_description_from_style(SPStyle const *style)
{
    Pango::FontDescription descr;

    descr.set_family(style->font_family.value());

    // This duplicates Layout::EnumConversionItem... perhaps we can share code?
    switch (style->font_style.computed) {
        case SP_CSS_FONT_STYLE_ITALIC:
            descr.set_style(Pango::Style::ITALIC);
            break;

        case SP_CSS_FONT_STYLE_OBLIQUE:
            descr.set_style(Pango::Style::OBLIQUE);
            break;

        case SP_CSS_FONT_STYLE_NORMAL:
        default:
            descr.set_style(Pango::Style::NORMAL);
            break;
    }

    // CSS now allows any value between 1 and 1000, including 1000.
    auto weight = style->font_weight.computed;
    if (weight > 0 && weight <= 1000) {
        descr.set_weight(static_cast<Pango::Weight>(style->font_weight.computed));
    } else {
        // SP_CSS_FONT_WEIGHT_LIGHTER, SP_CSS_FONT_WEIGHT_BOLDER (shouldn't be in computed).
        g_warning("FaceFromStyle: Unrecognized font_weight.computed value");
        descr.set_weight(Pango::Weight::NORMAL);
    }

    switch (style->font_stretch.computed) {
        case SP_CSS_FONT_STRETCH_ULTRA_CONDENSED:
            descr.set_stretch(Pango::Stretch::ULTRA_CONDENSED);
            break;

        case SP_CSS_FONT_STRETCH_EXTRA_CONDENSED:
            descr.set_stretch(Pango::Stretch::EXTRA_CONDENSED);
            break;

        case SP_CSS_FONT_STRETCH_CONDENSED:
            descr.set_stretch(Pango::Stretch::CONDENSED);
            break;

        case SP_CSS_FONT_STRETCH_SEMI_CONDENSED:
            descr.set_stretch(Pango::Stretch::SEMI_CONDENSED);
            break;

        case SP_CSS_FONT_STRETCH_NORMAL:
            descr.set_stretch(Pango::Stretch::NORMAL);
            break;

        case SP_CSS_FONT_STRETCH_SEMI_EXPANDED:
            descr.set_stretch(Pango::Stretch::SEMI_EXPANDED);
            break;

        case SP_CSS_FONT_STRETCH_EXPANDED:
            descr.set_stretch(Pango::Stretch::EXPANDED);
            break;

        case SP_CSS_FONT_STRETCH_EXTRA_EXPANDED:
            descr.set_stretch(Pango::Stretch::EXTRA_EXPANDED);
            break;

        case SP_CSS_FONT_STRETCH_ULTRA_EXPANDED:
            descr.set_stretch(Pango::Stretch::ULTRA_EXPANDED);

        case SP_CSS_FONT_STRETCH_WIDER:
        case SP_CSS_FONT_STRETCH_NARROWER:
        default:
            g_warning("FaceFromStyle: Unrecognized font_stretch.computed value");
            descr.set_stretch(Pango::Stretch::NORMAL);
            break;
    }

    switch (style->font_variant.computed) {
        case SP_CSS_FONT_VARIANT_SMALL_CAPS:
            descr.set_variant(Pango::Variant::SMALL_CAPS);
            break;

        case SP_CSS_FONT_VARIANT_NORMAL:
        default:
            descr.set_variant(Pango::Variant::NORMAL);
            break;
    }

    // Check if not empty as Pango will add @ to string even if empty (bug in Pango?).
    if (!style->font_variation_settings.axes.empty()) {
        descr.set_variations(style->font_variation_settings.toString());
    }

    return descr;
}

std::shared_ptr<FontInstance> ink_font_from_style(SPStyle const *style)
{
    std::shared_ptr<FontInstance> font;

    g_assert(style);

    if (style) {

        //  First try to use the font specification if it is set
        char const *val;
        if (style->font_specification.set
            && (val = style->font_specification.value())
            && val[0]) {

            font = FontFactory::get().FaceFromFontSpecification(val);
        }

        // If that failed, try using the CSS information in the style
        if (!font) {
            font = FontFactory::get().Face(ink_font_description_from_style(style).gobj());
        }
    }

    return font;
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
