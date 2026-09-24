// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing context for Inkscape pixel surfaces.
 *//*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011-2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_CONTEXT_H
#define SEEN_INKSCAPE_RENDERER_CONTEXT_H

#include <memory>
#include <2geom/transforms.h>
#include <2geom/point.h>
#include <2geom/rect.h>
#include <cairomm/context.h>
#include <cairomm/region.h>
#include <glibmm/refptr.h>

#include "colors/forward.h"
#include "renderer/enums.h"

namespace Pango {
class Layout;
}

namespace Inkscape::Renderer {

class Pattern;
class PatternComplex;
class Surface;

Cairo::Matrix geom_to_cairo(const Geom::Affine &affine);
Cairo::RectangleInt geom_to_cairo(Geom::IntRect const &rect);
Geom::IntRect cairo_to_geom(const Cairo::RectangleInt &rect);
std::vector<Geom::IntRect> cairo_to_geom(const Cairo::Region &region);
Geom::Affine rect_to_matrix(Geom::OptRect const &bbox);
Geom::Affine viewbox_matrix(Geom::Affine const &m, Geom::OptRect const &rect);

/**
 * @class Context
 * Maximal wrapper over Cairo::Context for drawing on multiple surfaces.
 */
class Context
{
public:
    Context(Context const &parent);
    Context(Surface &surface, Geom::IntPoint logicalBounds = {}, Geom::Scale const &scale = {});
    ~Context();

    /**
     * For widget drawing thar wants to use the advanced features of the renderer, we allow this constructore.
     */
    Context(Cairo::RefPtr<Cairo::Context> ct);

    /** Compatibility for old code, basically the same thing as making a copy of Context */
    class Save
    {
        Context *_ct;
public:
        Save(Context &ct) : _ct(new Context(ct)) {}
        Save(Save &sv) : _ct(new Context(*sv._ct)) {}
        ~Save() { delete _ct; }
    };

    class SaveRestoreError : public std::exception {
public:
        SaveRestoreError() = default;
        const char* what() const noexcept override {
            return "Context is already saved and can not be saved again without destroying the child first";
        }
    };

    // Wrapper around Cairo::Contexts with Cairo's API, some of Cairo's APIs are not expressed
    // delibrately in order to prevent the use of Cairo Surfaces, or Cairo Pattern objects which
    // Would not work with this multi-surface context construct.
    void save() { for (auto &ct : _cts) { ct->save(); } }
    void restore() { for (auto &ct : _cts) { ct->restore(); } }
    void flush() { for (auto &ct : _cts) { ct->get_target()->flush(); } }

    // These functions are VERY BAD as they subvert our own control over the surface pixels, Remove if possible
    void push_group() { for (auto &ct : _cts) { ct->push_group(); } }
    void push_group_with_content(Cairo::Content type) { for (auto &ct : _cts) { ct->push_group_with_content(type); } }
    void pop_group_to_source() { for (auto &ct : _cts) { ct->pop_group_to_source(); } }

      // Cairo transform
    void transform(Cairo::Matrix const &matrix) { for (auto &ct : _cts) { ct->transform(matrix); } }
    void translate(double x, double y) { for (auto &ct : _cts) { ct->translate(x, y); } }
    void rotate(double r) { for (auto &ct : _cts) { ct->rotate(r); } }
    void scale(double x, double y) { for (auto &ct : _cts) { ct->scale(x, y); } }

      // Cairo paths
    void begin_new_path() { for (auto &ct : _cts) { ct->begin_new_path(); } }
    void begin_new_sub_path() { for (auto &ct : _cts) { ct->begin_new_sub_path(); } }
    void move_to(double x, double y) { for (auto &ct : _cts) { ct->move_to(x , y); } }
    void line_to(double x, double y) { for (auto &ct : _cts) { ct->line_to(x , y); } }
    void curve_to(double x1, double y1, double x2, double y2, double x3, double y3) { for (auto &ct : _cts) { ct->curve_to(x1, y1, x2, y2, x3, y3); } }
    void arc(double cx, double cy, double r, double a1, double a2) { for (auto &ct : _cts) { ct->arc(cx, cy, r, a1, a2); } }
    void arc_negative(double cx, double cy, double r, double a1, double a2) { for (auto &ct : _cts) { ct->arc_negative(cx, cy, r, a1, a2); } }
    void close_path() { for (auto &ct : _cts) { ct->close_path(); } }
    void rectangle(double l, double r, double w, double h) { for (auto &ct : _cts) { ct->rectangle(l, r, w, h); } }

