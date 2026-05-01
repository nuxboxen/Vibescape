// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Color quantization for bitmap tracing.
 *
 * This file implements several color quantization algorithms used to
 * reduce a bitmap image to a small number of representative colors
 * before vectorization:
 *
 * 1. Octree quantization (rgbMapQuantize)
 *    The original algorithm. Builds an octree in RGB space and prunes
 *    leaves by pixel-count-weighted impact. Fast and deterministic,
 *    but operates in raw RGB (not perceptually uniform) and tends to
 *    allocate palette entries proportionally to pixel count, which can
 *    miss small but visually distinctive color regions.
 *    Used for QUANT_MONO and BRIGHTNESS_MULTI modes where the output
 *    is converted to grayscale.
 *
 * 2. Saliency-weighted k-means in Oklab (rgbMapQuantizePerceptual)
 *    A perceptually-aware algorithm designed for the QUANT_COLOR mode.
 *    Operates in the Oklab perceptual color space, where Euclidean
 *    distance matches perceived color difference. Before clustering,
 *    each pixel is weighted by its local chroma saliency (how much its
 *    hue/saturation differs from its immediate neighborhood), so that
 *    color edges and small vivid regions attract cluster centroids
 *    more strongly than uniform background areas. See the detailed
 *    comment block above the implementation for the full rationale.
 *
 * 3. Palette mapping (rgbMapWithPalette)
 *    Maps pixels to a user-supplied palette using Oklab distance.
 *
 * 4. Optimal palette estimation (estimateOptimalPalette)
 *    Runs saliency-weighted k-means for k=2..maxK and selects the
 *    best k using the elbow method on inertia.
 *
 * 5. Incremental palette extension (findNextPaletteColor)
 *    Given an existing palette, finds the best next color via
 *    constrained k-means (existing colors frozen, only the new
 *    centroid moves).
 *
 * Authors:
 *   Stéphane Gimenez <dev@gim.name>
 *   Séverin Lemaignan <severin@guakamole.org>
 *
 * Copyright (C) 2006-2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>
#include <glib.h>

#include "pool.h"
#include "imagemap.h"
#include "quantize.h"

