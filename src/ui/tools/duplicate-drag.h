// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_TOOLS_DUPLICATE_DRAG_H
#define INKSCAPE_UI_TOOLS_DUPLICATE_DRAG_H

#include <vector>
#include <2geom/point.h>
#include "util-string/context-string.h"

class SPItem;
namespace Inkscape { class ObjectSet; }

namespace Inkscape::UI::Tools {

// Owns the pending duplicate and restores the original selection on cancellation.
// The caller must finish its transform without committing a separate undo event.
class DuplicateDrag
{
public:
    DuplicateDrag(ObjectSet &selection, SPItem &hit);
    ~DuplicateDrag();
    DuplicateDrag(DuplicateDrag const &) = delete;
    DuplicateDrag &operator=(DuplicateDrag const &) = delete;

    void commit(Util::Internal::ContextString description);
    void cancel();

private:
    ObjectSet &_selection;
    std::vector<SPItem *> _originals;
    bool _active = true;
};

// Project a displacement onto the nearest horizontal, vertical, or 45-degree line.
Geom::Point constrain_duplicate_drag(Geom::Point const &delta);

} // namespace Inkscape::UI::Tools
#endif
