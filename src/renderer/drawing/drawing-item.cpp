// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Canvas item belonging to an SVG drawing element.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <climits>

#include "colors/manager.h"
#include "colors/spaces/base.h"

#include "renderer/code-builder.h"
#include "renderer/context.h"
#include "renderer/drawing-filters/filter.h"
#include "renderer/drawing-filters/primitive.h"
#include "renderer/surface.h"
#include "renderer/surface-cache.h"

#include "drawing.h"
#include "drawing-group.h"
#include "drawing-item.h"
#include "drawing-pattern.h"
#include "drawing-style.h"
#include "drawing-text.h"

namespace Inkscape::Renderer {

/**
 * @class DrawingItem
 * SVG drawing item for display.
 *
 * This class represents the renderable portion of the SVG document. Typically this
 * is created by the SP tree, in particular the invoke_show() virtual function.
 *
 * @section ObjectLifetime Object lifetime
 * Deleting a DrawingItem will cause all of its children to be deleted as well.
 * This can lead to nasty surprises if you hold references to things
 * which are children of what is being deleted. Therefore, in the SP tree,
 * you always need to delete the item views of children before deleting
 * the view of the parent. Do not call delete on things returned from invoke_show()
 * - this will cause dangling pointers inside the SPItem and lead to a crash.
 * Use the corresponding invoke_hide() method.
 *
 * Outside of the SP tree, you should not use any references after the root node
 * has been deleted.
 */

DrawingItem::DrawingItem(Drawing &drawing)
    : _drawing(drawing)
    , _parent(nullptr)
    , _key(0)
    , _contains_unisolated_blend(false)
    , _opacity(1.0)
    , _clip(nullptr)
    , _mask(nullptr)
    , _fill_pattern(nullptr)
    , _stroke_pattern(nullptr)
    , _item(nullptr)
    , _state(0)
    , _child_type(ChildType::ORPHAN)
    , _background_accumulate(0)
    , _visible(true)
    , _sensitive(true)
    , _cached_persistent(0)
    , _has_cache_iterator(0)
    , _propagate_state(0)
    , _pick_children(0)
    , _antialias(Antialiasing::Good)
    , _isolation(SP_CSS_ISOLATION_AUTO)
    , _blend_mode(SP_CSS_BLEND_NORMAL)
{
    if (drawing._code_build) CodeBuilder::Construct(*this, "DrawingItem", "item", {}, {"DrawingGroup", "DrawingImage", "DrawingShape", "DrawingGlyphs"}) << drawing;
}

DrawingItem::~DrawingItem()
{
    _drawing._item_deleted_signal.emit(_key);

    // Remove caching candidate entry.
    if (_has_cache_iterator) {
        _drawing._candidate_items.erase(_cache_iterator);
    }

    // Remove from the set of cached items and delete cache.
    _setCached(false, true);

    _children.clear_and_dispose([] (auto c) { delete c; });
    delete _clip;
    delete _mask;
    delete static_cast<DrawingItem*>(_fill_pattern);
    delete static_cast<DrawingItem*>(_stroke_pattern);
}

/// Returns true if item is among the descendants. Will return false if item == this.
bool DrawingItem::isAncestorOf(DrawingItem const *item) const
{
    for (auto c = item->_parent; c; c = c->_parent) {
        if (c == this) return true;
    }
    return false;
}

bool DrawingItem::unisolatedBlend() const
{
    if (_blend_mode != SP_CSS_BLEND_NORMAL) {
        return true;
    } else if (_mask || _filter || hasOpacity() || _isolation == SP_CSS_ISOLATION_ISOLATE) {
        return false;
    } else {
        return _contains_unisolated_blend;
    }
}

void DrawingItem::appendChild(DrawingItem *item)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "appendChild") << item;

    // Ok to perform non-deferred modification of child, because not part of rendering tree yet.
    assert(item->_child_type == ChildType::ORPHAN);
    item->_parent = this;
    item->_child_type = ChildType::NORMAL;

    defer([=, this] {
        _children.push_back(*item);

        // This ensures that _markForUpdate() called on the child will recurse to this item
        item->_state = STATE_ALL;
        // Because _markForUpdate recurses through ancestors, we can simply call it
        // on the just-added child. This has the additional benefit that we do not
        // rely on the appended child being in the default non-updated state.
        // We set propagate to true, because the child might have descendants of its own.
        item->_markForUpdate(STATE_ALL, true);
    });
}

void DrawingItem::prependChild(DrawingItem *item)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "prependChild") << item;

    // See appendChild for explanations.
    assert(item->_child_type == ChildType::ORPHAN);
    item->_parent = this;
    item->_child_type = ChildType::NORMAL;

    defer([=, this] {
        _children.push_front(*item);
        item->_state = STATE_ALL;
        item->_markForUpdate(STATE_ALL, true);
    });
}

