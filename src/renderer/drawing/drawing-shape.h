// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Group belonging to an SVG drawing element.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_SHAPE_H
#define INKSCAPE_RENDERER_DRAWING_SHAPE_H

#include "drawing-item.h"

namespace Inkscape::Renderer {

class DrawingShape
    : public DrawingItem
{
public:
    DrawingShape(Drawing &drawing);

    int tag() const override { return tag_of<decltype(*this)>; }

    void setPath(std::shared_ptr<Geom::PathVector const> curve);

protected:
    ~DrawingShape() override = default;

    unsigned _updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset) override;
    unsigned _renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const override;
    void _clipItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const override;
    DrawingItem *_pickItem(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags) override;
    bool _canClip() const override { return true; }

    void _renderFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const;
    void _renderStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags) const;
    void _renderMarkers(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const;

    std::shared_ptr<Geom::PathVector const> _curve;

    DrawingItem *_last_pick;
    unsigned _repick_after;
};

} // namespace Inkscape::Renderer

#endif // INKSCAPE_RENDERER_DRAWING_SHAPE_H

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
