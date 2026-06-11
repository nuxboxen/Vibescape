// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Test utilities from src/util
 */
/*
 * Authors:
 *   Thomas Holder
 *   Martin Owens
 *
 * Copyright (C) 2020-2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include "util/longest-common-suffix.h"
#include "util/ordered-list.h"
#include "util/parse-int-range.h"
#include "util/delete-with.h"

TEST(UtilTest, NearestCommonAncestor)
{
#define nearest_common_ancestor(a, b, c) \
    Inkscape::Algorithms::nearest_common_ancestor(a, b, c)

    // simple node with a parent
    struct Node
    {
        Node const *parent;
        Node(Node const *p) : parent(p){};
        Node(Node const &other) = delete;
    };

    // iterator which traverses towards the root node
    struct iter
    {
        Node const *node;
        iter(Node const &n) : node(&n) {}
        bool operator==(iter const &rhs) const { return node == rhs.node; }
        bool operator!=(iter const &rhs) const { return node != rhs.node; }
        iter &operator++()
        {
            node = node->parent;
            return *this;
        }

        // TODO remove, the implementation should not require this
        Node const &operator*() const { return *node; }
    };

    // construct a tree
    auto const node0 = Node(nullptr);
    auto const node1 = Node(&node0);
    auto const node2 = Node(&node1);
    auto const node3a = Node(&node2);
    auto const node4a = Node(&node3a);
    auto const node5a = Node(&node4a);
    auto const node3b = Node(&node2);
    auto const node4b = Node(&node3b);
    auto const node5b = Node(&node4b);

    // start at each node from 5a to 0 (first argument)
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node5b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node4a), iter(node5b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node3a), iter(node5b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node2), iter(node5b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node1), iter(node5b), iter(node0)), iter(node1));
    ASSERT_EQ(nearest_common_ancestor(iter(node0), iter(node5b), iter(node0)), iter(node0));

    // start at each node from 5b to 0 (second argument)
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node5b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node4b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node3b), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node2), iter(node0)), iter(node2));
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node1), iter(node0)), iter(node1));
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node0), iter(node0)), iter(node0));

    // identity (special case in implementation)
    ASSERT_EQ(nearest_common_ancestor(iter(node5a), iter(node5a), iter(node0)), iter(node5a));

    // identical parents (special case in implementation)
    ASSERT_EQ(nearest_common_ancestor(iter(node3a), iter(node3b), iter(node0)), iter(node2));
}

TEST(UtilTest, ParseIntRangeTest)
{
    // Single number
    ASSERT_EQ(Inkscape::parseIntRange("1"), std::set<unsigned int>({1}));
    ASSERT_EQ(Inkscape::parseIntRange("3"), std::set<unsigned int>({3}));

    // Out of range numbers
    ASSERT_EQ(Inkscape::parseIntRange("11", 1, 10), std::set<unsigned int>({}));
    ASSERT_EQ(Inkscape::parseIntRange("3", 5, 10), std::set<unsigned int>({}));
    ASSERT_EQ(Inkscape::parseIntRange("3", 5), std::set<unsigned int>({}));

    // Comma seperated in various orders
    ASSERT_EQ(Inkscape::parseIntRange("1,3,5"), std::set<unsigned int>({1, 3, 5}));
    ASSERT_EQ(Inkscape::parseIntRange("3,1,4"), std::set<unsigned int>({1, 3, 4}));
    ASSERT_EQ(Inkscape::parseIntRange("3,2,9,"), std::set<unsigned int>({2, 3, 9}));

    // Including whitespace
    ASSERT_EQ(Inkscape::parseIntRange(" 5 , 2 -  3,   9  , "), std::set<unsigned int>({2, 3, 5, 9}));

    // Range of numbers using a dash
    ASSERT_EQ(Inkscape::parseIntRange("1-4"), std::set<unsigned int>({1, 2, 3, 4}));
    ASSERT_EQ(Inkscape::parseIntRange("2-4"), std::set<unsigned int>({2, 3, 4}));
    ASSERT_EQ(Inkscape::parseIntRange("-"), std::set<unsigned int>({1})); // 1 is the implied start
    ASSERT_EQ(Inkscape::parseIntRange("-3"), std::set<unsigned int>({1, 2, 3}));
    ASSERT_EQ(Inkscape::parseIntRange("8-"), std::set<unsigned int>({8}));
    ASSERT_EQ(Inkscape::parseIntRange("-", 4, 6), std::set<unsigned int>({4, 5, 6}));
    ASSERT_EQ(Inkscape::parseIntRange("-7", 5), std::set<unsigned int>({5, 6, 7}));
    ASSERT_EQ(Inkscape::parseIntRange("8-", 1, 10), std::set<unsigned int>({8, 9, 10}));
    ASSERT_EQ(Inkscape::parseIntRange("all", 4, 6), std::set<unsigned int>({4, 5, 6}));

    // Mixeed formats
    ASSERT_EQ(Inkscape::parseIntRange("2-4,7-9", 1, 10), std::set<unsigned int>({2,3,4,7,8,9}));

    // Huge range of mostly invalid numbers
    ASSERT_EQ(Inkscape::parseIntRange("1-4294967295", 2000000000, 2000000001), std::set<unsigned int>({2000000000, 2000000001}));
}

namespace {

bool flag;

void set_flag(bool *)
{
    flag = true;
}

} // namespace

TEST(UtilTest, DeleteWithTest)
{
    using Inkscape::Util::delete_with;

    // Deleting non-null pointer runs function.
    flag = false;
    {
        auto x = delete_with<set_flag>(&flag);
        ASSERT_EQ(flag, false);
    }
    ASSERT_EQ(flag, true);

    // Deleting null pointer does nothing.
    flag = false;
    {
        auto x = delete_with<set_flag>(static_cast<bool *>(nullptr));
        ASSERT_EQ(flag, false);
    }
    ASSERT_EQ(flag, false);
}

namespace {

struct MyObject;
struct LengthAggregate
{
    using value_type = uint16_t;
    static value_type contribution(MyObject const &obj);
};

struct CoolClubAggregate
{
    using value_type = size_t;
    static value_type contribution(MyObject const &obj);
};

struct MyObject
{
    Inkscape::Util::OrderedListNode<LengthAggregate, CoolClubAggregate> _hook;
    uint16_t length;
    std::string club;
};

LengthAggregate::value_type LengthAggregate::contribution(MyObject const &obj)
{
    return obj.length;
};

CoolClubAggregate::value_type CoolClubAggregate::contribution(MyObject const &obj)
{
    return obj.club.starts_with("cool");
}

} // namespace

TEST(UtilTest, OrderedListTest)
{
    Inkscape::Util::OrderedList<MyObject, &MyObject::_hook> list;
    ASSERT_EQ(list.empty(), true);
    ASSERT_EQ(list.size(), 0);

    MyObject a = {.length = 5, .club = "cows"};
    MyObject b = {.length = 1, .club = "coolBirds"};
    MyObject c = {.length = 0, .club = ""};
    MyObject d = {.length = 3, .club = "cool horses"};

    list.push_back(a);
    list.push_front(b);
    /**
     * Item view        : [ba]
     * Length view      : [baaaaa]
     * CoolClub view    : [b]
     */
    ASSERT_EQ(list.empty(), false);
    ASSERT_EQ(list.size(), 2);
    ASSERT_EQ(list.atIndex(0), &b);
    ASSERT_EQ(list.atIndex(1), &a);
    ASSERT_EQ(list.getIndex(a), 1);
    ASSERT_EQ(list.getIndex(b), 0);
    ASSERT_EQ(list.getIndex<LengthAggregate>(a), 1);
    ASSERT_EQ(list.atIndex<LengthAggregate>(0), &b);
    ASSERT_EQ(list.atIndex<LengthAggregate>(1), &a);
    ASSERT_EQ(list.atIndex<LengthAggregate>(5), &a);
    ASSERT_EQ(list.atIndex<LengthAggregate>(6), nullptr);
    ASSERT_EQ(list.atIndex<CoolClubAggregate>(0), &b);

    list.insert_after(a, d);
    list.insert_at(c, 2);
    /**
     * Item view        : [bacd]
     * Length view      : [baaaaaddd]
     * CoolClub view    : [bd]
     */
    ASSERT_EQ(list.size(), 4);
    ASSERT_EQ(list.iterator_to(a), ++list.begin());
    ASSERT_EQ(list.getIndex(c), 2);
    ASSERT_EQ(list.atIndex<LengthAggregate>(5), &a);
    ASSERT_EQ(list.atIndex<LengthAggregate>(6), &d);
    ASSERT_EQ(list.getIndex<CoolClubAggregate>(d), 1);
    ASSERT_EQ(list.iterator_to(c), --(--list.end()));

    list.erase(a);
    list.insert_after(c, a);
    /**
     * Item view        : [bcad]
     * Length view      : [baaaaaddd]
     * CoolClub view    : [bd]
     */
    ASSERT_EQ(list.size(), 4);
    ASSERT_EQ(&list.front(), &b);
    ASSERT_EQ(list.getIndex(c), 1);
    ASSERT_EQ(list.getIndex(a), 2);

    std::string clubs = "";
    for (auto &i : list) {
        clubs.append(i.club);
    }
    ASSERT_EQ(clubs, "coolBirdscowscool horses");

    list.erase(b);
    /**
     * Item view        : [cad]
     * Length view      : [aaaaaddd]
     * CoolClub view    : [d]
     */
    ASSERT_EQ(&list.front(), &c);

    size_t remaining_lengths = 0;
    list.clear_and_dispose([&](MyObject *obj) { remaining_lengths += obj->length; });
    ASSERT_EQ(remaining_lengths, 8);
    ASSERT_EQ(list.empty(), true);
}

// vim: filetype=cpp:expandtab:shiftwidth=4:softtabstop=4:fileencoding=utf-8:textwidth=99 :