// Clear this node's ordinary children, deleting them and their descendants without otherwise changing them in any way.
void DrawingItem::clearChildren()
{
    defer([=, this] {
        if (_children.empty()) return;
        _markForRendering();
        _children.clear_and_dispose([] (auto c) { delete c; });
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingItem::setTransform(Geom::Affine const &transform)
{
    defer([=, this] {
        auto constexpr EPS = 1e-18;
        auto current = _transform ? *_transform : Geom::identity();
        if (Geom::are_near(transform, current, EPS)) return;

        if (drawing()._code_build) CodeBuilder::Call(*this, "setTransform") << transform;

        _markForRendering();
        _transform = transform.isIdentity(EPS) ? nullptr : std::make_unique<Geom::Affine>(transform);
        _markForUpdate(STATE_ALL, true);
    });
}

void DrawingItem::setOpacity(float opacity)
{
    defer([=, this] {
        if (opacity == _opacity) return;

        if (drawing()._code_build) CodeBuilder::Call(*this, "setOpacity") << opacity;

        _opacity = opacity;
        _markForRendering();
    });
}

void DrawingItem::setOpacityOverride(std::optional<double> opacity)
{
    defer([=, this] {
        if ((!opacity && !_opacity_override) || (opacity && _opacity_override && *opacity == *_opacity_override)) {
            return;
        }
        _opacity_override = opacity;
        _markForRendering();
    });
}

void DrawingItem::setAntialiasing(Antialiasing antialias)
{
    defer([=, this] {
        if (_antialias == antialias) return;

        if (drawing()._code_build) CodeBuilder::Call(*this, "setAntialiasing") << "Antialiasing::Good";

        _antialias = antialias;
        _markForRendering();
    });
}

void DrawingItem::setIsolation(bool isolation)
{
    defer([=, this] {
        if (isolation == _isolation) return;
        _isolation = isolation;
        _markForRendering();
    });
}

void DrawingItem::setBlendMode(SPBlendMode blend_mode)
{
    defer([=, this] {
        if (blend_mode == _blend_mode) return;

        if (drawing()._code_build) CodeBuilder::Call(*this, "setBlendMode") << blend_mode;

        _blend_mode = blend_mode;
        _markForRendering();
    });
}

void DrawingItem::setVisible(bool visible)
{
    defer([=, this] {
        if (visible == _visible) return;

        if (drawing()._code_build) CodeBuilder::Call(*this, "setVisible") << visible;
        
        _visible = visible;
        _markForRendering();
    });
}

void DrawingItem::setSensitive(bool sensitive)
{
    defer([=, this] { // Must be deferred, since in bitfield.
        _sensitive = sensitive;
    });
}

void DrawingItem::setStyle(SPStyle const *style, SPStyle const *context_style)
{
    if (style && drawing()._code_build) {
        CodeBuilder::get().maybeConstruct(*style);
        if (context_style) {
            CodeBuilder::get().maybeConstruct(*context_style);
            CodeBuilder::Call(*this, "setStyle", true) << style << context_style;
        } else {
            CodeBuilder::Call(*this, "setStyle", true) << style;
        }
    }

    defer([this, style_obj = DrawingStyle(style, context_style)] () mutable {
        _style = std::move(style_obj);
    });
}

void DrawingItem::setChildrenStyle(SPStyle const *style)
{
    if (style) {
        if (drawing()._code_build) CodeBuilder::get().maybeConstruct(*style);
        CodeBuilder::Call(*this, "setChildrenStyle") << style;
    }
    defer([this, style] () mutable {
        _style.set_context_style(style);
    });
    for (auto &i : _children) {
        i.setChildrenStyle(style);
    }
}

/**
 * Enable / disable storing the rendering in memory.
 * Calling setCached(false, true) will also remove the persistent status
 */
void DrawingItem::_setCached(bool cached, bool persistent)
{
    static bool const cache_env = getenv("_INKSCAPE_DISABLE_CACHE");
    if (cache_env) {
        return;
    }

    if (persistent) {
        _cached_persistent = cached && persistent;
    } else if (_cached_persistent) {
        return;
    }

    if (cached == (bool)_cache) {
        return;
    }

    if (cached) {
        _cache = std::make_unique<CacheData>();
        _drawing._cached_items.insert(this);
    } else {
        _cache.reset();
        _drawing._cached_items.erase(this);
    }
}

void DrawingItem::setClip(DrawingItem *item)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "setClip") << item;

    if (item) {
        assert(item->_child_type == ChildType::ORPHAN);
        item->_parent = this;
        item->_child_type = ChildType::CLIP;
    }

    defer([=, this] {
        _markForRendering();
        delete _clip;
        _clip = item;
        _markForUpdate(STATE_ALL, true);
    });
}

void DrawingItem::setMask(DrawingItem *item)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "setMask") << item;

    if (item) {
        assert(item->_child_type == ChildType::ORPHAN);
        item->_parent = this;
        item->_child_type = ChildType::MASK;
    }

    defer([=, this] {
        _markForRendering();
        delete _mask;
        _mask = item;
        _markForUpdate(STATE_ALL, true);
    });
}

void DrawingItem::setFillPattern(DrawingPattern *pattern)
{
    if (drawing()._code_build && pattern) CodeBuilder::Call(*this, "setFillPattern") << pattern;

    if (pattern) {
        assert(pattern->_child_type == ChildType::ORPHAN);
        pattern->_parent = this;
        pattern->_child_type = ChildType::FILL;
    }

    defer([=, this] {
        _markForRendering();
        delete static_cast<DrawingItem*>(_fill_pattern);
        _fill_pattern = pattern;
        _markForUpdate(STATE_ALL, false);
    });
}

