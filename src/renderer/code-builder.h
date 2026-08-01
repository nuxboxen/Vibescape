// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Construct code by printing out instructions.
 *
 * This code is not expected to be high quality, it's scratch code
 * for generating tests; Not production code.
 *//*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2026 Authors
 */

#ifndef SEEN_INKSCAPE_RENDERER_CODE_BUILDER_H
#define SEEN_INKSCAPE_RENDERER_CODE_BUILDER_H

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <typeindex>
#include <vector>
#include <2geom/curves.h>
#include <2geom/pathvector.h>
#include <2geom/point.h>
#include <2geom/rect.h>
#ifndef _WIN32
#include <execinfo.h>
#endif
#include <glibmm.h>
#include <stdio.h>
#include <stdlib.h>

#include "style.h"
#include "colors/spaces/base.h"
#include "object/sp-paint-server.h"
#include "object/sp-text.h"
#include "object/sp-tspan.h"
#include "libnrtype/font-factory.h"
#include "renderer/drawing-filters/filter.h"
#include "renderer/drawing-filters/gaussian-blur.h"

#define FE_0(WHAT, n, p)
#define FE_1(WHAT, n, p, X)      WHAT(n, p, X) 
#define FE_2(WHAT, n, p, X, ...) WHAT(n, p, X)FE_1(WHAT, n, p, __VA_ARGS__)
#define FE_3(WHAT, n, p, X, ...) WHAT(n, p, X)FE_2(WHAT, n, p, __VA_ARGS__)
#define FE_4(WHAT, n, p, X, ...) WHAT(n, p, X)FE_3(WHAT, n, p, __VA_ARGS__)
#define FE_5(WHAT, n, p, X, ...) WHAT(n, p, X)FE_4(WHAT, n, p, __VA_ARGS__)
#define GET_MACRO(_0,_1,_2,_3,_4,_5,NAME,...) NAME 
#define FOR_EACH(action,n,p,...) \
  GET_MACRO(_0,__VA_ARGS__,FE_5,FE_4,FE_3,FE_2,FE_1,FE_0)(action,n,p,__VA_ARGS__)

namespace Inkscape {

static std::vector<std::vector<std::string>> parse_callstack(char **syms, int count)
{
    std::vector<std::vector<std::string>> ret;
    for (auto i = 0; i < count; i++) {
        std::vector<std::string> names;
        std::string scratch;
        std::istringstream ss(syms[i]);
        // Look for gcc symbol string, ignore plain paths and anything else
        if (std::getline(ss, scratch, '(') && !ss.eof() && ss.get() == '_' && ss.get() == 'Z') {
            // Skip classifiers, const(K) and namespace(N) 
            while (!ss.eof() && (ss.peek() == 'K' || ss.peek() == 'N')) ss.get();
            while (!ss.eof() && !scratch.empty()) {
                scratch = "";
                int c = 0;
                for (ss >> c; c > 0; c--) {
                    scratch += ss.get();
                }
                if (!scratch.empty()) {
                    names.emplace_back(scratch);
                }
            }
        }
        if (!names.empty()) {
            ret.push_back(std::move(names));
        }
    }
    return ret;
}

/**
 * Use backtrace information to test if this caller came from inside
 * the display tree (self-call) or from outside (API call)
 */
static bool is_api_call(std::string const &ns, std::set<std::string> const &clss)
{
    bool ret = false;
#ifndef _WIN32
    void *frames[5];
    int count = backtrace(frames, 5);
    char **syms = backtrace_symbols(frames, count);
    if (count > 3) {
        auto names = parse_callstack(syms+3, 1);
        ret = !names.empty() && names[0].size() > 1 && names[0][0] == ns && std::find(clss.begin(), clss.end(), names[0][1]) != clss.end();
    }
    free(syms);
#endif
    return ret;
}

/**
 * Try to detect if a contractor call is a parent call or the original API call.
 */
static std::pair<std::string, std::string> get_inherited_constructor(std::vector<std::string> const &chlds)
{
#ifndef _WIN32
    void *frames[10];
    int count = backtrace(frames, 10);
    char **syms = backtrace_symbols(frames, count);
    auto stack = parse_callstack(syms + 2, std::min(count, 8));
    std::string this_call = stack[0][0];
    for (unsigned i = 1; i < stack[0].size(); i++) this_call += "::" + stack[0][i];

    if (stack.size() > 1 && stack[0][0] == stack[1][0] && std::find(chlds.begin(), chlds.end(), stack[1].back()) != chlds.end()) {
        std::string prev_call = stack[1][0];
        for (unsigned i = 1; i < stack[1].size(); i++) prev_call += "::" + stack[1][i];
        return {this_call, prev_call};
    }
    return {this_call, ""};
#else
    return {};
#endif
}

static std::string to_snake_case(const std::string &input) {
    std::string result = "";
    for (auto c : input) {
        if (!result.empty() && std::isupper(c)) result += '_';
        if (c == ':')
            result = "";
        else
            result += std::tolower(c);
    }
    return result;
}

class CodeBuilder
{
    using ObjectPtr = const void *;
public:
    // Singleton pattern
    static CodeBuilder &get()
    {
        static auto obj = CodeBuilder();
        return obj;
    }

