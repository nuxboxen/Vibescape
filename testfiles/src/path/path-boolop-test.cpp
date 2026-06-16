// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>

#include <2geom/pathvector.h>
#include <2geom/svg-path-parser.h>
#include <2geom/svg-path-writer.h>

#include "path/path-boolop.h"

// This functionality is duplicated from svg/svg-path.cpp, which ultimately
// pulls in preferences. We avoid this by not writing the path on exception.
Geom::PathVector read_pathv(const char *str)
{
    Geom::PathVector pathv;
    if (!str) {
        return pathv; // return empty pathvector when str == NULL
    }

    Geom::PathBuilder builder(pathv);
    Geom::SVGPathParser parser(builder);
    parser.setZSnapThreshold(Geom::EPSILON);

    parser.parse(str);

    return pathv;
}

class PathBoolopTest : public ::testing::Test
{
public:
    Geom::PathVector const rectangle_bigger = read_pathv("M 0,0 L 0,2 L 2,2 L 2,0 z");
    Geom::PathVector const rectangle_smaller = read_pathv("M 0.5,0.5 L 0.5,1.5 L 1.5,1.5 L 1.5,0.5 z");
    Geom::PathVector const rectangle_outside = read_pathv("M 0,1.5 L 0.5,1.5 L 0.5,2.5 L 0,2.5 z");
    Geom::PathVector const reference_union =
        read_pathv("M 0,0 L 0,1.5 L 0,2 L 0,2.5 L 0.5,2.5 L 0.5,2 L 2,2 L 2,0 L 0,0 z");
    Geom::PathVector const empty = read_pathv("");

    // shapes to test fill rules
    Geom::PathVector const star = read_pathv("M 0,10 20,0 15,25 5,0 25,15 z");
    Geom::PathVector const star_odd_even =
        read_pathv("M 5 0 L 7.5 6.25 L 11 4.5 z M 11 4.5 L 18.04296875 9.783203125 L 20 0 z M 18.04296875 "
                   "9.783203125 L 17.30859375 13.4609375 L 25 15 z M 17.30859375 13.4609375 L 9.783203125 "
                   "11.95703125 L 15 25 z M 9.783203125 11.95703125 L 7.5 6.25 L 0 10 z");
    Geom::PathVector const star_non_zero =
        read_pathv("M 5 0 L 7.5 6.25 L 0 10 L 9.783203125 11.95703125 L 15 25 L 17.30859375 13.4609375 L 25 15 "
                   "L 18.04296875 9.783203125 L 20 0 L 11 4.5 z");
    Geom::PathVector const star_bbox = read_pathv("M 0,0 L 0,25 L 25,25 L 25,0 z");

    static void comparePaths(Geom::PathVector const &result, Geom::PathVector const &reference)
    {
        Geom::SVGPathWriter wr;
        wr.feed(result);
        auto const resultD = wr.str();
        wr.clear();
        wr.feed(reference);
        auto const referenceD = wr.str();
        EXPECT_EQ(resultD, referenceD);
        EXPECT_EQ(result, reference);
    }
};

TEST_F(PathBoolopTest, UnionOutside) {
    // test that the union of two objects where one is outside the other results in a new larger shape
    auto pathv = sp_pathvector_boolop(rectangle_bigger, rectangle_outside, bool_op_union, fill_oddEven, fill_oddEven);
    comparePaths(pathv, reference_union);
}

TEST_F(PathBoolopTest, UnionOutsideSwap) {
    // test that the union of two objects where one is outside the other results in a new larger shape, even when the order is reversed
    auto pathv = sp_pathvector_boolop(rectangle_outside, rectangle_bigger, bool_op_union, fill_oddEven, fill_oddEven);
    comparePaths(pathv, reference_union);
}

TEST_F(PathBoolopTest, UnionInside) {
    // test that the union of two objects where one is completely inside the other is the larger shape
    auto pathv = sp_pathvector_boolop(rectangle_bigger, rectangle_smaller, bool_op_union, fill_oddEven, fill_oddEven);
    comparePaths(pathv, rectangle_bigger);
}

TEST_F(PathBoolopTest, UnionInsideSwap) {
    // test that the union of two objects where one is completely inside the other is the larger shape, even when the order is swapped
    auto pathv = sp_pathvector_boolop(rectangle_smaller, rectangle_bigger, bool_op_union, fill_oddEven, fill_oddEven);
    comparePaths(pathv, rectangle_bigger);
}

TEST_F(PathBoolopTest, IntersectionInside) {
    // test that the intersection of two objects where one is completely inside the other is the smaller shape
    auto pathv = sp_pathvector_boolop(rectangle_bigger, rectangle_smaller, bool_op_inters, fill_oddEven, fill_oddEven);
    comparePaths(pathv, rectangle_smaller);
}

