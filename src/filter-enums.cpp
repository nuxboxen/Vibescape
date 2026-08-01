// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Conversion data for filter and filter primitive enumerations
 *
 * Authors:
 *   Nicholas Bishop
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm.h>
#include <glibmm/i18n.h>

#include "filter-enums.h"

using Inkscape::Util::EnumData;
using Inkscape::Util::EnumDataConverter;

const EnumData<PrimitiveType> FPData[ENUM_VALUE(PrimitiveType, ENDPRIMITIVETYPE)] = {
    // clang-format off
    {PrimitiveType::BLEND,             _("Blend"),              "svg:feBlend"},
    {PrimitiveType::COLORMATRIX,       _("Color Matrix"),       "svg:feColorMatrix"},
    {PrimitiveType::COMPONENTTRANSFER, _("Component Transfer"), "svg:feComponentTransfer"},
    {PrimitiveType::COMPOSITE,         _("Composite"),          "svg:feComposite"},
    {PrimitiveType::CONVOLVEMATRIX,    _("Convolve Matrix"),    "svg:feConvolveMatrix"},
    {PrimitiveType::DIFFUSELIGHTING,   _("Diffuse Lighting"),   "svg:feDiffuseLighting"},
    {PrimitiveType::DISPLACEMENTMAP,   _("Displacement Map"),   "svg:feDisplacementMap"},
    {PrimitiveType::DROPSHADOW,        _("Drop Shadow"),        "svg:feDropShadow"},
    {PrimitiveType::FLOOD,             _("Flood"),              "svg:feFlood"},
    {PrimitiveType::GAUSSIANBLUR,      _("Gaussian Blur"),      "svg:feGaussianBlur"},
    {PrimitiveType::IMAGE,             _("Image"),              "svg:feImage"},
    {PrimitiveType::MERGE,             _("Merge"),              "svg:feMerge"},
    {PrimitiveType::MORPHOLOGY,        _("Morphology"),         "svg:feMorphology"},
    {PrimitiveType::OFFSET,            _("Offset"),             "svg:feOffset"},
    {PrimitiveType::SPECULARLIGHTING,  _("Specular Lighting"),  "svg:feSpecularLighting"},
    {PrimitiveType::TILE,              _("Tile"),               "svg:feTile"},
    {PrimitiveType::TURBULENCE,        _("Turbulence"),         "svg:feTurbulence"}
    // clang-format on
};
const EnumDataConverter<PrimitiveType> FPConverter(FPData, (int)PrimitiveType::ENDPRIMITIVETYPE);

const EnumData<FilterPrimitiveInput> FPInputData[FPINPUT_END] = {
    // clang-format off
    {FPINPUT_SOURCEGRAPHIC,     _("Source Graphic"),     "SourceGraphic"},
    {FPINPUT_SOURCEALPHA,       _("Source Alpha"),       "SourceAlpha"},
    {FPINPUT_BACKGROUNDIMAGE,   _("Background Image"),   "BackgroundImage"},
    {FPINPUT_BACKGROUNDALPHA,   _("Background Alpha"),   "BackgroundAlpha"},
    {FPINPUT_FILLPAINT,         _("Fill Paint"),         "FillPaint"},
    {FPINPUT_STROKEPAINT,       _("Stroke Paint"),       "StrokePaint"},
    // clang-format on
};
const EnumDataConverter<FilterPrimitiveInput> FPInputConverter(FPInputData, FPINPUT_END);

const EnumData<ColorMatrixType> ColorMatrixTypeData[ENUM_VALUE(ColorMatrixType, ENDTYPE)] = {
    // clang-format off
    {ColorMatrixType::MATRIX,           _("Matrix"),             "matrix"},
    {ColorMatrixType::SATURATE,         _("Saturate"),           "saturate"},
    {ColorMatrixType::HUEROTATE,        _("Hue Rotate"),         "hueRotate"},
    {ColorMatrixType::LUMINANCETOALPHA, _("Luminance to Alpha"), "luminanceToAlpha"}
    // clang-format on
};
const EnumDataConverter<ColorMatrixType> ColorMatrixTypeConverter(ColorMatrixTypeData, (int)ColorMatrixType::ENDTYPE);