    CodeBuilder &operator<<(const char *v) { std::cout << v; return *this; }
    CodeBuilder &operator<<(double v) { std::cout << v; return *this; }
    CodeBuilder &operator<<(int v) { std::cout << v; return *this; }
    CodeBuilder &operator<<(const void *v) { std::cout << "{" << v << "}"; return *this; }

    void line_start(std::string line, bool comment = false)
    {
        for (auto t = 0; t < tabs; t++) {
            std::cout << (t == 0 && (paused || comment) ? "//  " : "    ");
        }
        std::cout << line;
    }
    template <bool Uniform = false, typename T>
    void add_arg(T const &arg)
    {
        if constexpr (std::is_same<T, std::string>::value) {
            *this << "\"" << arg.c_str() << "\"";
        } else if constexpr (std::is_same<T, char>::value) {
            std::cout << "\'" << arg << "\'";
        } else if constexpr (std::is_same<T, bool>::value) {
            *this << (arg ? "true" : "false");
        } else if constexpr (std::is_same<T, Geom::Affine>::value) {
            InlineObject<false>("Geom::Affine") << arg[0] << arg[1] << arg[2] << arg[3] << arg[4] << arg[5];
        } else if constexpr (std::is_same<T, Geom::Point>::value) {
            InlineObject<Uniform>("Geom::Point") << arg[Geom::X] << arg[Geom::Y];
        } else if constexpr (std::is_same<T, Geom::IntRect>::value) {
            InlineObject<Uniform>("Geom::IntRect") << arg[Geom::X][0] << arg[Geom::Y][0] << arg[Geom::X][1] << arg[Geom::Y][1];
        } else if constexpr (std::is_same<T, Geom::Rect>::value) {
            InlineObject<Uniform>("Geom::Rect") << arg[Geom::X][0] << arg[Geom::Y][0] << arg[Geom::X][1] << arg[Geom::Y][1];
        } else if constexpr (std::is_same<T, Geom::LineSegment>::value) {
            InlineObject<false>("Geom::LineSegment").uniform(1) << arg[0] << arg[1];
        } else if constexpr (std::is_same<T, Geom::QuadraticBezier>::value) {
            InlineObject<false>("Geom::QuadraticBezier") << arg[0] << arg[1] << arg[2];
        } else if constexpr (std::is_same<T, Geom::CubicBezier>::value) {
            InlineObject<false>("Geom::CubicBezier") << arg[0] << arg[1] << arg[2] << arg[3];
        } else if constexpr (std::is_same<T, Geom::EllipticalArc>::value) {
            InlineObject<false>("Geom::EllipticalArc") << arg.initialPoint() << arg.rays() << (double)arg.rotationAngle() << arg.largeArc() << arg.sweep() << arg.finalPoint();
        } else if constexpr (std::is_same<T, Colors::Color>::value) {
            InlineObject<false>("Colors::Color") << *arg.getSpace() <<  arg.getValues();
        } else if constexpr (std::is_same<T, SVGLength>::value) {
            // This object shouldn't be used in the rendering engine at all, yet it is.
            InlineObject("SVGLength") << arg.write();
        } else if constexpr (std::is_same<T, SPBlendMode>::value) {
            switch (arg) {
                case SP_CSS_BLEND_NORMAL: *this << "SP_CSS_BLEND_NORMAL"; break;
                case SP_CSS_BLEND_MULTIPLY: *this << "SP_CSS_BLEND_MULTIPLY"; break;
                case SP_CSS_BLEND_SCREEN: *this << "SP_CSS_BLEND_SCREEN"; break;
                case SP_CSS_BLEND_DARKEN: *this << "SP_CSS_BLEND_DARKEN"; break;
                case SP_CSS_BLEND_LIGHTEN: *this << "SP_CSS_BLEND_LIGHTEN"; break;
                case SP_CSS_BLEND_OVERLAY: *this << "SP_CSS_BLEND_OVERLAY"; break;
                case SP_CSS_BLEND_COLORDODGE: *this << "SP_CSS_BLEND_COLORDODGE"; break;
                case SP_CSS_BLEND_COLORBURN: *this << "SP_CSS_BLEND_COLORBURN"; break;
                case SP_CSS_BLEND_HARDLIGHT: *this << "SP_CSS_BLEND_HARDLIGHT"; break;
                case SP_CSS_BLEND_SOFTLIGHT: *this << "SP_CSS_BLEND_SOFTLIGHT"; break;
                case SP_CSS_BLEND_DIFFERENCE: *this << "SP_CSS_BLEND_DIFFERENCE"; break;
                case SP_CSS_BLEND_EXCLUSION: *this << "SP_CSS_BLEND_EXCLUSION"; break;
                case SP_CSS_BLEND_HUE: *this << "SP_CSS_BLEND_HUE"; break;
                case SP_CSS_BLEND_SATURATION: *this << "SP_CSS_BLEND_SATURATION"; break;
                case SP_CSS_BLEND_COLOR: *this << "SP_CSS_BLEND_COLOR"; break;
                case SP_CSS_BLEND_LUMINOSITY: *this << "SP_CSS_BLEND_LUMINOSITY"; break;
            }
        } else if constexpr (std::is_same<T, Colors::Space::AnySpace>::value) {
            switch (arg.getType()) { // These are static variables in color-testbase.h
                case Colors::Space::Type::RGB:
                case Colors::Space::Type::CSSNAME:
                    *this << "rgb"; break;
                case Colors::Space::Type::linearRGB:
                    *this << "lrgb"; break;
                case Colors::Space::Type::CMYK:
                    *this << "cmyk_cpp"; break;
                case Colors::Space::Type::HSL:
                    *this << "hsl"; break;
                case Colors::Space::Type::OKLAB:
                    *this << "oklab"; break;
                case Colors::Space::Type::CMS:
                    // A static cmyk profile in color-testbase.h
                    *this << "cmyk_icc"; break;
                default:
                    *this << "unsupported_color_space"; break;
            }
        } else if constexpr (std::is_same<T, SPIFloat>::value
                          || std::is_same<T, SPILength>::value
                          || std::is_same<T, SPIScale24>::value) {
            *this << arg.as_double();
        } else if constexpr (std::is_same<T, SPIDashArray>::value) {
            InlineObject<true>("SPIDashArray") << arg.get_computed();
        } else if constexpr(std::is_same<T, SPPaintServer>::value) {
            auto s2 = InlineObject("std::make_unique<StyleMockSource::PaintMockSource::Href>");
            {
                switch (arg.getPaintType()) {
                    case PaintServerType::SOLID_COLOR:
                        InlineObject("PaintServerMockSource::make_color") << arg.getSolidColor();
                        break;
                    case PaintServerType::GROUP_PATTERN:
                        InlineObject("PaintServerMockSource::make_group_pattern");
                        break;
                    case PaintServerType::HATCH_PATTERN:
                        std::cerr << "Hatch pattern not implemented yet.\n";
                        break;
                    case PaintServerType::LINEAR_GRADIENT:
                    case PaintServerType::RADIAL_GRADIENT:
                        InlineObject("PaintServerMockSource::make_gradient") << (arg.getPaintType() == PaintServerType::LINEAR_GRADIENT)
                            << arg.getSpread() << arg.getUnits() << arg.getGradientTransform() << *arg.getGradientVector();
                        break;
                    case PaintServerType::MESH_GRADIENT:
                        InlineObject("PaintServerMockSource::make_mesh")
                            << arg.getGradientTransform()
                            << *arg.getGradientMesh();
                        break;
                    case PaintServerType::INVALID:
                        break;
                }
            }
        } else if constexpr (std::is_same<T, SPGradientMesh>::value) {
            auto s2 = InlineObject<false, true>("SPGradientMesh");
            s2.named("built", true);
            s2.named("rows", arg.rows);
            s2.named("cols", arg.cols);
            // mesh gradients that use bicubics will produce *thousands* of patches
            // and will crash the compiler. We can't handle this as data input like this.
            if (arg.patches.size() > 0 && arg.patches.size() <= 6) {
                if (arg.patches[0].size() > 0 && arg.patches[0].size() <= 6) {
                    s2.named("patches", arg.patches);
                }
            }
        } else if constexpr (std::is_same<T, SPGradientPatch>::value) {
            // missing tensorIsSet[4], tensorpoints[4];
            auto s2 = InlineObject<false, true>("SPGradientPatch");
            s2.named("points", std::vector<std::vector<Geom::Point>>{
                    std::vector<Geom::Point>(arg.points[0], arg.points[0]+4),
                    std::vector<Geom::Point>(arg.points[1], arg.points[1]+4),
                    std::vector<Geom::Point>(arg.points[2], arg.points[2]+4),
                    std::vector<Geom::Point>(arg.points[3], arg.points[3]+4),
                });
            s2.named("pathtype", std::vector<char>(arg.pathtype, arg.pathtype + 4));
            s2.named("color", std::vector<std::optional<Colors::Color>>(arg.color, arg.color + 4));
        } else if constexpr (std::is_same<T, SPGradientUnits>::value) {
            *this << (arg ? "SP_GRADIENT_UNITS_USERSPACEONUSE" : "SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX");
        } else if constexpr (std::is_same<T, SPGradientSpread>::value) {
            *this << (arg == SP_GRADIENT_SPREAD_REPEAT ? "SP_GRADIENT_SPREAD_REPEAT" :
                     (arg == SP_GRADIENT_SPREAD_REFLECT ? "SP_GRADIENT_SPREAD_REFLECT" : "SP_GRADIENT_SPREAD_PAD"));
        } else if constexpr (std::is_same<T, SPGradientVector>::value) {
            auto s2 = InlineObject<true, true>("SPGradientVector");
            s2.named("built", true);
            s2.named("stops", arg.stops);
            s2.named("geom", arg.geom);
        } else if constexpr (std::is_same<T, SPGradientStop>::value) {
            InlineObject("SPGradientStop") << *arg.color << arg.offset;
        } else if constexpr (std::is_same<T, SPIColor>::value) {
            add_arg(arg.getColor());
        } else if constexpr (std::is_same<T, SPIPaint>::value) {
            auto s2 = InlineObject<true, true>("SPIPaint");
            if (arg.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_FILL) {
                s2.named("paintOrigin", "SP_CSS_PAINT_ORIGIN_CONTEXT_FILL");
            } else if (arg.paintOrigin == SP_CSS_PAINT_ORIGIN_CONTEXT_STROKE) {
                s2.named("paintOrigin", "SP_CSS_PAINT_ORIGIN_CONTEXT_STROKE");
            } else if (arg.isPaintserver() && arg.href->getObject() && arg.href->getObject()->isValid()) {
                SPPaintServer* server = arg.href->getObject();
                s2.named("href", *server);
            } else if (arg.isColor()) {
                auto color = arg.getColor();
                s2.named("color", color);
            } else if (arg.isNone()) {
                s2.named("is_none", true);
            }
        } else if constexpr (std::is_same<T, SPIPaintOrder>::value) {
            auto s2 = InlineObject<true, true>("SPIPaintOrder");
            s2.named("layer", "{");
            bool start = true;
            for (auto v : arg.get_layers()) {
                if (!start) *this << ", ";
                switch (v) {
                    case SP_CSS_PAINT_ORDER_NORMAL: *this << "SP_CSS_PAINT_ORDER_NORMAL"; break;
                    case SP_CSS_PAINT_ORDER_FILL:   *this << "SP_CSS_PAINT_ORDER_FILL";   break;
                    case SP_CSS_PAINT_ORDER_STROKE: *this << "SP_CSS_PAINT_ORDER_STROKE"; break;
                    case SP_CSS_PAINT_ORDER_MARKER: *this << "SP_CSS_PAINT_ORDER_MARKER"; break;
                }
                start = false;
            }
            *this << "}";
        } else if constexpr (std::is_same<T, PangoFontDescription>::value) {
            add_arg(std::string(pango_font_description_to_string(&arg)));
        } else if constexpr (std::is_fundamental<T>::value
                          || std::is_array<T>::value
                          || std::is_same<T, SPIFontSize>::value) {
            std::cout << arg;
        } else {
            *this << as_object(arg).c_str();
        }
    }

