// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing multiple patterns in a complex
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_COMPLEX_H
#define SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_COMPLEX_H

#include <2geom/rect.h>
#include "colors/forward.h"
#include "context-pattern.h"

namespace Inkscape::Renderer {

class Context;
class CubicBezierEasingSteps;

// Paint a bunch of patterns
class PatternComplex
{
public:
    virtual void paint(Context ct) const = 0;
};

/**
 * A shadow build fromm gradients
 */
class GradientShadow : public PatternComplex
{
    enum Patch {
        TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT,
        TOP, RIGHT, BOTTOM, LEFT
    };
public:
    using Gradients = std::pair<RadialGradientPattern, LinearGradientPattern>;

    /**
     * Prepare a gradient shadow for painting. This is not a Pattern, but a PatternComplex.
     *
     * @arg rect - The original size of the rectangle that would cast this shadow
     * @arg size - How much bigger is the shadow (far away from the surface in Z)
     * @arg bias - How much to shift the rect inside it's shadow. This is not a translation
     *             The width and height of the shadow fridges will be changed.
     * @arg color - The color of the shadow to paint.
     */
    GradientShadow(Geom::Rect const &rect, double size, Geom::Point const &bias, Colors::Color const &color);

    void paint(Context ct) const override;
private:
    Gradients _build_gradients(Colors::Color const &color) const;
    void _paint(Context &ct, Gradients &gradient) const;

    Colors::Color const &_color;
    CubicBezierEasingSteps _ease;

    std::array<Geom::Rect, 8> _rects; // 8 == Patch::size
    mutable std::optional<Gradients> _default_gradients;
};

} // end namespace Inkscape::Renderer

#endif // !SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_COMPLEX_H

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
