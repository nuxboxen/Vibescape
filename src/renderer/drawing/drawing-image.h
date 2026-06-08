// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Bitmap image belonging to an SVG drawing.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_IMAGE_H
#define INKSCAPE_RENDERER_DRAWING_IMAGE_H

#include <memory>
#include <2geom/transforms.h>

#include "renderer/surface.h"

#include "drawing-item.h"

namespace Inkscape::Renderer {

class DrawingImage
    : public DrawingItem
{
public:
    DrawingImage(Drawing &drawing);
    int tag() const override { return tag_of<decltype(*this)>; }

    void setImage(std::shared_ptr<Surface const> image);
    void setScale(double sx, double sy);
    void setOrigin(Geom::Point const &o);
    void setClipbox(Geom::Rect const &box);
    void setExtend(Cairo::Pattern::Extend extend);
    Geom::Rect imageBounds() const;
    Geom::Rect bounds() const;

protected:
    ~DrawingImage() override = default;

    unsigned _updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset) override;
    unsigned _renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const override;
    DrawingItem *_pickItem(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags) override;

    std::shared_ptr<Surface const> _image;

    // TODO: the following three should probably be merged into a new Geom::Viewbox object
    Geom::Rect _clipbox; ///< for preserveAspectRatio
    Geom::Point _origin;
    Geom::Scale _scale;
    Cairo::Pattern::Extend _extend;
};

} // namespace Inkscape

#endif // INKSCAPE_RENDERER_DRAWING_IMAGE_H

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