      // Cairo painting
    void paint(double alpha = 1.0);
    void fill() { for (auto &ct : _cts) { ct->fill(); } }
    void fill_preserve() { for (auto &ct : _cts) { ct->fill_preserve(); } }
    void stroke() { for (auto &ct : _cts) { ct->stroke(); } }
    void stroke_preserve() { for (auto &ct : _cts) { ct->stroke_preserve(); } }
    void clip() { for (auto &ct : _cts) { ct->clip(); } }
    void reset_clip() { for (auto &ct : _cts) { ct->reset_clip(); } }

      // Cairo style
    void set_line_width(double w) { for (auto &ct : _cts) { ct->set_line_width(w); } }
    void set_line_cap(Cairo::Context::LineCap cap) { for (auto &ct : _cts) { ct->set_line_cap(cap); } }
    void set_line_join(Cairo::Context::LineJoin join) { for (auto &ct : _cts) { ct->set_line_join(join); } }
    void set_fill_rule(Cairo::Context::FillRule rule) { for (auto &ct : _cts) { ct->set_fill_rule(rule); } }
    void set_operator(Cairo::Context::Operator op) { for (auto &ct : _cts) { ct->set_operator(op); } }
    void set_miter_limit(double miter) { for (auto &ct : _cts) { ct->set_miter_limit(miter); } }
    void set_dash(std::vector<double> const &dashes, double offset) { for (auto &ct : _cts) { ct->set_dash(dashes, offset); } }
    void set_antialias(Cairo::Antialias antialias) { for (auto &ct : _cts) { ct->set_antialias(antialias); } }
    void set_tolerance(double tol) { for(auto &ct : _cts) { ct->set_tolerance(tol); } }

      // Cairo get data
    Cairo::Context::Operator get_operator() { return _cts[0]->get_operator(); }
    Cairo::Antialias get_antialias() const { return _cts[0]->get_antialias(); }
    double get_tolerance() const { return _cts[0]->get_tolerance(); }

    // Inkscape data input APIs
    void pushGroup() { push_group(); }
    void pushAlphaGroup() { push_group_with_content(Cairo::Content::CONTENT_COLOR_ALPHA); }
    void popGroupToSource() { pop_group_to_source(); }

      // Inkscape transform
    void transform(Geom::Affine const &affine);
    void translate(Geom::Translate const &translate) { transform(translate); }
    void rotate(Geom::Rotate const &rotate) { transform(rotate); }
    void scale(Geom::Scale const &scale) { transform(scale); }

      // Inkscape paths
    void moveTo(Geom::Point const &p) { for (auto &ct : _cts) { ct->move_to(p[Geom::X], p[Geom::Y]); } }
    void lineTo(Geom::Point const &p) { for (auto &ct : _cts) { ct->line_to(p[Geom::X], p[Geom::Y]); } }
    void curveTo(Geom::Point const &p1, Geom::Point const &p2, Geom::Point const &p3) { curve_to(p1[Geom::X], p1[Geom::Y], p2[Geom::X], p2[Geom::Y], p3[Geom::X], p3[Geom::Y]); }
    void arc(Geom::Point const &center, double radius, Geom::AngleInterval const &angle);
    void closePath() { for (auto &ct : _cts) { ct->close_path(); } }

    template <typename Rect>
    void rectangle(Rect const &r, double radius = 0.0)
        requires (std::is_base_of<Geom::GenericRect<int>, Rect>::value
               || std::is_base_of<Geom::GenericRect<double>, Rect>::value)
    {
        if (radius > 0.0) {
            auto x = r.left(); auto width = r.width();
            auto y = r.top(); auto height = r.height();
            arc(x + width - radius, y + radius, radius, -M_PI_2, 0);
            arc(x + width - radius, y + height - radius, radius, 0, M_PI_2);
            arc(x + radius, y + height - radius, radius, M_PI_2, M_PI);
            arc(x + radius, y + radius, radius, M_PI, 3 * M_PI_2);
            close_path();
        } else {
            rectangle(r.left(), r.top(), r.width(), r.height());
        }
    }
    template <typename Rect>
    void rectangle(Rect const &r)
        requires (std::is_base_of<Cairo::RectangleInt, Rect>::value
               || std::is_base_of<Cairo::Rectangle, Rect>::value)
    {
        rectangle(r.x, r.y, r.width, r.height);
    }
    void rectangles(Cairo::RefPtr<Cairo::Region> const &region);
    // Used in drawing-text.cpp to overwrite glyphs, which have the opposite path rotation as a regular rect
    template <typename Rect>
    void reversedRectangle(Rect const &r) {
        for(auto &ct : _cts) {
            ct->move_to(r.left(), r.top());
            ct->rel_line_to(0, r.height());
            ct->rel_line_to(r.width(), 0);
            ct->rel_line_to(0, -r.height());
            ct->close_path();
        }
    }
    void circle(const Geom::Point& center, double radius) {
        arc(center.x(), center.y(), radius, 0, 2 * M_PI);
    }
    void newPath() { begin_new_path(); }
    void newSubpath() { begin_new_sub_path(); }
    void path(Geom::PathVector const &pv);
    void path(Geom::PathVector const &pv, Geom::Affine trans, Geom::OptRect area, bool optimize_stroke, double stroke_width);

