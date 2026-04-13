// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Quantization for Inkscape
 *
 * Authors:
 *   Stéphane Gimenez <dev@gim.name>
 *
 * Copyright (C) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_TRACE_QUANTIZE_H
#define INKSCAPE_TRACE_QUANTIZE_H

#include <vector>
#include "imagemap.h"

namespace Inkscape {
namespace Trace {

/**
 * Quantize an RGB image to a reduced number of colors.
 */
IndexedMap rgbMapQuantize(RgbMap const &rgbmap, int nrColors);

/**
 * Map an RGB image to a user-supplied palette of colors.
 * Each pixel is assigned to the nearest color in the palette.
 */
IndexedMap rgbMapWithPalette(RgbMap const &rgbmap, std::vector<RGB> const &palette);

} // namespace Trace
} // namespace Inkscape

#endif // INKSCAPE_TRACE_QUANTIZE_H