// feComposite
const EnumData<CompositeOperator> CompositeOperatorData[ENUM_VALUE(CompositeOperator, ENDOPERATOR)] = {
    // clang-format off
    {CompositeOperator::DEFAULT,          _("Default"),         ""                 },
    {CompositeOperator::OVER,             _("Over"),            "over"             },
    {CompositeOperator::IN,               _("In"),              "in"               },
    {CompositeOperator::OUT,              _("Out"),             "out"              },
    {CompositeOperator::ATOP,             _("Atop"),            "atop"             },
    {CompositeOperator::XOR,              _("XOR"),             "xor"              },
    {CompositeOperator::LIGHTER,          _("Lighter"),         "lighter"          },
    {CompositeOperator::ARITHMETIC,       _("Arithmetic"),      "arithmetic"       }
    // clang-format on
};
const EnumDataConverter<CompositeOperator> CompositeOperatorConverter(CompositeOperatorData, (int)CompositeOperator::ENDOPERATOR);

// feComponentTransfer
const EnumData<ComponentTransferType> ComponentTransferTypeData[ComponentTransferType::ERROR] = {
    // clang-format off
    {ComponentTransferType::IDENTITY, _("Identity"), "identity"},
    {ComponentTransferType::TABLE,    _("Table Lookup"),    "table"},
    {ComponentTransferType::DISCRETE, _("Discrete Values"), "discrete"},
    {ComponentTransferType::LINEAR,   _("Linear"),   "linear"},
    {ComponentTransferType::GAMMA,    _("Gamma"),    "gamma"},
    // clang-format on
};
const EnumDataConverter<ComponentTransferType> ComponentTransferTypeConverter(ComponentTransferTypeData, ComponentTransferType::ERROR);

// feConvolveMatrix
const EnumData<ConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeData[ENUM_VALUE(ConvolveMatrixEdgeMode, ENDTYPE)] = {
    // clang-format off
    {ConvolveMatrixEdgeMode::DUPLICATE, _("Duplicate"), "duplicate"},
    {ConvolveMatrixEdgeMode::WRAP,      _("Wrap"),      "wrap"},
    {ConvolveMatrixEdgeMode::NONE,      C_("Convolve matrix, edge mode", "None"),      "none"}
    // clang-format on
};
const EnumDataConverter<ConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeConverter(ConvolveMatrixEdgeModeData, (int)ConvolveMatrixEdgeMode::ENDTYPE);

// feDisplacementMap
const EnumData<FilterDisplacementMapChannelSelector> DisplacementMapChannelData[DISPLACEMENTMAP_CHANNEL_ENDTYPE] = {
    // clang-format off
    {DISPLACEMENTMAP_CHANNEL_RED, _("Red"),   "R"},
    {DISPLACEMENTMAP_CHANNEL_GREEN, _("Green"), "G"},
    {DISPLACEMENTMAP_CHANNEL_BLUE, _("Blue"),  "B"},
    {DISPLACEMENTMAP_CHANNEL_ALPHA, _("Alpha"), "A"}
    // clang-format on
};
const EnumDataConverter<FilterDisplacementMapChannelSelector> DisplacementMapChannelConverter(DisplacementMapChannelData, DISPLACEMENTMAP_CHANNEL_ENDTYPE);

// feMorphology
const EnumData<MorphologyOperator> MorphologyOperatorData[ENUM_VALUE(MorphologyOperator, END)] = {
    // clang-format off
    {MorphologyOperator::ERODE,  _("Erode"),   "erode"},
    {MorphologyOperator::DILATE, _("Dilate"),  "dilate"}
    // clang-format on
};
const EnumDataConverter<MorphologyOperator> MorphologyOperatorConverter(MorphologyOperatorData, (int)MorphologyOperator::END);

// feTurbulence
const EnumData<TurbulenceType> TurbulenceTypeData[ENUM_VALUE(TurbulenceType, ENDTYPE)] = {
    // clang-format off
    {TurbulenceType::FRACTALNOISE, _("Fractal Noise"), "fractalNoise"},
    {TurbulenceType::TURBULENCE,   _("Turbulence"),    "turbulence"}
    // clang-format on
};
const EnumDataConverter<TurbulenceType> TurbulenceTypeConverter(TurbulenceTypeData, (int)TurbulenceType::ENDTYPE);

// Light source
const EnumData<LightSource> LightSourceData[LIGHT_ENDSOURCE] = {
    // clang-format off
    {LIGHT_DISTANT, _("Distant Light"), "svg:feDistantLight"},
    {LIGHT_POINT,   _("Point Light"),   "svg:fePointLight"},
    {LIGHT_SPOT,    _("Spot Light"),    "svg:feSpotLight"}
    // clang-format on
};
const EnumDataConverter<LightSource> LightSourceConverter(LightSourceData, LIGHT_ENDSOURCE);

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
