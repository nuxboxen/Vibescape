// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "hsl.h"

#include <cmath>

#include "colors/color.h"
#include "colors/printer.h"

namespace Inkscape::Colors::Space {

/**
 * Print the HSL color to a CSS string.
 *
 * @arg values - A vector of doubles for each channel in the HSL space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string HSL::toString(std::vector<double> const &values, bool opacity) const
{
    static constexpr double CSS_SL_SCALE = 100.0;
    auto oo = CssLegacyPrinter(3, "hsl", opacity && values.size() == 4);
    // First entry is Hue, which is in degrees
    return oo << (int)(values[0] * 360) << values[1] * CSS_SL_SCALE << values[2] * CSS_SL_SCALE << values.back();
}

}; // namespace Inkscape::Colors::Space
