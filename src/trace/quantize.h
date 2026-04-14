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
 * Estimate the optimal number of colors for an image and return the
 * corresponding palette. Uses the elbow method on k-means inertia
 * in Oklab space, testing k=2..maxColors.
 */
std::vector<RGB> estimateOptimalPalette(RgbMap const &rgbmap, int maxColors = 16);

/**
 * Given an existing palette and an image, find the best next color to add.
 * Uses constrained k-means: existing colors are frozen, only the new
 * centroid is free to converge. Initialized at the sample point most
 * distant from the current palette.
 */
RGB findNextPaletteColor(RgbMap const &rgbmap, std::vector<RGB> const &existingPalette);

/**
 * Map an RGB image to a user-supplied palette of colors.
 * Each pixel is assigned to the perceptually nearest color in the palette
 * (using Oklab distance).
 */
IndexedMap rgbMapWithPalette(RgbMap const &rgbmap, std::vector<RGB> const &palette);

} // namespace Trace
} // namespace Inkscape

#endif // INKSCAPE_TRACE_QUANTIZE_H
