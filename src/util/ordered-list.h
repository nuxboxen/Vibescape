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

#ifndef INKSCAPE_UTIL_ORDERED_LIST_H
#define INKSCAPE_UTIL_ORDERED_LIST_H

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>
#include <boost/intrusive/parent_from_member.hpp>

namespace Inkscape {
namespace Util {

namespace detail {
template <typename A>
concept OrderedListAggregate = requires {
    typename A::value_type;
    requires std::default_initializable<typename A::value_type>;
    requires std::unsigned_integral<typename A::value_type>;
} && requires(typename A::value_type a, typename A::value_type b) {
    { a + b } -> std::convertible_to<typename A::value_type>;
};

template <typename A, typename T>
concept OrderedListAgregateFor = OrderedListAggregate<A> && requires(T const &obj) {
    { A::contribution(obj) } -> std::convertible_to<typename A::value_type>;
};

template <typename T, typename... Ts>
concept OneOf = (std::same_as<T, Ts> || ...);
} // namespace detail

class OrderedListNodeBase
{
    using Self = OrderedListNodeBase;

    void insertAtImpl(Self *obj, unsigned idx, void (*recompute)(Self *));
    void insertAfterImpl(Self *pos, Self *obj, void (*recompute)(Self *));
    void prependImpl(Self *obj, void (*recompute)(Self *));
    void appendImpl(Self *obj, void (*recompute)(Self *));
    void eraseImpl(Self *obj, void (*recompute)(Self *));
    void clearAndDisposeImpl(void (*disposer)(Self *, void *), void *ctx);
    Self *prevNodeImpl();
    Self *nextNodeImpl();
    Self const *nextNodeImpl() const;
    Self const *prevNodeImpl() const;
    void headerInitImpl();

    struct FindContext;
    Self *_findByIndex(unsigned idx, FindContext &to_update);
    void _updateAggregates(Self *node, void (*recompute)(Self *));
    void _updateAggregates(Self *node, void (*recompute)(Self *), FindContext &to_zero);
    void _collectPath(Self *node, FindContext &to_collect);

    template <typename, auto, typename>
    friend class OrderedListImpl;

    template <detail::OrderedListAggregate...>
    friend class OrderedListNode;

    Self *parent, *left, *right;
    bool color;
    unsigned subtree_size;

public:
    struct NodeTraits;
};

/**
 * Ordered List node for list items.
 * Every item of the list must have this node as a member.
 * By default, it maintains enough information to get its index in the list in logarithmic time at worst.
 * You can attach custom aggregator traits to maintain more indices also in logarithmic time,
 * assuming it takes constant time to decide the contribution of an item.
 */
template <detail::OrderedListAggregate... As>
class OrderedListNode : OrderedListNodeBase
{
    using Self = OrderedListNode<As...>;
    std::tuple<typename As::value_type...> agg;

    template <typename, auto, typename>
    friend class OrderedListImpl;

    template <typename T>
        requires detail::OneOf<T, As...>
    static consteval std::size_t packIndex()
    {
        std::size_t i = 0;
        [[maybe_unused]] auto _ = ((std::is_same_v<T, As> ? false : (++i, true)) && ...);
        return i;
    }

    template <typename A>
    auto get() const
    {
        return std::get<packIndex<A>()>(agg);
    }

    using AggregatesTuple = std::tuple<As...>;
    template <std::size_t I>
    using Agg = std::tuple_element_t<I, AggregatesTuple>;

    // Recompute size and aggregators statistics of this node.
    template <typename T>
    constexpr void recompute(T const &obj)
    {
        subtree_size = 1 + (left ? left->subtree_size : 0) + (right ? right->subtree_size : 0);

        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((std::get<I>(agg) =
                  (left ? std::get<I>(static_cast<Self *>(left)->agg) : typename Agg<I>::value_type{}) +
                  Agg<I>::contribution(obj) +
                  (right ? std::get<I>(static_cast<Self *>(right)->agg) : typename Agg<I>::value_type{})),
             ...);
        }(std::index_sequence_for<As...>{});
    }
};

namespace detail {
template <typename Node, typename T>
struct ValidOrderedListNode : std::false_type
{
};

template <typename... As, typename T>
struct ValidOrderedListNode<OrderedListNode<As...>, T> : std::bool_constant<(OrderedListAgregateFor<As, T> && ...)>
{
};

template <typename T, auto Hook>
concept ValidOrderedListHook = std::is_member_object_pointer_v<decltype(Hook)> &&
                               ValidOrderedListNode<std::remove_cvref_t<decltype(std::declval<T>().*Hook)>, T>::value;

template <typename A>
struct value_t
{
    using type = typename A::value_type;
};

template <>
struct value_t<void>
{
    using type = unsigned;
};
} // namespace detail

template <typename T, auto Hook, typename Node>
class OrderedListImpl;

template <typename T, auto Hook, typename... As>
class OrderedListImpl<T, Hook, OrderedListNode<As...>>
{
    using Node = OrderedListNode<As...>;

