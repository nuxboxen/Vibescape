// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * SVG drawing for display.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "drawing.h"

#include <array>
#include <thread>

#include "colors/manager.h"

#include "renderer/context.h"
#include "renderer/drawing/drawing-item.h"
#include "renderer/pixel-filters/average-color.h"
#include "renderer/surface.h"
#include "renderer/threading.h"
#include "renderer/code-builder.h"

namespace Inkscape::Renderer {

static auto rendermode_to_renderflags(RenderMode mode)
{
    switch (mode) {
        case RenderMode::OUTLINE:           return DrawingItem::RENDER_OUTLINE;
        case RenderMode::NO_FILTERS:        return DrawingItem::RENDER_NO_FILTERS;
        case RenderMode::VISIBLE_HAIRLINES: return DrawingItem::RENDER_VISIBLE_HAIRLINES;
        default:                            return DrawingItem::RenderFlags::RENDER_DEFAULT;
    }
}

Drawing::Drawing()
    : _outline_color{0xFF}
    , _clip_outline_color{0xFF}
    , _mask_outline_color{0xFF}
    , _image_outline_color{0xFF}
{}

Drawing::~Drawing()
{
    delete _root;
}

void Drawing::setCodeBuild()
{
    _code_build = true;
    CodeBuilder::Construct(*this, "Drawing");
}

void Drawing::setRoot(DrawingItem *root)
{
    delete _root;
    _root = root;
    if (_root) {
        if (_code_build) CodeBuilder::Call(*this, "setRoot") << root;
        assert(_root->_child_type == DrawingItem::ChildType::ORPHAN);
        _root->_child_type = DrawingItem::ChildType::ROOT;
    }
}

void Drawing::setRenderMode(RenderMode mode)
{
    assert(mode != RenderMode::OUTLINE_OVERLAY && "Drawing::setRenderMode: OUTLINE_OVERLAY is not a true render mode");

    defer([=, this] {
        if (mode == _rendermode) return;
        _root->_markForRendering();
        _rendermode = mode;
        _root->_markForUpdate(STATE_ALL, true);
        _clearCache();
    });
}

void Drawing::setColorMode(ColorMode mode)
{
    defer([=, this] {
        if (mode == _colormode) return;
        _colormode = mode;
        if (_rendermode != RenderMode::OUTLINE || _image_outline_mode) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setOutlineOverlay(bool outlineoverlay)
{
    defer([=, this] {
        if (outlineoverlay == _outlineoverlay) return;
        _outlineoverlay = outlineoverlay;
        _root->_markForUpdate(STATE_ALL, true);
    });
}

void Drawing::setOutlineColor(Colors::Color col)
{
    defer([=, this] {
        _outline_color = std::move(col);
        if (_rendermode == RenderMode::OUTLINE || _outlineoverlay) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setClipOutlineColor(Colors::Color col)
{
    defer([=, this] {
        _clip_outline_color = std::move(col);
        if (_rendermode == RenderMode::OUTLINE || _outlineoverlay) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setMaskOutlineColor(Colors::Color col)
{
    defer([=, this] {
        _mask_outline_color = std::move(col);
        if (_rendermode == RenderMode::OUTLINE || _outlineoverlay) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setImageOutlineColor(Colors::Color col)
{
    defer([=, this] {
        _image_outline_color = std::move(col);
        if ((_rendermode == RenderMode::OUTLINE || _outlineoverlay) && !_image_outline_mode) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setImageOutlineMode(bool enabled)
{
    defer([=, this] {
        _image_outline_mode = enabled;
        if (_rendermode == RenderMode::OUTLINE || _outlineoverlay) {
            _root->_markForRendering();
        }
    });
}

void Drawing::setFilterQuality(DrawingFilter::Quality quality)
{
    defer([=, this] {
        _filter_quality = quality;
        if (!(_rendermode == RenderMode::OUTLINE || _rendermode == RenderMode::NO_FILTERS)) {
            _root->_markForUpdate(STATE_ALL, true);
            _clearCache();
        }
    });
}

void Drawing::setBlurQuality(DrawingFilter::BlurQuality quality)
{
    defer([=, this] {
        _blur_quality = quality;
        if (!(_rendermode == RenderMode::OUTLINE || _rendermode == RenderMode::NO_FILTERS)) {
            if (_root) {
                _root->_markForUpdate(STATE_ALL, true);
            }
            _clearCache();
        }
    });
}

void Drawing::setDithering(bool use_dithering)
{
    defer([=, this] {
        _use_dithering = use_dithering;
        #if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 18, 0)
        if (_rendermode != RenderMode::OUTLINE) {
            _root->_markForUpdate(STATE_ALL, true);
            _clearCache();
        }
        #endif
    });
}

void Drawing::setCacheBudget(size_t bytes)
{
    defer([=, this] {
        _cache_budget = bytes;
        _pickItemsForCaching();
    });
}

void Drawing::setCacheLimit(Geom::OptIntRect const &rect)
{
    defer([=, this] {
        _cache_limit = rect;
        /*
        for (auto item : _cached_items) {
            item->_markForUpdate(STATE_CACHE, false);
        }
        */
    });
}

void Drawing::setClip(std::optional<Geom::PathVector> &&clip)
{
    defer([=, this] {
        if (clip == _clip) return;
        _clip = std::move(clip);
        _root->_markForRendering();
    });
}

void Drawing::setAntialiasingOverride(std::optional<Antialiasing> antialiasing_override)
{
    if (_code_build) CodeBuilder::Call(*this, "setAntialiasingOverride") << "Antialiasing::Good";
    defer([=, this] {
        _antialiasing_override = antialiasing_override;
        _root->_markForUpdate(STATE_ALL, true);
        _clearCache();
    });
}

void Drawing::setNumDispatchThreads(int num)
{
    set_num_dispatch_threads(num);
}

void Drawing::update(Geom::IntRect const &area, Geom::Affine const &affine, unsigned flags, unsigned reset)
{
    if (_root) {
        if (_code_build) CodeBuilder::Call(*this, "update") << area << affine << flags << reset;
        _root->update(area, { affine }, flags, reset);
    }
    if (flags & STATE_CACHE) {
        // Process the updated cache scores.
        _pickItemsForCaching();
    }
}

void Drawing::render(Context &dc, Geom::IntRect const &area, unsigned flags) const
{
    if (_code_build) CodeBuilder::Call(*this, "render", true) << "*context" << area << flags;

    auto opt = DrawingOptions{
        .outline_color = _outline_color,
        .antialiasing_override = _antialiasing_override,
        .dithering = _use_dithering
        // TODO: Add blurquality and filterquality
    };
    flags |= rendermode_to_renderflags(_rendermode);

    dc.setAntialiasing(_antialiasing_override.value_or(Antialiasing(_root->_antialias)));
    if (_clip) {
        dc.save();
        dc.path(*_clip * _root->_ctm);
        dc.clip();
    }
    _root->render(dc, opt, area, flags);
    if (_clip) {
        dc.restore();
    }
}

DrawingItem *Drawing::pick(Geom::Point const &p, double delta, Geom::OptIntRect const &area_world, unsigned flags)
{
    return _root->pick(p, delta, area_world, flags);
}

void Drawing::snapshot()
{
    assert(!_snapshotted);
    _snapshotted = true;
}

void Drawing::unsnapshot()
{
    assert(_snapshotted);
    _snapshotted = false; // Unsnapshot before replaying log so further work is not deferred.
    _funclog();
}

void Drawing::_pickItemsForCaching()
{
    // Build sorted list of items that should be cached.
    /*
    std::vector<DrawingItem*> to_cache;
    size_t used = 0;
    for (auto &rec : _candidate_items) {
        if (used + rec.cache_size > _cache_budget) break;
        to_cache.emplace_back(rec.item);
        used += rec.cache_size;
    }
    std::reverse(to_cache.begin(), to_cache.end());

    // Uncache the items that are cached but should not be cached.
    // Note: setCached() modifies _cached_items, so the temporary container is necessary.
    std::vector<DrawingItem*> to_uncache;
    std::set_difference(_cached_items.begin(), _cached_items.end(),
                        to_cache.begin(), to_cache.end(),
                        std::back_inserter(to_uncache));
    for (auto item : to_uncache) {
        item->_setCached(false);
    }

    // Cache all items that should be cached (no-op if already cached).
    for (auto item : to_cache) {
        item->_setCached(true);
    }
    */
}

void Drawing::_clearCache()
{
    // Note: setCached() modifies _cached_items, so the temporary container is necessary.
    /*
    std::vector<DrawingItem*> to_uncache;
    std::copy(_cached_items.begin(), _cached_items.end(), std::back_inserter(to_uncache));
    for (auto item : to_uncache) {
        item->_setCached(false, true);
    }
    */
}

/*
 * Return average color over area. Used by Calligraphic, Dropper, and Spray tools.
 */
Colors::Color Drawing::averageColor(Geom::IntRect const &area) const
{
    // TODO: Replace color_space with target color space useful for this average
    auto color_space = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto surface = std::make_shared<Surface>(area.dimensions(), 1, color_space);
    auto dc = Context(*surface, area.min());
    render(dc, area);
    return Colors::Color(color_space, surface->run_pixel_filter(PixelFilter::AverageColor()));
}

/*
 * Return the average color inside the given path.
 */
Colors::Color Drawing::averageColor(Geom::PathVector const &path, bool evenodd) const
{
    auto area = path.boundsExact();
    if (!area || area->hasZeroArea()) {
        return Colors::Color(0x0); // Transparent black sRGB
    }

    // Scale the graphic so there's a predictable number of pixels to choose from
    static constexpr auto width = 200.0;
    static constexpr auto height = 200.0;

    auto affine = Geom::Scale(width / area->width(), height / area->height());

    // Build a mask of pixels to ignore
    auto alpha = Colors::Manager::get().find(Colors::Space::Type::Alpha);
    auto mask = std::make_shared<Surface>(Geom::IntPoint(width, height), 1, alpha);
    auto dc_mask = Context(*mask, (*area * affine).roundInwards()->min());
    dc_mask.scale(affine);

    dc_mask.set_fill_rule(evenodd ? Cairo::Context::FillRule::EVEN_ODD : Cairo::Context::FillRule::WINDING);
    dc_mask.path(path);
    dc_mask.clip();
    dc_mask.resetSource(1.0);
    dc_mask.set_operator(Cairo::Context::Operator::SOURCE);
    dc_mask.paint();

    // Render the output, no need to clip as the mask will say what values to use
    auto color_space = Colors::Manager::get().find(Colors::Space::Type::RGB);
    auto image = std::make_shared<Surface>(Geom::IntPoint(width, height), 1, color_space);
    auto dc = Context(*image, (*area * affine).roundInwards()->min());
    dc.scale(affine);
    render(dc, area->roundOutwards());
    return Colors::Color(color_space, image->run_pixel_filter(PixelFilter::AverageColor(), *mask));
}

/*
 * Convenience function to set high quality options for export.
 */
void Drawing::setExact()
{
    setFilterQuality(DrawingFilter::Quality::BEST);
    setBlurQuality(DrawingFilter::BlurQuality::BEST);
}

/*
 * Set the opacity of the drawing root drawing-item
 */
void Drawing::setOpacity(double opacity)
{
    _root->setOpacity(opacity);
}

} // namespace Inkscape

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
