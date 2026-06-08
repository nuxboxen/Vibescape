// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Canvas item belonging to an SVG drawing element.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_ITEM_H
#define INKSCAPE_RENDERER_DRAWING_ITEM_H

#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <2geom/affine.h>
#include <2geom/rect.h>
#include <sigc++/signal.h>
#include <sigc++/connection.h>

#include "style-enums.h"

#include "drawing-item-tags.h"
#include "drawing-options.h"
#include "drawing.h"

namespace Glib { class ustring; }

class SPItem;

namespace Inkscape::Renderer {

class Context;
class Drawing;
class DrawingItem;
class DrawingPattern;

namespace DrawingFilter { class Filter; }

struct UpdateContext
{
    Geom::Affine ctm;
};

struct InvalidItemException : std::exception
{
    char const *what() const noexcept override { return "Invalid item in drawing"; }
};

class DrawingItem
{
public:
    enum RenderFlags
    {
        RENDER_DEFAULT           = 0,
        RENDER_CACHE_ONLY        = 1 << 0,
        RENDER_BYPASS_CACHE      = 1 << 1,
        RENDER_FILTER_BACKGROUND = 1 << 2,
        RENDER_OUTLINE           = 1 << 3,
        RENDER_NO_FILTERS        = 1 << 4,
        RENDER_VISIBLE_HAIRLINES = 1 << 5
    };
    enum PickFlags
    {
        PICK_NORMAL  = 0,      // normal pick
        PICK_STICKY  = 1 << 0, // sticky pick - ignore visibility and sensitivity
        PICK_AS_CLIP = 1 << 1, // pick with no stroke and opaque fill regardless of item style
        PICK_OUTLINE = 1 << 2  // pick in outline mode
    };

    DrawingItem(Drawing &drawing);
    DrawingItem(DrawingItem const &) = delete;
    DrawingItem &operator=(DrawingItem const &) = delete;
    void unlink(); /// Unlink this node and its subtree from the rendering tree and destroy.
    virtual int tag() const { return tag_of<decltype(*this)>; }

    Geom::OptIntRect const &bbox() const { return _bbox; }
    Geom::OptIntRect const &drawbox() const { return _drawbox; }
    Geom::OptRect const &itemBounds() const { return _item_bbox; }
    Geom::Affine const &ctm() const { return _ctm; }
    Geom::Affine transform() const { return _transform ? *_transform : Geom::identity(); }
    Drawing &drawing() const { return _drawing; }
    DrawingItem *parent() const { return _parent; }
    bool isAncestorOf(DrawingItem const *item) const;
    int getUpdateComplexity() const { return _update_complexity; }
    bool unisolatedBlend() const;

    void appendChild(DrawingItem *item);
    void prependChild(DrawingItem *item);
    void clearChildren();

    bool visible() const { return _visible; }
    void setVisible(bool visible);
    bool sensitive() const { return _sensitive; }
    void setSensitive(bool sensitive);

    template <typename StyleSource>
    void setStyle(StyleSource const *style, StyleSource const *context_style = nullptr)
    {
        defer([this, nrstyle = DrawingStyle(style, context_style)] () mutable {
            _nrstyle = std::move(nrstyle);
        });
    }

    // Recursively update context styles in all children
    template <typename StyleSource>
    void setChildrenStyle(StyleSource const *style) {
        defer([this, style] () mutable {
            _nrstyle.set_context_style(style);
        });
        for (auto &i : _children) {
            i.setChildrenStyle(style);
        }
    }

    DrawingStyle _nrstyle;

    void setOpacity(float opacity);
    void setOpacityOverride(std::optional<double> opacity);
    void setAntialiasing(Antialiasing antialias);
    void setIsolation(bool isolation); // CSS Compositing and Blending
    void setBlendMode(SPBlendMode blend_mode);
    void setTransform(Geom::Affine const &trans);
    void setClip(DrawingItem *item);
    void setMask(DrawingItem *item);
    void setFillPattern(DrawingPattern *pattern);
    void setStrokePattern(DrawingPattern *pattern);
    void setZOrder(unsigned zorder);
    void setItemBounds(Geom::OptRect const &bounds);
    void setFilterRenderer(std::unique_ptr<DrawingFilter::Filter> renderer);

    void setKey(unsigned key) { _key = key; }
    unsigned key() const { return _key; }
    void setItem(SPItem *item) { _item = item; }
    SPItem *getItem() const { return _item; } // SPItem

