// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shape (styled path) belonging to an SVG drawing.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm.h>
#include <2geom/curves.h>
#include <2geom/pathvector.h>
#include <2geom/path-sink.h>
#include <2geom/svg-path-parser.h>

#include "renderer/context.h"

#include "drawing.h"
#include "drawing-shape.h"
#include "drawing-style.h"

#include "helper/geom.h"

namespace Inkscape::Renderer {

DrawingShape::DrawingShape(Drawing &drawing)
    : DrawingItem(drawing)
    , _last_pick(nullptr)
    , _repick_after(0)
{
}

void DrawingShape::setPath(std::shared_ptr<Geom::PathVector const> curve)
{
    defer([this, curve = std::move(curve)] () mutable {
        _markForRendering();
        _curve = std::move(curve);
        _markForUpdate(STATE_ALL, false);
    });
}

unsigned DrawingShape::_updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    // update markers
    for (auto &c : _children) {
        c.update(area, ctx, flags, reset);
    }

    // clear Cairo data to force update
    if (flags & STATE_RENDER) {
        _nrstyle.invalidate();
    }

    auto calc_curve_bbox = [&, this] () -> Geom::OptIntRect {
        if (!_curve) {
            return {};
        }

        auto rect = bounds_exact_transformed(*_curve, ctx.ctm);
        if (!rect) {
            return {};
        }

        float stroke_max = 0.0f;

        // Get the normal stroke.
        if (_drawing.renderMode() != RenderMode::OUTLINE && _nrstyle.stroke.type != DrawingStyle::PaintType::NONE) {
            // Expand by stroke width.
            stroke_max = _nrstyle.stroke_width * 0.5f;

            // Scale by view transformation, unless vector effect stroke.
            if (!_nrstyle.vector_effect_stroke) {
                stroke_max *= max_expansion(ctx.ctm);
            }

            // Cap minimum line width if asked.
            if (_drawing.renderMode() == RenderMode::VISIBLE_HAIRLINES || _nrstyle.stroke_extensions_hairline) {
                stroke_max = std::max(stroke_max, 0.5f);
            }
        }

        // Get the outline stroke.
        if (_drawing.renderMode() == RenderMode::OUTLINE || _drawing.outlineOverlay()) {
            stroke_max = std::max(stroke_max, 0.5f);
        }

        if (stroke_max > 0.0f) {
            // Expand by mitres, if present.
            if (_nrstyle.line_join == SP_STROKE_LINEJOIN_MITER && _nrstyle.miter_limit >= 1.0f) {
                stroke_max *= _nrstyle.miter_limit;
            }

            // Apply expansion if non-zero.
            if (stroke_max > 0.01) {
                rect->expandBy(stroke_max);
            }
        }

        return rect->roundOutwards();
    };

    if (flags & STATE_BBOX) {
        _bbox = calc_curve_bbox();

        for (auto &c : _children) {
            _bbox.unionWith(c.bbox());
        }
    }

    return _state | flags;
}

