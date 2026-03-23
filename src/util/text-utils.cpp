// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#include "text-utils.h"

#include <cstring>
#include <glibmm/regex.h>
#include "desktop-style.h"
#include "svg/css-ostringstream.h"
#include "util/units.h"
#include "font-discovery.h"
#include "object/sp-flowdiv.h"
#include "object/sp-flowtext.h"
#include "object/sp-item.h"
#include "object/sp-text.h"
#include "object/sp-textpath.h"
#include "object/sp-tref.h"
#include "object/sp-tspan.h"
#include "style.h"
#include "style-internal.h"
#include "text-editing.h"
#include "ui/tools/text-tool.h"
#include "xml/repr.h"

namespace Inkscape {

namespace {

// TODO: Once layout iterators conform to std iterator trait,
// return filtered view instead of vector.
std::vector<SPItem*> get_text_spans(SPItem* text) {
    if (!text) {
        return {};
    }

    auto layout = te_get_layout(text);
    if (!layout) {
        return {};
    }

    std::vector<SPItem*> styles_list;

    // Get iterators for the entire text object
    Inkscape::Text::Layout::iterator begin_it = layout->begin();
    Inkscape::Text::Layout::iterator end_it = layout->end();

    for (auto it = begin_it; it < end_it; it.nextStartOfSpan()) {
        SPObject *pos_obj = nullptr;
        layout->getSourceOfCharacter(it, &pos_obj);
        if (!pos_obj) {
            continue;
        }
        if (!pos_obj->parent) { // the string is not in the document anymore (deleted)
            return {};
        }

        if (is<SPString>(pos_obj)) {
           pos_obj = pos_obj->parent;   // SPStrings don't have style
        }
        styles_list.emplace_back(cast_unsafe<SPItem>(pos_obj));
    }

    return styles_list;
}

bool is_textual_item(SPObject const* obj) {
    return is<SPText>(obj)
        || is<SPFlowtext>(obj)
        || is<SPTSpan>(obj)
        || is<SPTRef>(obj)
        || is<SPTextPath>(obj)
        || is<SPFlowdiv>(obj)
        || is<SPFlowpara>(obj)
        || is<SPFlowtspan>(obj);
}

} // anonymous namespace

TextProperties query_text_properties(const std::vector<SPItem*>& items) {
    TextProperties props;

    for (auto item : items) {
        if (!item || !is_textual_item(item)) continue;

        auto* style = item->style;
        if (!style) continue;

        // --- font family ---
        auto family = style->font_family.value() ? Glib::ustring(style->font_family.value()) : Glib::ustring();
        props.font_family.merge(family);

        // --- font style (face style string for combo lookup) ---
        // Build a Pango description from the CSS font properties and extract the face style
        Pango::FontDescription desc;
        if (family.size()) desc.set_family(family.raw());
        // CSS and Pango swap italic/oblique enum values:
        // CSS: NORMAL=0, ITALIC=1, OBLIQUE=2; Pango: NORMAL=0, OBLIQUE=1, ITALIC=2
        static const Pango::Style css_to_pango_style[] = {
            Pango::Style::NORMAL, Pango::Style::ITALIC, Pango::Style::OBLIQUE
        };
        auto fs = style->font_style.computed;
        desc.set_style(fs < 3 ? css_to_pango_style[fs] : Pango::Style::NORMAL);
        desc.set_weight(static_cast<Pango::Weight>(style->font_weight.computed));
        desc.set_stretch(static_cast<Pango::Stretch>(style->font_stretch.computed));
        auto face_style = get_face_style(desc);
        props.font_style.merge(face_style);

        // --- font variation settings ---
        props.font_variation.merge(style->font_variation_settings);

        // --- font size ---
        props.font_size.merge(style->font_size.computed);

        // --- line height ---
        double lh = style->line_height.computed;
        int lh_unit = style->line_height.unit;
        if (style->line_height.normal) {
            lh = -1; // sentinel for "normal"
        } else if (style->line_height.unit == SP_CSS_UNIT_NONE ||
                   style->line_height.unit == SP_CSS_UNIT_PERCENT ||
                   style->line_height.unit == SP_CSS_UNIT_EM ||
                   style->line_height.unit == SP_CSS_UNIT_EX) {
            lh = style->line_height.value;
        }
        props.line_height.merge(lh);
        props.line_height_unit.merge(lh_unit);

        // --- letter spacing ---
        double ls = style->letter_spacing.normal ? 0.0 : style->letter_spacing.computed;
        props.letter_spacing.merge(ls);

        // --- word spacing ---
        double ws = style->word_spacing.normal ? 0.0 : style->word_spacing.computed;
        props.word_spacing.merge(ws);

        // --- text align ---
        bool rtl = (style->direction.computed == SP_CSS_DIRECTION_RTL);
        int align_idx = get_text_align_button_index(rtl, static_cast<SPCSSTextAlign>(style->text_align.computed));
        props.text_align.merge(align_idx);

        // --- writing mode ---
        props.writing_mode.merge(style->writing_mode.computed);

        // --- direction ---
        props.direction.merge(style->direction.computed);

        // --- text orientation ---
        props.text_orientation.merge(style->text_orientation.computed);

        // --- baseline shift (superscript / subscript) ---
        bool is_super = style->baseline_shift.set && style->baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
            style->baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUPER;
        bool is_sub = style->baseline_shift.set && style->baseline_shift.type == SP_BASELINE_SHIFT_LITERAL &&
            style->baseline_shift.literal == SP_CSS_BASELINE_SHIFT_SUB;
        props.superscript.merge(is_super);
        props.subscript.merge(is_sub);

        // --- text decorations ---
        bool ul = style->text_decoration_line.underline;
        bool ol = style->text_decoration_line.overline;
        bool st = style->text_decoration_line.line_through;
        props.underline.merge(ul);
        props.overline.merge(ol);
        props.strikethrough.merge(st);

        bool spelling_error = style->text_decoration_line.spelling_error;
        props.decoration_spelling_error.merge(spelling_error);

        // --- decoration style ---
        int ds = 0; // solid by default
        if (style->text_decoration_style.isdouble) ds = 1;
        else if (style->text_decoration_style.dotted) ds = 2;
        else if (style->text_decoration_style.dashed) ds = 3;
        else if (style->text_decoration_style.wavy) ds = 4;
        props.decoration_style.merge(ds);

        // --- decoration color (not inherited — check parent if unset) ---
        {
            const auto* dc_ptr = &style->text_decoration_color;
            if (!dc_ptr->set && item->parent && item->parent->style) {
                dc_ptr = &item->parent->style->text_decoration_color;
            }
            bool dc_is_set = dc_ptr->set && !dc_ptr->inherit;
            auto dc = dc_is_set ? std::optional<Colors::Color>(dc_ptr->getColor()) : std::nullopt;
            if (dc_is_set) {
                props.decoration_color.merge(dc);
            }
        }

        // --- decoration thickness (not inherited — check parent if unset) ---
        {
            auto* tdt_ptr = &style->text_decoration_thickness;
            if (!tdt_ptr->set && item->parent && item->parent->style) {
                tdt_ptr = &item->parent->style->text_decoration_thickness;
            }
            auto& tdt = *tdt_ptr;
            if (tdt.set) {
                props.decoration_thickness.merge(DecorationThicknessProp{
                    tdt.computed, tdt.auto_val, tdt.from_font
                });
            }
        }
    }

    return props;
}

int get_text_align_button_index(bool rtl, SPCSSTextAlign text_align) {
    int activeButton = -1; //prefs->getInt("/tools/text/align_mode", 0);
    // bool rtl = (query.direction.computed == SP_CSS_DIRECTION_RTL);

    if ((text_align == SP_CSS_TEXT_ALIGN_START && !rtl) ||
        (text_align == SP_CSS_TEXT_ALIGN_END   &&  rtl) ||
         text_align == SP_CSS_TEXT_ALIGN_LEFT) {
        activeButton = 0;
    } else if (text_align == SP_CSS_TEXT_ALIGN_CENTER) {
        activeButton = 1;
    } else if ((text_align == SP_CSS_TEXT_ALIGN_START &&  rtl) ||
               (text_align == SP_CSS_TEXT_ALIGN_END   && !rtl) ||
                text_align == SP_CSS_TEXT_ALIGN_RIGHT) {
        activeButton = 2;
    } else if (text_align == SP_CSS_TEXT_ALIGN_JUSTIFY) {
        activeButton = 3;
    }
    return activeButton;
}

SPCSSTextAlign text_align_to_side(SPCSSTextAlign align, SPCSSDirection direction) {
    if ((align == SP_CSS_TEXT_ALIGN_START && direction == SP_CSS_DIRECTION_LTR) ||
        (align == SP_CSS_TEXT_ALIGN_END   && direction == SP_CSS_DIRECTION_RTL)) {
        return SP_CSS_TEXT_ALIGN_LEFT;
    }
    if ((align == SP_CSS_TEXT_ALIGN_START && direction == SP_CSS_DIRECTION_RTL) ||
        (align == SP_CSS_TEXT_ALIGN_END   && direction == SP_CSS_DIRECTION_LTR)) {
        return SP_CSS_TEXT_ALIGN_RIGHT;
    }
    return align;
}

bool apply_text_alignment(SPText* text, int align_mode) {
    if (!text || align_mode < 0 || align_mode > 3) return false;

    // Determine axis based on writing mode
    Geom::Dim2 axis;
    unsigned writing_mode = text->style->writing_mode.value;
    if (writing_mode == SP_CSS_WRITING_MODE_LR_TB || writing_mode == SP_CSS_WRITING_MODE_RL_TB) {
        axis = Geom::X;
    } else {
        axis = Geom::Y;
    }

    // Get text bounding box for position adjustment
    Geom::OptRect bbox = text->get_frame();
    if (!bbox) {
        bbox = text->geometricBounds();
    }
    if (!bbox) return false;

    double width = bbox->dimensions()[axis];
    double move = 0;
    auto direction = text->style->direction.value;

    // Calculate position adjustment based on old alignment
    auto old_side = text_align_to_side(text->style->text_align.value, direction);
    switch (old_side) {
        case SP_CSS_TEXT_ALIGN_LEFT:
            switch (align_mode) {
                case 1: move =  width / 2; break;
                case 2: move =  width;     break;
                default: break;
            }
            break;
        case SP_CSS_TEXT_ALIGN_CENTER:
            switch (align_mode) {
                case 0: move = -width / 2; break;
                case 2: move =  width / 2; break;
                default: break;
            }
            break;
        case SP_CSS_TEXT_ALIGN_RIGHT:
            switch (align_mode) {
                case 0: move = -width;     break;
                case 1: move = -width / 2; break;
                default: break;
            }
            break;
        default:
            break;
    }

    // Set text-anchor and text-align CSS
    auto css = make_css();
    if ((align_mode == 0 && direction == SP_CSS_DIRECTION_LTR) ||
        (align_mode == 2 && direction == SP_CSS_DIRECTION_RTL)) {
        sp_repr_css_set_property(css.get(), "text-anchor", "start");
        sp_repr_css_set_property(css.get(), "text-align",  "start");
    }
    if ((align_mode == 0 && direction == SP_CSS_DIRECTION_RTL) ||
        (align_mode == 2 && direction == SP_CSS_DIRECTION_LTR)) {
        sp_repr_css_set_property(css.get(), "text-anchor", "end");
        sp_repr_css_set_property(css.get(), "text-align",  "end");
    }
    if (align_mode == 1) {
        sp_repr_css_set_property(css.get(), "text-anchor", "middle");
        sp_repr_css_set_property(css.get(), "text-align",  "center");
    }
    if (align_mode == 3) {
        sp_repr_css_set_property(css.get(), "text-anchor", "start");
        sp_repr_css_set_property(css.get(), "text-align",  "justify");
    }
    text->changeCSS(css.get(), "style");

    // Adjust text position to preserve visual bounding box
    Geom::Point XY = text->attributes.firstXY();
    if (axis == Geom::X) {
        XY += Geom::Point(move, 0);
    } else {
        XY += Geom::Point(0, move);
    }
    text->attributes.setFirstXY(XY);
    text->updateRepr();
    text->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);

