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

#ifndef INKSCAPE_DISPLAY_RENDERER_SURFACE_CACHE_H
#define INKSCAPE_DISPLAY_RENDERER_SURFACE_CACHE_H

#include <2geom/affine.h>
#include <2geom/rect.h>
#include <2geom/transforms.h>
#include <cairomm/region.h>

#include "surface.h"
#include "context.h"

namespace Inkscape::Renderer {

class SurfaceCache : public Surface
{
public:
    void markDirty(Geom::IntRect const &area = Geom::IntRect::infinite());
    void markClean(Geom::IntRect const &area = Geom::IntRect::infinite());
    void scheduleTransform(Geom::IntRect const &new_area, Geom::Affine const &trans);
    void prepare();
    void paintFromCache(Context &dc, Geom::OptIntRect &area, bool is_filter);

private:
    Cairo::Region _clean_region;
    Geom::IntRect _pending_area;
    Geom::Affine _pending_transform;
};

} // namespace Inkscape::Renderer

#endif // INKSCAPE_DISPLAY_RENDERER_SURFACE_CACHE_H

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