    template <typename T>
    std::string as_object(T const &arg, std::optional<std::string> prefix = {})
    {
        ObjectPtr addr;
        if constexpr (std::is_pointer<T>::value) {
            if (arg == nullptr) return "nullptr";
            addr = arg;
        } else {
            addr = &arg;
        }
        auto iter = _constructs.find(addr);
        if (iter == _constructs.end()) {
            std::ostringstream s;
            s << "/* missing_object " << addr << " */";
            return s.str();
        }
        auto &c = iter->second;

        if (!prefix) {
            if constexpr (std::is_pointer<T>::value) {
                prefix = (c.is_ptr ? (c.is_smartptr ? "&*" : "") : "&");
            } else {
                prefix = (c.is_ptr ? "*" : "");
            }
        }
        auto paren = prefix->find('(') != std::string::npos;
        return *prefix + c.var_name.c_str() + (paren ? ")" : "");
    }

    // A tabbed block with an opening and closing brace
    struct Block
    {
        Block() { get().line_start("{\n"); get().tabs++; }
        ~Block() { get().tabs--; get().line_start("}\n"); }
        Block(Block &&b) = delete;
    };
    // A generic argument list
    template <bool Dict = false>
    struct ArgList
    {
        void _end_prev() { if (!start) get() << "," << (Dict ? "" : " "); start = false; }
        void _start_next(std::string const &name) { get() << "\n"; get().line_start("." + name + " = "); }