    return std::abs(move) > 0;
}

CssPtr apply_text_script(bool setSuper, bool setSub) {
    auto css = make_css();
    // Openoffice 2.3 and Adobe use 58%, Microsoft Word 2002 uses 65%, LaTeX about 70%.
    // 58% looks too small, especially if a superscript is placed on a superscript.
    // If you make a change here, consider making a change to baseline-shift amount in style.cpp.
    sp_repr_css_set_property(css.get(), "font-size", (setSuper || setSub) ? "65%" : "");
    if (setSuper) {
        sp_repr_css_set_property(css.get(), "baseline-shift", "super");
    } else if (setSub) {
        sp_repr_css_set_property(css.get(), "baseline-shift", "sub");
    } else {
        sp_repr_css_set_property(css.get(), "baseline-shift", "baseline");
    }
    return css;
}

void fill_css_from_font_description(SPCSSAttr* css, const Glib::ustring& family,
                                     const Pango::FontDescription& desc, const Glib::ustring& fontspec) {
    if (!css) return;

    // font-family — properly quoted for CSS
    Glib::ustring quoted_family = family;
    css_font_family_quote(quoted_family);
    sp_repr_css_set_property(css, "font-family", quoted_family.c_str());

    // font-weight — full Pango weight mapping (matches FontLister::fill_css)
    auto weight = static_cast<int>(desc.get_weight());
    switch (weight) {
        case PANGO_WEIGHT_THIN:       sp_repr_css_set_property(css, "font-weight", "100"); break;
        case PANGO_WEIGHT_ULTRALIGHT: sp_repr_css_set_property(css, "font-weight", "200"); break;
        case PANGO_WEIGHT_LIGHT:      sp_repr_css_set_property(css, "font-weight", "300"); break;
        case PANGO_WEIGHT_SEMILIGHT:  sp_repr_css_set_property(css, "font-weight", "350"); break;
        case PANGO_WEIGHT_BOOK:       sp_repr_css_set_property(css, "font-weight", "380"); break;
        case PANGO_WEIGHT_NORMAL:     sp_repr_css_set_property(css, "font-weight", "normal"); break;
        case PANGO_WEIGHT_MEDIUM:     sp_repr_css_set_property(css, "font-weight", "500"); break;
        case PANGO_WEIGHT_SEMIBOLD:   sp_repr_css_set_property(css, "font-weight", "600"); break;
        case PANGO_WEIGHT_BOLD:       sp_repr_css_set_property(css, "font-weight", "bold"); break;
        case PANGO_WEIGHT_ULTRABOLD:  sp_repr_css_set_property(css, "font-weight", "800"); break;
        case PANGO_WEIGHT_HEAVY:      sp_repr_css_set_property(css, "font-weight", "900"); break;
        case PANGO_WEIGHT_ULTRAHEAVY: sp_repr_css_set_property(css, "font-weight", "1000"); break;
        default:
            if (weight > 0 && weight < 1000) {
                sp_repr_css_set_property(css, "font-weight", std::to_string(weight).c_str());
            }
            break;
    }

    // font-style
    switch (desc.get_style()) {
        case Pango::Style::NORMAL:  sp_repr_css_set_property(css, "font-style", "normal"); break;
        case Pango::Style::OBLIQUE: sp_repr_css_set_property(css, "font-style", "oblique"); break;
        case Pango::Style::ITALIC:  sp_repr_css_set_property(css, "font-style", "italic"); break;
    }

    // font-stretch
    switch (desc.get_stretch()) {
        case Pango::Stretch::ULTRA_CONDENSED: sp_repr_css_set_property(css, "font-stretch", "ultra-condensed"); break;
        case Pango::Stretch::EXTRA_CONDENSED: sp_repr_css_set_property(css, "font-stretch", "extra-condensed"); break;
        case Pango::Stretch::CONDENSED:       sp_repr_css_set_property(css, "font-stretch", "condensed"); break;
        case Pango::Stretch::SEMI_CONDENSED:  sp_repr_css_set_property(css, "font-stretch", "semi-condensed"); break;
        case Pango::Stretch::NORMAL:          sp_repr_css_set_property(css, "font-stretch", "normal"); break;
        case Pango::Stretch::SEMI_EXPANDED:   sp_repr_css_set_property(css, "font-stretch", "semi-expanded"); break;
        case Pango::Stretch::EXPANDED:        sp_repr_css_set_property(css, "font-stretch", "expanded"); break;
        case Pango::Stretch::EXTRA_EXPANDED:  sp_repr_css_set_property(css, "font-stretch", "extra-expanded"); break;
        case Pango::Stretch::ULTRA_EXPANDED:  sp_repr_css_set_property(css, "font-stretch", "ultra-expanded"); break;
    }

    // font-variant
    switch (desc.get_variant()) {
        case Pango::Variant::NORMAL:     sp_repr_css_set_property(css, "font-variant", "normal"); break;
        case Pango::Variant::SMALL_CAPS: sp_repr_css_set_property(css, "font-variant", "small-caps"); break;
    }

    // font-variation-settings — convert Pango format "axis=value,..." to CSS "'axis' value, ..."
    auto vars = desc.get_variations();
    if (!vars.empty()) {
        std::string css_vars;
        auto tokens = Glib::Regex::split_simple(",", vars);
        auto regex = Glib::Regex::create("(\\w{4})=([-+]?\\d*\\.?\\d+([eE][-+]?\\d+)?)");
        Glib::MatchInfo match_info;
        for (auto const& token : tokens) {
            regex->match(token, match_info);
            if (match_info.matches()) {
                css_vars += "'";
                css_vars += match_info.fetch(1).raw();
                css_vars += "' ";
                css_vars += match_info.fetch(2).raw();
                css_vars += ", ";
            }
        }
        if (css_vars.length() >= 2) {
            css_vars.pop_back();
            css_vars.pop_back();
        }
        sp_repr_css_set_property(css, "font-variation-settings", css_vars.c_str());
    } else {
        sp_repr_css_unset_property(css, "font-variation-settings");
    }

    if (fontspec.empty()) {
        sp_repr_css_unset_property(css, "-inkscape-font-specification");
    }
    else {
        // -inkscape-font-specification is FontLister-specific (single-quoted fontspec)
        Glib::ustring fontspec_quoted(fontspec);
        css_quote(fontspec_quoted);
        sp_repr_css_set_property(css, "-inkscape-font-specification", fontspec_quoted.c_str());
    }
}

