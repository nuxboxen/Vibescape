// SPDX-License-Identifier: GPL-2.0-or-later
#include "duplicate-drag.h"

#include <array>
#include <limits>

#include "document.h"
#include "document-undo.h"
#include "object/object-set.h"

namespace Inkscape::UI::Tools {

DuplicateDrag::DuplicateDrag(ObjectSet &selection, SPItem &hit)
    : _selection(selection)
{
    for (auto original : selection.items()) {
        _originals.push_back(static_cast<SPItem *>(sp_object_ref(original)));
    }
    if (!selection.includes(&hit, true)) {
        selection.set(&hit);
    }
    selection.duplicate(true);
    // Text and other dependent geometry must be current before the drag takes its bounds.
    selection.document()->ensureUpToDate();
}

DuplicateDrag::~DuplicateDrag()
{
    cancel();
    for (auto original : _originals) {
        sp_object_unref(original);
    }
}

void DuplicateDrag::commit(Util::Internal::ContextString description)
{
    if (!_active) return;
    DocumentUndo::done(_selection.document(), description, "edit-duplicate");
    _active = false;
}

void DuplicateDrag::cancel()
{
    if (!_active) return;
    _selection.clear();
    DocumentUndo::cancel(_selection.document());
    for (auto original : _originals) {
        if (original->document == _selection.document()) {
            _selection.add(original);
        }
    }
    _selection.document()->ensureUpToDate();
    _active = false;
}

Geom::Point constrain_duplicate_drag(Geom::Point const &delta)
{
    static std::array<Geom::Point, 4> const directions = {
        Geom::Point(1, 0), Geom::Point(0, 1), Geom::Point(1, 1), Geom::Point(1, -1)
    };
    auto best = delta;
    auto distance = std::numeric_limits<double>::infinity();
    for (auto const &direction : directions) {
        auto const projected = direction * (Geom::dot(delta, direction) / Geom::dot(direction, direction));
        auto const difference = delta - projected;
        auto const candidate = Geom::dot(difference, difference);
        if (candidate < distance) {
            best = projected;
            distance = candidate;
        }
    }
    return best;
}

} // namespace Inkscape::UI::Tools