        template<typename T>
        ArgList &operator<<(std::optional<T> const &obj) {
            if (enabled) {
                if (obj) *this << *obj; else *this << "{}";
            }
            return *this;
        }
        template<typename T>
        ArgList &operator<<(std::vector<T> const &obj) {
            if (enabled) {
                if constexpr (!Dict) _end_prev();
                auto s = InlineObject<true>("");
                for (auto v : obj)
                    s << v;
            }
            return *this;
        }

        template<typename T>
        ArgList &operator<<(T const &obj) {
            if (enabled) {
                if constexpr (!Dict) _end_prev();
                if (uniform_after > 0) {
                    get().add_arg<false>(obj);
                    uniform_after--;
                } else {
                    get().add_arg<true>(obj);
                }
            }
            return *this;
        }
        template<typename T>
        void named(std::string const &name, T const &obj)
            requires (Dict) {
            if (enabled) {
                _end_prev();
                _start_next(name);
                *this << obj;
            }
        }
        ArgList &uniform(int after = 0) {
            uniform_after = after;
            return *this;
        }
        int uniform_after = 0;
        bool start = true;
        bool enabled = true;
    };
    // A constructed inline object
    template <bool Uniform = false, bool Dict = false>
    struct InlineObject : ArgList<Dict>
    {
        InlineObject(std::string name) {
            get().tabs++;
            if constexpr (Uniform) {
                get() << "{";
            } else {
                get() << name.c_str() << (Dict ? "({" : "(");
            }
        }
        ~InlineObject() {
            get().tabs--;
            if constexpr (Dict) {
                get() << "\n";
                get().line_start(!Uniform ? "})" : "}");
            } else {
                get() << (Uniform ? "}" : ")");
            }
        }
    };
    struct ConstructData {
        std::string var_name;
        bool is_ptr;
        bool is_smartptr = false;
    };
    // A tracked object as part of the API
    template <bool Uniform = false, bool Dict = Uniform>
    struct Construct : ArgList<Uniform>
    {
        std::string _end_bracket;