bool apply_text_dx(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_dx) {
    if (!tool || !tool->textItem()) return false;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return false;

    double delta = new_dx - attributes->getDx(char_index);
    sp_te_adjust_dx(tool->textItem(), tool->text_sel_start, tool->text_sel_end, desktop, delta);
    return true;
}

bool apply_text_dy(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_dy) {
    if (!tool || !tool->textItem()) return false;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return false;

    double delta = new_dy - attributes->getDy(char_index);
    sp_te_adjust_dy(tool->textItem(), tool->text_sel_start, tool->text_sel_end, desktop, delta);
    return true;
}

std::optional<double> query_text_dx(UI::Tools::TextTool* tool) {
    if (!tool || !tool->textItem()) return std::nullopt;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return std::nullopt;

    return attributes->getDx(char_index);
}

std::optional<double> query_text_dy(UI::Tools::TextTool* tool) {
    if (!tool || !tool->textItem()) return std::nullopt;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return std::nullopt;

    return attributes->getDy(char_index);
}

bool apply_text_char_rotation(UI::Tools::TextTool* tool, SPDesktop* desktop, double new_degrees) {
    if (!tool || !tool->textItem()) return false;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return false;

    double old_degrees = attributes->getRotate(char_index);
    double delta_deg = new_degrees - old_degrees;
    sp_te_adjust_rotation(tool->textItem(), tool->text_sel_start, tool->text_sel_end, desktop, delta_deg);
    return true;
}

