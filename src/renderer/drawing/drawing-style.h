// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Style information for rendering.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_STYLE_H
#define INKSCAPE_RENDERER_DRAWING_STYLE_H

#include <memory>
#include <array>
#include <2geom/rect.h>

#include "renderer/context.h"

#include "drawing-paintserver.h"
#include "display/initlock.h"

namespace Inkscape::Renderer {

class DrawingPattern;
class DrawingOptions;

class DrawingStyle
{
public:
    // All default values
    DrawingStyle() = default;

    // Disable copy but allow move and assignment
    DrawingStyle(const DrawingStyle&) = delete;
    DrawingStyle(DrawingStyle&& other) noexcept = default;
    DrawingStyle& operator=(DrawingStyle&& other) noexcept = default;

    template <typename StyleSource>
    DrawingStyle(StyleSource const *style, StyleSource const *context_style = nullptr)
    {
        if (context_style) {
            set_context_style(context_style);
        }

        opacity = style->opacity.as_double();

        /* Fill Style */
        fill.set(style->fill);
        fill.opacity = style->fill_opacity.as_double();

        fill_rule = style->fill_rule.as_enum();
        clip_rule = style->clip_rule.as_enum();

        // See: http://www.w3.org/TR/SVG/painting.html#ImageRenderingProperty
        //      https://drafts.csswg.org/css-images-3/#the-image-rendering
        //      style.h/style.cpp, cairo-render-context.cpp
        //
        // CSS 3 defines:
        //   'optimizeSpeed' as alias for "pixelated"
        //   'optimizeQuality' as alias for "smooth"
        switch (style->image_rendering.as_enum()) {
            case SP_CSS_IMAGE_RENDERING_OPTIMIZESPEED:
                image_rendering = Cairo::SurfacePattern::Filter::FAST;
                break;
            case SP_CSS_IMAGE_RENDERING_PIXELATED:
                // we don't have an implementation for crisp-edges, but it should *not* smooth or blur
            case SP_CSS_IMAGE_RENDERING_CRISPEDGES:
                image_rendering = Cairo::SurfacePattern::Filter::NEAREST;
                break;
            case SP_CSS_IMAGE_RENDERING_AUTO:
            case SP_CSS_IMAGE_RENDERING_OPTIMIZEQUALITY:
            default:
                // In recent Cairo, BEST used Lanczos3, which is prohibitively slow
                image_rendering = Cairo::SurfacePattern::Filter::GOOD;
                break;
        }

        /* Stroke Style */
        stroke.set(style->stroke);
        stroke.opacity = style->stroke_opacity.as_double();
        stroke_width = style->stroke_width.as_double();
        hairline = style->stroke_extensions.hairline;
        line_cap = style->stroke_linecap.as_enum(); 
        line_join = style->stroke_linejoin.as_enum();
        miter_limit = style->stroke_miterlimit.as_double();

        if (style->stroke_dasharray.is_valid()) {
            dash_offset = style->stroke_dashoffset.as_double();
            dash = style->stroke_dasharray.get_computed();
        }

        auto layers = style->paint_order.get_layers();
        for (int i = 0; i < PAINT_ORDER_LAYERS && i < layers.size(); ++i) {
            switch (layers[i]) {
                case SP_CSS_PAINT_ORDER_FILL:
                    paint_order_layer[i]=PAINT_ORDER_FILL;
                    break;
                case SP_CSS_PAINT_ORDER_STROKE:
                    paint_order_layer[i]=PAINT_ORDER_STROKE;
                    break;
                case SP_CSS_PAINT_ORDER_MARKER:
                    paint_order_layer[i]=PAINT_ORDER_MARKER;
                    break;
            }
        }

        text_decoration_line = TEXT_DECORATION_LINE_CLEAR;
        if (style->text_decoration_line.inherit     ) { text_decoration_line |= TEXT_DECORATION_LINE_INHERIT;                                }
        if (style->text_decoration_line.underline   ) { text_decoration_line |= TEXT_DECORATION_LINE_UNDERLINE   + TEXT_DECORATION_LINE_SET; }
        if (style->text_decoration_line.overline    ) { text_decoration_line |= TEXT_DECORATION_LINE_OVERLINE    + TEXT_DECORATION_LINE_SET; }
        if (style->text_decoration_line.line_through) { text_decoration_line |= TEXT_DECORATION_LINE_LINETHROUGH + TEXT_DECORATION_LINE_SET; }
        if (style->text_decoration_line.blink       ) { text_decoration_line |= TEXT_DECORATION_LINE_BLINK       + TEXT_DECORATION_LINE_SET; }

        text_decoration_style = TEXT_DECORATION_STYLE_CLEAR;
        if (style->text_decoration_style.inherit ) { text_decoration_style |= TEXT_DECORATION_STYLE_INHERIT;                              }
        if (style->text_decoration_style.solid   ) { text_decoration_style |= TEXT_DECORATION_STYLE_SOLID    + TEXT_DECORATION_STYLE_SET; }
        if (style->text_decoration_style.isdouble) { text_decoration_style |= TEXT_DECORATION_STYLE_ISDOUBLE + TEXT_DECORATION_STYLE_SET; }
        if (style->text_decoration_style.dotted  ) { text_decoration_style |= TEXT_DECORATION_STYLE_DOTTED   + TEXT_DECORATION_STYLE_SET; }
        if (style->text_decoration_style.dashed  ) { text_decoration_style |= TEXT_DECORATION_STYLE_DASHED   + TEXT_DECORATION_STYLE_SET; }
        if (style->text_decoration_style.wavy    ) { text_decoration_style |= TEXT_DECORATION_STYLE_WAVY     + TEXT_DECORATION_STYLE_SET; }

        /* FIXME
           The meaning of text-decoration-color in CSS3 for SVG is ambiguous (2014-05-06).  Set
           it for fill, for stroke, for both?  Both would seem like the obvious choice but what happens
           is that for text which is just fill (very common) it makes the lines fatter because it
           enables stroke on the decorations when it wasn't present on the text.  That contradicts the
           usual behavior where the text and decorations by default have the same fill/stroke.
           
           The behavior here is that if color is defined it is applied to text_decoration_fill/stroke
           ONLY if the corresponding fill/stroke is also present.
           
           Hopefully the standard will be clarified to resolve this issue.
        */

        // Unless explicitly set on an element, text decoration is inherited from
        // closest ancestor where 'text-decoration' was set. That is, setting
        // 'text-decoration' on an ancestor fixes the fill and stroke of the
        // decoration to the fill and stroke values of that ancestor.
        if (auto style_td = style->text_decoration.style_td) {
            // Priority is given in order:
            //   * text_decoration_fill
            //   * text_decoration_color (only if fill set)
            //   * fill
            text_decoration_stroke.opacity = style_td->stroke_opacity.as_double();
            text_decoration_stroke_width = style_td->stroke_width.as_double();

            if (style_td->text_decoration_fill.set) {
                text_decoration_fill.set(style_td->text_decoration_fill);
            } else if (style_td->text_decoration_color.set) {
                if(style->fill.isPaintserver() || style->fill.isColor()) {
                    // SVG sets color specifically
                    text_decoration_fill.set(style->text_decoration_color.getColor());
                } else {
                    // No decoration fill because no text fill
                    text_decoration_fill.clear();
                }
            } else {
                // Pick color/pattern from text
                text_decoration_fill.set(style_td->fill);
            }

            if (style_td->text_decoration_stroke.set) {
                text_decoration_stroke.set(style_td->text_decoration_stroke);
            } else if (style_td->text_decoration_color.set) {
                if(style->stroke.isPaintserver() || style->stroke.isColor()) {
                    // SVG sets color specifically
                    text_decoration_stroke.set(style->text_decoration_color.getColor());
                } else {
                    // No decoration stroke because no text stroke
                    text_decoration_stroke.clear();
                }
            } else {
                // Pick color/pattern from text
                text_decoration_stroke.set(style_td->stroke);
            }
        }

        if (text_decoration_line != TEXT_DECORATION_LINE_CLEAR) {
            phase_length           = style->text_decoration_data.phase_length;
            tspan_line_start       = style->text_decoration_data.tspan_line_start;
            tspan_line_end         = style->text_decoration_data.tspan_line_end;
            tspan_width            = style->text_decoration_data.tspan_width;
            ascender               = style->text_decoration_data.ascender;
            descender              = style->text_decoration_data.descender;
            underline_thickness    = style->text_decoration_data.underline_thickness;
            underline_position     = style->text_decoration_data.underline_position;
            line_through_thickness = style->text_decoration_data.line_through_thickness;
            line_through_position  = style->text_decoration_data.line_through_position;
            font_size              = style->font_size;
        }

        text_direction = style->direction.as_enum();

        background_new = style->enable_background.as_enum() == SP_CSS_BACKGROUND_NEW;
        vector_effect_size   = style->vector_effect.size;
        vector_effect_rotate = style->vector_effect.rotate;
        vector_effect_fixed  = style->vector_effect.fixed;
        vector_effect_stroke = style->vector_effect.stroke;

        stroke_extensions_hairline = style->stroke_extensions.hairline;
        color_interpolation = style->color_interpolation.getInterpolationSpace();
    }

