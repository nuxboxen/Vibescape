// SPDX-License-Identifier: GPL-2.0-or-later

// Low-level operations that modify SPItem paint/style/visibility properties.
// These are document-level helpers with no dependency on GTK or widget state.

#pragma once

#include <optional>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <glibmm/ustring.h>
#include <2geom/affine.h>
#include <2geom/point.h>
#include <2geom/transforms.h>

#include "colors/color.h"
#include "object/sp-gradient.h"
#include "style-enums.h"
#include "ui/widget/edit-operation.h"

class SPCSSAttr;
class SPDesktop;
class SPGradient;
class SPItem;
class SPDocument;
class SPHatch;
class SPPattern;


namespace Inkscape::Util {

class Unit;

// Delegate that receives all editing operations from PaintAttribute.
// The delegate owns/knows the target items.
class PaintEditDelegate {
public:
    struct CssOp       { boost::intrusive_ptr<SPCSSAttr> css; };
    struct GradientOp  { SPGradient* vector; SPGradientType type; bool fill; };
    struct PatternOp   { SPPattern* pattern; bool fill;
                         std::optional<Colors::Color> color; Glib::ustring label;
                         Geom::Affine transform; Geom::Point offset;
                         bool uniform_scale; Geom::Scale gap; };
    struct HatchOp     { SPHatch* hatch; bool fill;
                         std::optional<Colors::Color> color; Glib::ustring label;
                         Geom::Affine transform; Geom::Point offset;
                         double pitch; double rotation; double thickness; };
    struct MeshOp      { SPGradient* mesh; bool fill; };
    struct SwatchOp    { SPGradient* vector; Inkscape::UI::EditOperation op;
                         SPGradient* replacement; std::optional<Colors::Color> color;
                         Glib::ustring label; bool fill; };
    struct StrokeWidthOp { double width; bool hairline; const Unit* unit; };
    struct DashOp        { std::vector<double> dash; double offset; };
    struct OpacityOp     { double opacity; bool clear; };
    struct BlendModeOp   { SPBlendMode mode; bool clear; };
    struct VisibilityOp  { bool hidden; };

    using Op = std::variant<
        CssOp, GradientOp, PatternOp, HatchOp, MeshOp, SwatchOp,
        StrokeWidthOp, DashOp, OpacityOp, BlendModeOp, VisibilityOp>;

    virtual ~PaintEditDelegate() = default;
    virtual void set_desktop(SPDesktop*) {}
    virtual void apply(const Op& op) = 0;
};

// Create an empty CSS attribute object wrapped in an intrusive_ptr.
boost::intrusive_ptr<SPCSSAttr> new_css_attr();

// Apply css to item, scaling property values to compensate for item transform.
void set_item_style(SPItem* item, SPCSSAttr* css);

// Set stroke width on item (handles hairline flag and unit conversion).
void set_stroke_width(SPItem* item, double width, bool hairline, const Unit* unit);

// Apply a swatch editing operation (new / change / delete / rename) to item.
void swatch_operation(SPItem* item, SPGradient* vector, SPDesktop* desktop, bool fill,
                      Inkscape::UI::EditOperation op, SPGradient* replacement,
                      std::optional<Colors::Color> color, Glib::ustring label,
                      unsigned int tag);

// Apply a single paint edit operation to one item.
// Used by custom delegates that need to handle non-CSS ops for a specific item.
void apply_paint_op_to_item(SPItem* item, const PaintEditDelegate::Op& op,
                             SPDesktop* desktop, unsigned int tag);

} // namespace Inkscape::Util
