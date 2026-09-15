// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Caching for a drawing item
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_CACHE_H
#define INKSCAPE_RENDERER_DRAWING_CACHE_H

#include <memory>

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/operators.hpp>

static constexpr auto CACHE_SCORE_THRESHOLD = 50000.0; ///< Do not consider objects for caching below this score.

namespace Inkscape::Renderer {

class DrawingItem;
class SurfaceCache;

struct CacheData
{
    mutable std::mutex mutables;
    mutable std::shared_ptr<SurfaceCache> surface;
};

struct CacheRecord : boost::totally_ordered<CacheRecord>
{
    bool operator<(CacheRecord const &other) const { return score < other.score; }
    bool operator==(CacheRecord const &other) const { return score == other.score; }
    operator DrawingItem*() const { return item; }
    double score;
    size_t cache_size;
    DrawingItem *item;

    using SetHook = boost::intrusive::set_member_hook<>;
    SetHook _cache_hook;
};
using CacheSet = boost::intrusive::multiset<
    CacheRecord, boost::intrusive::member_hook<CacheRecord, CacheRecord::SetHook, &CacheRecord::_cache_hook>,
    boost::intrusive::compare<std::greater<CacheRecord>>>;

} // namespace Inkscape::Renderer

#endif // INKSCAPE_RENDERER_DRAWING_CACHE_H

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
