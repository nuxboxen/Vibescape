// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Utility functions for generating export previews.
 */
/* Authors:
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2021 Anshudhar Kumar Singh
 *               2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "preview.h"

#include <2geom/generic-rect.h>
#include "colors/manager.h"
#include "renderer/context.h"
#include "renderer/context-pattern.h"
#include "renderer/surface.h"
#include "renderer/drawing/drawing-item.h"

namespace Inkscape::UI::Preview {

std::shared_ptr<Renderer::Surface>
render_preview(SPDocument *doc, std::shared_ptr<Renderer::Drawing> drawing, Colors::Color bg,
               Renderer::DrawingItem *item, unsigned width_in, unsigned height_in, Geom::Rect const &dboxIn)
{
    if (!drawing->root())
        return {};

    // Calculate a scaling factor for the requested bounding box.
    double sf = 1.0;
    Geom::IntRect ibox = dboxIn.roundOutwards();
    if (ibox.width() != width_in || ibox.height() != height_in) {
        // Adjust by one pixel to fit in anti-aliasing pixels
        sf = std::min((double)(width_in - 1) / dboxIn.width(),
                      (double)(height_in - 1) / dboxIn.height());
        auto scaled_box = dboxIn * Geom::Scale(sf);
        ibox = scaled_box.roundOutwards();
    }

    auto pdim = Geom::IntPoint(width_in, height_in);
    // The unsigned width/height can wrap around when negative.
    int dx = ((int)width_in - ibox.width()) / 2;
    int dy = ((int)height_in - ibox.height()) / 2;
    auto area = Geom::IntRect::from_xywh(ibox.min() - Geom::IntPoint(dx, dy), pdim);

    /* Actual renderable area */
    auto const ua = Geom::intersect(ibox, area);
    if (!ua) {
        return {};
    }

    auto color_space = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto surface = std::make_shared<Renderer::Surface>(ua->dimensions(), 1.0, color_space);

    auto on_error = [&] (char const *err) {
        std::cerr << "render_preview: " << err << std::endl;
    };

    try {
        auto cr = Renderer::Context(*surface);
        cr.rectangle(Geom::Rect(0, 0, ua->width(), ua->height()));

        if (bg.hasOpacity()) {
            cr.paint(Renderer::CheckerboardPattern(bg, 6.0));
        }

        // We always draw the background on top to indicate partial backgrounds.
        cr.setSource(bg);
        cr.fill();

        // Resize the contents to the available space with a scale factor.
        drawing->root()->setTransform(Geom::Scale(sf));
        drawing->update();

        auto dc = Renderer::Context(*surface);
        dc.transform(Geom::Translate(-ua->min()));
        if (item) {
            // Render just one item
            item->render(dc, *ua);
        } else {
            // Render drawing.
            drawing->render(dc, *ua);
        }
    } catch (std::bad_alloc const &e) {
        on_error(e.what());
    } catch (Cairo::logic_error const &e) {
        on_error(e.what());
    }

    return surface;
}

} // namespace Preview::UI::Inkscape
