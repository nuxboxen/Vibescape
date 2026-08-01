// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feTile> implementation.
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "tile.h"

#include "renderer/drawing-filters/tile.h"  // for FilterTile

std::unique_ptr<Inkscape::Renderer::DrawingFilter::Primitive> SPFeTile::build_renderer(Inkscape::Renderer::DrawingItem*) const
{
    auto tile = std::make_unique<Inkscape::Renderer::DrawingFilter::Tile>();
    build_renderer_common(tile.get());
    return tile;
}

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