        template <typename T>
        Construct(T const &obj, std::string cls, std::optional<std::string> namor = {}, std::optional<std::string> smart_ptr = {}, std::vector<std::string> chlds = {}, bool overwrite = false) {
            auto &t = typeid(T);
            ObjectPtr addr = &obj;
            bool is_ptr = (std::is_pointer<T>::value || (bool)smart_ptr) && !Uniform;
            get()._clss.insert(cls);

            auto [this_call, that_call] = get_inherited_constructor(chlds);
            //std::cout << "// Construct " << addr << " (" << this_call << ", " << that_call << ")\n";
            if (!that_call.empty()) {
                // Constructed by parent, log it for later
                get().prior_calls.emplace_back(that_call, addr);
                this->enabled = false;
            } else {
                if (overwrite) get()._index[std::type_index(t)].overwrite(addr);
                auto var_index = std::to_string(get()._index[std::type_index(t)][addr]);
                auto var_name = (namor ? *namor : to_snake_case(cls)) + var_index;

                // Add every prior call's object address using the same name
                if (get()._constructs.find(addr) == get()._constructs.end() || overwrite) {
                    // Add memory of object so calls can use it as arg or this
                    //std::cout << "// Remember " << addr << " is " << var_name << "\n";
                    get()._constructs[addr] = {var_name, is_ptr, (bool)smart_ptr};

                    // Add constructor to code output
                    _end_bracket = ")";
                    if constexpr (Uniform) {
                        get().line_start(std::string(cls)+ " " + var_name + " = {");
                        _end_bracket = "}";
                        if constexpr (Dict) {
                            get().tabs++;
                        }
                    } else if (smart_ptr) {
                        get().line_start("auto " + var_name + " = " + *smart_ptr + "<" + cls + ">(");
                    } else {
                        get().line_start("auto " + var_name + " = " + (is_ptr ? "new " : "") + cls + "(");
                    }
                } else {
                    this->enabled = false;
                }

                if (!get().prior_calls.empty() && get().prior_calls.back().first == this_call) {
                    for (auto &[other_call, other_addr] : get().prior_calls) {
                        get()._constructs[other_addr] = {var_name, is_ptr, (bool)smart_ptr};
                    }
                }
                get().prior_calls = {}; // Empty stack memory, not thread safe
            }
        }
        ~Construct() {
            if (this->enabled) {
                if constexpr (Dict) {
                    get().tabs--;
                    get() << "\n";
                    get().line_start("");
                }
                get() << _end_bracket.c_str() << ";\n";
            }
        }
    };
    template <typename T>
    void maybeConstruct(T const &obj, bool force = false)
    {
        ObjectPtr addr = &obj;
        if (get()._constructs.find(addr) == get()._constructs.end() || force) {
            _populate(obj);
        }
    }
    // Expand the given object by adding data to it
    void _populate(Geom::PathVector const &pv)
    {
        Construct(pv, "Geom::PathVector", "pv", "std::make_shared");
        for (auto &path : pv) {
            auto block = Block();
            Construct(path, "Geom::Path", "path", "std::make_shared");
            for (auto &curve : path) {
                if (auto l = dynamic_cast<Geom::LineSegment const *>(&curve)) {
                    Call(path, "append", true) << *l;
                } else if (auto q = dynamic_cast<Geom::QuadraticBezier const *>(&curve)) {
                    Call(path, "append", true) << *q;
                } else if (auto c = dynamic_cast<Geom::CubicBezier const *>(&curve)) {
                    Call(path, "append", true) << *c;
                } else if (auto arc = dynamic_cast<Geom::EllipticalArc const *>(&curve)) {
                    Call(path, "append", true) << *arc;
                } else { // SBaisCurve
                    get().line_start("Error!, No SBasis support\n", true);
                }
            }
            if (path.closed()) {
                Call(path, "close");
            }
            Call(pv, "push_back", true).shared_obj(path, "*");
        }
    }
    void _populate(SPStyle const &style) {
        auto s = Construct<true>(style, "StyleMockSource", "style");

#define ADD_ARG(name, type, prop) s.named<type>(name, prop)
#define _ADD_ENUM(name, prop, e) if(prop.as_enum() == e) s.named(name, #e);
#define ADD_ENUM(name, prop, def,...) if (prop.as_enum() != def) FOR_EACH(_ADD_ENUM, name, prop, __VA_ARGS__)
#define ADD_DBL(name, type, prop, def) if (prop.as_double() != def) ADD_ARG(name, type, prop)
#define ADD_PAINT(name, prop) if (prop.isNoneSet() || !prop.isNone()) ADD_ARG(name, SPIPaint, prop)

        bool has_stroke = !style.stroke.isNone() && style.stroke_opacity.as_double() > 0.0 && style.stroke_width.as_double() > 0.0;
        bool has_fill   = !style.fill.isNone() && style.fill_opacity.as_double() > 0.0;
        bool has_marker = style.marker.set || style.marker_start.set || style.marker_mid.set || style.marker_end.set;

        ADD_DBL  ("opacity",      SPIScale24, style.opacity,      1.0);
        ADD_PAINT("fill",                     style.fill);
        ADD_DBL  ("fill_opacity", SPIScale24, style.fill_opacity, 1.0);

        if (has_stroke + has_fill + has_marker > 1) {
            if (style.paint_order.get_layers() != std::array<SPPaintOrderLayer, 3>{SP_CSS_PAINT_ORDER_FILL, SP_CSS_PAINT_ORDER_STROKE, SP_CSS_PAINT_ORDER_MARKER})
                ADD_ARG("paint_order",     SPIPaintOrder, style.paint_order);
        }

        if (auto space = style.color_interpolation.getInterpolationSpace())
            ADD_ARG("color_interpolation", Colors::Space::AnySpace, *space);

        if (has_stroke) {
            ADD_PAINT("stroke",                            style.stroke);
            ADD_DBL  ("stroke_opacity",     SPIScale24,    style.stroke_opacity,    1.0);
            ADD_DBL  ("stroke_width",       SPILength,     style.stroke_width,      1.0);
            ADD_DBL  ("stroke_miterlimit",  SPIFloat,      style.stroke_miterlimit, 4.0);
            if (style.stroke_dasharray.set && style.stroke_dasharray.values.size() > 0) {
                ADD_DBL("stroke_dashoffset", SPILength,    style.stroke_dashoffset, 0.0);
                ADD_ARG("stroke_dasharray",  SPIDashArray, style.stroke_dasharray);
            }
            ADD_ENUM ("stroke_linejoin",                   style.stroke_linejoin, SP_STROKE_LINEJOIN_MITER, SP_STROKE_LINEJOIN_ROUND, SP_STROKE_LINEJOIN_BEVEL);
            ADD_ENUM ("stroke_linecap",                    style.stroke_linecap,  SP_STROKE_LINECAP_BUTT, SP_STROKE_LINECAP_ROUND, SP_STROKE_LINECAP_SQUARE);
        }

        if (is<SPText>(style.object) || is<SPTSpan>(style.object)) {
            ADD_ARG("font_size",                SPIFontSize, style.font_size);
            ADD_PAINT("text_decoration_fill",                style.text_decoration_fill);
            ADD_PAINT("text_decoration_stroke",              style.text_decoration_stroke);
            //ADD_ARG("text_decoration_color",  SPIColor,    style.text_decoration_color);
            ADD_ENUM("direction",                            style.direction, SP_CSS_DIRECTION_LTR, SP_CSS_DIRECTION_RTL);
        }

        ADD_ENUM("fill_rule",         style.fill_rule,         SP_WIND_RULE_NONZERO, SP_WIND_RULE_EVENODD);
        ADD_ENUM("clip_rule",         style.clip_rule,         SP_WIND_RULE_NONZERO, SP_WIND_RULE_EVENODD);
        ADD_ENUM("image_rendering",   style.image_rendering,   SP_CSS_IMAGE_RENDERING_AUTO, SP_CSS_IMAGE_RENDERING_OPTIMIZESPEED,
                                                               SP_CSS_IMAGE_RENDERING_OPTIMIZEQUALITY, SP_CSS_IMAGE_RENDERING_CRISPEDGES,
                                                               SP_CSS_IMAGE_RENDERING_PIXELATED);
        ADD_ENUM("enable_background", style.enable_background, SP_CSS_BACKGROUND_ACCUMULATE, SP_CSS_BACKGROUND_NEW);
        //ADD_ARG("stroke_extensions", SPIStrokeExtensions, style.stroke_extensions);
    }
    void _populate(Cairo::Surface const &s)
    {
        static int image_count = 0;
        auto prefix = Glib::getenv("IMAGE_PREFIX");
        std::string filename = prefix + "-" + std::to_string(image_count++) + ".png";
        const_cast<Cairo::Surface *>(&s)->write_to_png(filename);
        Construct(s, "Renderer::Surface", "image_surface", "std::make_shared", {}, true) << filename;
    }
    /*
    void _populate(Renderer::DrawingFilter::Filter const &f)
    {
        for (auto &p : f.get_primitives()) {
            maybeConstruct(*p);
        }
        Construct(f, "DrawingFilter::Filter", "filter", "std::make_unique");
        Call(f, "set_region") << f._region_x << f._region_y
                              << f._region_width << f._region_height;
        if (f._x_pixels > 0.0)
            Call(f, "set_resolution") << f._x_pixels << f._y_pixels;

        f._code_build = true; // Track any future changes

        for (auto &p : f.primitives) {
            Call(f, "add_primitive").shared_obj(*p, "std::move(");
        }
    }
    void _populate(Filters::FilterPrimitive const &fp) {
        if (auto blur = dynamic_cast<Filters::FilterGaussian const *>(&fp)) {
            Construct(*blur, "Renderer::DrawingFilter::GaussianBlur", "blur", "std::make_unique").uniform(1) << blur->get_deviation();
            Call(*blur, "set_input") << blur->get_input();
            Call(*blur, "set_output") << blur->get_output();
        }
    }
    */
    struct Call : ArgList<false>
    {
        template <typename T>
        Call(T const &obj, std::string const &fn, bool internal = false) {
            enabled = internal || !is_api_call("Inkscape", get()._clss);
            ObjectPtr addr = &obj;
            auto iter = get()._constructs.find(addr);
            if (iter == get()._constructs.end()) {
                get().line_start("", true);
                get() << addr << "." << fn.c_str() << "(";
                enabled = true;
            } else if (enabled) {
                auto &con = iter->second;
                get().line_start(con.var_name + (con.is_ptr ? "->" : ".") + fn + "(");
            }
        }
        template <typename T>
        void shared_obj(T const &arg, std::string prefix)
        { 
            get() << get().as_object(arg, prefix).c_str();
        }
        ~Call() {
            if (enabled) get() << ");\n";
        }
    };
    CodeBuilder() = default;

    int tabs = 1;
    bool paused = false;

    // This tracks objects by an index based on when we first see them
    struct incrementing_map : public std::map<ObjectPtr, int>
    {
        int operator[](ObjectPtr addr)
        {
            if (find(addr) == end()) {
                emplace(addr, size() + offset);
            }
            return find(addr)->second;
        }
        void overwrite(ObjectPtr addr)
        {
            offset += erase(addr);
        }
        int offset = 0;
    };
    std::set<std::string> _clss;
    std::map<std::type_index, incrementing_map> _index;
    std::map<ObjectPtr, ConstructData> _constructs;
    std::vector<std::pair<std::string, ObjectPtr>> prior_calls;
};

} // namespace Inkscape::Renderer

#endif // SEEN_INKSCAPE_RENDERER_CODE_BUILDER_H

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
