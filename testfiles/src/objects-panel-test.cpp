// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Regression tests for the Layers and Objects panel.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtest/gtest.h>

namespace Inkscape::UI::Dialog {
bool objects_panel_use_native_tree_reordering(bool over_name_column);
}

TEST(ObjectsPanelTest, DoesNotUseNativeTreeReordering)
{
    EXPECT_FALSE(Inkscape::UI::Dialog::objects_panel_use_native_tree_reordering(false));
    EXPECT_FALSE(Inkscape::UI::Dialog::objects_panel_use_native_tree_reordering(true));
}
