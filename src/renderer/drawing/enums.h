// SPDX-License-Identifier: GPL-2.0-or-later
 /*
 * Authors: see git history
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_ENUMS_H
#define SEEN_INKSCAPE_RENDERER_ENUMS_H

namespace Inkscape::Renderer {

enum class RenderMode {
    NORMAL,
    OUTLINE,
    NO_FILTERS,
    VISIBLE_HAIRLINES,
    OUTLINE_OVERLAY,
    size
};

enum class SplitMode {
    NORMAL,
    SPLIT,
    XRAY,
    size
};

enum class SplitDirection {
    NONE,
    NORTH,
    EAST,
    SOUTH,
    WEST,
    HORIZONTAL, // Only used when hovering
    VERTICAL    // Only used when hovering
};

enum class ColorMode {
    NORMAL,
    GRAYSCALE,
    PRINT_COLORS_PREVIEW
};

} // Namespace Inkscape::Renderer

#endif // SEEN_INKSCAPE_RENDERER_ENUMS_H

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