void DrawingItem::setStrokePattern(DrawingPattern *pattern)
{
    if (drawing()._code_build && pattern) CodeBuilder::Call(*this, "setStrokePattern") << pattern;

    if (pattern) {
        assert(pattern->_child_type == ChildType::ORPHAN);
        pattern->_parent = this;
        pattern->_child_type = ChildType::STROKE;
    }

    defer([=, this] {
        _markForRendering();
        delete static_cast<DrawingItem*>(_stroke_pattern);
        _stroke_pattern = pattern;
        _markForUpdate(STATE_ALL, false);
    });
}

/// Move this item to the given place in the Z order of siblings. Does nothing if the item is not a normal child.
void DrawingItem::setZOrder(unsigned zorder)
{
    if (_child_type != ChildType::NORMAL) return;

    defer([=, this] {
        auto it = _parent->_children.iterator_to(*this);
        _parent->_children.erase(it);

        auto it2 = _parent->_children.begin();
        std::advance(it2, std::min<unsigned>(zorder, _parent->_children.size()));
        _parent->_children.insert(it2, *this);
        _markForRendering();
    });
}

void DrawingItem::setItemBounds(Geom::OptRect const &bounds)
{
    if (bounds && drawing()._code_build) CodeBuilder::Call(*this, "setItemBounds") << *bounds;

    defer([=, this] {
        _item_bbox = bounds;
    });
}

void DrawingItem::setFilterRenderer(std::unique_ptr<DrawingFilter::Filter> filter)
{
    defer([=, this, filter = std::move(filter)] () mutable {
        _filter = std::move(filter);
        _markForRendering();
    });
}

/**
 * Update derived data before operations.
 * The purpose of this call is to recompute internal data which depends
 * on the attributes of the object, but is not directly settable by the user.
 * Precomputing this data speeds up later rendering, because some items
 * can be omitted.
 *
 * Currently this method handles updating the visual and geometric bounding boxes
 * in pixels, storing the total transformation from item space to the screen
 * and cache invalidation.
 *
 * @param area Area to which the update should be restricted. Only takes effect
 *             if the bounding box is known.
 * @param ctx A structure to store cascading state.
 * @param flags Which internal data should be recomputed. This can be any combination
 *              of StateFlags.
 * @param reset State fields that should be reset before processing them. This is
 *              a means to force a recomputation of internal data even if the item
 *              considers it up to date. Mainly for internal use, such as
 *              propagating bounding box recomputation to children when the item's
 *              transform changes.
 */
