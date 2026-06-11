// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Ordered list container
 */
/*
 * Authors:
 *   Nagata Aptana <nagata.parama@protonmail.com>
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ordered-list.h"

#include <algorithm>
#include <array>
#include <boost/intrusive/rbtree_algorithms.hpp>

namespace Inkscape {
namespace Util {

using OTNB = OrderedListNodeBase;

struct OTNB::NodeTraits
{
    using node = OTNB;
    using node_ptr = OTNB *;
    using const_node_ptr = OTNB const *;
    using color = bool;
    static node_ptr get_parent(const_node_ptr n) { return n->parent; }
    static void set_parent(node_ptr n, node_ptr parent) { n->parent = parent; }
    static node_ptr get_left(const_node_ptr n) { return n->left; }
    static void set_left(node_ptr n, node_ptr left) { n->left = left; }
    static node_ptr get_right(const_node_ptr n) { return n->right; }
    static void set_right(node_ptr n, node_ptr right) { n->right = right; }
    static color get_color(const_node_ptr n) { return n->color; }
    static void set_color(node_ptr n, color c) { n->color = c; }
    static color black() { return false; }
    static color red() { return true; }
};

using RBAlgo = boost::intrusive::rbtree_algorithms<OTNB::NodeTraits>;

/**
 * For collecting the nodes on the path from targeted node to root.
 * When a tree structure gets updated, we always recompute the nodes on the new path to root.
 * However, there could be some nodes in the old path to root that gets rotated down,
 * losing subtree size and no longer in the new path to root.
 * We can find such nodes by collecting it here before the tree operation.
 *
 * If later we're willing to use our own Red-Black Tree implementation, this struct will be no longer needed
 * since we can just recompute the nodes on each structural change.
 */
struct OTNB::FindContext
{
    std::array<OTNB *, 32> data;
};

// Find node at index `idx` and collect the nodes on the path from root to that node.
// `this` should be the list header.
OTNB *OTNB::_findByIndex(unsigned idx, FindContext &to_update)
{
    int arr_idx = 0;
    for (auto curr = this->parent; curr;) {
        to_update.data[arr_idx++] = curr;
        unsigned left_sz = curr->left ? curr->left->subtree_size : 0;
        if (idx < left_sz) {
            curr = curr->left;
        } else if (idx > left_sz) {
            idx -= left_sz + 1;
            curr = curr->right;
        } else {
            return curr;
        }
    }
    return nullptr;
}

// Recompute size and aggregate statistics for all the nodes in the path from `node` to root.
// `this` should be the list header.
void OTNB::_updateAggregates(OTNB *node, void (*recompute)(OTNB *))
{
    for (; node != this && node != nullptr; node = node->parent)
        recompute(node);
}

// Recompute size and aggregate statistics for all the nodes in the path from `node` to root,
// and set each nodes corresponding entry in `to_zero` to zero.
// `this` should be the list header.
void OTNB::_updateAggregates(OTNB *node, void (*recompute)(OTNB *), FindContext &to_zero)
{
    for (; node != this && node != nullptr; node = node->parent) {
        recompute(node);
        std::ranges::replace(to_zero.data, node, nullptr);
    }
}

// Collect the nodes on the path from `node` to root.
// `this` should be the list header.
void OTNB::_collectPath(OTNB *node, FindContext &to_collect)
{
    for (int idx = 0; node != this && node != nullptr; node = node->parent)
        to_collect.data[idx++] = node;
}

void OTNB::insertAtImpl(OTNB *obj, unsigned idx, void (*recompute)(OTNB *))
{
    FindContext to_update{};
    auto target = _findByIndex(idx, to_update);
    RBAlgo::insert_before(this, target ? target : this, obj);

    _updateAggregates(obj, recompute, to_update);
    for (auto n : to_update.data)
        if (n != nullptr)
            _updateAggregates(n, recompute, to_update);
}

void OTNB::insertAfterImpl(OTNB *pos, OTNB *obj, void (*recompute)(OTNB *))
{
    auto target = RBAlgo::next_node(pos);
    auto collect_target = pos->right ? target : pos;
    FindContext to_update{};
    _collectPath(collect_target, to_update);
    RBAlgo::insert_before(this, target, obj);

    _updateAggregates(obj, recompute, to_update);
    for (auto n : to_update.data)
        if (n != nullptr)
            _updateAggregates(n, recompute, to_update);
}

void OTNB::prependImpl(OTNB *obj, void (*recompute)(OTNB *))
{
    FindContext to_update{};
    _collectPath(this->left, to_update);
    RBAlgo::push_front(this, obj);

    _updateAggregates(obj, recompute, to_update);
    for (auto n : to_update.data)
        if (n != nullptr)
            _updateAggregates(n, recompute, to_update);
}

void OTNB::appendImpl(OTNB *obj, void (*recompute)(OTNB *))
{
    FindContext to_update{};
    _collectPath(this->left, to_update);
    RBAlgo::push_back(this, obj);

    _updateAggregates(obj, recompute, to_update);
    for (auto n : to_update.data)
        if (n != nullptr)
            _updateAggregates(n, recompute, to_update);
}

void OTNB::eraseImpl(OTNB *obj, void (*recompute)(OTNB *))
{
    OTNB *to_update;
    if (obj->left && obj->right) {
        auto next = RBAlgo::next_node(obj);
        to_update = next == obj->right ? next : next->parent;
    } else if (obj->left || obj->right) {
        to_update = obj->left ? obj->left : obj->right;
    } else {
        to_update = obj->parent;
    }

    RBAlgo::erase(this, obj);
    if (to_update != this)
        _updateAggregates(to_update, recompute);
}

void OTNB::clearAndDisposeImpl(void (*disposer)(OTNB *, void *), void *ctx)
{
    RBAlgo::clear_and_dispose(this, [ctx, disposer](OTNB *node) { disposer(node, ctx); });
}

OTNB *OTNB::prevNodeImpl()
{
    return RBAlgo::prev_node(this);
}

OTNB *OTNB::nextNodeImpl()
{
    return RBAlgo::next_node(this);
}

OTNB const *OTNB::prevNodeImpl() const
{
    return RBAlgo::prev_node(const_cast<OTNB *>(this));
}

OTNB const *OTNB::nextNodeImpl() const
{
    return RBAlgo::next_node(const_cast<OTNB *>(this));
}

void OTNB::headerInitImpl()
{
    RBAlgo::init_header(this);
}

} // namespace Util
} // namespace Inkscape
