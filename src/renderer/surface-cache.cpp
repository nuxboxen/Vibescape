// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * A surface with a bit of extra meta data for caches
 *//*
 * Authors:
 *  Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "renderer/surface-cache.h"

namespace Inkscape::Renderer {

void SurfaceCache::markDirty(Geom::IntRect const &area)
{
    std::cout << "markDirty ToBeImplemented\n";
}

void SurfaceCache::markClean(Geom::IntRect const &area)
{
    std::cout << "markClean ToBeImplemented\n";
}

void SurfaceCache::scheduleTransform(Geom::IntRect const &new_area, Geom::Affine const &trans)
{
    std::cout << "scheduleTransform ToBeImplemented\n";
}

void SurfaceCache::prepare()
{
    std::cout << "prepare ToBeImplemented\n";
}

void SurfaceCache::paintFromCache(Context &dc, Geom::OptIntRect &area, bool is_filter)
{
    std::cout << "paintFromCache ToBeImplemented\n";
}

} // namespace Inkscape::Renderer

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
