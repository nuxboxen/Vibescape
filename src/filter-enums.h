// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_INKSCAPE_FILTER_ENUMS_H
#define SEEN_INKSCAPE_FILTER_ENUMS_H

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

#include "renderer/drawing-filters/blend.h"
#include "renderer/drawing-filters/color-matrix.h"
#include "renderer/drawing-filters/component-transfer.h"
#include "renderer/drawing-filters/composite.h"
#include "renderer/drawing-filters/convolve-matrix.h"
#include "renderer/drawing-filters/morphology.h"
#include "renderer/drawing-filters/turbulence.h"
#include "renderer/drawing-filters/enums.h"
#include "object/filters/displacementmap.h"
#include "util/enums.h"

using namespace Inkscape::Renderer::DrawingFilter;
#define ENUM_VALUE(c, v) static_cast<typename std::underlying_type<c>::type>(c::v)

// Filter primitives
extern const Inkscape::Util::EnumData<PrimitiveType> FPData[ENUM_VALUE(PrimitiveType, ENDPRIMITIVETYPE)];
extern const Inkscape::Util::EnumDataConverter<PrimitiveType> FPConverter;

enum FilterPrimitiveInput {
    FPINPUT_SOURCEGRAPHIC,
    FPINPUT_SOURCEALPHA,
    FPINPUT_BACKGROUNDIMAGE,
    FPINPUT_BACKGROUNDALPHA,
    FPINPUT_FILLPAINT,
    FPINPUT_STROKEPAINT,
    FPINPUT_END
};

extern const Inkscape::Util::EnumData<FilterPrimitiveInput> FPInputData[FPINPUT_END];
extern const Inkscape::Util::EnumDataConverter<FilterPrimitiveInput> FPInputConverter;

// ColorMatrix type
extern const Inkscape::Util::EnumData<ColorMatrixType> ColorMatrixTypeData[ENUM_VALUE(ColorMatrixType, ENDTYPE)];
extern const Inkscape::Util::EnumDataConverter<ColorMatrixType> ColorMatrixTypeConverter;
// ComponentTransfer type
extern const Inkscape::Util::EnumData<ComponentTransferType> ComponentTransferTypeData[ENUM_VALUE(ComponentTransferType, ERROR)];
extern const Inkscape::Util::EnumDataConverter<ComponentTransferType> ComponentTransferTypeConverter;
// Composite operator
extern const Inkscape::Util::EnumData<CompositeOperator> CompositeOperatorData[ENUM_VALUE(CompositeOperator, ENDOPERATOR)];
extern const Inkscape::Util::EnumDataConverter<CompositeOperator> CompositeOperatorConverter;
// ConvolveMatrix edgeMode
extern const Inkscape::Util::EnumData<ConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeData[ENUM_VALUE(ConvolveMatrixEdgeMode, ENDTYPE)];
extern const Inkscape::Util::EnumDataConverter<ConvolveMatrixEdgeMode> ConvolveMatrixEdgeModeConverter;
// DisplacementMap channel
extern const Inkscape::Util::EnumData<FilterDisplacementMapChannelSelector> DisplacementMapChannelData[4];
extern const Inkscape::Util::EnumDataConverter<FilterDisplacementMapChannelSelector> DisplacementMapChannelConverter;
// Morphology operator
extern const Inkscape::Util::EnumData<MorphologyOperator> MorphologyOperatorData[ENUM_VALUE(MorphologyOperator, END)];
extern const Inkscape::Util::EnumDataConverter<MorphologyOperator> MorphologyOperatorConverter;
// Turbulence type
extern const Inkscape::Util::EnumData<TurbulenceType> TurbulenceTypeData[ENUM_VALUE(TurbulenceType, ENDTYPE)];
extern const Inkscape::Util::EnumDataConverter<TurbulenceType> TurbulenceTypeConverter;
// Lighting
enum LightSource {
    LIGHT_DISTANT,
    LIGHT_POINT,
    LIGHT_SPOT,
    LIGHT_ENDSOURCE
};
extern const Inkscape::Util::EnumData<LightSource> LightSourceData[LIGHT_ENDSOURCE];
extern const Inkscape::Util::EnumDataConverter<LightSource> LightSourceConverter;

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
