// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Stringstream internal linking
 *
 * Copyright (C) 2026 AUTHORS
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "precision.h"
#include "preferences.h"

// WARNING: Do not include this file in Unit Testing! Use the mock file instead.

namespace Inkscape::Util {

int get_default_numeric_precision()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    return prefs->getInt("/options/svgoutput/numericprecision", 8);
}

} // namespace Inkscape::Util

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
