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
    _clean_region.subtract(geom_to_cairo(area));
}

void SurfaceCache::markClean(Geom::IntRect const &area)
{
    if (auto const r = area & cacheArea()) {
        _clean_region.do_union(geom_to_cairo(*r));
    }
}

void SurfaceCache::scheduleTransform(Geom::IntRect const &new_area, Geom::Affine const &trans)
{
    _pending_area = new_area;
    _pending_transform *= trans;
}

void SurfaceCache::prepare()
{
    Geom::IntRect old_area = cacheArea();
    bool is_identity = _pending_transform.isIdentity();
    if (is_identity && _pending_area == old_area) return; // no change

    bool is_integer_translation = is_identity;
    if (!is_identity && _pending_transform.isTranslation()) {
        Geom::IntPoint t = _pending_transform.translation().round();
        if (Geom::are_near(Geom::Point(t), _pending_transform.translation())) {
            is_integer_translation = true;
            _clean_region.translate(t.x(), t.y());
            if (old_area + t == _pending_area) {
                // if the areas match, the only thing to do
                // is to ensure that the clean area is not too large
                // we can exit early
                _clean_region.intersect(geom_to_cairo(_pending_area));
                _origin += t;
                _pending_transform.setIdentity();
                return;
            }
        }
    }   
  
    // the area has changed, so the cache content needs to be copied
    auto new_surface = Surface(_pending_area.dimensions(), _device_scale, _color_space);

    if (is_integer_translation) {
        // transform the cache only for integer translations and identities
        auto ct = Context(new_surface);
        if (!is_identity) {
            ct.transform(_pending_transform);
        }
        ct.setSource(*this, _origin.x(), _origin.y(), Cairo::SurfacePattern::Filter::NEAREST);
        ct.set_operator(Cairo::Context::Operator::SOURCE);
        ct.paint();

        _clean_region.intersect(geom_to_cairo(_pending_area));
    } else {
        // dirty everything
        _clean_region = Cairo::Region(cairo_region_create());
    }

    // Steal the new surface contents, I don't like this, but its what was here before
    _surfaces = new_surface.getCairoSurfaces();
    _dimensions = new_surface.dimensions();
    _origin = _pending_area.min();
    _pending_transform.setIdentity();
}

void SurfaceCache::paintFromCache(Context &dc, Geom::OptIntRect &area, bool is_filter)
{
    if (!area) return;

    // We subtract the clean region from the area, then get the bounds
    // of the resulting region. This is the area that needs to be repainted
    // by the item.
    // Then we subtract the area that needs to be repainted from the
    // original area and paint the resulting region from cache.
    auto const area_c = geom_to_cairo(*area);

    auto dirty_region = Cairo::Region(cairo_region_create_rectangle(&area_c));
    auto cache_region = dirty_region.copy();
    dirty_region.subtract(_clean_region.copy());
  
      if (is_filter && !dirty_region.empty()) { // To allow fast panning on high zoom on filters
          _clean_region = Cairo::Region(cairo_region_create());
          return;
      }
  
      if (dirty_region.empty()) {
          area = Geom::OptIntRect();
      } else {
          area = cairo_to_geom(dirty_region.get_extents());
          cache_region->subtract(dirty_region.get_extents());
      }
  
      if (!cache_region->empty()) {
          dc.rectangles(cache_region);
          dc.setSource(*this);
          dc.fill();
      }
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