    static constexpr T *_objFromNodeBase(OrderedListNodeBase *node)
    {
        return boost::intrusive::get_parent_from_member(static_cast<Node *>(node), Hook);
    }

    static constexpr T const *_objFromNodeBase(OrderedListNodeBase const *node)
    {
        return boost::intrusive::get_parent_from_member(static_cast<Node const *>(node), Hook);
    }

    static inline constinit auto recompute = [](OrderedListNodeBase *node_base) {
        auto node = static_cast<Node *>(node_base);
        node->recompute(*_objFromNodeBase(node_base));
    };

    template <typename Disposer>
    static inline constinit auto disposer_trampoline =
        [](OrderedListNodeBase *node, void *ctx) { (*static_cast<Disposer *>(ctx))(_objFromNodeBase(node)); };

public:
    OrderedListImpl() { _header.headerInitImpl(); }
    OrderedListImpl(OrderedListImpl const &) = delete;
    OrderedListImpl &operator=(OrderedListImpl const &) = delete;
    OrderedListImpl(OrderedListImpl &&) = delete;

    // Insert `obj` at list index `idx`. Time complexity: logarithmic in the length of the list.
    void insert_at(T &obj, unsigned idx) { _header.insertAtImpl(&(obj.*Hook), idx, recompute); }
    // Insert `obj` after `pos`. Time complexity: logarithmic in the length of the list.
    void insert_after(T &pos, T &obj) { _header.insertAfterImpl(&(pos.*Hook), &(obj.*Hook), recompute); }
    // Insert `obj` at the beginning of the list. Time complexity: logarithmic in the length of the list.
    void push_front(T &obj) { _header.prependImpl(&(obj.*Hook), recompute); }
    // Insert `obj` at the end of the list. Time complexity: logarithmic in the length of the list.
    void push_back(T &obj) { _header.appendImpl(&(obj.*Hook), recompute); }
    // Remove `obj` from the list. Time complexity: logarithmic in the length of the list.
    void erase(T &obj) { _header.eraseImpl(&(obj.*Hook), recompute); }

    // Clear the list and run `disposer` on each list item. Time complexity: linear in the length of the list.
    template <typename Disposer>
    void clear_and_dispose(Disposer &&disposer)
    {
        _header.clearAndDisposeImpl(disposer_trampoline<Disposer>, &disposer);
    }

    /**
     * Get the index of `obj` counted from the start of the list.
     * If a custom aggregator is provided as a template parameter, it will get the corresponding sum
     * from the start of the list.
     * Time complexity: logarithmic in the length of the list.
     */
    template <typename A = void>
        requires std::is_void_v<A> || detail::OneOf<A, As...>
    auto getIndex(T const &obj) const
    {
        Node const *cur = &(obj.*Hook);

        if constexpr (std::is_void_v<A>) {
            unsigned idx = cur->left ? cur->left->subtree_size : 0;
            while (cur->parent != &_header) {
                if (cur == cur->parent->right) {
                    unsigned ls = cur->parent->left ? cur->parent->left->subtree_size : 0;
                    idx += ls + 1;
                }
                cur = static_cast<Node const *>(cur->parent);
            }
            return idx;
        } else {
            static_assert(detail::OrderedListAggregate<A>, "must use a valid aggregate");
            using Av = typename A::value_type;

            Av idx = cur->left ? static_cast<Node const *>(cur->left)->template get<A>() : Av{};
            while (cur->parent != &_header) {
                if (cur == cur->parent->right) {
                    idx += A::contribution(*_objFromNodeBase(cur->parent)) +
                           (cur->parent->left ? static_cast<Node const *>(cur->parent->left)->template get<A>() : Av{});
                }
                cur = static_cast<Node const *>(cur->parent);
            }
            return idx;
        }
    }