namespace Inkscape {
namespace Trace {

namespace {

// =====================================================================
// 1. Octree quantization in RGB space
// =====================================================================

/*
-- Octree algorithm principle:

Builds a tree where each level corresponds to one bit of the R, G, B
channels. Leaf nodes accumulate pixel counts and color sums. The tree
is pruned to the target number of colors by removing leaves with the
smallest "impact" (pixel count × color range²). Final palette colors
are the weighted averages of the remaining leaves.

See the detailed comments below for merge/prune mechanics and
optimizations over the standard octree method.
*/

/**
 * an octree node datastructure
 */
struct Ocnode
{
    Ocnode *parent;           // parent node
    Ocnode **ref;             // node's reference
    Ocnode *child[8];         // children
    int nchild;               // number of children
    int width;                // width level of this node
    RGB rgb;                  // rgb's prefix of that node
    unsigned long weight;     // number of pixels this node accounts for
    unsigned long rs, gs, bs; // sum of pixels colors this node accounts for
    int nleaf;                // number of leaves under this node
    unsigned long mi;         // minimum impact
};

/*
-- Detailed octree algorithm description:

- inspired by the octree method, we associate a tree to a given color map

- nodes in those trees have this shape:

                                parent
                                   |
        color_prefix(stored in rgb):width
     colors_sum(stored in rs,gs,bs)/weight
         /               |               \
     child1           child2           child3

- (grayscale) trees associated to pixels with colors 87 = 0b1010111 and
  69 = 0b1000101 are:

           .                 .    <-- roots of the trees
           |                 |
    1010111:0  and    1000101:0   <-- color prefixes, written in binary form
         87/1              69/1   <-- color sums, written in decimal form

- the result of merging the two trees is:

                   .
                   |
                 10:5       <----- longest common prefix and binary width
                156/2       <---.  of the covered color range.
            /            \      |
    1000101:0      1010111:0    '- sum of colors and quantity of pixels
         69/1           87/1       this node accounts for

  one should consider three cases when two trees are to be merged:
  - one tree range is included in the range of the other one, and the first
    tree has to be inserted as a child (or merged with the corresponding
    child) of the other.
  - their ranges are the same, and their children have to be merged under
    a single root.
  - ranges have no intersection, and a fork node has to be created (like in
    the given example).

- a tree for an image is built dividing the image in 2 parts and merging
  the trees obtained recursively for the two parts. a tree for a one pixel
  part is a leaf like one of those which were given above.

- last, this tree is reduced a specified number of leaves, deleting first
  leaves with minimal impact i.e. [ weight * 2^(2*parentwidth) ] value :
  a fair approximation of the impact a leaf removal would have on the final
  result : it's the corresponding covered area times the square of the
  introduced color distance.

  deletion of a node A below a node with only two children is done as
  follows :

  - when the sibling is a leaf, the sibling is deleted as well, both nodes
    are then represented by their parent.

     |               |
     .       ==>     .
    / \
   A   .

  - otherwise the deletion of A deletes also its parent, which plays no
    role anymore:

     |                |
     .       ==>       \
    / \                 |
   A   .                .
      / \              / \

  in that way, every leaf removal operation really decreases the remaining
  total number of leaves by one.

- very last, color indexes are attributed to leaves; associated colors are
  averages, computed from weight and color components sums.

-- improvements to the usual octree method:

- since this algorithm shall often be used to perform quantization using a
  very low (2-16) set of colors and not with a usual 256 value, we choose
  more carefully which nodes are to be deleted.

- depth of leaves is not fixed to an arbitrary number (which should be 8
  when color components are in 0-255), so there is no need to go down to a
  depth of 8 for each pixel (at full precision), unless it is really
  required.

- tree merging also fastens the overall tree building, and intermediate
  processing could be done.

- a huge optimization against the stupid removal algorithm (i.e. find a best
  match over the whole tree, remove it and do it again) was implemented:
  nodes are marked with the minimal impact of the removal of a leaf below
  it. we proceed to the removal recursively. we stop when current removal
  level is above the current node minimal, otherwise reached leaves are
  removed, and every change over minimal impacts is propagated back to the
  whole tree when the recursion ends.

-- specific optimizations

- pool allocation is used to allocate nodes (increased performance on large
  images).

*/

RGB operator>>(RGB rgb, int s)
{
    RGB res;
    res.r = rgb.r >> s;
    res.g = rgb.g >> s;
    res.b = rgb.b >> s;
    return res;
}

bool operator==(RGB rgb1, RGB rgb2)
{
    return rgb1.r == rgb2.r && rgb1.g == rgb2.g && rgb1.b == rgb2.b;
}

int childIndex(RGB rgb)
{
    return ((rgb.r & 1) << 2) | ((rgb.g & 1) << 1) | (rgb.b & 1);
}

/**
 * allocate a new node
 */
Ocnode *ocnodeNew(Pool<Ocnode> &pool)
{
    Ocnode *node = pool.draw();
    node->ref = nullptr;
    node->parent = nullptr;
    node->nchild = 0;
    for (auto &i : node->child) {
        i = nullptr;
    }
    node->mi = 0;
    return node;
}

void ocnodeFree(Pool<Ocnode> &pool, Ocnode *node)
{
    pool.drop(node);
}

/**
 * free a full octree
 */
void octreeDelete(Pool<Ocnode> &pool, Ocnode *node)
{
    if (!node) return;
    for (auto &i : node->child) {
        octreeDelete(pool, i);
    }
    ocnodeFree(pool, node);
}

/**
 *  pretty-print an octree, debugging purposes
 */
#if 0
void ocnodePrint(Ocnode *node, int indent)
{
    if (!node) return;
    printf("width:%d weight:%lu rgb:%6x nleaf:%d mi:%lu\n",
           node->width,
           node->weight,
           (unsigned int)(
           ((node->rs / node->weight) << 16) +
           ((node->gs / node->weight) << 8) +
           (node->bs / node->weight)),
           node->nleaf,
           node->mi
           );
    for (int i = 0; i < 8; i++) if (node->child[i])
        {
        for (int k = 0; k < indent; k++) printf(" ");//indentation
        printf("[%d:%p] ", i, node->child[i]);
        ocnodePrint(node->child[i], indent+2);
        }
}

void octreePrint(Ocnode *node)
{
    printf("<<octree>>\n");
    if (node) printf("[r:%p] ", node); ocnodePrint(node, 2);
}
#endif

/**
 * builds a single <rgb> color leaf at location <ref>
 */
void ocnodeLeaf(Pool<Ocnode> &pool, Ocnode **ref, RGB rgb)
{
    assert(ref);
    Ocnode *node = ocnodeNew(pool);
    node->width = 0;
    node->rgb = rgb;
    node->rs = rgb.r; node->gs = rgb.g; node->bs = rgb.b;
    node->weight = 1;
    node->nleaf = 1;
    node->mi = 0;
    node->ref = ref;
    *ref = node;
}

/**
 *  merge nodes <node1> and <node2> at location <ref> with parent <parent>
 */
int octreeMerge(Pool<Ocnode> &pool, Ocnode *parent, Ocnode **ref, Ocnode *node1, Ocnode *node2)
{
    assert(ref);
    if (!node1 && !node2) return 0;
    assert(node1 != node2);
    if (parent && !*ref) parent->nchild++;
    if (!node1) {
        *ref = node2; node2->ref = ref; node2->parent = parent;
        return node2->nleaf;
    }
    if (!node2) {
        *ref = node1; node1->ref = ref; node1->parent = parent;
        return node1->nleaf;
    }
    int dwitdth = node1->width - node2->width;
    if (dwitdth > 0 && node1->rgb == node2->rgb >> dwitdth) {
        // place node2 below node1
        *ref = node1; node1->ref = ref; node1->parent = parent;
        int i = childIndex(node2->rgb >> (dwitdth - 1));
        node1->rs += node2->rs; node1->gs += node2->gs; node1->bs += node2->bs;
        node1->weight += node2->weight;
        node1->mi = 0;
        if (node1->child[i]) node1->nleaf -= node1->child[i]->nleaf;
        node1->nleaf += octreeMerge(pool, node1, &node1->child[i], node1->child[i], node2);
        return node1->nleaf;
    } else if (dwitdth < 0 && node2->rgb == node1->rgb >> (-dwitdth)) {
        // place node1 below node2
        *ref = node2; node2->ref = ref; node2->parent = parent;
        int i = childIndex(node1->rgb >> (-dwitdth - 1));
        node2->rs += node1->rs; node2->gs += node1->gs; node2->bs += node1->bs;
        node2->weight += node1->weight;
        node2->mi = 0;
        if (node2->child[i]) node2->nleaf -= node2->child[i]->nleaf;
        node2->nleaf += octreeMerge(pool, node2, &node2->child[i], node2->child[i], node1);
        return node2->nleaf;
    } else {
        // nodes have either no intersection or the same root
        Ocnode *newnode;
        newnode = ocnodeNew(pool);
        newnode->rs = node1->rs + node2->rs;
        newnode->gs = node1->gs + node2->gs;
        newnode->bs = node1->bs + node2->bs;
        newnode->weight = node1->weight + node2->weight;
        *ref = newnode; newnode->ref = ref; newnode->parent = parent;
        if (dwitdth == 0 && node1->rgb == node2->rgb) {
            // merge the nodes in <newnode>
            newnode->width = node1->width; // == node2->width
            newnode->rgb = node1->rgb;     // == node2->rgb
            newnode->nchild = 0;
            newnode->nleaf = 0;
            if (node1->nchild == 0 && node2->nchild == 0) {
                newnode->nleaf = 1;
            } else {
                for (int i = 0; i < 8; i++) {
                    if (node1->child[i] || node2->child[i]) {
                        newnode->nleaf += octreeMerge(pool, newnode, &newnode->child[i], node1->child[i], node2->child[i]);
                    }
                }
            }
            ocnodeFree(pool, node1); ocnodeFree(pool, node2);
            return newnode->nleaf;
        } else {
            // use <newnode> as a fork node with children <node1> and <node2>
            int newwidth = std::max(node1->width, node2->width);
            RGB rgb1 = node1->rgb >> (newwidth - node1->width);
            RGB rgb2 = node2->rgb >> (newwidth - node2->width);
            // according to the previous tests <rgb1> != <rgb2> before the loop
            while (!(rgb1 == rgb2)) {
                rgb1 = rgb1 >> 1;
                rgb2 = rgb2 >> 1;
                newwidth++;
            }
            newnode->width = newwidth;
            newnode->rgb = rgb1; // == rgb2
            newnode->nchild = 2;
            newnode->nleaf = node1->nleaf + node2->nleaf;
            int i1 = childIndex(node1->rgb >> (newwidth - node1->width - 1));
            int i2 = childIndex(node2->rgb >> (newwidth - node2->width - 1));
            node1->parent = newnode;
            node1->ref = &newnode->child[i1];
            newnode->child[i1] = node1;
            node2->parent = newnode;
            node2->ref = &newnode->child[i2];
            newnode->child[i2] = node2;
            return newnode->nleaf;
        }
    }
}

/**
 * upatade mi value for leaves
 */
void ocnodeMi(Ocnode *node)
{
    node->mi = node->parent ? node->weight << (2 * node->parent->width) : 0;
}

/**
 * remove leaves whose prune impact value is lower than <lvl>. at most
 * <count> leaves are removed, and <count> is decreased on each removal.
 * all parameters including minimal impact values are regenerated.
 */
void ocnodeStrip(Pool<Ocnode> &pool, Ocnode **ref, int &count, unsigned long lvl)
{
    Ocnode *node = *ref;
    if (!node) return;
    assert(ref == node->ref);
    if (node->nchild == 0) { // leaf node
        if (!node->mi) ocnodeMi(node); // mi generation may be required
        if (node->mi > lvl) return; // leaf is above strip level
        ocnodeFree(pool, node);
        *ref = nullptr;
        count--;
    } else {
        if (node->mi && node->mi > lvl) return; // node is above strip level
        node->nchild = 0;
        node->nleaf = 0;
        node->mi = 0;
        Ocnode **lonelychild = nullptr;
        for (auto & i : node->child) {
            if (i) {
                ocnodeStrip(pool, &i, count, lvl);
                if (i) {
                    lonelychild = &i;
                    node->nchild++;
                    node->nleaf += i->nleaf;
                    if (!node->mi || node->mi > i->mi) {
                        node->mi = i->mi;
                    }
                }
            }
        }
        // tree adjustments
        if (node->nchild == 0) {
            count++;
            node->nleaf = 1;
            ocnodeMi(node);
        } else if (node->nchild == 1) {
            if ((*lonelychild)->nchild == 0) {
                // remove the <lonelychild> leaf under a 1 child node
                node->nchild = 0;
                node->nleaf = 1;
                ocnodeMi(node);
                ocnodeFree(pool, *lonelychild);
                *lonelychild = nullptr;
            } else {
                // make a bridge to <lonelychild> over a 1 child node
                (*lonelychild)->parent = node->parent;
                (*lonelychild)->ref = ref;
                ocnodeFree(pool, node);
                *ref = *lonelychild;
            }
        }
    }
}

/**
 * reduce the leaves of an octree to a given number
 */
void octreePrune(Pool<Ocnode> &pool, Ocnode **ref, int ncolor)
{
    assert(ref);
    assert(ncolor > 0);
    int n = (*ref)->nleaf - ncolor;
    if (!*ref || n <= 0) return;
    while (n > 0) {
        ocnodeStrip(pool, ref, n, (*ref)->mi);
    }
}

/**
 * build an octree associated to the area of a color map <rgbmap>,
 * included in the specified (x1,y1)--(x2,y2) rectangle.
 */
void octreeBuildArea(Pool<Ocnode> &pool, RgbMap const &rgbmap, Ocnode **ref, int x1, int y1, int x2, int y2, int ncolor)
{
    int dx = x2 - x1, dy = y2 - y1;
    int xm = x1 + dx / 2, ym = y1 + dy / 2;
    Ocnode *ref1 = nullptr;
    Ocnode *ref2 = nullptr;
    if (dx == 1 && dy == 1) {
        ocnodeLeaf(pool, ref, rgbmap.getPixel(x1, y1));
    } else if (dx > dy) {
        octreeBuildArea(pool, rgbmap, &ref1, x1, y1, xm, y2, ncolor);
        octreeBuildArea(pool, rgbmap, &ref2, xm, y1, x2, y2, ncolor);
        octreeMerge(pool, nullptr, ref, ref1, ref2);
    } else {
        octreeBuildArea(pool, rgbmap, &ref1, x1, y1, x2, ym, ncolor);
        octreeBuildArea(pool, rgbmap, &ref2, x1, ym, x2, y2, ncolor);
        octreeMerge(pool, nullptr, ref, ref1, ref2);
	}

    // octreePrune(ref, 2 * ncolor);
    // affects result quality for almost same performance :/
}

/**
 * build an octree associated to the <rgbmap> color map,
 * pruned to <ncolor> colors.
 */
Ocnode *octreeBuild(Pool<Ocnode> &pool, RgbMap const &rgbmap, int ncolor)
{
    // create the octree
    Ocnode *node = nullptr;
    octreeBuildArea(pool,
                    rgbmap, &node,
                    0, 0, rgbmap.width, rgbmap.height, ncolor);

    // prune the octree
    octreePrune(pool, &node, ncolor);

    return node;
}

/**
 * compute the color palette associated to an octree.
 */
void octreeIndex(Ocnode *node, RGB *rgbpal, int &index)
{
    if (!node) return;
    if (node->nchild == 0) {
        rgbpal[index].r = node->rs / node->weight;
        rgbpal[index].g = node->gs / node->weight;
        rgbpal[index].b = node->bs / node->weight;
        index++;
    } else {
        for (auto &i : node->child) {
            if (i) {
                octreeIndex(i, rgbpal, index);
            }
        }
    }
}

/**
 * compute the squared distance between two colors
 */
int distRGB(RGB rgb1, RGB rgb2)
{
    return (rgb1.r - rgb2.r) * (rgb1.r - rgb2.r)
         + (rgb1.g - rgb2.g) * (rgb1.g - rgb2.g)
         + (rgb1.b - rgb2.b) * (rgb1.b - rgb2.b);
}

/**
 * find the index of closest color in a palette
 */
int findRGB(RGB const *rgbs, int ncolor, RGB rgb)
{
    int index = -1, dist = 0;
    for (int k = 0; k < ncolor; k++) {
        int d = distRGB(rgbs[k], rgb);
        if (index == -1 || d < dist) { dist = d; index = k; }
    }
    return index;
}

// =====================================================================
// 2. Oklab perceptual color space utilities
// =====================================================================
//
// Oklab is a perceptual color space designed by Björn Ottosson where
// Euclidean distance closely matches human-perceived color difference.
// L encodes lightness (0–1), a and b encode green-red and blue-yellow
// opponent channels (roughly ±0.35).
//
// The conversion is: sRGB → linear sRGB → LMS (via 3×3 matrix) →
// cube root → Oklab (via another 3×3 matrix). Inverse is the reverse.
//
// We implement these directly here rather than reusing the existing
// ok_color.h functions in src/colors/spaces/, because those are
// internal to the color picker widget, use different types, and
// require callers to handle sRGB linearization separately.

struct OklabColor { float L; float a; float b; };

/**
 * Convert sRGB (0-255) to Oklab.
 * sRGB -> linear sRGB -> Oklab, following Björn Ottosson's formulation.
 */
OklabColor rgbToOklab(RGB c)
{
    // sRGB to linear sRGB
    auto linearize = [] (unsigned char v) -> float {
        float x = v / 255.0f;
        return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
    };
    float r = linearize(c.r);
    float g = linearize(c.g);
    float b = linearize(c.b);

    // Linear sRGB to LMS
    float l = 0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b;
    float m = 0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b;
    float s = 0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b;

    // LMS to Oklab
    float l_ = std::cbrt(l);
    float m_ = std::cbrt(m);
    float s_ = std::cbrt(s);

    return {
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
    };
}

/**
 * Convert Oklab back to sRGB (0-255), clamping to valid range.
 */
RGB oklabToRgb(OklabColor c)
{
    // Oklab to LMS
    float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
    float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
    float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    // LMS to linear sRGB
    float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;

    // Linear sRGB to sRGB
    auto delinearize = [] (float x) -> unsigned char {
        x = std::max(0.0f, std::min(1.0f, x));
        float v = x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
        return static_cast<unsigned char>(std::round(std::max(0.0f, std::min(255.0f, v * 255.0f))));
    };

    return { delinearize(r), delinearize(g), delinearize(b) };
}

/**
 * Squared distance in Oklab space (perceptual).
 */
float distOklab(OklabColor a, OklabColor b)
{
    float dL = a.L - b.L;
    float da = a.a - b.a;
    float db = a.b - b.b;
    return dL * dL + da * da + db * db;
}

/**
 * Find the index of the perceptually closest color in an Oklab palette.
 */
int findOklab(OklabColor const *palette, int ncolor, OklabColor c)
{
    int best = 0;
    float bestDist = distOklab(palette[0], c);
    for (int k = 1; k < ncolor; k++) {
        float d = distOklab(palette[k], c);
        if (d < bestDist) { bestDist = d; best = k; }
    }
    return best;
}

// =====================================================================
// 3. Median cut in Oklab (k-means initialization)
// =====================================================================

struct OklabBox {
    std::vector<int> indices; // indices into the sample array
};

/**
 * Median-cut initialization: split the samples into ncolor boxes,
 * return the centroid of each box as initial k-means centroids.
 */
std::vector<OklabColor> medianCutInit(std::vector<OklabColor> const &samples, int ncolor)
{
    std::vector<OklabBox> boxes(1);
    boxes[0].indices.resize(samples.size());
    std::iota(boxes[0].indices.begin(), boxes[0].indices.end(), 0);

    while ((int)boxes.size() < ncolor) {
        // Find the box with the largest range on any axis.
        int splitBox = -1;
        float maxRange = -1;
        int splitAxis = 0;

        for (int i = 0; i < (int)boxes.size(); i++) {
            if (boxes[i].indices.size() < 2) continue;

            float minL = 1e9, maxL = -1e9;
            float mina = 1e9, maxa = -1e9;
            float minb = 1e9, maxb = -1e9;
            for (int idx : boxes[i].indices) {
                auto &s = samples[idx];
                minL = std::min(minL, s.L); maxL = std::max(maxL, s.L);
                mina = std::min(mina, s.a); maxa = std::max(maxa, s.a);
                minb = std::min(minb, s.b); maxb = std::max(maxb, s.b);
            }
            float rangeL = maxL - minL;
            float rangea = maxa - mina;
            float rangeb = maxb - minb;
            float range = std::max({rangeL, rangea, rangeb});
            int axis = (range == rangeL) ? 0 : (range == rangea) ? 1 : 2;

            if (range > maxRange) {
                maxRange = range;
                splitBox = i;
                splitAxis = axis;
            }
        }

        if (splitBox < 0) break; // can't split further

        // Sort the box on the split axis and split at median.
        auto &box = boxes[splitBox];
        std::sort(box.indices.begin(), box.indices.end(), [&](int a, int b) {
            auto &sa = samples[a];
            auto &sb = samples[b];
            switch (splitAxis) {
                case 0: return sa.L < sb.L;
                case 1: return sa.a < sb.a;
                default: return sa.b < sb.b;
            }
        });

        int mid = box.indices.size() / 2;
        OklabBox newBox;
        newBox.indices.assign(box.indices.begin() + mid, box.indices.end());
        box.indices.resize(mid);
        boxes.push_back(std::move(newBox));
    }

    // Compute centroids.
    std::vector<OklabColor> centroids;
    centroids.reserve(boxes.size());
    for (auto &box : boxes) {
        if (box.indices.empty()) continue;
        float sumL = 0, suma = 0, sumb = 0;
        for (int idx : box.indices) {
            sumL += samples[idx].L;
            suma += samples[idx].a;
            sumb += samples[idx].b;
        }
        float n = box.indices.size();
        centroids.push_back({ sumL / n, suma / n, sumb / n });
    }

    return centroids;
}

// =====================================================================
// 4. Saliency-weighted k-means for perceptual color quantization
// =====================================================================

/*

Standard k-means allocates clusters proportionally to pixel count: a
large gray background gets many centroids while a small vivid region
(e.g. a purple flower) may not get any. This is because k-means
minimizes total reconstruction error, which is dominated by the
majority color.

To address this, we weight each pixel sample by its local chroma
saliency before running k-means. The saliency of a pixel measures how
much its chroma (the a,b channels in Oklab, which encode hue and
saturation) differs from the average chroma of a surrounding
neighborhood block. This is a lightweight variant of Frequency-Tuned
saliency, adapted for solid-color artwork:

  - We compare against the *local* neighborhood rather than the global
    image mean, because vectorizable drawings typically have flat color
    regions where the interesting signal is at color boundaries, not
    distance from the overall average.

  - We measure chroma distance only (ignoring lightness), because
    lightness variations within a region (shading, gradients) are less
    important than hue changes for palette selection.

The resulting per-pixel weight is:  w = 1.0 + 20.0 * chroma_saliency

The baseline of 1.0 ensures uniform regions still contribute (they are
real colors in the image), while the 20x boost for salient pixels
causes chromatically distinctive areas to attract k-means centroids
disproportionately. A single purple flower pixel at a chroma boundary
now counts as much as ~20 uniform gray background pixels.

The weighted k-means iteration replaces the standard centroid update
(mean of assigned samples) with a weighted mean, so centroids drift
toward high-saliency colors while still converging to well-defined
cluster centers — no mushy averaging artifacts.
*/

struct WeightedSample {
    OklabColor color;
    float weight;
};

/**
 * Weighted k-means in Oklab space.
 *
 * Each sample has a saliency weight that influences centroid computation:
 * centroids are the weighted mean of assigned samples. This causes
 * chromatically distinctive pixels (color edges, small vivid regions)
 * to attract centroids more strongly than uniform background areas.
 */
struct KMeansResult {
    std::vector<OklabColor> centroids;
    float inertia;
};

KMeansResult runWeightedKMeans(std::vector<WeightedSample> const &samples, int k, int maxIter = 15)
{
    int const n = samples.size();

    // Extract unweighted colors for median-cut initialization.
    std::vector<OklabColor> colors(n);
    for (int i = 0; i < n; i++) colors[i] = samples[i].color;

    auto centroids = medianCutInit(colors, k);
    int const actualK = centroids.size();

    std::vector<int> assignments(n, 0);
    float const convergenceThreshold = 1e-6f;

    for (int iter = 0; iter < maxIter; iter++) {
        for (int i = 0; i < n; i++) {
            assignments[i] = findOklab(centroids.data(), actualK, samples[i].color);
        }

        // Weighted centroid recomputation.
        std::vector<float> sumL(actualK, 0), suma(actualK, 0), sumb(actualK, 0);
        std::vector<float> sumW(actualK, 0);

        for (int i = 0; i < n; i++) {
            int c = assignments[i];
            float w = samples[i].weight;
            sumL[c] += samples[i].color.L * w;
            suma[c] += samples[i].color.a * w;
            sumb[c] += samples[i].color.b * w;
            sumW[c] += w;
        }

        float maxShift = 0;
        for (int c = 0; c < actualK; c++) {
            if (sumW[c] <= 0) continue;
            OklabColor newCentroid = {
                sumL[c] / sumW[c],
                suma[c] / sumW[c],
                sumb[c] / sumW[c]
            };
            maxShift = std::max(maxShift, distOklab(centroids[c], newCentroid));
            centroids[c] = newCentroid;
        }

        if (maxShift < convergenceThreshold) break;
    }

    // Compute weighted inertia.
    float inertia = 0;
    for (int i = 0; i < n; i++) {
        inertia += samples[i].weight * distOklab(samples[i].color, centroids[assignments[i]]);
    }

    return { std::move(centroids), inertia };
}

/**
 * Downsample an RgbMap to saliency-weighted Oklab samples.
 *
 * Computes a local chroma saliency for each sampled pixel: the Oklab
 * chroma distance between the pixel and the mean of a surrounding block.
 * Pixels that stand out chromatically from their local neighborhood
 * (color edges, small distinctive regions like a purple flower on green
 * grass) get a high weight, while pixels in uniform regions (gray
 * background, large flat areas) get a low weight.
 *
 * This causes weighted k-means to allocate centroids toward visually
 * distinctive colors rather than proportionally to pixel count.
 */
std::vector<WeightedSample> downsampleWithSaliency(RgbMap const &rgbmap, int maxSamples = 12000)
{
    int const W = rgbmap.width;
    int const H = rgbmap.height;
    int const totalPixels = W * H;
    int step = std::max(1, totalPixels / maxSamples);

    // Local neighborhood radius (in pixels at the sampling stride).
    // We use a block of ~7x7 pixels around each sample point.
    int const radius = 3 * std::max(1, (int)std::sqrt(step));

    std::vector<WeightedSample> samples;
    samples.reserve(std::min(totalPixels, maxSamples));

    for (int i = 0; i < totalPixels; i += step) {
        int cx = i % W;
        int cy = i / W;
        auto pixLab = rgbToOklab(rgbmap.getPixel(cx, cy));

        // Compute local mean chroma in the neighborhood.
        float localSumA = 0, localSumB = 0;
        int localCount = 0;
        int x0 = std::max(0, cx - radius);
        int x1 = std::min(W - 1, cx + radius);
        int y0 = std::max(0, cy - radius);
        int y1 = std::min(H - 1, cy + radius);

        // Sub-sample the neighborhood for speed.
        int nstep = std::max(1, radius / 2);
        for (int ny = y0; ny <= y1; ny += nstep) {
            for (int nx = x0; nx <= x1; nx += nstep) {
                auto nLab = rgbToOklab(rgbmap.getPixel(nx, ny));
                localSumA += nLab.a;
                localSumB += nLab.b;
                localCount++;
            }
        }

        float meanA = localSumA / localCount;
        float meanB = localSumB / localCount;

        // Saliency = chroma distance from local mean.
        float da = pixLab.a - meanA;
        float db = pixLab.b - meanB;
        float saliency = std::sqrt(da * da + db * db);

        // Weight = baseline + saliency boost.
        // The baseline (1.0) ensures uniform regions still contribute
        // (they are real colors in the image), but salient pixels get
        // disproportionately more influence.
        float weight = 1.0f + 20.0f * saliency;

        samples.push_back({ pixLab, weight });
    }

    return samples;
}

} // namespace

// =====================================================================
// Public API
// =====================================================================

/**
 * Quantize an RGB image to a reduced number of colors (octree, RGB space).
 * Used for QUANT_MONO and BRIGHTNESS_MULTI modes.
 */
IndexedMap rgbMapQuantize(RgbMap const &rgbmap, int ncolor)
{
    assert(ncolor > 0);

    auto imap = IndexedMap(rgbmap.width, rgbmap.height);

    Pool<Ocnode> pool;
    auto tree = octreeBuild(pool, rgbmap, ncolor);

    auto rgbs = std::make_unique<RGB[]>(ncolor);
    int index = 0;
    octreeIndex(tree, rgbs.get(), index);

    octreeDelete(pool, tree);

    // stacking with increasing contrasts
    std::sort(rgbs.get(), rgbs.get() + ncolor, [] (auto &ra, auto &rb) {
        return (ra.r + ra.g + ra.b) < (rb.r + rb.g + rb.b);
    });

    // make the new map
    // fill in the color lookup table
    for (int i = 0; i < index; i++) {
        imap.clut[i] = rgbs[i];
    }
    imap.nrColors = index;

    // fill in new map pixels
    for (int y = 0; y < rgbmap.height; y++) {
        for (int x = 0; x < rgbmap.width; x++) {
            auto rgb = rgbmap.getPixel(x, y);
            int index = findRGB(rgbs.get(), ncolor, rgb);
            imap.setPixel(x, y, index);
        }
    }

    return imap;
}

/**
 * Given an existing palette and an image, find the best next color to add.
 *
 * Runs constrained k-means with K+1 centroids: the K existing colors are
 * frozen and only the new centroid is updated each iteration. The new
 * centroid is initialized at the sample point that is most distant from
 * any existing palette color (maximizing initial coverage).
 */
RGB findNextPaletteColor(RgbMap const &rgbmap, std::vector<RGB> const &existingPalette)
{
    auto samples = downsampleWithSaliency(rgbmap);
    int const n = samples.size();
    int const K = existingPalette.size();

    // Convert existing palette to Oklab (these are frozen).
    std::vector<OklabColor> centroids(K + 1);
    for (int i = 0; i < K; i++) {
        centroids[i] = rgbToOklab(existingPalette[i]);
    }

    // Initialize the new centroid at the sample with the highest
    // saliency-weighted distance from all existing centroids.
    float maxScore = -1;
    int bestIdx = 0;
    for (int i = 0; i < n; i++) {
        float minDist = std::numeric_limits<float>::max();
        for (int c = 0; c < K; c++) {
            minDist = std::min(minDist, distOklab(samples[i].color, centroids[c]));
        }
        float score = minDist * samples[i].weight;
        if (score > maxScore) {
            maxScore = score;
            bestIdx = i;
        }
    }
    centroids[K] = samples[bestIdx].color;

    // Constrained weighted k-means: assign all samples, but only update centroid K.
    int const totalK = K + 1;
    int const maxIter = 15;
    float const convergenceThreshold = 1e-6f;

    for (int iter = 0; iter < maxIter; iter++) {
        float sumL = 0, suma = 0, sumb = 0;
        float sumW = 0;

        for (int i = 0; i < n; i++) {
            int nearest = findOklab(centroids.data(), totalK, samples[i].color);
            if (nearest == K) {
                float w = samples[i].weight;
                sumL += samples[i].color.L * w;
                suma += samples[i].color.a * w;
                sumb += samples[i].color.b * w;
                sumW += w;
            }
        }

        if (sumW <= 0) break;

        OklabColor newCentroid = { sumL / sumW, suma / sumW, sumb / sumW };
        float shift = distOklab(centroids[K], newCentroid);
        centroids[K] = newCentroid;

        if (shift < convergenceThreshold) break;
    }

    return oklabToRgb(centroids[K]);
}

/**
 * Map an RGB image to a user-supplied palette of colors.
 * Each pixel is assigned to the perceptually nearest color (Oklab distance).
 */
IndexedMap rgbMapWithPalette(RgbMap const &rgbmap, std::vector<RGB> const &palette)
{
    int ncolor = palette.size();
    assert(ncolor > 0);

    auto imap = IndexedMap(rgbmap.width, rgbmap.height);

    // Sort palette by perceptual lightness (Oklab L) for consistent stacking.
    auto sorted = palette;
    std::sort(sorted.begin(), sorted.end(), [] (auto &a, auto &b) {
        return rgbToOklab(a).L < rgbToOklab(b).L;
    });

    // Precompute Oklab values for the palette.
    std::vector<OklabColor> palOklab(ncolor);
    for (int i = 0; i < ncolor; i++) {
        palOklab[i] = rgbToOklab(sorted[i]);
    }

    // Fill in the color lookup table.
    imap.nrColors = ncolor;
    for (int i = 0; i < ncolor; i++) {
        imap.clut[i] = sorted[i];
    }

    // Map each pixel to the perceptually nearest palette color.
    for (int y = 0; y < rgbmap.height; y++) {
        for (int x = 0; x < rgbmap.width; x++) {
            auto rgb = rgbmap.getPixel(x, y);
            auto lab = rgbToOklab(rgb);
            int index = findOklab(palOklab.data(), ncolor, lab);
            imap.setPixel(x, y, index);
        }
    }

    return imap;
}

/**
 * Convert k-means centroids to a sorted RGB palette.
 */
static std::vector<RGB> centroidsToSortedPalette(std::vector<OklabColor> centroids)
{
    // Sort by perceptual lightness for consistent stacking order.
    std::sort(centroids.begin(), centroids.end(), [] (auto &a, auto &b) {
        return a.L < b.L;
    });

    std::vector<RGB> palette(centroids.size());
    for (int i = 0; i < (int)centroids.size(); i++) {
        palette[i] = oklabToRgb(centroids[i]);
    }
    return palette;
}

/**
 * Quantize an RGB image using saliency-weighted k-means in Oklab space.
 */
IndexedMap rgbMapQuantizePerceptual(RgbMap const &rgbmap, int ncolor)
{
    assert(ncolor > 0);

    auto samples = downsampleWithSaliency(rgbmap);
    auto result = runWeightedKMeans(samples, ncolor);
    auto palette = centroidsToSortedPalette(std::move(result.centroids));

    return rgbMapWithPalette(rgbmap, palette);
}

/**
 * Estimate the optimal number of colors for an image using the elbow
 * method on k-means inertia in Oklab space.
 *
 * Runs k-means for k=2..maxColors, computes inertia for each, then
 * finds the elbow point: the k where the marginal reduction in inertia
 * drops off most sharply.
 */
std::vector<RGB> estimateOptimalPalette(RgbMap const &rgbmap, int maxColors)
{
    assert(maxColors >= 2);

    auto samples = downsampleWithSaliency(rgbmap);

    // Run weighted k-means for each candidate k and record inertia.
    std::vector<float> inertias;
    std::vector<std::vector<OklabColor>> all_centroids;
    inertias.reserve(maxColors - 1);
    all_centroids.reserve(maxColors - 1);

    for (int k = 2; k <= maxColors; k++) {
        auto result = runWeightedKMeans(samples, k);
        inertias.push_back(result.inertia);
        all_centroids.push_back(std::move(result.centroids));
    }

    // Find the elbow using maximum distance from the line connecting
    // the first and last points (Kneedle method).
    int npoints = inertias.size();
    if (npoints <= 1) {
        return centroidsToSortedPalette(std::move(all_centroids[0]));
    }

    // Normalize k and inertia to [0,1] so the geometry is unbiased.
    float iMin = inertias.back(), iMax = inertias.front();
    float iRange = (iMax - iMin > 0) ? (iMax - iMin) : 1.0f;

    // Line from first point (0, 1) to last point (1, 0) in normalized coords.
    // Distance from point (px, py) to line ax + by + c = 0.
    // Line: y - 1 + x = 0  =>  x + y - 1 = 0  =>  a=1, b=1, c=-1
    float a = 1.0f, b = 1.0f, c = -1.0f;
    float denom = std::sqrt(a * a + b * b);

    int bestIdx = 0;
    float bestDist = -1;

    for (int i = 0; i < npoints; i++) {
        float px = (float)(i) / (npoints - 1);            // normalized k
        float py = (inertias[i] - iMin) / iRange;         // normalized inertia
        float dist = std::abs(a * px + b * py + c) / denom;
        if (dist > bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    // Bias toward a richer palette: the elbow is the minimum useful k,
    // but a couple more colors typically improve coverage noticeably.
    bestIdx = std::min(bestIdx + 2, npoints - 1);

    return centroidsToSortedPalette(std::move(all_centroids[bestIdx]));
}

} // namespace Trace
} // namespace Inkscape