    template <typename StyleSource>
    void set_context_style(StyleSource const *context_style)
    {
        fill.set(context_style->fill, PaintContext::FILL);
        fill.set(context_style->stroke, PaintContext::STROKE);
        stroke.set(context_style->fill, PaintContext::FILL);
        stroke.set(context_style->stroke, PaintContext::STROKE);
    }

    enum class PaintType
    {
        NONE,
        COLOR,
        SERVER,
        CONTEXT
    };
    enum class PaintContext
    {
        NONE,
        FILL,
        STROKE
    };

    struct Paint
    {
        PaintType type = PaintType::NONE;
        std::optional<Colors::Color> color;
        std::unique_ptr<DrawingPaintServer> server;
        PaintContext context;
        double opacity = 1.0;

        void clear()
        {
            server.reset();
            color.reset();
            context = PaintContext::NONE;
        }
        void set(Colors::Color const &new_color)
        {
            clear();
            color = new_color;
            type = PaintType::COLOR;
        }
        void set(std::unique_ptr<DrawingPaintServer> ps)
        {
            clear();
            if (ps) {
                server = std::move(ps);
                type = PaintType::SERVER;
            }
        }
        void set()
        {
            clear();
            type = PaintType::NONE;
        }

        template <typename PaintSource>
        PaintType _set(PaintSource const &paint, bool is_context)
        {
            if (!is_context && paint.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_FILL) {
                context = PaintContext::FILL;
                return PaintType::CONTEXT;
            } else if (!is_context && paint.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_STROKE) {
                context = PaintContext::STROKE;
                return PaintType::CONTEXT;
            } else if (auto ps = create_paintserver(paint)) {
                server = std::move(ps);
                return PaintType::SERVER;
            } else if (paint.isColor()) {
                color = paint.getColor();
                return PaintType::COLOR;
            } else if (paint.isNone()) {
                clear();
            } else if (paint.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_FILL ||
                       paint.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_STROKE) {
                // A marker in the defs section will result in ending up here.
                // std::cerr << "DrawingStyle::Paint::set: Double" << std::endl;
            }
            return PaintType::NONE;
        }

