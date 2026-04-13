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
 * Quantize an RGB image using k-means in Oklab perceptual color space.
 * Produces better hue diversity than octree quantization, especially
 * for low color counts.
 */
IndexedMap rgbMapQuantizePerceptual(RgbMap const &rgbmap, int nrColors);

/**
 * Map an RGB image to a user-supplied palette of colors.
 * Each pixel is assigned to the perceptually nearest color in the palette
 * (using Oklab distance).
 */
IndexedMap rgbMapWithPalette(RgbMap const &rgbmap, std::vector<RGB> const &palette);

} // namespace Trace
} // namespace Inkscape

#endif // INKSCAPE_TRACE_QUANTIZE_H
