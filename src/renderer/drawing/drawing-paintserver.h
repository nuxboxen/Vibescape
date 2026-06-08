// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Representation of paint servers used when rendering.
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_RENDERER_DRAWING_PAINT_SERVER_H
#define INKSCAPE_RENDERER_DRAWING_PAINT_SERVER_H

#include <array>
#include <vector>
#include <2geom/rect.h>
#include <2geom/affine.h>

#include "colors/forward.h"
#include "object/sp-paint-server-data.h"

namespace Inkscape::Renderer {

class Context;
class Pattern;

/**
 * A DrawingPaintServer is a lightweight copy of the resources needed to paint using a paint server.
 *
 * It is built by create_drawing_paintserver, stored in the DrawingItem tree, and used when rendering.
 *
 * The pattern information is stored in a rendering-backend agnostic way, and the object remains
 * valid even after the original SPPaintServer is modified or destroyed.
 */
class DrawingPaintServer
{
public:
    virtual ~DrawingPaintServer() = 0;

    /// Produce a pattern that can be used for painting with Cairo.
    virtual std::shared_ptr<Pattern> create_pattern(Context *, Geom::OptRect const &bbox, double opacity) const = 0;

    /// Return whether this paint server could benefit from dithering.
    virtual bool ditherable() const { return false; }

    /// Return whether create_pattern() uses its DrawingContext argument. Such pattern cannot be cached, but recreated each time.
    /// Fixme: The only reson this exists is to work around https://gitlab.freedesktop.org/cairo/cairo/-/issues/146.
    virtual bool uses_cairo_ctx() const { return false; }
};

// Todo: Remove, merging with existing implementation for solid colours.
/**
 * A simple solid color, storing an RGB color and an opacity.
 */
class DrawingSolidColor final
    : public DrawingPaintServer
{
public:
    DrawingSolidColor(Colors::Color color);
    std::shared_ptr<Pattern> create_pattern(Context *, Geom::OptRect const &, double opacity) const override;

private:
    Colors::Color color;
};

class DrawingPatternFlag final : public DrawingPaintServer
{
    std::shared_ptr<Pattern> create_pattern(Context *, Geom::OptRect const &, double opacity) const override
    {
        return {}; // Handled outside, todo, fix me.
    }
};

/**
 * The base class for all gradients.
 */
class DrawingGradient
    : public DrawingPaintServer
{
protected:
    DrawingGradient(SPGradientSpread spread, SPGradientUnits units, Geom::Affine const &transform, std::vector<SPGradientStop> stops = {})
        : spread(spread)
        , units(units)
        , transform(transform)
        , stops(stops)
    {}

    bool ditherable() const override { return true; }
    void _create_pattern(Renderer::Pattern &gradient, Geom::OptRect const &bbox, double opacity) const;

    SPGradientSpread spread;
    SPGradientUnits units;
    Geom::Affine transform;
    std::vector<SPGradientStop> stops;
};

/**
 * A linear gradient.
 */
class DrawingLinearGradient final
    : public DrawingGradient
{
public:
    DrawingLinearGradient(SPGradientSpread spread, SPGradientUnits units, Geom::Affine const &transform,
                          SPGradientVector const *vector)
        : DrawingGradient(spread, units, transform, vector ? vector->stops : std::vector<SPGradientStop>())
        , x1(vector && vector->geom.size() == 4 ? vector->geom[0] : 0.0)
        , y1(vector && vector->geom.size() == 4 ? vector->geom[1] : 0.0)
        , x2(vector && vector->geom.size() == 4 ? vector->geom[2] : 1.0)
        , y2(vector && vector->geom.size() == 4 ? vector->geom[3] : 1.0)
    {}

    std::shared_ptr<Pattern> create_pattern(Context *, Geom::OptRect const &bbox, double opacity) const override;
private:
    float x1, y1, x2, y2;
};

/**
 * A radial gradient.
 */
class DrawingRadialGradient final
    : public DrawingGradient
{
public:
    DrawingRadialGradient(SPGradientSpread spread, SPGradientUnits units, Geom::Affine const &transform,
                          SPGradientVector const *vector)
        : DrawingGradient(spread, units, transform, vector->stops)
        , cx(vector->geom[0])
        , cy(vector->geom[1])
        , r(vector->geom[2])
        , fx(vector->geom[3])
        , fy(vector->geom[4])
        , fr(vector->geom[5])
    {}

    std::shared_ptr<Pattern> create_pattern(Context *ct, Geom::OptRect const &bbox, double opacity) const override;

    bool uses_cairo_ctx() const override { return true; }

private:
    float cx, cy, r, fx, fy, fr;
};

/**
 * A mesh gradient.
 */
class DrawingMeshGradient final
    : public DrawingGradient
{
public:
    DrawingMeshGradient(SPGradientSpread spread, SPGradientUnits units, Geom::Affine const &transform,
                        SPGradientMesh const *mesh)
        : DrawingGradient(spread, units, transform)
        , rows(mesh->rows)
        , cols(mesh->cols)
        , patchdata(mesh->patches) {}

    std::shared_ptr<Pattern> create_pattern(Context*, Geom::OptRect const &bbox, double opacity) const override;

private:
    int rows;
    int cols;
    std::vector<std::vector<SPGradientPatch>> patchdata;
};

template <typename PaintSource>
std::unique_ptr<DrawingPaintServer> create_drawing_paintserver(PaintSource *ps)
{
    if (!ps || !ps->isValid()) {
        return {};
    }
    switch (ps->getPaintType()) {
        case PaintServerType::SOLID_COLOR:
            return std::make_unique<DrawingSolidColor>(ps->getSolidColor());
        case PaintServerType::GROUP_PATTERN:
            return std::make_unique<DrawingPatternFlag>();
        case PaintServerType::LINEAR_GRADIENT:
            return std::make_unique<DrawingLinearGradient>(ps->getSpread(), ps->getUnits(), ps->getGradientTransform(), ps->getGradientVector());
        case PaintServerType::RADIAL_GRADIENT:
            return std::make_unique<DrawingRadialGradient>(ps->getSpread(), ps->getUnits(), ps->getGradientTransform(), ps->getGradientVector());
        case PaintServerType::MESH_GRADIENT:
            return std::make_unique<DrawingMeshGradient>(ps->getSpread(), ps->getUnits(), ps->getGradientTransform(), ps->getGradientMesh());
    }
    return {}; 
}

} // namespace Inkscape::Renderer

#endif // INKSCAPE_RENDERER_DRAWING_PAINT_SERVER_H
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