void DrawingShape::_renderFill(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const
{
    Context::Save save(dc);
    dc.transform(_ctm);

    auto has_fill = _nrstyle.prepareFill(dc, rc, area, _item_bbox, _fill_pattern);

    if (has_fill) {
        dc.path(*_curve);
        _nrstyle.applyFill(dc, *has_fill);
        dc.fillPreserve();
        dc.newPath(); // clear path
    }
}

void DrawingShape::_renderStroke(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags) const
{
    Context::Save save(dc);
    dc.transform(_ctm);

    auto has_stroke = _nrstyle.prepareStroke(dc, rc, area, _item_bbox, _stroke_pattern);
    if (!_nrstyle.stroke_extensions_hairline && _nrstyle.stroke_width == 0) {
        has_stroke.reset();
    }

    if (has_stroke) {
        // TODO: remove segments outside of bbox when no dashes present
        dc.path(*_curve);
        if (_nrstyle.vector_effect_stroke) {
            dc.restore();
            dc.save();
        }
        _nrstyle.applyStroke(dc, *has_stroke);

        // If the stroke is a hairline, set it to exactly 1px on screen.
        // If visible hairline mode is on, make sure the line is at least 1px.
        if (flags & RENDER_VISIBLE_HAIRLINES || _nrstyle.stroke_extensions_hairline) {
            auto pixel_size = dc.device_to_user_distance({1.0, 1.0}).length();
            if (_nrstyle.stroke_extensions_hairline || _nrstyle.stroke_width < pixel_size) {
                dc.setHairline();
            }
        }

        dc.strokePreserve();
        dc.newPath(); // clear path
    }
}

void DrawingShape::_renderMarkers(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const
{
    // marker rendering
    for (auto &i : _children) {
        i.render(dc, rc, area, flags, stop_at);
    }
}

unsigned DrawingShape::_renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const
{
    if (!_curve) return RENDER_OK;

    auto visible = area & _bbox;
    if (!visible) return RENDER_OK; // skip if not within bounding box

    bool outline = flags & RENDER_OUTLINE;

    if (outline) {
        // paint-order doesn't matter
        {
            Context::Save save(dc);
            dc.transform(_ctm);
            dc.path(*_curve);
        }
        {
            Context::Save save(dc);
            dc.setSource(*rc.outline_color);
            dc.setLineWidth(0.5);
            dc.set_tolerance(0.5);
            dc.stroke();
        }

        _renderMarkers(dc, rc, area, flags, stop_at);
        return RENDER_OK;
    }

    if (_nrstyle.paint_order_layer[0] == DrawingStyle::PAINT_ORDER_NORMAL) {
        // This is the most common case, special case so we don't call get_pathvector(), etc. twice

        {
            // we assume the context has no path
            Context::Save save(dc);
            dc.transform(_ctm);

            // update fill and stroke paints.
            // this cannot be done during nr_arena_shape_update, because we need a Cairo context
            // to render svg:pattern
            auto has_fill   = _nrstyle.prepareFill(dc, rc, *visible, _item_bbox, _fill_pattern);
            auto has_stroke = _nrstyle.prepareStroke(dc, rc, *visible, _item_bbox, _stroke_pattern);
            if (!_nrstyle.hairline && _nrstyle.stroke_width == 0) {
                has_stroke.reset();
            }
            if (has_fill || has_stroke) {
                dc.path(*_curve);
                // TODO: remove segments outside of bbox when no dashes present
                if (has_fill) {
                    _nrstyle.applyFill(dc, *has_fill);
                    dc.fillPreserve();
                }
                if (_nrstyle.vector_effect_stroke) {
                    dc.restore();
                    dc.save();
                }
                if (has_stroke) {
                    _nrstyle.applyStroke(dc, *has_stroke);

                    // If the draw mode is set to visible hairlines, don't let anything get smaller
                    // than half a pixel.
                    if (flags & RENDER_VISIBLE_HAIRLINES) {
                        auto half_pixel_size = dc.device_to_user_distance({1.0, 0.0}).length() * 0.5;
                        if (_nrstyle.stroke_width < half_pixel_size) {
                            dc.setLineWidth(half_pixel_size);
                        }
                    }

                    dc.strokePreserve();
                }
                dc.newPath(); // clear path
            } // has fill or stroke pattern
        }
        _renderMarkers(dc, rc, area, flags, stop_at);
        return RENDER_OK;

    }

    // Handle different paint orders
    for (auto &i : _nrstyle.paint_order_layer) {
        switch (i) {
            case DrawingStyle::PAINT_ORDER_FILL:
                _renderFill(dc, rc, *visible);
                break;
            case DrawingStyle::PAINT_ORDER_STROKE:
                _renderStroke(dc, rc, *visible, flags);
                break;
            case DrawingStyle::PAINT_ORDER_MARKER:
                _renderMarkers(dc, rc, area, flags, stop_at);
                break;
            default:
                // PAINT_ORDER_AUTO Should not happen
                break;
        }
    }

    return RENDER_OK;
}

void DrawingShape::_clipItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &/*area*/) const
{
    if (!_curve) return;

    Context::Save save(dc);
    dc.setFillRule(_nrstyle.clip_rule);
    dc.transform(_ctm);
    dc.path(*_curve);
    dc.fill();
}

DrawingItem *DrawingShape::_pickItem(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags)
{
    if (_repick_after > 0)
        --_repick_after;

    if (_repick_after > 0) { // we are a slow, huge path
        return _last_pick;   // skip this pick, returning what was returned last time
    }

    if (!_curve) return nullptr;
    bool outline = flags & PICK_OUTLINE;
    bool pick_as_clip = flags & PICK_AS_CLIP;

    if (_nrstyle.opacity == 0.0 && !outline && !pick_as_clip && !_drawing.selectZeroOpacity()) {
        // fully transparent, no pick unless outline mode
        return nullptr;
    }

    gint64 tstart = g_get_monotonic_time();

    double width;
    if (pick_as_clip) {
        width = 0; // no width should be applied to clip picking
                   // this overrides display mode and stroke style considerations
    } else if (outline) {
        width = 0.5; // in outline mode, everything is stroked with the same 0.5px line width
    } else if (_nrstyle.stroke.type != DrawingStyle::PaintType::NONE && (_nrstyle.stroke.opacity > 1e-3 || _drawing.selectZeroOpacity())) {
        auto stroke_width = _nrstyle.hairline ? 1 : _nrstyle.stroke_width;
        // for normal picking calculate the distance corresponding top the stroke width
        double scale = max_expansion(_ctm);
        width = std::max(0.125, stroke_width * scale) / 2;
    } else {
        width = 0;
    }

    double dist = Geom::infinity();
    int wind = 0;
    bool needfill = pick_as_clip || (_nrstyle.fill.type != DrawingStyle::PaintType::NONE && (_nrstyle.fill.opacity > 1e-3  || _drawing.selectZeroOpacity()) && !outline);
    bool wind_evenodd = (pick_as_clip ? _nrstyle.clip_rule : _nrstyle.fill_rule) == SP_WIND_RULE_EVENODD;

    // actual shape picking
    if (area_world) {
        Geom::Rect viewbox = *area_world;
        viewbox.expandBy (width);
        pathv_matrix_point_bbox_wind_distance(*_curve, _ctm, p, nullptr, needfill? &wind : nullptr, &dist, 0.5, &viewbox);
    } else {
        pathv_matrix_point_bbox_wind_distance(*_curve, _ctm, p, nullptr, needfill? &wind : nullptr, &dist, 0.5, nullptr);
    }

    gint64 tfinish = g_get_monotonic_time();
    gint64 this_pick = tfinish - tstart;
    //g_print ("pick time %lu\n", this_pick);
    if (this_pick > 10000) { // slow picking, remember to skip several new picks
        _repick_after = this_pick / 5000;
    }

    // covered by fill?
    if (needfill) {
        if (wind_evenodd) {
            if (wind & 0x1) {
                _last_pick = this;
                return this;
            }
        } else {
            if (wind != 0) {
                _last_pick = this;
                return this;
            }
        }
    }

    // close to the edge, as defined by strokewidth and delta?
    // this ignores dashing (as if the stroke is solid) and always works as if caps are round
    if (needfill || width > 0) { // if either fill or stroke visible,
        if ((dist - width) < delta) {
            _last_pick = this;
            return this;
        }
    }

    // if not picked on the shape itself, try its markers
    for (auto &i : _children) {
        DrawingItem *ret = i.pick(p, delta, area_world, flags & ~PICK_STICKY);
        if (ret) {
            _last_pick = this;
            return this;
        }
    }

    _last_pick = nullptr;
    return nullptr;
}

} // namespace Inkscape

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
