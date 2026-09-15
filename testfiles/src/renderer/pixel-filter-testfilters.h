// SPDX-License-Identifier: GPL-2.0-or-later
/***** TEST FILTERS *******/
  
#ifndef INKSCAPE_TEST_RENDERER_TESTFILTERS_H
#define INKSCAPE_TEST_RENDERER_TESTFILTERS_H

#include <2geom/rect.h>
#include <2geom/transforms.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

#include <iostream>
#include <bitset>

#include <termios.h>
#include <unistd.h>

/**
 * Return a single pixel color for testing.
 */
struct SampleColor
{
    int x, y;

    template <typename AccessSrc>
    std::vector<double> filter(AccessSrc const &src) const
    {
        auto color = src.colorAt(x, y, true);
        return {color.begin(), color.end()};
    }
};

/**
 * Build a list of pixels which will be set into a surface when the
 * filter is run. Allows creating testing textures.
 *
 * All colors are NOT alpha pre-multiplied.
 */
template <int channels = 4>
struct SetPixels
{
    std::vector<std::tuple<int, int, std::array<double, channels>>> _pixels;

    void pixelWillBe(int x, int y, std::array<double, channels> color)
    {
        _pixels.emplace_back(x, y, color);
    }

    template <typename Access>
    void filter(Access &surface) const
    {
        for (auto &[x, y, color] : _pixels) {
            typename Access::Color out;
            for (auto c = 0; c < out.size() && c < color.size(); c++) {
                out[c] = color[c];
            }
            surface.colorTo(x, y, out, true);
        }
    }  
};

struct ClearPixels
{
    template <typename Access>
    void filter(Access &surface) const
    {
        typename Access::Color blank;
        for (auto y = 0; y < surface.height(); y++) {
            for (auto x = 0; x < surface.width(); x++) {
                surface.colorTo(x, y, blank);
            }
        }

    }
};

/**
 * Construct a string reprentation of the image pixels.
 */
class PatchResult : public std::string
{
public:
    unsigned _stride;
    unsigned delta;

    PatchResult(std::string const &in, unsigned stride, unsigned delta = 0)
        : std::string(in)
        , _stride(stride)
        , delta(delta)
    {}

    /**
     * Format a string into a character image for test output when failing.
     */
    friend void PrintTo(const PatchResult &obj, std::ostream *oo)
    {
        obj.pretty_print(oo);
    }
    friend std::ostream& operator<<(std::ostream& os, PatchResult const &p) {
        return os << p.pretty_print();
    }
    std::string pretty_print() const
    {
        std::stringstream output;
        pretty_print(&output);
        return output.str();
    }
    void pretty_print(std::ostream *oo) const
    {
        for (unsigned c = 0; c < size(); c++) {
            if (_stride == 0 || c % _stride == 0) {
                if (c) 
                    *oo << "\"";
                *oo << "\n    \"";
            }
            *oo << (*this)[c];
        }   
        *oo << "\"\n";
    }
    PatchResult diff(PatchResult const &other) const
    {
        int delta = 0;
        std::ostringstream diff;
        for (unsigned c = 0; c < size() || c < other.size(); c++) {
            if (c >= size() || c >= other.size() || (*this)[c] != other[c]) {
               delta++;
                diff << "X";
            } else if ((*this)[c] == ' ') {
                diff << " ";
            } else {
                diff << "-";
            }
        }
        return PatchResult(diff.str(), _stride, delta);
    }
};

template <int Channels>
struct TestColor : std::array<double, Channels>
{
    explicit operator bool() const { return enabled; }
    bool is_background = false;
    bool enabled = false;

    bool operator==(TestColor<Channels> const &other) const { return isNear(other); }
    double howNear(TestColor<Channels> const &c) const {
        if (!c.enabled || !enabled)
            return 1.0;
        double tot = 0.0;
        for (auto i = 0; i < Channels; i++) {
            tot += std::abs((*this)[i] - c[i]);
        }
        return tot;
    }
    double howFar(TestColor<Channels> const &c) const { return 1 - std::clamp(howNear(c), 0.0, 1.0); }
    bool isNear(TestColor<Channels> const &c, double e = 1.0 / 25) const { return howNear(c) < e; }
    bool isDark() const { return (*this)[0] + (*this)[1] + (*this)[2] < 1.5; }
};

