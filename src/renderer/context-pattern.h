// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing patterns in cairo
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H
#define SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H

#include <array>
#include <memory>
#include <2geom/forward.h>
#include <2geom/rect.h>
#include <cairomm/pattern.h>

#include "object/sp-paint-server-data.h"

#include "colors/forward.h"

namespace Inkscape::Renderer {

class Surface;

/**
 * See: https://www.w3.org/TR/css-easing-2/#typedef-cubic-bezier-easing-function
 */
class CubicBezierEasingSteps : public std::vector<double> {
public:
    CubicBezierEasingSteps(Geom::Point P1, Geom::Point P2, unsigned steps = 15);
    void reverse() {
        std::reverse(begin(), end());
    }
    void invert() {
        reverse();
        std::vector<double> a = *this;
        erase(begin(), end());
        for (auto v : a) {
            push_back(1 - v);
        }
    }
};

class Pattern
{
    class FirstColorStopInEasingError : public std::exception {
    public:
        FirstColorStopInEasingError() = default;
        const char* what() const noexcept override {
            return "Easing functions can not be used for the first stop.";
        }
    };

protected:
    Pattern(std::shared_ptr<Colors::Space::AnySpace> const &space)
        : _color_space(space)
    {}

    std::vector<Cairo::RefPtr<Cairo::Pattern>> _pts;
    std::shared_ptr<Colors::Space::AnySpace> _color_space;

    void setSurface(Surface const &surface);
public:
    Pattern(Surface const &surface);

    auto &getCairoPatterns() const { return _pts; }
    auto getColorSpace() const { return _color_space; }

    void setFilter(Cairo::SurfacePattern::Filter filter);
    void setExtend(Cairo::Pattern::Extend extend);
    void setExtend(SPGradientSpread spread);
    void setMatrix(Geom::Affine const &m, Geom::OptRect const &rect = {});
    void setDither(bool enable);

    void addColorStop(double pos, Colors::Color color);
    void addColorStop(double offset, Colors::Color const &color, std::vector<double> const &ease);
    void copyColorStops(Pattern const &other);
    std::pair<double, Colors::Color> getColorStop(int stop) const;
    int numColorStops() const;

    static Geom::Affine rectToMatrix(Geom::OptRect const &rect);
};

class SolidColorPattern : public Pattern
{
public:
    SolidColorPattern(Colors::Color solid_color);
};

class LinearGradientPattern : public Pattern
{
public:
    LinearGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double x0, double y0, double x1, double y1);
};

class RadialGradientPattern : public Pattern
{
public:
    RadialGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double cx0, double cy0, double cr0, double cx1, double cy1, double cr1);
};

class MeshGradientPattern : public Pattern
{
public:
    MeshGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space);
    ~MeshGradientPattern();

    void beginPatch();
    void endPatch();
    void moveTo(Geom::Point const &p);
    void lineTo(Geom::Point const &p);
    void curveTo(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2);
    void setControlPoint(int corner, Geom::Point const &p);
    void setCornerColor(int corner, Colors::Color color);
};

class CheckerboardPattern : public Pattern
{
public:
    CheckerboardPattern(Colors::Color color, int size);
    CheckerboardPattern(Colors::Color color1, Colors::Color color2, int size);

    static Colors::Color darker(Colors::Color color) {
        auto opacity = color.stealOpacity();
        return Colors::make_contrasted_color(color, 1.0 - opacity);
    }
private:
    std::shared_ptr<Surface> _surface;
};

class StripesPattern : public Pattern
{
public:
    StripesPattern(Colors::Color color);
private:
    std::shared_ptr<Surface> _surface;
};

} // end namespace Inkscape::Renderer

#endif // !SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H

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
