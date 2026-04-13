// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Quantization for Inkscape
 *
 * Authors:
 *   Stéphane Gimenez <dev@gim.name>
 *
 * Copyright (C) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
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
-- algorithm principle:

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

// ---- Oklab perceptual color space ----

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

// ---- Median cut in Oklab for k-means initialization ----

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

} // namespace

/**
 * quantize an RGB image to a reduced number of colors.
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
 * Quantize an RGB image using k-means in Oklab perceptual color space.
 *
 * Steps:
 * 1. Downsample the image to a manageable number of pixel samples.
 * 2. Convert samples to Oklab.
 * 3. Initialize centroids via median cut in Oklab.
 * 4. Run k-means (max 15 iterations, early termination).
 * 5. Assign all pixels to the nearest centroid using Oklab distance.
 */
IndexedMap rgbMapQuantizePerceptual(RgbMap const &rgbmap, int ncolor)
{
    assert(ncolor > 0);

    int const totalPixels = rgbmap.width * rgbmap.height;

    // Step 1: Downsample - collect up to ~10000 pixel samples.
    int const maxSamples = 10000;
    int step = std::max(1, totalPixels / maxSamples);

    std::vector<OklabColor> samples;
    samples.reserve(std::min(totalPixels, maxSamples));
    for (int i = 0; i < totalPixels; i += step) {
        int x = i % rgbmap.width;
        int y = i / rgbmap.width;
        samples.push_back(rgbToOklab(rgbmap.getPixel(x, y)));
    }

    int const nsamples = samples.size();

    // Step 2: Initialize centroids via median cut in Oklab.
    auto centroids = medianCutInit(samples, ncolor);
    int const actualColors = centroids.size();

    // Step 3: K-means iteration.
    std::vector<int> assignments(nsamples, 0);
    int const maxIter = 15;
    float const convergenceThreshold = 1e-6f;

    for (int iter = 0; iter < maxIter; iter++) {
        // Assign each sample to nearest centroid.
        for (int i = 0; i < nsamples; i++) {
            assignments[i] = findOklab(centroids.data(), actualColors, samples[i]);
        }

        // Recompute centroids.
        std::vector<float> sumL(actualColors, 0), suma(actualColors, 0), sumb(actualColors, 0);
        std::vector<int> counts(actualColors, 0);

        for (int i = 0; i < nsamples; i++) {
            int c = assignments[i];
            sumL[c] += samples[i].L;
            suma[c] += samples[i].a;
            sumb[c] += samples[i].b;
            counts[c]++;
        }

        float maxShift = 0;
        for (int c = 0; c < actualColors; c++) {
            if (counts[c] == 0) continue;
            OklabColor newCentroid = {
                sumL[c] / counts[c],
                suma[c] / counts[c],
                sumb[c] / counts[c]
            };
            maxShift = std::max(maxShift, distOklab(centroids[c], newCentroid));
            centroids[c] = newCentroid;
        }

        if (maxShift < convergenceThreshold) break;
    }

    // Step 4: Convert centroids back to RGB.
    std::vector<RGB> rgbPalette(actualColors);
    for (int i = 0; i < actualColors; i++) {
        rgbPalette[i] = oklabToRgb(centroids[i]);
    }

    // Sort by perceptual lightness for stacking order.
    // Keep centroids in sync for the final pixel assignment.
    std::vector<int> order(actualColors);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return centroids[a].L < centroids[b].L;
    });

    std::vector<OklabColor> sortedCentroids(actualColors);
    std::vector<RGB> sortedRgb(actualColors);
    for (int i = 0; i < actualColors; i++) {
        sortedCentroids[i] = centroids[order[i]];
        sortedRgb[i] = rgbPalette[order[i]];
    }

    // Step 5: Build the IndexedMap.
    auto imap = IndexedMap(rgbmap.width, rgbmap.height);
    imap.nrColors = actualColors;
    for (int i = 0; i < actualColors; i++) {
        imap.clut[i] = sortedRgb[i];
    }

    // Assign every pixel to nearest centroid in Oklab space.
    for (int y = 0; y < rgbmap.height; y++) {
        for (int x = 0; x < rgbmap.width; x++) {
            auto lab = rgbToOklab(rgbmap.getPixel(x, y));
            int index = findOklab(sortedCentroids.data(), actualColors, lab);
            imap.setPixel(x, y, index);
        }
    }

    return imap;
}

} // namespace Trace
} // namespace Inkscape