std::optional<double> query_text_char_rotation(UI::Tools::TextTool* tool) {
    if (!tool || !tool->textItem()) return std::nullopt;

    unsigned char_index = -1;
    auto attributes = text_tag_attributes_at_position(
        tool->textItem(), std::min(tool->text_sel_start, tool->text_sel_end), &char_index);
    if (!attributes) return std::nullopt;

    double rotation = attributes->getRotate(char_index);
    // SVG value is 0..360 but we use -180..180
    if (rotation > 180.0) {
        rotation -= 360.0;
    }
    return rotation;
}

void apply_text_css(SPItem* text_item, UI::Tools::TextTool* tool, SPCSSAttr* css) {
    if (!text_item || !css) return;

    // If text tool has a subselection, apply to that range directly
    if (tool && tool->textItem() == text_item && tool->text_sel_start != tool->text_sel_end) {
        sp_te_apply_style(text_item, tool->text_sel_start, tool->text_sel_end, css);
        if (auto sptext = cast<SPText>(text_item)) {
            sptext->rebuildLayout();
            sptext->updateRepr();
        }
    }
    else {
        // Apply CSS to the entire text using sp_te_apply_style so it operates at the
        // character/span level and does not touch properties on the root element itself
        // (e.g. font-size set by the user). sp_desktop_apply_css_recursive is too broad
        // for text: it sets every property on every node including the root.
        // sp_desktop_apply_css_recursive(text_item, css, true);
        if (auto sptext = cast<SPText>(text_item)) {
            sp_te_apply_style(text_item, sptext->layout.begin(), sptext->layout.end(), css);
            sptext->rebuildLayout();
        }
        text_item->updateRepr();
    }
}