        template <typename PaintSource>
        void set(PaintSource const &paint)
        {
            type = _set(paint, false);
        }

        template <typename PaintSource>
        void set(PaintSource const &paint, PaintContext for_context)
        {
            if (type == PaintType::CONTEXT && context == for_context) {
                _set(paint, true);
            }
        }
        
        template <typename PaintSource>
        static std::unique_ptr<DrawingPaintServer> create_paintserver(PaintSource const &paint)
        {
            if (paint.isPaintserver()) {
                return create_drawing_paintserver(paint.href->getObject());
            }
            return {};
        }

        bool ditherable() const;
    };

    double opacity = 1.0;
    Paint fill;
    Paint stroke;
    double stroke_width = 1.0;
    bool hairline = false;
    double miter_limit = 4.0;
    std::vector<double> dash;
    double dash_offset = 0.0;
    SPWindRule fill_rule;
    SPWindRule clip_rule;
    SPStrokeCapType line_cap;
    SPStrokeJoinType line_join;

    enum PaintOrderType
    {
        PAINT_ORDER_NORMAL,
        PAINT_ORDER_FILL,
        PAINT_ORDER_STROKE,
        PAINT_ORDER_MARKER
    };

    std::array<PaintOrderType, 3> paint_order_layer;

    enum TextDecorationLine
    {
        TEXT_DECORATION_LINE_CLEAR       = 0x00,
        TEXT_DECORATION_LINE_SET         = 0x01,
        TEXT_DECORATION_LINE_INHERIT     = 0x02,
        TEXT_DECORATION_LINE_UNDERLINE   = 0x04,
        TEXT_DECORATION_LINE_OVERLINE    = 0x08,
        TEXT_DECORATION_LINE_LINETHROUGH = 0x10,
        TEXT_DECORATION_LINE_BLINK       = 0x20
    };

