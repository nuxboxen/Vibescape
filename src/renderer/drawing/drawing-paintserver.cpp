// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing paintserver turning svg fills into cairo paints
 *
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "drawing-paintserver.h"

#include <utility>

#include "colors/color.h"

#include "renderer/context.h"
#include "renderer/context-pattern.h"

namespace Inkscape::Renderer {

DrawingPaintServer::~DrawingPaintServer() = default;

DrawingSolidColor::DrawingSolidColor(Colors::Color color)
    : color(std::move(color)) {}

std::shared_ptr<Pattern> DrawingSolidColor::create_pattern(Context *ct, Geom::OptRect const &, double opacity) const
{
    return std::make_shared<SolidColorPattern>(*color.withOpacity(opacity).converted(ct->getColorSpace()));
}

void DrawingGradient::_create_pattern(Renderer::Pattern &gradient, Geom::OptRect const &bbox, double opacity) const
{
    gradient.setExtend(spread);
    gradient.setMatrix(transform, units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX ? bbox : Geom::OptRect());

    // add stops
    for (auto &stop : stops) {
        // multiply stop opacity by paint opacity
        if (stop.color.has_value()) {
            gradient.addColorStop(stop.offset, stop.color->withOpacity(opacity));
        }
    }
}

std::shared_ptr<Pattern> DrawingLinearGradient::create_pattern(Context *ct, Geom::OptRect const &bbox, double opacity) const
{
    auto pat = std::make_shared<LinearGradientPattern>(ct->getColorSpace(), x1, y1, x2, y2);
    _create_pattern(*pat, bbox, opacity);
    return pat;
}

std::shared_ptr<Pattern> DrawingRadialGradient::create_pattern(Context *ct, Geom::OptRect const &bbox, double opacity) const
{
    Geom::Point focus(fx, fy);
    Geom::Point center(cx, cy);

    double radius = r;
    double focusr = fr;
    double scale = 1.0;
    double tolerance = ct->get_tolerance();

    Geom::Affine gs2user = transform;

    if (units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX && bbox) {
        Geom::Affine bbox2user(bbox->width(), 0, 0, bbox->height(), bbox->left(), bbox->top());
        gs2user *= bbox2user;
    }

    // we need to use vectors with the same direction to represent the transformed
    // radius and the focus-center delta, because gs2user might contain non-uniform scaling
    Geom::Point d(focus - center);
    Geom::Point d_user(d.length(), 0);
    Geom::Point r_user(radius, 0);
    Geom::Point fr_user(focusr, 0);
    d_user *= gs2user.withoutTranslation();
    r_user *= gs2user.withoutTranslation();
    fr_user *= gs2user.withoutTranslation();

    auto d_device = ct->user_to_device_distance(d_user);

    // compute the tolerance distance in user space
    // create a vector with the same direction as the transformed d,
    // with the length equal to tolerance
    double dl = d_device.length();
    auto t_device = Geom::Point(tolerance * d_device.x() / dl,
                                tolerance * d_device.y() / dl);
    auto tolerance_user = ct->device_to_user_distance(t_device).length();

    if (d_user.length() + tolerance_user > r_user.length()) {
        scale = r_user.length() / d_user.length();

        // nudge the focus slightly inside
        scale *= 1.0 - 2.0 * tolerance / dl;
    }

    auto pat = std::make_shared<RadialGradientPattern>(ct->getColorSpace(), scale * d.x() + center.x(), scale * d.y() + center.y(), focusr, center.x(), center.y(), radius);
    _create_pattern(*pat, bbox, opacity);
    return pat;
}

std::shared_ptr<Pattern> DrawingMeshGradient::create_pattern(Context *ct, Geom::OptRect const &bbox, double opacity) const
{
#ifdef MESH_DEBUG
    std::cout << "sp_meshgradient_create_pattern: " << bbox << " " << opacity << std::endl;
#endif

    auto pat = std::make_shared<MeshGradientPattern>(ct->getColorSpace());
    _create_pattern(*pat, bbox, opacity);

    for (int i = 0; i < rows && i < (int)patchdata.size(); i++) {
        for (int j = 0; j < cols && j < (int)patchdata[i].size(); j++) {
            auto &data = patchdata[i][j];

            pat->beginPatch();
            pat->moveTo(data.points[0][0]);

            for (int k = 0; k < 4; k++) {
                switch (data.pathtype[k]) {
                case 'l':
                case 'L':
                case 'z':
                case 'Z':
                    pat->lineTo(data.points[k][3]);
                    break;
                case 'c':
                case 'C':
                    pat->curveTo(data.points[k][1], data.points[k][2], data.points[k][3]);
                    break;
                default:
                    // Shouldn't happen
                    std::cerr << "sp_mesh_create_pattern: path error" << std::endl;
                }

                if (data.tensorIsSet[k]) {
                    pat->setControlPoint(k, data.tensorpoints[k]);
                }

                if (data.color[k]) {
                    pat->setCornerColor(k, data.color[k]->withOpacity(opacity));
                } else {
                    std::cerr << "Bad mesh color at pos " << k << "\n";
                    static auto const black = *Colors::Color::parse("black");
                    pat->setCornerColor(k, black.withOpacity(opacity));
                }
            }

            pat->endPatch();
        }
    }
    return pat;
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