void DrawingItem::update(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    if (drawing()._code_build) CodeBuilder::Call(*this, "update") << area << ctx.ctm << (int)flags << reset;

    // We don't need to update what is not visible
    if (!_visible) {
        _state = STATE_ALL; // Touch the state for future change to this item
        return;
    }

    bool const outline = _drawing.renderMode() == RenderMode::OUTLINE || _drawing.outlineOverlay();
    bool const filters = _drawing.renderMode() != RenderMode::NO_FILTERS;
    bool const forcecache = _filter && filters;

    // Set reset flags according to propagation status
    reset |= _propagate_state;
    _propagate_state = 0;

    _state &= ~reset; // reset state of this item

    if ((~_state & flags) == 0) return;  // nothing to do

    // TODO this might be wrong
    if (_state & STATE_BBOX) {
        // we have up-to-date bbox
        if (!area.intersects(outline ? _bbox : _drawbox)) return;
    }

    // compute which elements need an update
    unsigned to_update = _state ^ flags;

    // this needs to be called before we recurse into children
    if (to_update & STATE_BACKGROUND) {
        _background_accumulate = _style.background_new;
        if (_child_type == ChildType::NORMAL && _parent->_background_accumulate)
            _background_accumulate = true;
    }

    UpdateContext child_ctx(ctx);
    if (_transform) {
        child_ctx.ctm = *_transform * ctx.ctm;
    }

    // Vector effects
    if (_style.vector_effect_fixed) {
        child_ctx.ctm.setTranslation(Geom::Point(0, 0));
    }

    if (_style.vector_effect_size) {
        double value = child_ctx.ctm.descrim();
        if (value > 0.0) {
            child_ctx.ctm[0] /= value;
            child_ctx.ctm[1] /= value;
            child_ctx.ctm[2] /= value;
            child_ctx.ctm[3] /= value;
        }
    }

    if (_style.vector_effect_rotate) {
        double value = child_ctx.ctm.descrim();
        child_ctx.ctm[0] = value;
        child_ctx.ctm[1] = 0.0;
        child_ctx.ctm[2] = 0.0;
        child_ctx.ctm[3] = value;
    }

    // Remember the transformation matrix.
    Geom::Affine ctm_change;
    bool affine_changed = false;
    if (!Geom::are_near(_ctm, child_ctx.ctm)) {
        ctm_change = _ctm.inverse() * child_ctx.ctm;
        affine_changed = true;
    }
    _ctm = child_ctx.ctm;

    bool const totally_invalidated = reset & STATE_TOTAL_INV;
    if (totally_invalidated) {
        // Perform work that would have been done by our call to _markForRendering(),
        // had it not been overshadowed by a totally-invalidating node.
        if (_cache && _cache->surface) {
            _cache->surface->markDirty();
        }
        _dropPatternCache();
    }

    // Decide whether this node should be a totally-invalidating node.
    bool const totally_invalidate = _update_complexity >= 20 && affine_changed;
    if (totally_invalidate) {
        reset |= STATE_TOTAL_INV;
    }

    // Recalculate update complexity; to be recalculated immediately below and by _updateItem().
    _update_complexity = 1;
    auto add_complexity_if = [&] (DrawingItem *c) {
        if (c) {
            _update_complexity += c->_update_complexity;
        }
    };
    add_complexity_if(_clip);
    add_complexity_if(_mask);
    add_complexity_if(_fill_pattern);
    add_complexity_if(_stroke_pattern);

    // Reset contains_unisolated_blend; to be recalculated by  _updateItem().
    _contains_unisolated_blend = false;

    // Moved from code that was previously in render().
    if (forcecache) {
        _setCached((bool)_cacheRect(), true);
    }

    // update _bbox and call this function for children
    _state = _updateItem(area, child_ctx, flags, reset);

    // update drawingitems contained in filter
    if (_filter) {
        _filter->update();
    }

    if (to_update & STATE_BBOX) {
        // compute drawbox
        if (_filter && filters) {
            Geom::OptRect enlarged = _filter->filter_effect_area(_item_bbox);
            if (enlarged) {
                *enlarged *= ctm();
                _drawbox = enlarged->roundOutwards();
            } else {
                _drawbox = Geom::OptIntRect();
            }
        } else {
            _drawbox = _bbox;
        }

        // Clipping
        if (_clip) {
            _clip->update(area, child_ctx, flags, reset);
            if (outline) {
                _bbox.unionWith(_clip->_bbox);
            } else {
                _drawbox.intersectWith(_clip->_bbox);
            }
        }
        // Masking
        if (_mask) {
            _mask->update(area, child_ctx, flags, reset);
            if (outline) {
                _bbox.unionWith(_mask->_bbox);
            } else {
                // for masking, we need full drawbox of mask
                _drawbox.intersectWith(_mask->_drawbox);
            }
        }
        // Crude fix for outline overlay bbox issues with filtered objects.
        // (Real solution is to carefully review all bbox/drawbox uses.)
        if (_drawing.outlineOverlay()) {
            _bbox |= _drawbox;
        }
    }
    if (to_update & STATE_CACHE) {
        // Remove old cache iterator.
        if (_has_cache_iterator) {
            _drawing._candidate_items.erase(_cache_iterator);
            _has_cache_iterator = false;
        }

        // Determine whether this item is cachable.
        bool isolated = _mask || _filter || hasOpacity()
            || _blend_mode != SP_CSS_BLEND_NORMAL
            || _isolation == SP_CSS_ISOLATION_ISOLATE
            || _child_type == ChildType::ROOT;
        bool cacheable = !_contains_unisolated_blend || isolated;

        // Determine whether to make this item eligible for caching, by creating a cache iterator.
        double score = _cacheScore();
        if (score >= CACHE_SCORE_THRESHOLD && cacheable) {
            _cache_record.score = score;
            // if _cacheRect() is empty, a negative score will be returned from _cacheScore(),
            // so this will not execute (cache score threshold must be positive)
            _cache_record.cache_size = _cacheRect()->area() * 4;
            _cache_record.item = this;
            _cache_iterator = _drawing._candidate_items.insert(_cache_record);
            _has_cache_iterator = true;
        }

        /* Update cache if enabled.
         * General note: here we only tell the cache how it has to transform
         * during the render phase. The transformation is deferred because
         * after the update the item can have its caching turned off,
         * e.g. because its filter was removed. This way we avoid temporarily
         * using more memory than the cache budget */
        if (_cache && _cache->surface) {
            Geom::OptIntRect cl = _cacheRect();
            if (_visible && cl && _has_cache_iterator) { // never create cache for invisible items
                // this takes care of invalidation on transform
                _cache->surface->scheduleTransform(*cl, ctm_change);
            } else {
                // Destroy cache for this item - outside of canvas or invisible.
                // The opposite transition (invisible -> visible or object
                // entering the canvas) is handled during the render phase
                _setCached(false, true);
            }
        }
    }

    if (to_update & STATE_RENDER) {
        // now that we know drawbox, dirty the corresponding rect on canvas
        // unless filtered, groups do not need to render by themselves, only their members
        if (_fill_pattern) {
            _fill_pattern->update(area, child_ctx, flags, reset);
        }
        if (_stroke_pattern) {
            _stroke_pattern->update(area, child_ctx, flags, reset);
        }
        if (!totally_invalidated) {
            if (!is<DrawingGroup>(this) || (_filter && filters) || totally_invalidate) {
                _markForRendering();
            }
        }
    }
}

/**
 * Rasterize items.
 * This method submits the drawing operations required to draw this item
 * to the supplied Context, restricting drawing the specified area.
 *
 * This method does some common tasks and calls the item-specific rendering
 * function, _renderItem(), to render e.g. paths or bitmaps.
 *
 * @param flags Rendering options. This deals mainly with cache control.
 */
