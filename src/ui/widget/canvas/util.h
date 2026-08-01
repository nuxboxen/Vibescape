// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_CANVAS_UTIL_H
#define INKSCAPE_UI_WIDGET_CANVAS_UTIL_H

#include <cairomm/context.h>
#include <cairomm/refptr.h>
#include <cairomm/region.h>

#include "colors/color.h"

namespace Inkscape {
namespace UI {
namespace Widget {

// Cairo additions

/**
 * Shrink a region by d/2 in all directions, while also translating it by (d/2 + t, d/2 + t).
 */
Cairo::RefPtr<Cairo::Region> shrink_region(Cairo::RefPtr<Cairo::Region> const &reg, int d, int t = 0);

inline auto unioned(Cairo::RefPtr<Cairo::Region> a, Cairo::RefPtr<Cairo::Region> const &b)
{
    a->do_union(b);
    return a;
}

inline auto premultiplied(std::array<float, 4> arr)
{
    arr[0] *= arr[3];
    arr[1] *= arr[3];
    arr[2] *= arr[3];
    return arr;
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_CANVAS_UTIL_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