      // Inkscape painting
    void paint(Colors::Color const &color);
    void paint(Pattern const &pattern, double scale = 1.0);
    void paint(PatternComplex const &c);
    void mask(Surface const &surface);
    void fillPreserve() { fill_preserve(); }
    void strokePreserve() { stroke_preserve(); }
    void setSource(Colors::Color const &color);
    void setSource(Surface const &surface, double x = 0, double y = 0,
                   std::optional<Cairo::SurfacePattern::Filter> filter = {},
                   std::optional<Cairo::Pattern::Extend> extend = {});
    void setSource(Pattern const &pattern);
    void resetSource(double a = 1.0) { for (auto &ct : _cts) { ct->set_source_rgba(0, 0, 0, a); } }

    // This is used by some canvas items to draw non-svg text
    void paintLayout(Glib::RefPtr<Pango::Layout> layout);
    void paintText(std::string const &font_desc, std::string const &text);
    void paintDropShadow(const Geom::Rect& rect, double size, Colors::Color const &color);

      // Inkscape style
    void setHairline();
    void setLineWidth(double w) { set_line_width(w); }
    void setLineCap(SPStrokeCapType cap);
    void setLineJoin(SPStrokeJoinType join);
    void setOperator(SPBlendMode op);
    void setFillRule(SPWindRule rule);
    void setDash(std::vector<double> const &dashes, double offset) { set_dash(dashes, offset); }
    void setMiterLimit(double miter) { set_miter_limit(miter); }
    void setAntialiasing(Antialiasing antialias);

    Geom::Point user_to_device_distance(Geom::Point const &pt) {
        double x = pt.x(), y = pt.y();
        cairo_user_to_device_distance(_cts[0]->cobj(), &x, &y); // Cairomm 1.16
        return {x, y};
    }
    Geom::Point device_to_user_distance(Geom::Point const &pt) {
        double x = pt.x(), y = pt.y();
        cairo_device_to_user_distance(_cts[0]->cobj(), &x, &y);
        return {x, y};
    }

    // This breaks the barrier between context and surface layers, so don't use it unless you know
    // exactly why you need to. It should be possible to design this away.
    int getDeviceScale() const { return _device_scale; }
    // The color space of the surface might be empty, this means sRGB in Integer format
    std::shared_ptr<Colors::Space::AnySpace> getSurfaceColorSpace() const { return _surface_color_space; }
    // The context's color space is always set, either as the surface's or sRGB itself
    std::shared_ptr<Colors::Space::AnySpace> getColorSpace() const;
    Geom::IntPoint getDimensions() const { return _dimensions; }
    Geom::IntRect logicalBounds() const { return Geom::IntRect::from_xywh(_origin, getDimensions()); }
    cairo_format_t getSurfaceFormat() const { return _format; }

#ifdef UNIT_TEST
    bool hasParent() const { return _parent; }
    bool hasChild() const { return _child; }
#endif
private:

    std::vector<Cairo::RefPtr<Cairo::Context>> _cts;
    Geom::IntPoint _origin;
    cairo_format_t _format;
    int _device_scale;
    Geom::IntPoint _dimensions;
    std::shared_ptr<Colors::Space::AnySpace> _surface_color_space;

    // Save mechanism
    Context const *_parent = nullptr;
    mutable Context *_child = nullptr;
};

} // end namespace Inkscape::Renderer

#endif // !SEEN_INKSCAPE_RENDERER_CONTEXT_H

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