    enum TextDecorationStyle
    {
        TEXT_DECORATION_STYLE_CLEAR      = 0x00,
        TEXT_DECORATION_STYLE_SET        = 0x01,
        TEXT_DECORATION_STYLE_INHERIT    = 0x02,
        TEXT_DECORATION_STYLE_SOLID      = 0x04,
        TEXT_DECORATION_STYLE_ISDOUBLE   = 0x08,
        TEXT_DECORATION_STYLE_DOTTED     = 0x10,
        TEXT_DECORATION_STYLE_DASHED     = 0x20,
        TEXT_DECORATION_STYLE_WAVY       = 0x40
    };

    int    text_decoration_line;
    int    text_decoration_style;
    Paint  text_decoration_fill;
    Paint  text_decoration_stroke;
    double text_decoration_stroke_width;

    // These are the same as in style.h
    double phase_length;
    bool   tspan_line_start;
    bool   tspan_line_end;
    double tspan_width;
    double ascender;
    double descender;
    double underline_thickness;
    double underline_position; 
    double line_through_thickness;
    double line_through_position;
    double font_size;

    int   text_direction;

    bool background_new = false;
    bool vector_effect_size = false;
    bool vector_effect_rotate = false;
    bool vector_effect_fixed = false;
    bool vector_effect_stroke = false;
    bool stroke_extensions_hairline = false;

    std::shared_ptr<Colors::Space::AnySpace> color_interpolation;

    Cairo::SurfacePattern::Filter image_rendering;

    std::shared_ptr<Pattern> prepareFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const;
    std::shared_ptr<Pattern> prepareStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const;
    std::shared_ptr<Pattern> prepareTextDecorationFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const;
    std::shared_ptr<Pattern> prepareTextDecorationStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const;

    void applyFill(Context &dc, Pattern const &cp) const;
    void applyStroke(Context &dc, Pattern const &cp) const;
    void applyTextDecorationFill(Context &dc, Pattern const &cp) const;
    void applyTextDecorationStroke(Context &dc, Pattern const &cp) const;
    void invalidate();

private:
    struct CachedPattern
    {
        // Compile problems!
        mutable std::unique_ptr<InitLock> _inited;
        mutable std::shared_ptr<Pattern> pattern;

        InitLock &inited() const {
            if (!_inited) {
                _inited = std::make_unique<InitLock>();
            }
            return *_inited;
        }
        void reset()
        {
            if (_inited) {
                _inited->reset();
            }
            pattern.reset();
        }
    };

    std::shared_ptr<Pattern> preparePaint(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern, DrawingStyle::Paint const &paint, CachedPattern const &cp) const;

    CachedPattern fill_pattern;
    CachedPattern stroke_pattern;
    CachedPattern text_decoration_fill_pattern;
    CachedPattern text_decoration_stroke_pattern;
};

} // namespace Inkscape::Renderer

#endif // INKSCAPE_RENDERER_DRAWING_STYLE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