unsigned DrawingItem::render(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const
{
    bool const outline = flags & RENDER_OUTLINE;
    bool const render_filters = !(flags & RENDER_NO_FILTERS);
    bool const forcecache = _filter && render_filters;

    // stop_at is handled in DrawingGroup, but this check is required to handle the case
    // where a filtered item with background-accessing filter has enable-background: new
    if (this == stop_at) {
        return RENDER_STOP;
    }

    // If we are invisible, return immediately
    if (!_visible) {
        return RENDER_OK;
    }

    if (_ctm.isSingular(1e-18)) {
        return RENDER_OK;
    }

    // TODO convert outline rendering to a separate virtual function
    if (outline) {
        _renderOutline(dc, rc, area, flags);
        return RENDER_OK;
    }

    Geom::OptIntRect carea = area & _drawbox;
    if (!carea) {
        return RENDER_OK;
    }

    Geom::OptIntRect iarea = carea;
    // expand carea to contain the dependent area of filters.
    if (forcecache) {
        iarea = _cacheRect();
        if (!iarea) {
            iarea = carea;
            _filter->area_enlarge(*iarea, ctm());
            iarea.intersectWith(_drawbox);
        }
    }
    // carea is the area to paint
    carea = iarea & _drawbox;
    if (!carea) {
        return RENDER_OK;
    }

    // Device scale for HiDPI screens (typically 1 or 2)
    int const device_scale = rc.device_scale;

    // When this happens, we want to enforce the use of RGB so the results can
    // be combined correctly. Use of INT8 surface can only combine with other INT8 surfaces
    static auto srgb = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto interp_space = _style.color_interpolation;
    auto target_space = (!interp_space && dc.getColorSpace()) ? srgb : interp_space;

    std::unique_lock<std::mutex> lock;

    // Render from cache if possible, unless requested not to (hatches).
    if (_cache && !(flags & RENDER_BYPASS_CACHE)) {
        lock = std::unique_lock(_cache->mutables);

        if (_cache->surface) {
            if (_cache->surface->getDeviceScale() != device_scale) {
                _cache->surface->markDirty();
            }
            _cache->surface->prepare();
            dc.setOperator(_blend_mode);
            _cache->surface->paintFromCache(dc, carea, forcecache);
            // carea contains everything still dirty, and being empty means it's a perfect cache
            if (!carea) {
                dc.resetSource(0);
                return RENDER_OK;
            }
        } else {
            // There is no cache. This could be because caching of this item
            // was just turned on after the last update phase, or because
            // we were previously outside of the canvas.
            Geom::OptIntRect cl = _cacheRect();
            if (!cl)
                cl = carea;
            // TODO _cache->surface = std::make_shared<SurfaceCache>(*cl, device_scale, target_space);
        }

        if (!forcecache) {
            lock.unlock(); // Only hold the lock for the full duration of rendering for filters.
        }
    } else {
        // if our caching was turned off after the last update, it was already deleted in setCached()
    }


    // determine whether this shape needs intermediate rendering.
    bool const greyscale = _drawing.colorMode() == ColorMode::GRAYSCALE && !(flags & RENDER_OUTLINE);
    bool const isolate_root = _contains_unisolated_blend || greyscale;
    bool const needs_intermediate_rendering =
           _clip                                  // 1. it has a clipping path
        || _mask                                  // 2. it has a mask
        || (_filter && render_filters)            // 3. it has a filter
        || hasOpacity()                           // 4. it is non-opaque
        || _blend_mode != SP_CSS_BLEND_NORMAL     // 5. it has blend mode
        || _isolation == SP_CSS_ISOLATION_ISOLATE // 6. it is isolated
        || (_child_type == ChildType::ROOT && isolate_root) // 7. it is the root and needs isolation
        || (dc.getColorSpace() != target_space)   // 9. different rendering color spaces
        || (bool)_cache                           // 8. it is to be cached
        ;

    auto antialias = rc.antialiasing_override.value_or(_antialias);

    /* How the rendering is done.
     *
     * Clipping, masking and opacity are done by rendering them to a surface
     * and then compositing the object's rendering onto it with the IN operator.
     * The object itself is rendered to a group.
     *
     * Opacity is done by rendering the clipping path with an alpha
     * value corresponding to the opacity. If there is no clipping path,
     * the entire intermediate surface is painted with alpha corresponding
     * to the opacity value.
     * 
     */
    // Short-circuit the simple case.
    // We also use this path for filter background rendering, because masking, clipping,
    // filters and opacity do not apply when rendering the ancestors of the filtered
    // element

    if ((flags & RENDER_FILTER_BACKGROUND) || !needs_intermediate_rendering) {
        Context ict(dc);
        ict.rectangle(*carea);
        ict.clip(); // Do not paint outside the requested area
        ict.set_operator(Cairo::Context::Operator::OVER);
        ict.setAntialiasing(antialias);
        return _renderItem(ict, rc, *carea, flags & ~RENDER_FILTER_BACKGROUND, stop_at);
    }

    unsigned render_result = RENDER_OK;

    auto intermediate = std::make_shared<Surface>(carea->dimensions(), device_scale, target_space);
{
    Context ict(*intermediate, carea->min());
    ict.set_antialias(dc.get_antialias()); // propagate antialias setting

    // This path fails for patterns/hatches when stepping the pattern to handle overflows.
    // The offsets are applied to drawing context (dc) but they are not copied to the
    // intermediate context. Something like this is needed:
    // Copy cairo matrix from dc to intermediate, needed for patterns/hatches
    // cairo_matrix_t cairo_matrix;
    // cairo_get_matrix(dc.raw(), &cairo_matrix);
    // cairo_set_matrix(ict.raw(), &cairo_matrix);
    // For the moment we disable caching for patterns,
    //   see https://gitlab.com/inkscape/inkscape/-/issues/309

    // 1. Render clipping path with alpha = opacity.
    ict.resetSource(getOpacity());
    // Since clip can be combined with opacity, the result could be incorrect
    // for overlapping clip children. To fix this we use the SOURCE operator
    // instead of the default OVER.
    ict.set_operator(Cairo::Context::Operator::SOURCE);
    ict.paint();
    if (_clip) {
        ict.pushGroup();
        _clip->clip(ict, rc, *carea);
        ict.popGroupToSource();
        ict.set_operator(Cairo::Context::Operator::IN);
        ict.paint();
    }
    ict.set_operator(Cairo::Context::Operator::OVER); // reset back to default

    // 2. Render the mask if present and compose it with the clipping path + opacity.
    if (_mask) {
        ict.pushGroup();
        _mask->render(ict, rc, *carea, flags);

        // Convert mask's luminance to alpha
        // TODO: ict.filter(MaskLuminanceToAlpha());
        ict.popGroupToSource();
        ict.set_operator(Cairo::Context::Operator::IN);
        ict.paint();
        ict.set_operator(Cairo::Context::Operator::OVER);
    }

    // 3. Render object itself
    ict.pushGroup();
    ict.setAntialiasing(antialias);
    render_result = _renderItem(ict, rc, *carea, flags, stop_at);

    // 5. Render object inside the composited mask + clip
    ict.popGroupToSource();
    ict.set_operator(Cairo::Context::Operator::IN);
    ict.paint();

    // 4. Apply filter.
    if (_filter && render_filters) {
        std::shared_ptr<Surface> bg;
        if ((
                 _filter->uses_input(DrawingFilter::SLOT_BACKGROUND_IMAGE)
              || _filter->uses_input(DrawingFilter::SLOT_BACKGROUND_ALPHA)
            ) && _background_accumulate) {
            auto bg_root = this;
            for (; bg_root; bg_root = bg_root->_parent) {
                if (bg_root->_style.background_new || bg_root->_filter) break;
            }
            if (bg_root) {
                bg = std::make_shared<Surface>(carea->dimensions(), device_scale, target_space);
                Context bgdc(*bg);
                bg_root->render(bgdc, rc, *carea, flags | RENDER_FILTER_BACKGROUND, this);
            }
        }
        _filter->render(*carea, ctm(), itemBounds(), intermediate, bg, rc);
    }
}

    // Both may ne null, not not either, see above where a null target_space is
    // converted to sRGB when the dc has a non-null color space set.
    auto src_space = intermediate->getColorSpace();
    auto dest_space = dc.getSurfaceColorSpace();
    if (!src_space && dest_space) {
        // This means the destination is floating point and the source is integer
        std::cerr << "Warning: Slowly converting from Integer to Floating point in drawing surface!\n";
        auto floating_surface = intermediate->convertedToFloat();
        intermediate = std::make_shared<Renderer::Surface>(std::move(floating_surface));
        src_space = intermediate->getColorSpace();
    }

    if (src_space && dest_space && src_space != dest_space) {
        if (target_space->getComponentCount() == dc.getColorSpace()->getComponentCount()) {
            std::cout << "Converting color space in place\n";
            intermediate->convertToColorSpace(dc.getColorSpace());
        } else {
            // Makes another copy, so not good.
            std::cout << "Converting color space with copy\n";
            intermediate = intermediate->convertedToColorSpace(dc.getColorSpace());
        }
    }
    if (!dest_space && src_space) {
        // This means the target is integer and the source is floating point
        std::cerr << "Warning: Slowly converting from Floating point to Integer in drawing surface!\n";
        auto int_surface = intermediate->convertedToInt();
        intermediate = std::make_shared<Renderer::Surface>(std::move(int_surface));
    }

    // 6. Paint the completed rendering onto the base context (or into cache)
    if (_cache && !(flags & RENDER_BYPASS_CACHE)) {
        if (!forcecache) {
            lock.lock(); // Only hold the lock for the full duration of rendering for filters.
        }
        assert(lock);
        assert(_cache->surface);

        auto cachect = Context(*_cache->surface);
        cachect.rectangle(*carea);
        cachect.set_operator(Cairo::Context::Operator::SOURCE);
        cachect.setSource(*intermediate);
        cachect.fill();
        _cache->surface->markClean(*carea);
    }

    dc.save(); // Prevent Translate from accumulating
    dc.translate(Geom::Translate(carea->min()));
    dc.rectangle(Geom::Rect::from_xywh({0, 0}, carea->dimensions()));

    dc.setSource(*intermediate);

    // 7. Render blend mode
    dc.setOperator(_blend_mode);
    dc.fill();
    dc.resetSource(0);
    dc.restore();

    // Web isolation only works if parent doesn't have transform

    // the call above is to clear a ref on the intermediate surface held by dc

    return render_result;
}

/**
 * A stand alone render, ignoring all other objects in the document.
 */
unsigned DrawingItem::render(Context &dc, Geom::IntRect const &area, unsigned flags) const
{
    auto rc = DrawingOptions{
        .outline_color = _drawing.outlineColor(),
        .antialiasing_override = _drawing._antialiasing_override,
        .dithering = _drawing._use_dithering
    };
    return render(dc, rc, area, flags);
}

void DrawingItem::_renderOutline(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags) const
{
    // intersect with bbox rather than drawbox, as we want to render things outside
    // of the clipping path as well
    auto carea = Geom::intersect(area, _bbox);
    if (!carea) return;

    // just render everything: item, clip, mask
    // First, render the object itself
    _renderItem(dc, rc, *carea, flags, nullptr);

    // render clip and mask, if any
    auto saved = rc.outline_color; // save current outline color
    // render clippath as an object, using a different color
    if (_clip) {
        rc.outline_color = _drawing.clipOutlineColor();
        _clip->render(dc, rc, *carea, flags);
    }
    // render mask as an object, using a different color
    if (_mask) {
        rc.outline_color = _drawing.maskOutlineColor();
        _mask->render(dc, rc, *carea, flags);
    }
    rc.outline_color = saved; // restore outline color
}

/**
 * Rasterize the clipping path.
 * This method submits drawing operations required to draw a basic filled shape
 * of the item to the supplied drawing context. Rendering is limited to the
 * given area. The rendering of the clipped object is composited into
 * the result of this call using the IN operator. See the implementation
 * of render() for details.
 */
void DrawingItem::clip(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const
{
    // don't bother if the object does not implement clipping (e.g. DrawingImage)
    if (!_canClip()) return;
    if (!_visible) return;
    if (!area.intersects(_bbox)) return;

    dc.resetSource(1);
    dc.pushGroup();
    // rasterize the clipping path
    _clipItem(dc, rc, area);
    if (_clip) {
        // The item used as the clipping path itself has a clipping path.
        // Render this item's clipping path onto a temporary surface, then composite it
        // with the item using the IN operator
        dc.pushGroup();
        _clip->clip(dc, rc, area);
        dc.popGroupToSource();
        dc.set_operator(Cairo::Context::Operator::IN);
        dc.paint();
    }
    dc.popGroupToSource();
    dc.set_operator(Cairo::Context::Operator::OVER);
    dc.paint();
    dc.resetSource(0);
}

/**
 * Get the item under the specified point.
 * Searches the tree for the first item in the Z-order which is closer than
 * @a delta to the given point. The pick should be visual - for example
 * an object with a thick stroke should pick on the entire area of the stroke.
 * @param p Search point
 * @param delta Maximum allowed distance from the point
 * @param sticky Whether the pick should ignore visibility and sensitivity.
 *               When false, only visible and sensitive objects are considered.
 *               When true, invisible and insensitive objects can also be picked.
 */
DrawingItem *DrawingItem::pick(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags)
{
    // Sometimes there's no BBOX in state, reason unknown (bug 992817)
    // I made this not an assert to remove the warning
    if (!(_state & STATE_BBOX) || !(_state & STATE_PICK)) {
        g_warning("Invalid state when picking: STATE_BBOX = %d, STATE_PICK = %d", _state & STATE_BBOX, _state & STATE_PICK);
        return nullptr;
    }
    // ignore invisible and insensitive items unless sticky
    if (!(flags & PICK_STICKY) && !(_visible && _sensitive)) {
        return nullptr;
    }

    bool outline = flags & PICK_OUTLINE;

    if (!outline) {
        // pick inside clipping path; if NULL, it means the object is clipped away there
        if (_clip) {
            DrawingItem *cpick = _clip->pick(p, delta, area_world, flags | PICK_AS_CLIP);
            if (!cpick) {
                return nullptr;
            }
        }
        // same for mask
        if (_mask) {
            DrawingItem *mpick = _mask->pick(p, delta, area_world, flags);
            if (!mpick) {
                return nullptr;
            }
        }
    }

    Geom::OptIntRect box = outline || (flags & PICK_AS_CLIP) ? _bbox : _drawbox;
    if (!box) {
        return nullptr;
    }

    Geom::Rect expanded = *box;
    expanded.expandBy(delta);
    auto dglyps = cast<DrawingGlyphs>(this);
    if (dglyps && !(flags & PICK_AS_CLIP)) {
        expanded = dglyps->getPickBox();
    }

    if (expanded.contains(p)) {
        return _pickItem(p, delta, area_world, flags);
    }
    return nullptr;
}

// For debugging: Print drawing tree structure.
/*
void DrawingItem::recursivePrintTree(unsigned level) const
{
    if (level == 0) {
        std::cout << "Display Item Tree" << std::endl;
    }
    std::cout << "DI: ";
    for (int i = 0; i < level; i++) {
        std::cout << "  ";
    }
    std::cout << name() << std::endl;
    for (auto &i : _children) {
        i.recursivePrintTree(level + 1);
    }
}
*/

/**
 * Marks the current visual bounding box of the item for redrawing.
 * This is called whenever the object changes its visible appearance.
 * For some cases (such as setting opacity) this is enough, but for others
 * _markForUpdate() also needs to be called.
 */
void DrawingItem::_markForRendering()
{
    bool outline = _drawing.renderMode() == RenderMode::OUTLINE || _drawing.outlineOverlay();
    Geom::OptIntRect dirty = outline ? _bbox : _drawbox;
    if (!dirty) return;

    // dirty the caches of all parents
    DrawingItem *bkg_root = nullptr;

    for (auto i = this; i; i = i->_parent) {
        if (i != this && i->_filter) {
            i->_filter->area_enlarge(*dirty, i->ctm());
        }
        if (i->_cache && i->_cache->surface) {
            i->_cache->surface->markDirty(*dirty);
        }
        i->_dropPatternCache();
        if (i->_background_accumulate) {
            bkg_root = i;
        }
    }

    if (bkg_root && bkg_root->_parent && bkg_root->_parent->_parent) {
        bkg_root->_invalidateFilterBackground(*dirty);
    }

    _drawing._redraw_area_signal.emit(*dirty);
}

void DrawingItem::_invalidateFilterBackground(Geom::IntRect const &area)
{
    if (!_drawbox.intersects(area)) return;

    if (_cache && _cache->surface && _filter && (
                _filter->uses_input(DrawingFilter::SLOT_BACKGROUND_IMAGE)
             || _filter->uses_input(DrawingFilter::SLOT_BACKGROUND_ALPHA))) {
        _cache->surface->markDirty(area);
    }

    for (auto & i : _children) {
        i._invalidateFilterBackground(area);
    }
}

/**
 * Marks the item as needing a recomputation of internal data.
 *
 * This mechanism avoids traversing the entire rendering tree (which could be vast)
 * on every trivial state changed in any item. Only items marked as needing
 * an update (having some bits in their _state unset) will be traversed
 * during the update call.
 *
 * The _propagate variable is another optimization. We use it to specify that
 * all children should also have the corresponding flags unset before checking
 * whether they need to be traversed. This way there is one less traversal
 * of the tree. Without this we would need to unset state bits in all children.
 * With _propagate we do this during the update call, when we have to recurse
 * into children anyway.
 */
void DrawingItem::_markForUpdate(unsigned flags, bool propagate)
{
    if (propagate) {
        _propagate_state |= flags;
    }

    if (_state & flags) {
        unsigned oldstate = _state;
        _state &= ~flags;
        if (oldstate != _state && _parent) {
            // If we actually reset anything in state, recurse on the parent.
            _parent->_markForUpdate(flags, false);
        } else {
            // If nothing changed, it means our ancestors are already invalidated
            // up to the root. Do not bother recursing, because it won't change anything.
            // Also do this if we are the root item, because we have no more ancestors
            // to invalidate.
            _drawing._drawing_updated_signal.emit();
        }
    }
}

/**
 * Compute the caching score.
 *
 * Higher scores mean the item is more aggressively prioritized for automatic
 * caching by Inkscape::Drawing.
 */
double DrawingItem::_cacheScore()
{
    Geom::OptIntRect cache_rect = _cacheRect();
    if (!cache_rect) return -1.0;
    // a crude first approximation:
    // the basic score is the number of pixels in the drawbox
    double score = cache_rect->area();
    // this is multiplied by the filter complexity and its expansion
    if (_filter && _drawing.renderMode() != RenderMode::NO_FILTERS) {
        score *= _filter->complexity(_ctm);
        Geom::IntRect ref_area = Geom::IntRect::from_xywh(0, 0, 16, 16);
        Geom::IntRect test_area = ref_area;
        Geom::IntRect limit_area(0, INT_MIN, 16, INT_MAX);
        _filter->area_enlarge(test_area, ctm());
        // area_enlarge never shrinks the rect, so the result of intersection below must be non-empty
        score *= (double)(test_area & limit_area)->area() / ref_area.area();
    }
    // if the object is clipped, add 1/2 of its bbox pixels
    if (_clip && _clip->_bbox) {
        score += _clip->_bbox->area() * 0.5;
    }
    // if masked, add mask score
    if (_mask) {
        score += _mask->_cacheScore();
    }
    //g_message("caching score: %f", score);
    return score;
}

Geom::OptIntRect DrawingItem::_cacheRect() const
{
    return _drawbox & _drawing.cacheLimit();
}

void propagate_antialias(SPShapeRendering shape_rendering, DrawingItem &item)
{
    switch (shape_rendering) {
        case SP_CSS_SHAPE_RENDERING_AUTO:
            item.setAntialiasing(Antialiasing::Good);
            break;
        case SP_CSS_SHAPE_RENDERING_OPTIMIZESPEED:
            item.setAntialiasing(Antialiasing::Fast);
            break;
        case SP_CSS_SHAPE_RENDERING_CRISPEDGES:
            item.setAntialiasing(Antialiasing::None);
            break;
        case SP_CSS_SHAPE_RENDERING_GEOMETRICPRECISION:
            item.setAntialiasing(Antialiasing::Best);
            break;
        default:
            g_assert_not_reached();
    }
}

// Remove this node from its parent, then delete it.
void DrawingItem::unlink()
{
    defer([=, this] {
        // This only happens for the top-level deleted item.
        if (_parent) {
            _markForRendering();
        }

        switch (_child_type) {
            case ChildType::NORMAL: {
                auto it = _parent->_children.iterator_to(*this);
                _parent->_children.erase(it);
                break;
            }
            case ChildType::CLIP:
                _parent->_clip = nullptr;
                break;
            case ChildType::MASK:
                _parent->_mask = nullptr;
                break;
            case ChildType::FILL:
                _parent->_fill_pattern = nullptr;
                break;
            case ChildType::STROKE:
                _parent->_stroke_pattern = nullptr;
                break;
            case ChildType::ROOT:
                _drawing._root = nullptr;
                break;
            default:
                break;
        }

        if (_parent) {
            bool propagate = _child_type == ChildType::CLIP || _child_type == ChildType::MASK;
            _parent->_markForUpdate(STATE_ALL, propagate);
        }

        delete this;
    });
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