template <int Channels>
struct ColorWeight
{
    double weight = 0.0;
    TestColor<Channels> color;
};

template <int Channels>
struct ColorAvg
{
    explicit operator bool() const { return size > 0; }
    bool operator==(ColorAvg const &other) const { return isNear(other); }
    ColorAvg operator+(const ColorAvg<Channels>& other) const {
        // Flatten the average to a single sample building a new color
        ColorAvg<Channels> ret(1, color, alpha);
        ret.add(other.color);
        return ret;
    }
    void add(std::array<double, 3> const &c)
    {
        for (auto i = 0; i < Channels; i++) {
            color[i] = ((color[i] * size) + c[i]) / (size + 1);
        }
        size += 1;
        color.enabled = true;
    }
    void add(std::array<double, 4> const &c)
    {
        // Blend with background color before average is constructed
        double alpha_bg = 1 - c[3];
        if (c[3] > 0.0) {
            add(std::array<double, 3>{
                c[0] + alpha[0] * alpha_bg,
                c[1] + alpha[1] * alpha_bg,
                c[2] + alpha[2] * alpha_bg,
            });
        } else {
            add(alpha);
        }
    }

    bool isNear(ColorAvg const &a) const { return color.isNear(a.color); }
    void addIfNear(TestColor<Channels> const &c) { if (isNear(c)) add(c); }

    int size = 0;
    TestColor<Channels> color;
    TestColor<Channels> alpha;

    ColorWeight<Channels> weigh() const {
        return {1.0, color};
    }
    ColorWeight<Channels> weigh(ColorAvg<Channels> const &b) const {
        return {color.howFar(b.color), (*this + b).color};
    }
    ColorWeight<Channels> weigh(ColorAvg<Channels> const &b, ColorAvg<Channels> const &c) const {
        return {(color.howFar(b.color) + b.color.howFar(c.color) + c.color.howFar(color)) / 3, (*this + b + c).color};
    }
};
std::ostream &operator<<(std::ostream &out, const TestColor<3> &color)
{
    if (color) {
        out << "\x1B[" << (color.is_background ? 48 : 38) << ";2;"
                       << (int)(std::clamp(color[0], 0.0, 1.0) * 255) << ";"
                       << (int)(std::clamp(color[1], 0.0, 1.0) * 255) << ";"
                       << (int)(std::clamp(color[2], 0.0, 1.0) * 255) << "m";
    }
    return out;
}


struct PixelPaint
{
    static std::pair<int, int> get_samples_for_screen(int width, int height)
    {
        int ideal_width = 100;
        int horz = std::ceil(width / (double)ideal_width);
        // Vertical resolution is always half horz because glyph aspect ratio
        return {horz, horz*2};
    }
    static std::tuple<TestColor<3>, TestColor<3>, std::string> get_pixel(std::array<std::array<ColorAvg<3>, 2>, 2> const &in)
    {
        std::string block = "X";
        double weight = 0.0;
        TestColor<3> bg;
        TestColor<3> fg;

        auto select_result = [&block, &weight, &bg, &fg] (std::string c1, std::string c2, ColorWeight<3> a, ColorWeight<3> b) {
            auto w = a.weight + b.weight;
            if (w > weight) {
                weight = w;
                if (a.color.isDark() && !b.color.isDark()) {
                    block = c1;
                    bg = a.color;
                    fg = b.color;
                } else {
                    block = c2;
                    bg = b.color;
                    fg = a.color;
                }
            }
        };

        if (in[0][0] == in[1][0] && in[0][0] == in[1][1] && in[0][0] == in[0][1]) {
            auto c = (in[0][0] + in[1][0] + in[0][1] + in[1][1]).color;
            if (c.isDark()) {
                bg = c;
                block = " ";
            } else {
                fg = c;
                block = "█";
            }
        } else {
            select_result("▘", "▟", in[1][1].weigh(   in[0][1],      in[1][0]),     in[0][0].weigh());
            select_result("▝", "▙", in[1][1].weigh(   in[0][1],      in[0][0]),     in[1][0].weigh());
            select_result("▀", "▄", in[1][1].weigh(   in[0][1]),     in[0][0].weigh(in[1][0]));
            select_result("▖", "▜", in[1][1].weigh(   in[1][0],      in[0][0]),     in[0][1].weigh());
            select_result("▌", "▐", in[1][1].weigh(   in[1][0]),     in[0][1].weigh(in[0][0]));
            select_result("▞", "▚", in[1][1].weigh(   in[0][0]),     in[1][0].weigh(in[0][1]));
            select_result("▛", "▗", in[1][1].weigh(), in[0][0].weigh(in[1][0],      in[0][1]));
        }
        bg.is_background = true;
        fg.is_background = false;
        return {bg, fg, block};
    }

