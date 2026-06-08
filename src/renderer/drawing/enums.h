// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Enums for rendering content
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_ENUMS_H
#define SEEN_INKSCAPE_RENDERER_ENUMS_H

namespace Inkscape::Renderer {

enum StateFlags
{
    STATE_NONE       = 0,
    STATE_BBOX       = 1 << 0, // bounding boxes are up-to-date
    STATE_CACHE      = 1 << 1, // cache extents and clean area are up-to-date
    STATE_PICK       = 1 << 2, // can process pick requests
    STATE_RENDER     = 1 << 3, // can be rendered
    STATE_BACKGROUND = 1 << 4, // filter background data is up to date
    STATE_ALL        = (1 << 5) - 1,
    STATE_TOTAL_INV  = 1 << 5, // used as a reset flag only
};


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
