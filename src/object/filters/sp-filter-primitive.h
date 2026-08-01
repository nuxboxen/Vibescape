// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_FILTER_PRIMITIVE_H
#define SEEN_SP_FILTER_PRIMITIVE_H

/** \file
 * Document level base class for all SVG filter primitives.
 */
/*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Niko Kiirala <niko@kiirala.com>
 *
 * Copyright (C) 2006,2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <optional>
#include <memory>
#include <string>
#include <2geom/rect.h>
#include "colors/color.h"
#include "colors/manager.h"
#include "document.h"
#include "object/sp-object.h"
#include "object/sp-dimensions.h"
#include "renderer/drawing-filters/enums.h"

namespace Inkscape::Renderer {
class Drawing;
class DrawingItem;
namespace DrawingFilter {
class Filter;
class Primitive;
} // namespace Filters
} // namespace Inkscape::Renderer

class SlotResolver;

class SPFilterPrimitive
    : public SPObject
    , public SPDimensions
{
public:
	SPFilterPrimitive();
	~SPFilterPrimitive() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    int get_in() const { return in_slot; }
    int get_out() const { return out_slot; }

    virtual void show(Inkscape::Renderer::DrawingItem *item) {}
    virtual void hide(Inkscape::Renderer::DrawingItem *item) {}

    virtual std::unique_ptr<Inkscape::Renderer::DrawingFilter::Primitive> build_renderer(Inkscape::Renderer::DrawingItem *item) const = 0;

    /* Calculate the filter's effect on the region */
    virtual Geom::Rect calculate_region(Geom::Rect const &region) const;

    /* Return true if the object should be allowed to use this filter */
    virtual bool valid_for(SPObject const *obj) const
    {
        // This is used by feImage to stop infinite loops.
        return true;
    };

    void invalidate_parent_slots();
    virtual void resolve_slots(SlotResolver &);

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void release() override;
    void set(SPAttr key, char const *value) override;
    void update(SPCtx *ctx, unsigned flags) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, unsigned flags) override;

    // Common initialization for filter primitives.
    void build_renderer_common(Inkscape::Renderer::DrawingFilter::Primitive *primitive) const;

private:
    std::optional<std::string> in_name, out_name;
    int in_slot = Inkscape::Renderer::DrawingFilter::SLOT_NOT_SET;
    int out_slot = Inkscape::Renderer::DrawingFilter::SLOT_NOT_SET;
};

#endif // SEEN_SP_FILTER_PRIMITIVE_H

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