// --- Unit helpers for font-size / line-height ---

bool is_relative_unit(Util::Unit const *unit) {
    return unit->abbr == "" || unit->abbr == "lines" || unit->abbr == "em" || unit->abbr == "ex" || unit->abbr == "%";
}

bool is_relative_unit(int css_unit) {
    return css_unit == SP_CSS_UNIT_NONE || css_unit == SP_CSS_UNIT_EM ||
           css_unit == SP_CSS_UNIT_EX   || css_unit == SP_CSS_UNIT_PERCENT;
}

int unit_to_css_unit(Util::Unit const *unit) {
    if (unit->abbr == "" || unit->abbr == "lines") return SP_CSS_UNIT_NONE;
    SPILength temp;
    CSSOStringStream s;
    s << 1 << unit->abbr;
    temp.read(s.str().c_str());
    return temp.unit;
}

double convert_lineheight_between_units(double value, int old_unit,
                                        Util::Unit const *new_unit, double avg_font_size)
{
    auto const &abbr = new_unit->abbr;

    bool const new_is_lines = (abbr == "" || abbr == "lines" || abbr == "em");

    if (new_is_lines && (old_unit == SP_CSS_UNIT_NONE || old_unit == SP_CSS_UNIT_EM)) {
        // no conversion needed
    } else if (new_is_lines && old_unit == SP_CSS_UNIT_EX) {
        value *= 0.5;
    } else if (abbr == "ex" && (old_unit == SP_CSS_UNIT_EM || old_unit == SP_CSS_UNIT_NONE)) {
        value *= 2.0;
    } else if (new_is_lines && old_unit == SP_CSS_UNIT_PERCENT) {
        value /= 100.0;
    } else if (abbr == "%" && (old_unit == SP_CSS_UNIT_EM || old_unit == SP_CSS_UNIT_NONE)) {
        value *= 100;
    } else if (abbr == "ex" && old_unit == SP_CSS_UNIT_PERCENT) {
        value /= 50.0;
    } else if (abbr == "%" && old_unit == SP_CSS_UNIT_EX) {
        value *= 50;
    } else if (is_relative_unit(new_unit)) {
        // absolute → relative
        int ou = old_unit;
        if (ou == SP_CSS_UNIT_NONE) ou = SP_CSS_UNIT_EM;
        value = Util::Quantity::convert(value, sp_style_get_css_unit_string(ou), "px");
        if (avg_font_size > 0) value /= avg_font_size;
        if (abbr == "%") value *= 100;
        else if (abbr == "ex") value *= 2;
    } else if (is_relative_unit(old_unit)) {
        // relative → absolute
        if (old_unit == SP_CSS_UNIT_PERCENT) value /= 100.0;
        else if (old_unit == SP_CSS_UNIT_EX) value /= 2.0;
        value *= avg_font_size;
        value = Util::Quantity::convert(value, "px", new_unit);
    } else {
        // absolute → absolute
        value = Util::Quantity::convert(value, sp_style_get_css_unit_string(old_unit), new_unit);
    }
    return value;
}

std::string format_line_height_css(double value, Util::Unit const *unit) {
    CSSOStringStream osfs;
    if (unit->abbr == "" || unit->abbr == "lines") {
        osfs << value;
    } else if (is_relative_unit(unit)) {
        osfs << value << unit->abbr;
    } else {
        osfs << Util::Quantity::convert(value, unit, "px") << "px";
    }
    return osfs.str();
}

std::vector<SPItem*> get_all_text_spans(SPItem* text) {
    return get_text_spans(text);
}

} // namespace