    template<typename Access>
    void filter(Access const &src) const
    {
        auto [samples_x, samples_y] = get_samples_for_screen(src.width(), src.height());
        TestColor<3> prev_bg;
        TestColor<3> prev_fg;
        TestColor<3> alpha_background = get_alpha_color();

        bool is_terminal = isatty(STDOUT_FILENO);

        auto irec = Geom::IntRect(0, 0, src.width(), src.height());
        for (int y = irec.top(); y < irec.bottom(); y+=samples_y) {
            for (int x = irec.left(); x < irec.right(); x+=samples_x) {
                std::array<std::array<ColorAvg<3>, 2>, 2> avg;
                avg[0][0].alpha = alpha_background;
                avg[1][0].alpha = alpha_background;
                avg[0][1].alpha = alpha_background;
                avg[1][1].alpha = alpha_background;

                for (int sy = y; sy < y + samples_y && sy < irec.bottom(); sy++) {
                    for (int sx = x; sx < x + samples_x && sx < irec.right(); sx++) {
                        int cx = std::round((double)(sx - x) / (double)samples_x);
                        int cy = std::round((double)(sy - y) / (double)samples_y);
                        if constexpr (Access::channel_total == 4) {
                            avg[cx][cy].add(src.colorAt(sx, sy));
                        } else {
                            std::cerr << "Invalid size of colors\n";
                            return;
                        }
                    }
                }
                // Sample was actually really small X is 1 char per px
                if (!avg[1][0]) avg[1][0] = avg[0][0];
                if (!avg[0][1]) avg[0][1].add(alpha_background);
                if (!avg[1][1]) avg[1][1] = avg[0][1];

                auto res = get_pixel(avg);
                if (!prev_bg || std::get<0>(res) != prev_bg) {
                    prev_bg = std::get<0>(res);
                    std::cout << prev_bg;
                }
                if (!prev_fg || std::get<1>(res) != prev_fg) {
                    prev_fg = std::get<1>(res);
                    std::cout << prev_fg;
                }
                std::cout << std::get<2>(res);
            }
            std::cout << "\x1B[0m\x1B[49m\n";
            prev_bg = TestColor<3>();
            prev_fg = TestColor<3>();
        }
    }
    static void set_raw_mode(bool enable) {
        static struct termios oldt;
        struct termios newt;
        if (enable) {
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical and echo modes
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        } else {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        }
    }

    static TestColor<3> get_alpha_color() {
        set_raw_mode(true);
        std::cout << "\033]11;?\007" << std::flush;
        char c;
        bool start = false;
        std::string response = "";
        while (read(STDIN_FILENO, &c, 1) == 1 && c != '\007') {
            if (start) {
                response += c;
            } else {
                start = c == ':';
            }
        }
        set_raw_mode(false);
        if (response.size() == 14) {
            return TestColor<3>{
                (double)std::stoi(response.substr(0, 4), nullptr, 16) / 0x10000,
                (double)std::stoi(response.substr(5, 4), nullptr, 16) / 0x10000,
                (double)std::stoi(response.substr(10, 4), nullptr, 16) / 0x10000
            };
        }
        return TestColor<3>{1.0, 1.0, 1.0};
    }
};

