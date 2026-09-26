// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>

#include "document.h"
#include "document-undo.h"
#include "inkscape.h"
#include "object/object-set.h"
#include "object/sp-root.h"
#include "ui/tools/duplicate-drag.h"
#include "xml/node.h"

using namespace Inkscape;
using namespace Inkscape::UI::Tools;
using namespace std::literals;

class DuplicateDragTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite() { Application::create(false); }

    void SetUp() override
    {
        doc = SPDocument::createNewDocFromMem(R"svg(
<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200" viewBox="0 0 200 200">
  <rect id="a" x="10" y="10" width="20" height="30"/>
  <rect id="b" x="50" y="10" width="20" height="30"/>
  <g id="group" transform="translate(20,60)">
    <rect id="child" width="10" height="10"/>
    <path id="path" d="M 20,0 L 30,0 L 20,10 Z"/>
  </g>
</svg>)svg"sv);
        ASSERT_TRUE(doc);
        doc->ensureUpToDate();
        DocumentUndo::done(doc.get(), RC_("Undo", "Test setup"), "");
        DocumentUndo::clearUndo(doc.get());
        DocumentUndo::clearRedo(doc.get());
        selection = std::make_unique<ObjectSet>(doc.get());
    }

    SPItem *item(char const *id) { return cast<SPItem>(doc->getObjectById(id)); }
    unsigned count() const
    {
        unsigned result = 0;
        for (auto &child : doc->getRoot()->children) {
            if (is<SPItem>(&child)) ++result;
        }
        return result;
    }
    Geom::Point position(SPItem *object) { return object->documentGeometricBounds()->min(); }
    void expect_position(SPItem *object, Geom::Point expected)
    {
        auto const actual = position(object);
        EXPECT_NEAR(actual.x(), expected.x(), 1e-6);
        EXPECT_NEAR(actual.y(), expected.y(), 1e-6);
    }

    // The selection is destroyed before its document.
    std::unique_ptr<SPDocument> doc;
    std::unique_ptr<ObjectSet> selection;
};

TEST_F(DuplicateDragTest, CopiesUnselectedObjectAndLeavesOriginalInPlace)
{
    auto original = item("a");
    auto const before = position(original);
    DuplicateDrag drag(*selection, *original);
    ASSERT_EQ(selection->size(), 1u);
    auto copy = selection->singleItem();
    ASSERT_NE(copy, original);
    EXPECT_STRNE(copy->getId(), original->getId());
    selection->applyAffine(Geom::Translate(40, 20));
    drag.commit(RC_("Undo", "Duplicate and Move"));
    doc->ensureUpToDate();
    expect_position(original, before);
    expect_position(copy, before + Geom::Point(40, 20));
    EXPECT_EQ(count(), 4u);
}

TEST_F(DuplicateDragTest, CopiesWholeSelectionWithOneUndoAndRedo)
{
    selection->add(item("a"));
    selection->add(item("b"));
    auto const before_a = position(item("a"));
    auto const before_b = position(item("b"));
    std::vector<std::pair<std::string, Geom::Point>> copies;
    {
        DuplicateDrag drag(*selection, *item("a"));
        ASSERT_EQ(selection->size(), 2u);
        for (auto copy : selection->items()) copies.emplace_back(copy->getId(), position(copy));
        selection->applyAffine(Geom::Translate(40, 20));
        drag.commit(RC_("Undo", "Duplicate and Move"));
    }
    EXPECT_EQ(count(), 5u);
    ASSERT_TRUE(DocumentUndo::undo(doc.get()));
    EXPECT_EQ(count(), 3u);
    expect_position(item("a"), before_a);
    expect_position(item("b"), before_b);
    for (auto const &[id, pos] : copies) EXPECT_EQ(doc->getObjectById(id), nullptr);
    EXPECT_FALSE(DocumentUndo::undo(doc.get())); // No separate duplicate/move entry.
    ASSERT_TRUE(DocumentUndo::redo(doc.get()));
    EXPECT_EQ(count(), 5u);
    for (auto const &[id, pos] : copies) {
        auto copy = cast<SPItem>(doc->getObjectById(id));
        ASSERT_NE(copy, nullptr);
        expect_position(copy, pos + Geom::Point(40, 20));
    }
}

