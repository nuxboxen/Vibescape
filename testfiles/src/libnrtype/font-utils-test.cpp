// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Unit test for the font utils
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "libnrtype/font-utils.h"

#include <glibmm/init.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "util/statics.h"

namespace {

std::vector<std::pair<Glib::ustring, Glib::ustring>> data = {
    {"Thin @wght=100",               "Thin"},
    {"Ultra-Light @wght=200",        "Ultra-Light"},
    {"Light @wght=300",              "Light"},
    {"Medium @wght=500",             "Medium"},
    {"Semi-Bold @wght=600",          "Semi-Bold"},
    {"Bold @wght=700",               "Bold"},
    {"Ultra-Bold @wght=800",         "Ultra-Bold"},
    {"Heavy @wght=900",              "Heavy"},
    {"Thin Italic @wght=100",        "Thin Italic"},
    {"Ultra-Light Italic @wght=200", "Ultra-Light Italic"},
    {"Light Italic @wght=300",       "Light Italic"},
    {"Medium Italic @wght=500",      "Medium Italic"},
    {"Semi-Bold Italic @wght=600",   "Semi-Bold Italic"},
    {"Bold Italic @wght=700",        "Bold Italic"},
    {"Ultra-Bold Italic @wght=800",  "Ultra-Bold Italic"},
    {"Heavy Italic @wght=900",       "Heavy Italic"},

    {"Cascadia Code, Ultra-Light @wght=200",        "Cascadia Code Ultra-Light"},
    {"Cascadia Code, Ultra-Light Italic @wght=200", "Cascadia Code Ultra-Light Italic"},
    {"Cascadia Code, Light @wght=300",              "Cascadia Code Light"},
    {"Cascadia Code, Light Italic @wght=300",       "Cascadia Code Light Italic"},
    {"Cascadia Code, Normal",                       "Cascadia Code"},
    {"Cascadia Code, Italic",                       "Cascadia Code Italic"},
    {"Cascadia Code, Semi-Bold @wght=600",          "Cascadia Code Semi-Bold"},
    {"Cascadia Code, Semi-Bold Italic @wght=600",   "Cascadia Code Semi-Bold Italic"},
    {"Cascadia Code, Bold @wght=700",               "Cascadia Code Bold"},
    {"Cascadia Code, Bold Italic @wght=700",        "Cascadia Code Bold Italic"},
    {"Cascadia Code, Normal @wght=450",             "Cascadia Code weight=450"},
    {"Cascadia Code, Italic @wght=450",             "Cascadia Code weight=450 Italic"},

    {"Decovar Alpha, Normal  @WMX2=1000",           "Decovar Alpha @WMX2=1000"},
    {"Decovar Alpha, Normal @WMX2=1000",            "Decovar Alpha @WMX2=1000"},
    {"Decovar Alpha, Normal  @TRMG=1000",           "Decovar Alpha @TRMG=1000"},
    {"Decovar Alpha, Normal  @TRMG=1000,WMX2=1000", "Decovar Alpha @TRMG=1000,WMX2=1000"},
    {"Decovar Alpha, Normal  @WMX2=1000,TRMG=1000", "Decovar Alpha @TRMG=1000,WMX2=1000"},

    {"Nabla, Normal @EDPT=70,EHLT=20",              "Nabla @EDPT=70,EHLT=20"},
    {"Nabla, Normal @EHLT=20,EDPT=70",              "Nabla @EDPT=70,EHLT=20"},

    {"Red Hat Display, weight=596",                 "Red Hat Display weight=596"},
    {"Red Hat Display, weight=596 Italic",          "Red Hat Display weight=596 Italic"},

    {"Rocher Color, Normal",                        "Rocher Color"},
    {"Rocher Color, Normal  @BVEL=0",               "Rocher Color @BVEL=0"},
    {"Rocher Color, Normal  @SHDW=0",               "Rocher Color @SHDW=0"},
    {"Rocher Color, Normal  @SHDW=10,BVEL=20",      "Rocher Color @BVEL=20,SHDW=10"},

    {"Vazirmatn, Normal @wght=450", "Vazirmatn weight=450"},

};

class FontUtilsTest : public ::testing::Test
{
protected:
    FontUtilsTest()
    {
        Glib::init();
    }

    Inkscape::Util::Statics statics;
};

TEST_F(FontUtilsTest, canonize_fontspec)
{
    for (auto [in, out] : data) {
        EXPECT_EQ(Inkscape::canonize_fontspec(in), out) << in << std::endl;
    }
}

TEST_F(FontUtilsTest, parse_variations)
{
    const char* input = "@aaaa=100,bbbb=200,cccc=1.5";
    std::map<std::string, std::string> output = {
        {"aaaa", "100"},
        {"bbbb", "200"},
        {"cccc", "1.5"}
    };

    EXPECT_EQ(Inkscape::parse_variations(input), output) << input << std::endl;
}

} // namespace

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
