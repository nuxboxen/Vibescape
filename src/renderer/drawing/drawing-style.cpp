// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Style information for rendering.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "colors/manager.h"

#include "renderer/context.h"
#include "renderer/context-pattern.h"
#include "renderer/surface.h"

#include "drawing-style.h"
#include "drawing-options.h"
#include "drawing-pattern.h"

namespace Inkscape::Renderer {

bool DrawingStyle::Paint::ditherable() const
{
    return type == PaintType::SERVER && server && server->ditherable();
}

std::shared_ptr<Pattern> DrawingStyle::preparePaint(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern, DrawingStyle::Paint const &paint, CachedPattern const &cp) const
{
    auto space = color_interpolation ? color_interpolation : dc.getColorSpace();

    if (paint.type == DrawingStyle::PaintType::SERVER && pattern) {
        // If a DrawingPattern, then always regenerate the pattern, because it may depend on 'area'.
        // Even if not, regenerating the pattern is a no-op because DrawingPattern has a cache.
        return pattern->renderPattern(rc, area, space, paint.opacity);
    }

    // Otherwise, init or re-use cached pattern.
    cp.inited->init([&] {
        // Handle remaining non-DrawingPattern cases.
        switch (paint.type) {
            case DrawingStyle::PaintType::SERVER:
                if (paint.server) {
                    cp.pattern = paint.server->create_pattern(&dc, paintbox, paint.opacity);
                    cp.pattern->setDither(rc.dithering && paint.server->ditherable());
                } else {
                    cp.pattern = std::make_shared<SolidColorPattern>(Colors::Color(0x0));
                }
                break;
            case DrawingStyle::PaintType::COLOR: {
                auto color = paint.color->withOpacity(paint.opacity).converted(space);
                cp.pattern = std::make_shared<SolidColorPattern>(*color);
                break;
            }
            case DrawingStyle::PaintType::CONTEXT:
            case DrawingStyle::PaintType::NONE:
                cp.pattern.reset();
                break;
        }
    });
    return cp.pattern;
}

std::shared_ptr<Pattern> DrawingStyle::prepareFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const
{
    return preparePaint(dc, rc, area, paintbox, pattern, fill, fill_pattern);
}

std::shared_ptr<Pattern> DrawingStyle::prepareStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const
{
    return preparePaint(dc, rc, area, paintbox, pattern, stroke, stroke_pattern);
}

std::shared_ptr<Pattern> DrawingStyle::prepareTextDecorationFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const
{
    return preparePaint(dc, rc, area, paintbox, pattern, text_decoration_fill, text_decoration_fill_pattern);
}

std::shared_ptr<Pattern> DrawingStyle::prepareTextDecorationStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, Geom::OptRect const &paintbox, DrawingPattern const *pattern) const
{
    return preparePaint(dc, rc, area, paintbox, pattern, text_decoration_stroke, text_decoration_stroke_pattern);
}

void DrawingStyle::applyFill(Context &dc, Pattern const &cp) const
{
    dc.setSource(cp);
    dc.setFillRule(fill_rule);
}

void DrawingStyle::applyTextDecorationFill(Context &dc, Pattern const &cp) const
{
    dc.setSource(cp);
    // Fill rule does not matter, no intersections.
}

void DrawingStyle::applyStroke(Context &dc, Pattern const &cp) const
{
    dc.setSource(cp);
    if (hairline) {
        dc.setHairline();
    } else {
        dc.setLineWidth(stroke_width);
    }
    dc.setLineCap(line_cap);
    dc.setLineJoin(line_join);
    dc.setMiterLimit(miter_limit);
    dc.setDash(dash, dash_offset);
}

void DrawingStyle::applyTextDecorationStroke(Context &dc, Pattern const &cp) const
{
    dc.setSource(cp);
    if (hairline) {
        dc.setHairline();
    } else {
        dc.setLineWidth(text_decoration_stroke_width);
    }
    dc.setLineCap(SP_STROKE_LINECAP_BUTT);
    dc.setLineJoin(SP_STROKE_LINEJOIN_MITER);
    dc.setMiterLimit(miter_limit);
    dc.setDash({}, 0.0);
}

void DrawingStyle::invalidate()
{
    // force pattern update
    fill_pattern.reset();
    stroke_pattern.reset();
    text_decoration_fill_pattern.reset();
    text_decoration_stroke_pattern.reset();
}

} // namespace Inkscape

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
