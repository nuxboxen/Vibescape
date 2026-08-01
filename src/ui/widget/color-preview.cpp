// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Michael Kowalski
 *
 * Copyright (C) 2001-2005 Authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/rect.h>

#include "colors/color.h"
#include "colors/manager.h"
#include "ui/util.h"
#include "ui/widget/color-preview.h"
#include "util/theme-utils.h"

#include "renderer/context.h"
#include "renderer/context-pattern.h"

namespace Inkscape::UI::Widget {

ColorPreview::ColorPreview(std::uint32_t const rgba)
    : _rgba{rgba}
{
    construct();
}

ColorPreview::ColorPreview(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder) :
    Glib::ObjectBase("InkSpinButton"),
    Gtk::DrawingArea(cobject),
    _rgba(0) {

    construct();
}

void ColorPreview::construct() {
    set_name("ColorPreview");
    set_draw_func(sigc::mem_fun(*this, &ColorPreview::draw_func));
    setStyle(_style);
}

void ColorPreview::setRgba32(std::uint32_t const rgba) {
    _rgba = rgba;
    _pattern = {};
    queue_draw();
}
void ColorPreview::setPattern(std::shared_ptr<Renderer::Pattern> pattern)
{
    if (_pattern == pattern) return;

    _pattern = pattern;
    _rgba = 0;
    queue_draw();
}

Geom::Rect round_rect(const Cairo::RefPtr<Cairo::Context>& ctx, Geom::Rect rect, double radius) {
    auto x = rect.left();
    auto y = rect.top();
    auto width = rect.width();
    auto height = rect.height();
    ctx->arc(x + width - radius, y + radius, radius, -M_PI_2, 0);
    ctx->arc(x + width - radius, y + height - radius, radius, 0, M_PI_2);
    ctx->arc(x + radius, y + height - radius, radius, M_PI_2, M_PI);
    ctx->arc(x + radius, y + radius, radius, M_PI, 3 * M_PI_2);
    ctx->close_path();
    return rect.shrunkBy(1);
}

void ColorPreview::draw_func(Cairo::RefPtr<Cairo::Context> const &ct,
                             int const widget_width, int const widget_height)
{
    auto cr = std::make_shared<Renderer::Context>(ct);

    double width = widget_width;
    double height = widget_height;
    auto x = 0.0;
    auto y = 0.0;
    double radius = _style == Simple ? 0.0 : 2.0;
    if (_radius >= 0) radius = _radius;
    auto rect = Geom::Rect(x, y, x + width, y + height);

    auto white = Colors::Color(0xffffffff);
    auto black = Colors::Color(0xff);
    auto outline_color = white;
    auto border_color = black;

    bool dark_theme = Util::is_current_theme_dark(*this);
    auto state = get_state_flags();
    auto disabled = (state & Gtk::StateFlags::INSENSITIVE) == Gtk::StateFlags::INSENSITIVE;
    auto backdrop = (state & Gtk::StateFlags::BACKDROP) == Gtk::StateFlags::BACKDROP;
    if (dark_theme) {
        std::swap(outline_color, border_color);
    }

    Colors::Color check_color(dark_theme ? 0x606060ff : 0xe0e0e0ff, false);
    auto checkers = Renderer::CheckerboardPattern(check_color, _checkerboard_tile_size);
    checkers.setMatrix(Geom::Translate(-x, -y));

    if (_style == Outlined) {
        // outside outline
        cr->rectangle(rect, radius--);
        rect.shrinkBy(1);
        // opacity of outside outline is reduced
        cr->setSource(outline_color.withOpacity(disabled || backdrop ? 0.2 : 0.4));
        cr->fill();

        // inside border
        cr->rectangle(rect, radius--);
        rect.shrinkBy(1);
        cr->setSource(border_color);
        cr->fill();
    }

    if (_pattern) {
        // draw pattern-based preview
        cr->rectangle(rect, radius);

        // checkers first
        cr->setSource(checkers);
        cr->fill_preserve();

        // Set the user unit so the existing gradient will paint on any size widget correctly.
        _pattern->setMatrix(Geom::Affine(), Geom::Rect(0, 0, width, height));
        cr->setSource(*_pattern);
        cr->fill();
    }
    else {
        // color itself
        auto color = Colors::Color(_rgba);
        // if preview is disabled, render colors with reduced saturation and intensity
        if (disabled) {
            color = make_disabled_color(color, dark_theme);
        }
        auto side_rect = Geom::Rect::from_xywh(rect.min(), Geom::Point(rect.width() / 2, rect.height()));

        // solid on the right
        {
            auto right = *cr;
            right.rectangle(side_rect);
            right.clip();
            right.rectangle(rect, radius);
            right.setSource(color);
            right.fill();
        }
        // Semi-transparent on the left
        {
            auto left = *cr;
            left.rectangle(side_rect * Geom::Translate(side_rect.width(), 0));
            left.clip();
            left.rectangle(rect, radius);
            left.setSource(color);
            left.fill();
            if (color.getOpacity() < 1.0) {
                left.setSource(checkers);
                left.fill_preserve();
            }
            left.setSource(color);
            left.fill();
        }
    }

    // Draw fill/stroke indicators.
    if (_is_fill || _is_stroke) {
        auto color = Colors::Color(_rgba);
        double const lightness = Colors::get_perceptual_lightness(color);
        auto [gray, alpha] = Colors::get_contrasting_color(lightness);
        cr->setSource(Colors::Color(color.getSpace(), {gray, gray, gray, alpha}));

        // Scale so that the square -1...1 is the biggest possible square centred in the widget.
        auto w = rect.width();
        auto h = rect.height();
        auto minwh = std::min(w, h);
        cr->translate((w - minwh) / 2.0, (h - minwh) / 2.0);
        cr->scale(minwh / 2.0, minwh / 2.0);
        cr->translate(1.0, 1.0);

        if (_is_fill) {
            cr->arc(0.0, 0.0, 0.35, 0.0, 2 * M_PI);
            cr->fill();
        }

        if (_is_stroke) {
            cr->set_fill_rule(Cairo::Context::FillRule::EVEN_ODD);
            cr->arc(0.0, 0.0, 0.65, 0.0, 2 * M_PI);
            cr->arc(0.0, 0.0, 0.5, 0.0, 2 * M_PI);
            cr->fill();
        }
    }

    if (_indicator != None) {
        constexpr double side = 7.5;
        constexpr double line = 1.5; // 1.5 pixels b/c it's a diagonal line, so 1px is too thin
        const auto right = rect.right();
        const auto bottom = rect.bottom();
        if (_indicator & Swatch) {
            // draw swatch color indicator - a black corner
            cr->move_to(right, bottom - side);
            cr->line_to(right, bottom - side + line);
            cr->line_to(right - side + line, bottom);
            cr->line_to(right - side, bottom);
            cr->setSource(white);
            cr->fill();
            cr->move_to(right, bottom - side + line);
            cr->line_to(right, bottom);
            cr->line_to(right - side + line, bottom);
            cr->setSource(black);
            cr->fill();
        }
        else if (_indicator & SpotColor) {
            // draw spot color indicator - a black dot
            cr->move_to(right, bottom);
            cr->line_to(right, bottom - side);
            cr->line_to(right - side, bottom);
            cr->setSource(white);
            cr->fill();
            constexpr double r = 2;
            cr->arc(right - r, bottom - r, r, 0, 2*M_PI);
            cr->setSource(black);
            cr->fill();
        }

        if (_indicator & (LinearGradient | RadialGradient)) {
            auto s = 3.0; // arrow size
            auto h = s / 2; // half the size
            auto min = std::min(width, height);
            auto w = min - 2*s - 2;
            auto cx = std::round(x + width / 2);
            auto cy = std::round(y + height / 2);

            if (_indicator & LinearGradient) {
                auto start = Geom::Point(x + 1 + s + (width - min) / 2, cy);
                const Geom::Point path_linear[] = {
                    {0, h}, {-s, -h}, {s, -h}, {0, h}, {w, 0}, {0, h}, {s, -h}, {-s, -h}, {0, h}
                };
                auto p = start;
                cr->move_to(p.x(), p.y());
                for (auto& d: path_linear) {
                    p += d;
                    cr->line_to(p.x(), p.y());
                }
            }
            else {
                auto start = Geom::Point(cx, y + 1 + s);
                cx = std::min(cx, cy);
                const Geom::Point path_radial[] = {
                    {h, 0}, {-h, -s}, {-h, s}, {h, 0}, {0, cy - s - 1},
                    {cx - s - 1, 0}, {0, h}, {s, -h}, {-s, -h}, {0, h}, {-(cx - s - 1), 0}
                };
                auto p = start;
                cr->move_to(p.x(), p.y());
                for (auto& d : path_radial) {
                    p += d;
                    cr->line_to(p.x(), p.y());
                }
            }
            cr->close_path();
            cr->set_line_width(2.0);
            cr->set_miter_limit(10);
            cr->setSource(white);
            cr->stroke_preserve();
            cr->set_line_width(1.0);
            cr->setSource(black);
            cr->stroke_preserve();
            cr->fill();
        }
    }

    if (_style == Simple && _frame) {
        // subtle outline
        auto const fg = get_color();
        cr->rectangle(0.5, 0.5, rect.width() - 1, rect.height() - 1);
        cr->setSource(Colors::Color(white.getSpace(), {fg.get_red(), fg.get_green(), fg.get_blue(), 0.07}));
        cr->set_line_width(1);
        cr->stroke();
    }
}

void ColorPreview::setStyle(Style style) {
    _style = style;
    if (style == Simple) {
        add_css_class("simple");
    }
    else {
        remove_css_class("simple");
    }
    queue_draw();
}

void ColorPreview::setIndicator(Indicator indicator) {
    if (_indicator != indicator) {
        _indicator = indicator;
        queue_draw();
    }
}

void ColorPreview::set_frame(bool frame) {
    if (_frame != frame) {
        _frame = frame;
        queue_draw();
    }
}

void ColorPreview::set_border_radius(int radius) {
    if (_radius != radius) {
        _radius = radius;
        queue_draw();
    }
}

void ColorPreview::set_checkerboard_tile_size(unsigned size) {
    if (_checkerboard_tile_size != size) {
        _checkerboard_tile_size = size;
        queue_draw();
    }
}

void ColorPreview::set_fill(bool on) {
    _is_fill = on;
    queue_draw();
}

void ColorPreview::set_stroke(bool on) {
    _is_stroke = on;
    queue_draw();
}

void ColorPreview::set_gradient(std::vector<SPGradientStop> const &stops) {
    static auto srgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto lin = std::make_shared<Renderer::LinearGradientPattern>(srgb, 0, 0, 1, 0);
    for (auto const &stop : stops) {
        if (stop.color) {
            lin->addColorStop(stop.offset, *stop.color);
        }
    }
    _pattern = lin; // gradients are types of patterns 
    queue_draw();
}

} // namespace Inkscape::UI::Widget

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
