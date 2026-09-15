// SPDX-License-Identifier: GPL-2.0-or-later
//
// Shared tools for color space testing

#ifndef INKSCAPE_TEST_RENDERER_COLOR_TESTBASE_H
#define INKSCAPE_TEST_RENDERER_COLOR_TESTBASE_H

#include <memory>

#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/cms.h"
#include "colors/spaces/base.h"

using namespace Inkscape::Colors;

static std::string cmyk_filename = INKSCAPE_TESTS_DIR "/data/colors/default_cmyk.icc";

const std::shared_ptr<Space::AnySpace> alpha = Manager::get().find(Space::Type::Alpha);
const std::shared_ptr<Space::AnySpace> gray = Manager::get().find(Space::Type::Gray);
const std::shared_ptr<Space::AnySpace> rgb = Manager::get().find(Space::Type::RGB);
const std::shared_ptr<Space::AnySpace> lrgb = Manager::get().find(Space::Type::linearRGB);
const std::shared_ptr<Space::AnySpace> hsl = Manager::get().find(Space::Type::HSL);
const std::shared_ptr<Space::AnySpace> oklab = Manager::get().find(Space::Type::OKLAB);
const std::shared_ptr<Space::AnySpace> cmyk_cpp = Manager::get().find(Space::Type::CMYK);
const std::shared_ptr<CMS::Profile> cmyk_profile = CMS::Profile::create_from_uri(cmyk_filename);
const std::shared_ptr<Space::AnySpace> cmyk_icc = std::make_shared<Space::CMS>(cmyk_profile, "cmyk");

#endif