struct PixelPatch
{
    enum class Method
    {
        ALPHA,
        COLORS,
        LIGHT
    };

    Method _method;
    unsigned _patch_x = 3;
    unsigned _patch_y = 3;
    bool _alpha_unmultiplied = true;
    bool _use_float_coords = false;
    Geom::OptRect _clip;

    // Find a reasonable resolution for the patch with this many pixels
    static std::pair<int, int> get_patch_scale(int width, int height)
    {
        int ideal_width = 80 - 2 - 4;
        int horz = std::ceil(width / (double)ideal_width);
        // Vertical resolution is always half horz because glyph aspect ratio
        return {horz, horz * 2};
    }
    template<typename Access>
    PatchResult filter(Access const &src) const
    {
        static std::vector<unsigned char> const weights = {' ', ' ', ' ', '.', '.', '.', ':', ':', '-',
                                                           '+', '=', 'o', 'O', '*', 'x', 'X', '$', '&'};

        double size = _patch_x * _patch_y;
        char r0 = (_method == Method::ALPHA) ? 0x40 : 0x30;

        auto rect = Geom::Rect(0, 0, src.width(), src.height());
        if (_clip) {
            if (auto res = rect & *_clip) {
                rect = *res;
            } else {
                assert("Clipping pixel patch resulted in zero sized sample.");
            }
        }
        auto irec = *(rect * Geom::Scale(_patch_x, _patch_y).inverse()).roundInwards();

        // We collect a grid of pixels into a patch so it can be shown as test output
        std::stringstream output;
        for (int y = irec.top(); y < irec.bottom(); y++) {
            for (int x = irec.left(); x < irec.right(); x++) {
                // inital values
                typename Access::Color colors;
                typename Access::Color lights;
                colors.fill(0.0);
                lights.fill(0.0);
  
                for (unsigned cy = 0; cy < _patch_x; cy++) {
                    for (unsigned cx = 0; cx < _patch_x; cx++) {
                        int tx = x * _patch_x + cx;
                        int ty = y * _patch_y + cy;
                        if (_method == Method::ALPHA) {
                            lights.back() += _use_float_coords ? src.alphaAt((float)tx, (float)ty) : src.alphaAt(tx, ty);
                        } else {
                            auto color = _use_float_coords ? src.colorAt((float)tx, (float)ty, _alpha_unmultiplied) : src.colorAt(tx, ty, _alpha_unmultiplied);
                            for (int c = 0; c < colors.size(); c++) {
                                colors[c] += color[c] > 0.5;
                                lights[c] += color[c];
                            }
                        }
                    }
                }
                unsigned char r = r0;
                double light = 0.0;
                for (unsigned c = 0; c < colors.size() - 1; c++) {
                    r += (unsigned char)(((colors[c] / size > 0.3) + (colors[c] / size > 0.6)) << (c * 2));
                    light += lights[c] / lights.size() / size;
                }
                switch (_method) {
                    case Method::ALPHA:
                        r = weights[(int)(lights.back() / size * (weights.size() - 1))];
                        break;
                    case Method::LIGHT:
                        r = weights[(int)(std::clamp(light, 0.0, 1.0) * (weights.size() - 1))];
                        break;
                }
                // Map zero to space for readability
                if (r == r0) {
                    r = lights.back() / size > 0.3 ? '.' : ' ';
                }
                // Cap anything higher than ascii
                while (r > 'z') {
                    r -= ('z' - r0);
                }
                output << r;
            }
        }
        return PatchResult(output.str(), irec.width());
    }
};

#endif // INKSCAPE_TEST_RENDERER_TESTFILTERS_H

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
