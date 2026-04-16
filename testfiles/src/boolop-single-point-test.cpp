// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Test for boolean operations with single point path.
 *
 * https://gitlab.com/inkscape/inkscape/-/work_items/5036
 */
/*
 * Authors:
 *   Viktória Simon
 *
 * Copyright (C) 2026 Authors
 */

#include <gtest/gtest.h>

#include "doc-per-case-test.h"
#include "object/object-set.h"

using namespace Inkscape;
using namespace std::literals;

class BoolopSinglePointTest : public DocPerCaseTest
{
public:
    BoolopSinglePointTest()
    {
        constexpr auto docString = R"A(
<svg viewBox="0 0 210 110" xmlns="http://www.w3.org/2000/svg">
  <g id="union">
    <path
      d="M 10,10"
      id="Single point" />
    <path
      d="M 20,20 H 40 V 10 H 20 Z"
      id="path1" />
    </g>
</svg>
        )A"sv;
        doc = SPDocument::createNewDocFromMem(docString);
    }

    std::unique_ptr<SPDocument> doc;
};

TEST_F(BoolopSinglePointTest, Union)
{
    auto const d_combined = "M 20 10 L 20 20 L 40 20 L 40 10 L 20 10 z ";

    auto const paths = doc->getObjectsBySelector("#union path");
    ASSERT_EQ(paths.size(), 2);

    auto object_set = ObjectSet(doc.get());
    object_set.setList(paths);
    object_set.pathUnion(true);
    
    auto combined = object_set.single();
    ASSERT_TRUE(combined);

    auto const paths_after = doc->getObjectsBySelector("#union path");
    ASSERT_EQ(paths_after.size(), 1);

    ASSERT_STREQ(combined->getAttribute("d"), d_combined);
}
