// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief SVG composite filter effect
 *//*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SP_FECOMPOSITE_H_SEEN
#define SP_FECOMPOSITE_H_SEEN

#include "sp-filter-primitive.h"
#include "renderer/drawing-filters/enums.h"

class SPFeComposite final
    : public SPFilterPrimitive
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

    CompositeOperator get_composite_operator() const { return composite_operator; }
    int get_in2() const { return in2_slot; }

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) override;

    void resolve_slots(SlotResolver &) override;
    std::unique_ptr<Inkscape::Renderer::DrawingFilter::Primitive> build_renderer(Inkscape::Renderer::DrawingItem *item) const override;

private:
    CompositeOperator composite_operator = CompositeOperator::DEFAULT;
    double k1 = 0.0;
    double k2 = 0.0;
    double k3 = 0.0;
    double k4 = 0.0;

    std::optional<std::string> in2_name;
    int in2_slot = Inkscape::Renderer::DrawingFilter::SLOT_NOT_SET;
};

#endif // SP_FECOMPOSITE_H_SEEN

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
