// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A container class for filter slots. Allows for simple getting and
 * setting images in filter slots without having to bother with
 * table indexes and such.
 *
 * Author:
 *   Niko Kiirala <niko@kiirala.com>
 *
 * Copyright (C) 2006,2007 Niko Kiirala
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cassert>
#include <cstring>

#include "slot.h"

#include <2geom/transforms.h>
#include "renderer/surface.h"
#include "renderer/context.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"

namespace Inkscape::Renderer::DrawingFilter {

std::shared_ptr<Surface> Slot::get(int slot) const
{
    if (slot == SLOT_NOT_SET) {
        slot = _last_out;
    }

    auto found = _slots.find(slot);
    if (found == _slots.end()) {
        return {};
    }
    return found->second;
}

std::shared_ptr<Surface> Slot::get(int slot, std::shared_ptr<Colors::Space::AnySpace> const &space) const
{
    auto surface = get(slot);
    if (!surface) {
        return {};
    }
    // If a surface is in INT format, we refuse to convert it and instead just return as is.
    // Color Space support for filters is disabled for INT surfaces except for ALPHA.
    if ((_int_based && (!space || space->getType() != Colors::Space::Type::Alpha)) || space == surface->getColorSpace()) {
        return surface;
    }
    if (space) {
        // Return a version of the surface in the new color space
        return surface->convertedToColorSpace(space);
    }

    std::cerr << "Warning: filter had no color space set despite linearRGB being the default.\n";
    return get(slot, Colors::Manager::get().find(Colors::Space::Type::linearRGB));
}

std::shared_ptr<Surface> Slot::get_copy(int slot) const
{
    if (auto surface = get(slot)) {
        auto copy = surface->similar();
        auto context = Context(*copy);
        context.setSource(*surface);
        context.set_operator(Cairo::Context::Operator::SOURCE);
        context.paint();
        return copy;
    }
    return {};
}

std::shared_ptr<Surface> Slot::get_copy(int slot, std::shared_ptr<Colors::Space::AnySpace> const &space) const
{
    auto surface = get(slot);
    if (!surface) {
        return {};
    }
    if (!surface->getColorSpace() || surface->getColorSpace() != space) {
        // Converting makes a copy for us (if we actually convert), so check if we do
        auto copy = get(slot, space);
        if (copy != surface) {
            return copy;
        }
        // else fall back to get_copy below
    }
    return get_copy(slot);
}

void Slot::set_alpha(int slot_from, int slot_to)
{
    static auto alpha = Colors::Manager::get().find(Colors::Space::Type::Alpha);
    if (auto color_surface = get(slot_from)) {
        // Make an alpha surface and copy the alpha into it from the source slot
        set(slot_to, color_surface->convertedToColorSpace(alpha));
    } else {
        g_error("Couldn't convert a filter image source(%d) into an alpha(%d), source image missing.", slot_from, slot_to);
    }
}

void Slot::set(int slot, std::shared_ptr<Surface> surface)
{
    if (slot == SLOT_NOT_SET)
        slot = SLOT_UNNAMED;

    std::ostringstream msg;
    if (_int_based && surface->format() == CAIRO_FORMAT_RGBA128F) {
        msg << "Refusing to add floating point surface as filter slot to integer drawing: slot(" << slot << ")";
        throw Surface::SurfaceError(msg.str());
    }
    if (!_int_based && surface->format() == CAIRO_FORMAT_ARGB32) {
        msg << "Refusing to add integer surface as filter slot to floating point drawing: slot(" << slot << ")";
        throw Surface::SurfaceError(msg.str());
    }

    // This crufty bit of code *untransforms* the rendered source or background
    // so the filter can be applied to the original orientation before being
    // re-transformed when painted back.
    auto trans = _item_opt.get_matrix_item2filter();
    if ((slot == SLOT_SOURCE_IMAGE || slot == SLOT_BACKGROUND_IMAGE) && trans) {
        auto sbox = _item_opt.get_slot_box();
        auto tsg = surface->similar(sbox->dimensions().ceil());
        auto context = Context(*tsg);
        context.transform(*trans);
        context.setSource(*surface);
        context.set_operator(Cairo::Context::Operator::SOURCE);
        context.paint();

        // Save the input surface for later as it's the right size and format for the result
        if (slot == SLOT_SOURCE_IMAGE) {
            _slots[SLOT_RESULT] = std::move(surface);
        }
        surface = tsg;
    }

    auto found = _slots.find(slot);
    if (found == _slots.end() || found->second != surface) {
        _slots[slot] = std::move(surface);
    }

    _last_out = slot;
}

std::shared_ptr<Surface> Slot::get_result(int slot) const
{
    auto result = get(slot, get(SLOT_SOURCE_IMAGE)->getColorSpace());

    // This is the mirror of "crufty bit of code" above to undo the
    // transformation which was added to the source graphic.
    if (auto trans = _item_opt.get_matrix_item2filter()) {
        auto output = get(SLOT_RESULT); // Reuse from set(...)
        auto context = Context(*output);
        context.transform(trans->inverse());
        context.setSource(*result);
        context.set_operator(Cairo::Context::Operator::SOURCE);
        context.paint();
        return output;
    }

    return result;
}

void Slot::set_primitive_area(int slot, Geom::Rect &area)
{
    if (slot == SLOT_NOT_SET)
        slot = SLOT_UNNAMED;

    _primitiveAreas[slot] = area;
}

Geom::Rect Slot::get_primitive_area(int slot) const
{
    if (slot == SLOT_NOT_SET)
        slot = _last_out;

    auto s = _primitiveAreas.find(slot);

    if (s == _primitiveAreas.end()) {
        //return *_units.get_filter_area();
    }
    return s->second;
}

} // namespace Inkscape::Renderer::DrawingFilter

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