    void update(Geom::IntRect const &area = Geom::IntRect::infinite(), UpdateContext const &ctx = UpdateContext(), unsigned flags = STATE_ALL, unsigned reset = 0);
    unsigned render(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags = 0, DrawingItem const *stop_at = nullptr) const;
    unsigned render(Context &dc, Geom::IntRect const &area, unsigned flags = 0) const;
    void clip(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const;
    DrawingItem *pick(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags = 0);

    Glib::ustring name() const; // For debugging
    void recursivePrintTree(unsigned level = 0) const;  // For debugging

    sigc::connection connectItemDeleted(sigc::slot<void ()> const &slot) { return _delete_item_signal.connect(slot); }

    inline double getOpacity() const { return _opacity_override ? *_opacity_override : _opacity; }
    inline bool hasOpacity() const { return getOpacity() < 0.995; }

protected:
    enum class ChildType : unsigned char
    {
        ORPHAN = 0, // No parent - implies !parent.
        NORMAL = 1, // Contained in children of parent.
        CLIP   = 2, // Referenced by clip of parent.
        MASK   = 3, // Referenced by mask of parent.
        FILL   = 4, // Referenced by fill pattern of parent.
        STROKE = 5, // Referenced by stroke pattern of parent.
        ROOT   = 6  // Referenced by root of drawing.
    };
    enum RenderResult
    {
        RENDER_OK   = 0,
        RENDER_STOP = 1
    };
    virtual ~DrawingItem(); // Private to prevent deletion of items that are still in use by a snapshot.
    void _renderOutline(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags) const;
    void _markForUpdate(unsigned state, bool propagate);
    void _markForRendering();
    void _invalidateFilterBackground(Geom::IntRect const &area);
    double _cacheScore();
    Geom::OptIntRect _cacheRect() const;
    void _setCached(bool cached, bool persistent = false);
    virtual unsigned _updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset) { return 0; }
    virtual unsigned _renderItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const { return RENDER_OK; }
    virtual void _clipItem(Context &dc, DrawingOptions &rc, Geom::IntRect const &area) const {}
    virtual DrawingItem *_pickItem(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags) { return nullptr; }
    virtual bool _canClip() const { return false; }
    virtual void _dropPatternCache() {}

    Drawing &_drawing;
    DrawingItem *_parent;

    using ListHook = boost::intrusive::list_member_hook<>;
    ListHook _child_hook;

    using ChildrenList = boost::intrusive::list<
        DrawingItem,
        boost::intrusive::member_hook<DrawingItem, ListHook, &DrawingItem::_child_hook>
        >;
    ChildrenList _children;

    // Todo: Try to get rid of all of these variables, moving them into the object tree.
    unsigned _key; ///< Auxiliary key used by the object tree for showing clips/masks/patterns.
    SPItem *_item; ///< Used to associate DrawingItems with SPItems that created them

    float _opacity;
    std::optional<double> _opacity_override;
    std::unique_ptr<Geom::Affine> _transform; ///< Incremental transform from parent to this item's coords
    Geom::Affine _ctm; ///< Total transform from item coords to display coords
    Geom::OptIntRect _bbox; ///< Bounding box in display (pixel) coords including stroke
    Geom::OptIntRect _drawbox; ///< Full visual bounding box - enlarged by filters, shrunk by clips and masks
    Geom::OptRect _item_bbox; ///< Geometric bounding box in item's user space.
                               ///  This is used to compute the filter effect region and render in
                               ///  objectBoundingBox units.

    DrawingItem *_clip;
    DrawingItem *_mask;
    DrawingPattern *_fill_pattern;
    DrawingPattern *_stroke_pattern;
    std::unique_ptr<DrawingFilter::Filter> _filter;
    std::unique_ptr<CacheData> _cache;
    int _update_complexity = 0;
    bool _contains_unisolated_blend : 1;

    CacheSet::iterator _cache_iterator;
    CacheRecord _cache_record;

    unsigned _state : 8;
    unsigned _propagate_state : 8;
    ChildType _child_type : 3;
    unsigned _background_accumulate : 1; ///< Whether this element accumulates background
                                         ///  (has any ancestor with enable-background: new)
    unsigned _visible : 1;
    unsigned _sensitive : 1; ///< Whether this item responds to events
    unsigned _cached_persistent : 1; ///< If set, will always be cached regardless of score
    unsigned _has_cache_iterator : 1; ///< If set, _cache_iterator is valid
    unsigned _pick_children : 1; ///< For groups: if true, children are returned from pick(),
                                      ///  otherwise the group is returned
    Antialiasing _antialias : 2; ///< antialiasing level (default is Good)

    bool _isolation : 1;
    SPBlendMode _blend_mode;

    template<typename F>
    void defer(F &&f)
    {
        // Introduce artificial dependence on a template parameter to allow definition with Drawing forward-declared.
        auto &drawing = static_cast<std::enable_if_t<(sizeof(F) > 0), Drawing&>>(_drawing);
        drawing.defer(std::forward<F>(f));
    }

    sigc::signal<void()> _delete_item_signal;

    friend class Drawing;
};

/// Propagate element's shape rendering attribute into internal anti-aliasing setting of DrawingItem.
void propagate_antialias(SPShapeRendering shape_rendering, DrawingItem &item);

} // namespace Inkscape

#endif // INKSCAPE_RENDERER_DRAWING_ITEM_H

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