    /**
     * Get the item at index `idx` counted from the start of the list.
     * If a custom aggregator is provided as a template parameter, it will get the corresponding item according to
     * the defined aggregator from the start of the list.
     * Time complexity: logarithmic in the length of the list.
     */
    template <typename A = void>
        requires std::is_void_v<A> || detail::OneOf<A, As...>
    T *atIndex(detail::value_t<A>::type idx) const
    {
        using Av = decltype(idx);
        for (auto cur = _header.parent; cur;) {
            Av left_sz;
            if constexpr (std::is_void_v<A>) {
                left_sz = cur->left ? cur->left->subtree_size : 0;
            } else {
                left_sz = cur->left ? static_cast<Node *>(cur->left)->template get<A>() : Av{};
            }
            Av own;
            if constexpr (std::is_void_v<A>) {
                own = Av{1};
            } else {
                own = A::contribution(*_objFromNodeBase(cur));
            }
            if (idx < left_sz) {
                cur = cur->left;
            } else if (idx < left_sz + own) {
                return _objFromNodeBase(cur);
            } else {
                idx -= left_sz + own;
                cur = cur->right;
            }
        }
        return nullptr;
    }

    bool empty() const { return _header.parent == nullptr; }

    template <typename A = void>
        requires std::is_void_v<A> || detail::OneOf<A, As...>
    auto size() const
    {
        if (!_header.parent)
            return typename detail::value_t<A>::type{0};
        if constexpr (std::is_void_v<A>) {
            return _header.parent->subtree_size;
        } else {
            return static_cast<Node const *>(_header.parent)->template get<A>();
        }
    }

    T &front() const { return *_objFromNodeBase(_header.left); }
    T &back() const { return *_objFromNodeBase(_header.right); }

    template <bool is_const>
    struct iterator_t
    {
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::bidirectional_iterator_tag;
        using reference = std::conditional_t<is_const, T const &, T &>;
        using pointer = std::conditional_t<is_const, T const *, T *>;
        using node_ptr = std::conditional_t<is_const, Node const *, Node *>;

        node_ptr cur;
        iterator_t() = default;
        explicit iterator_t(node_ptr n)
            : cur(n)
        {}
        template <bool B = is_const, typename = std::enable_if_t<B>>
        iterator_t(iterator_t<false> &it)
            : cur(it.cur)
        {}

        reference operator*() const { return *boost::intrusive::get_parent_from_member(cur, Hook); }
        pointer operator->() const { return boost::intrusive::get_parent_from_member(cur, Hook); }

        iterator_t &operator++()
        {
            cur = static_cast<node_ptr>(cur->nextNodeImpl());
            return *this;
        }

        iterator_t &operator--()
        {
            cur = static_cast<node_ptr>(cur->prevNodeImpl());
            return *this;
        }

        iterator_t operator++(int)
        {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        iterator_t operator--(int)
        {
            auto tmp = *this;
            --*this;
            return tmp;
        }

        friend bool operator==(iterator_t, iterator_t) = default;
    };

    using iterator = iterator_t<false>;
    using const_iterator = iterator_t<true>;

    iterator iterator_to(T &obj) { return iterator{&(obj.*Hook)}; }

    iterator begin() { return iterator{static_cast<Node *>(_header.left)}; }
    iterator end() { return iterator{&_header}; }

    const_iterator begin() const { return const_iterator{static_cast<Node const *>(_header.left)}; }
    const_iterator end() const { return const_iterator{&_header}; }

private:
    Node _header;
};

/**
 * Ordered List with intrusive nodes backed by Boost's Red-Black Tree.
 * Logarithmic insert and erase, as well as logarithmic indexed insert/query.
 * Supports extra indices.
 */
template <typename T, auto Hook>
    requires detail::ValidOrderedListHook<T, Hook>
class OrderedList : public OrderedListImpl<T, Hook, std::remove_cvref_t<decltype(std::declval<T>().*Hook)>>
{
};

// Helper to verify bidirectional_range at instantiation time
namespace detail {
template <typename T, auto Hook>
struct VerifyOrderedListRange
{
    static_assert(std::ranges::bidirectional_range<OrderedList<T, Hook>>,
                  "OrderedList must implement bidirectional range.");
    static_assert(std::ranges::sized_range<OrderedList<T, Hook>>, "OrderedList must implement sized range.");
};
} // namespace detail

} // namespace Util
} // namespace Inkscape

#endif // INKSCAPE_UTIL_ORDERED_LIST_H
