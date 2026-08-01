// SPDX-License-Identifier: GPL-2.0-or-later

#include "graphics.h"

#include <2geom/parallelogram.h>

#include "ui/util.h"
#include "util.h"

#include "renderer/context.h"
#include "renderer/context-pattern.h"

namespace Inkscape::UI::Widget {

// Paint the background and pages using Cairo into the given fragment.
void Graphics::paint_background(Fragment const &fragment, PageInfo const &pi,
                                std::uint32_t const page, std::uint32_t const desk,
                                Renderer::Context cr)
{
    cr.set_operator(Cairo::Context::Operator::SOURCE);
    cr.transform(Geom::Translate(fragment.rect.min()).inverse());
    cr.rectangle(fragment.rect);
    cr.clip();
    
    auto page_color = *Colors::Color(page).converted(cr.getColorSpace());
    auto desk_color = *Colors::Color(desk).converted(cr.getColorSpace());

    // Desk and page are the same, or a single page fills the whole screen; just clear the fragment to page.
    bool nodesk = desk == page || check_single_page(fragment, pi);

    if (!nodesk) {
        // Paint the background to the complement of the pages. (Slightly overpaints when pages overlap.)
        cr.setSource(Renderer::CheckerboardPattern(desk_color, 6));
        cr.set_fill_rule(Cairo::Context::FillRule::EVEN_ODD);
        cr.rectangle(fragment.rect);

        cr.save();
        cr.transform(fragment.affine);
        for (auto &rect : pi.pages) {
            cr.rectangle(rect);
        }
        cr.fill();
        cr.restore();
    }

    // Paint the pages.
    cr.setSource(Renderer::CheckerboardPattern(page_color, 6));
    if (!nodesk) {
        cr.transform(fragment.affine);
        for (auto &rect : pi.pages) {
            cr.rectangle(rect);
        }
        cr.fill();
    } else {
        cr.paint(); // Everything
    }
}

std::pair<Geom::IntRect, Geom::IntRect> Graphics::calc_splitview_cliprects(Geom::IntPoint const &size, Geom::Point const &split_frac, Renderer::SplitDirection split_direction)
{
    auto window = Geom::IntRect({0, 0}, size);

    auto content = window;
    auto outline = window;
    auto split = [&] (Geom::Dim2 dim, Geom::IntRect &lo, Geom::IntRect &hi) {
        int s = std::round(split_frac[dim] * size[dim]);
        lo[dim].setMax(s);
        hi[dim].setMin(s);
    };

    switch (split_direction) {
        case Renderer::SplitDirection::NORTH: split(Geom::Y, content, outline); break;
        case Renderer::SplitDirection::EAST:  split(Geom::X, outline, content); break;
        case Renderer::SplitDirection::SOUTH: split(Geom::Y, outline, content); break;
        case Renderer::SplitDirection::WEST:  split(Geom::X, content, outline); break;
        default: assert(false); break;
    }

    return std::make_pair(content, outline);
}

void Graphics::paint_splitview_controller(Geom::IntPoint const &size, Geom::Point const &split_frac, Renderer::SplitDirection split_direction, Renderer::SplitDirection hover_direction, std::shared_ptr<Renderer::Context> const &cr)
{
    auto split_position = (split_frac * size).round();

    // Add dividing line.
    cr->setSource(Colors::Color(0xff));
    cr->set_line_width(1.0);
    if (split_direction == Renderer::SplitDirection::EAST ||
        split_direction == Renderer::SplitDirection::WEST) {
        cr->move_to(split_position.x() + 0.5, 0.0     );
        cr->line_to(split_position.x() + 0.5, size.y());
        cr->stroke();
    } else {
        cr->move_to(0.0     , split_position.y() + 0.5);
        cr->line_to(size.x(), split_position.y() + 0.5);
        cr->stroke();
    }

    // Add controller image.
    double a = hover_direction == Renderer::SplitDirection::NONE ? 0.5 : 1.0;
    cr->setSource(Colors::Color(cr->getColorSpace(), {0.2, 0.2, 0.2, a}));
    cr->arc(split_position.x(), split_position.y(), 20, 0, 2 * M_PI);
    cr->fill();

    for (int i = 0; i < 4; i++) {
        // The four direction triangles.
        cr->save();

        // Position triangle.
        cr->translate(split_position.x(), split_position.y());
        cr->rotate((i + 2) * M_PI / 2);

        // Draw triangle.
        cr->move_to(-5,  8);
        cr->line_to( 0, 18);
        cr->line_to( 5,  8);
        cr->close_path();

        double b = (int)hover_direction == (i + 1) ? 0.9 : 0.7;
        cr->setSource(Colors::Color(cr->getColorSpace(), {b, b, b, a}));
        cr->fill();

        cr->restore();
    }
}

bool Graphics::check_single_page(Fragment const &view, PageInfo const &pi)
{
    auto pl = Geom::Parallelogram(view.rect) * view.affine.inverse();
    return std::any_of(pi.pages.begin(), pi.pages.end(), [&] (auto &rect) {
        return Geom::Parallelogram(rect).contains(pl);
    });
}

} // namespace Inkscape::UI::Widget