TEST_F(PathBoolopTest, IntersectionOddEven) {
    // test that the intersection of a star in its bounding box with the odd/even rule is an empty star
    auto pathv = sp_pathvector_boolop(star, star_bbox, bool_op_inters, fill_oddEven, fill_oddEven);
    comparePaths(pathv, star_odd_even);
}

TEST_F(PathBoolopTest, IntersectionNonZero) {
    // test that the intersection of a star in its bounding box with the nonzero rule is a filled star
    auto pathv = sp_pathvector_boolop(star, star_bbox, bool_op_inters, fill_nonZero, fill_oddEven);
    comparePaths(pathv, star_non_zero);
}

TEST_F(PathBoolopTest, IntersectionBBoxNonZero) {
    // Make sure the bbox winding rule doesn't change anything
    auto pathv = sp_pathvector_boolop(star, star_bbox, bool_op_inters, fill_oddEven, fill_nonZero);
    comparePaths(pathv, star_odd_even);
}

TEST_F(PathBoolopTest, DifferenceInside) {
    // test that the difference of two objects where one is completely inside the other is an empty path
    auto pathv = sp_pathvector_boolop(rectangle_bigger, rectangle_smaller, bool_op_diff, fill_oddEven, fill_oddEven);
    comparePaths(pathv, empty);
}

TEST_F(PathBoolopTest, DifferenceOutside) {
    // test that the difference of two objects where one is completely outside the other is multiple shapes
    auto pathv = sp_pathvector_boolop(rectangle_smaller, rectangle_bigger, bool_op_diff, fill_oddEven, fill_oddEven);

    auto both_paths = rectangle_bigger;
    for (auto const &path : rectangle_smaller) {
        both_paths.push_back(path.reversed());
    }

    comparePaths(pathv, both_paths);
}

TEST_F(PathBoolopTest, CutOverlapping) {
    // Test that cutting one rectangle with an overlapping one results in a path
    // containing all but the portion of the first rectangle covered by the
    // second, and only the portion of the second covered by the first.
    // This test expects current behavior, which includes redundant paths and
    // expresses both result paths as a single string of path commands. As such,
    // it may break with improvements to the cut op.
    auto expected = read_pathv("M 0 0 z M 0 0 z M 0 1.5 V 2 H 0.5 V 1.5 z M 0.5 2 z M 0.5 2 H 0 V 2.5 H 0.5 z M 2 2 z");

    auto actual = sp_pathvector_boolop(rectangle_bigger, rectangle_outside, bool_op_cut, fill_oddEven, fill_oddEven);
    comparePaths(actual, expected);
}

TEST_F(PathBoolopTest, CutContained) {
    // Test that cutting one rectangle with one fully contained in it results in
    // a path containing all but the portion of the first rectangle covered by
    // the second, as well as the entirety of the second.
    // This test expects current behavior, which includes redundant paths and
    // expresses both result paths as a single string of path commands. As such,
    // it may break with improvements to the cut op.
    auto expected = read_pathv("M 0 0 z M 0 0 z M 0 2 z M 2 2 z M 0.5 0.5 V 1.5 H 1.5 V 0.5 z");

    auto actual = sp_pathvector_boolop(rectangle_bigger, rectangle_smaller, bool_op_cut, fill_oddEven, fill_oddEven);
    comparePaths(actual, expected);
}


TEST_F(PathBoolopTest, CutSurrounded) {
    // Test that cutting one rectangle with one fully surrounding it results in
    // a path representing only the surrounded rectangle.
    // This test expects current behavior, which includes redundant paths. As
    // such, it may break with improvements to the cut op.
    auto expected = read_pathv("M 0 0 V 2 H 2 V 0 z M 0.5 0.5 H 1.5 V 1.5 H 0.5 z M 0.5 0.5 V 1.5 H 1.5 V 0.5 z");

    auto actual = sp_pathvector_boolop(rectangle_smaller, rectangle_bigger, bool_op_cut, fill_oddEven, fill_oddEven);
    comparePaths(actual, expected);
}

TEST_F(PathBoolopTest, CutDisjoint) {
    // Test that cutting one rectangle with a non-overlapping one results in a
    // path representing only the non-overlapping rectangle.
    auto const rectangle_disjoint = read_pathv("m 3,0.5 h 0.5 v 1 H 3 Z");

    // This test expects current behavior, which includes redundant paths. As
    // such, it may break with improvements to the cut op.
    auto expected = read_pathv("M 0 0 z M 0 0 z M 0 1.5 V 2 H 0.5 V 1.5 z M 0.5 2 z M 0.5 2 H 0 V 2.5 H 0.5 z M 2 2 z");

    auto actual = sp_pathvector_boolop(rectangle_bigger, rectangle_outside, bool_op_cut, fill_oddEven, fill_oddEven);
    comparePaths(actual, expected);
}
