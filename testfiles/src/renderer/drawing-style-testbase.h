// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_TEST_RENDERER_DRAWING_STYLE_TESTBASE_H
#define INKSCAPE_TEST_RENDERER_DRAWING_STYLE_TESTBASE_H

#include "color-testbase.h"
#include "style-enums.h"

#include "renderer/drawing/drawing-style.h"
#include "renderer/drawing/drawing-paintserver.h"

using namespace Inkscape::Renderer;

struct PaintServerMockSource
{
    PaintServerType type = PaintServerType::INVALID;
    SPGradientSpread spread;
    SPGradientUnits units;
    Geom::Affine transform;

    std::optional<Color> color;
    std::optional<SPGradientVector> vector;
    std::optional<SPGradientMesh> mesh;
    
    bool isValid() const { return is_valid; }

    PaintServerType getPaintType() const { return type; }
 
    Color            const  getSolidColor() const { return color ? *color : Color(0x0); }
    SPGradientMesh   const *getGradientMesh() const { return mesh ? &*mesh : nullptr; }
    SPGradientVector const *getGradientVector() const { return vector ? &*vector : nullptr; }
    SPGradientSpread getSpread() const { return spread; }
    SPGradientUnits  getUnits() const { return units; }
    Geom::Affine     getGradientTransform() const { return transform; } 

    bool is_valid = true;

    static std::unique_ptr<PaintServerMockSource> make_color(Color color)
    {
        return std::unique_ptr<PaintServerMockSource>(new PaintServerMockSource(
            PaintServerType::SOLID_COLOR, SP_GRADIENT_SPREAD_REFLECT, SP_GRADIENT_UNITS_USERSPACEONUSE, {}, std::move(color), {}, {}
        ));
    }
    static std::unique_ptr<PaintServerMockSource> make_gradient(bool linear, SPGradientSpread spread, SPGradientUnits units, Geom::Affine transform, SPGradientVector vector)
    {
        return std::unique_ptr<PaintServerMockSource>(new PaintServerMockSource(
            linear ? PaintServerType::LINEAR_GRADIENT : PaintServerType::RADIAL_GRADIENT, spread, units, transform, {}, std::move(vector), {}
        ));
    }
    static std::unique_ptr<PaintServerMockSource> make_mesh(Geom::Affine transform, SPGradientMesh mesh)
    {
        return std::unique_ptr<PaintServerMockSource>(new PaintServerMockSource(
            PaintServerType::MESH_GRADIENT, SP_GRADIENT_SPREAD_REFLECT, SP_GRADIENT_UNITS_USERSPACEONUSE, transform, {}, {}, std::move(mesh)
        ));
    }
    static std::unique_ptr<PaintServerMockSource> make_group_pattern()
    {
        return std::unique_ptr<PaintServerMockSource>(new PaintServerMockSource(
            PaintServerType::GROUP_PATTERN
        ));
    }
};

struct StyleMockSource
{
    struct Double
    {
        double value = 0.0;
        double as_double() const { return value; }
    };
    template <typename T>
    struct Enum
    {
        T value;
        T as_enum() const { return value; }
    };

    struct PaintMockSource
    {
        struct Href
        {
            std::unique_ptr<PaintServerMockSource> server;
            PaintServerMockSource *getObject() const { return server.get(); }
        };

        std::optional<Color> color;
        std::unique_ptr<Href> href;
        bool is_none = false;

        SPPaintOrigin paintOrigin = SP_CSS_PAINT_ORIGIN_NORMAL;
        SPPaintOrigin paintSource = SP_CSS_PAINT_ORIGIN_NORMAL;

        // We could test href's getObject, but this would exclude patterns who do not
        // set a PaintServerMockSource, So the existance of an empty Paintserver acts
        // as a sort of flag for "this is a pattern"
        bool isPaintserver() const { return (bool)href; }
        bool isColor() const { return (bool)color; }
        bool isNone() const { return is_none; }
        Color getColor() const { return *color; }

        bool set = true;
    };
    struct ColorSpace {
        std::shared_ptr<Inkscape::Colors::Space::AnySpace> space;
        auto getInterpolationSpace() const { return space; }
    };
    struct PaintOrder {
        std::array<SPPaintOrderLayer, 3> layer;
        auto get_layers() const { return layer; }
    };
    struct StrokeArray
    {
        std::vector<double> values;
        bool is_valid() const { return true; }
        auto get_computed() const { return values; }
    };
    struct StrokeExtensions
    {
        bool hairline = false;
    };
    struct VectorEffect
    {
        double size;
        bool rotate;
        bool fixed;
        bool stroke;
    };
    struct TextDecoration {
        StyleMockSource *style_td = nullptr;
    };
    struct TextDecorationLine
    {
        bool inherit;
        bool underline;
        bool overline;
        bool line_through;
        bool blink;
    };
    struct TextDecorationStyle
    {
        bool inherit;
        bool solid;
        bool isdouble;
        bool dotted;
        bool dashed;
        bool wavy;
    };
    struct TextDecorationData
    {
        double phase_length;
        double tspan_line_start;
        double tspan_line_end;
        double tspan_width;
        double ascender;
        double descender;
        double underline_thickness;
        double underline_position;
        double line_through_thickness;
        double line_through_position;
    };

    // Defaults match code-builder's first argument in ADD_ENUM

    Double                 opacity           = {1.0};
    PaintMockSource        fill;
    Double                 fill_opacity      = {1.0};
    PaintOrder             paint_order;
    ColorSpace             color_interpolation;

    PaintMockSource        stroke;
    Double                 stroke_opacity    = {1.0};
    Double                 stroke_width      = {1.0};
    Double                 stroke_miterlimit = {4.0};
    Double                 stroke_dashoffset = {0.0};
    StrokeArray            stroke_dasharray;
    StrokeExtensions       stroke_extensions;
    Enum<SPStrokeJoinType> stroke_linejoin   = {SP_STROKE_LINEJOIN_MITER};
    Enum<SPStrokeCapType>  stroke_linecap    = {SP_STROKE_LINECAP_BUTT};

    double                 font_size;
    Enum<SPCSSDirection>   direction = {SP_CSS_DIRECTION_LTR};
    TextDecoration         text_decoration;
    PaintMockSource        text_decoration_fill;
    PaintMockSource        text_decoration_stroke;
    PaintMockSource        text_decoration_color;
    TextDecorationLine     text_decoration_line;
    TextDecorationStyle    text_decoration_style;
    TextDecorationData     text_decoration_data;

    Enum<SPWindRule>         fill_rule         = {SP_WIND_RULE_NONZERO};
    Enum<SPWindRule>         clip_rule         = {SP_WIND_RULE_NONZERO};
    Enum<SPImageRendering>   image_rendering   = {SP_CSS_IMAGE_RENDERING_AUTO};
    Enum<SPEnableBackground> enable_background = {SP_CSS_BACKGROUND_ACCUMULATE};

    VectorEffect vector_effect;
};

#endif

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