TEST_F(DuplicateDragTest, CancelRestoresPreviousSelectionAndPreservesEarlierEdit)
{
    auto original = item("a");
    original->getRepr()->setAttribute("fill", "red");
    DocumentUndo::done(doc.get(), RC_("Undo", "Change fill"), "");
    selection->add(original);
    selection->add(item("group"));
    auto const before = position(item("b"));
    DuplicateDrag drag(*selection, *item("b")); // Hit outside the previous selection.
    ASSERT_EQ(selection->size(), 1u);
    selection->applyAffine(Geom::Translate(50, 0));
    drag.cancel();
    EXPECT_EQ(count(), 3u);
    EXPECT_EQ(selection->size(), 2u);
    EXPECT_TRUE(selection->includes(original));
    EXPECT_TRUE(selection->includes(item("group")));
    expect_position(item("b"), before);
    EXPECT_STREQ(original->getRepr()->attribute("fill"), "red");
    ASSERT_TRUE(DocumentUndo::undo(doc.get()));
    EXPECT_EQ(original->getRepr()->attribute("fill"), nullptr);
    EXPECT_FALSE(DocumentUndo::undo(doc.get()));
}

TEST_F(DuplicateDragTest, CancelAtOriginRestoresEmptySelection)
{
    DuplicateDrag drag(*selection, *item("a"));
    EXPECT_EQ(count(), 4u);
    drag.cancel();
    EXPECT_EQ(count(), 3u);
    EXPECT_TRUE(selection->isEmpty());
    EXPECT_FALSE(DocumentUndo::undo(doc.get()));
}

TEST_F(DuplicateDragTest, ReturningToOriginStillCommitsAnUndoableCopy)
{
    selection->add(item("a"));
    auto const before = position(item("a"));
    DuplicateDrag drag(*selection, *item("a"));
    selection->applyAffine(Geom::Translate(50, 0));
    selection->applyAffine(Geom::Translate(-50, 0));
    drag.commit(RC_("Undo", "Duplicate and Move"));
    EXPECT_EQ(count(), 4u);
    expect_position(selection->singleItem(), before);
    ASSERT_TRUE(DocumentUndo::undo(doc.get()));
    EXPECT_EQ(count(), 3u);
    EXPECT_FALSE(DocumentUndo::undo(doc.get()));
}

TEST_F(DuplicateDragTest, AbandoningDragCancelsAndRestoresSelection)
{
    selection->add(item("a"));
    {
        DuplicateDrag drag(*selection, *item("a"));
        selection->applyAffine(Geom::Translate(50, 0));
    }
    EXPECT_EQ(count(), 3u);
    EXPECT_EQ(selection->singleItem(), item("a"));
    EXPECT_FALSE(DocumentUndo::undo(doc.get()));
}

TEST_F(DuplicateDragTest, HitInsideSelectedGroupCopiesGroupAndOtherSelectedObjects)
{
    selection->add(item("group"));
    selection->add(item("a"));
    DuplicateDrag drag(*selection, *item("child"));
    EXPECT_EQ(count(), 5u);
    EXPECT_EQ(selection->size(), 2u);
    SPItem *copied_group = nullptr;
    for (auto copy : selection->items()) {
        if (is<SPGroup>(copy)) copied_group = copy;
    }
    ASSERT_NE(copied_group, nullptr);
    unsigned child_count = 0;
    for (auto &child : copied_group->children) {
        ++child_count;
        EXPECT_STRNE(child.getId(), "child");
        EXPECT_STRNE(child.getId(), "path");
    }
    EXPECT_EQ(child_count, 2u);
    drag.cancel();
    EXPECT_EQ(count(), 3u);
    EXPECT_TRUE(selection->includes(item("group")));
    EXPECT_TRUE(selection->includes(item("a")));
}

TEST(DuplicateDragConstraint, ChoosesNearestAxisOrDiagonalInEveryQuadrant)
{
    struct Case { Geom::Point input; Geom::Point expected; };
    for (auto const &test : {
             Case{{0, 0}, {0, 0}}, Case{{10, 2}, {10, 0}}, Case{{2, 10}, {0, 10}},
             Case{{10, 8}, {9, 9}}, Case{{-10, 8}, {-9, 9}},
             Case{{10, -8}, {9, -9}}, Case{{-10, -8}, {-9, -9}},
             Case{{-10, 2}, {-10, 0}}, Case{{2, -10}, {0, -10}}}) {
        auto const actual = constrain_duplicate_drag(test.input);
        EXPECT_DOUBLE_EQ(actual.x(), test.expected.x());
        EXPECT_DOUBLE_EQ(actual.y(), test.expected.y());
    }
}