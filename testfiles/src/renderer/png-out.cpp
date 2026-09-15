// SPDX-License-Identifier: GPL-2.0-or-later
//
// Generate a patch text field from a png input

#include "renderer/surface.h"
#include "pixel-filter-testfilters.h"

int main(int argc, char *argv[])
{
    auto method = PixelPatch::Method::COLORS;

    if (argc != 2) {
        std::cerr << "Must specify the input png\n";
        return 1;
    }
    auto src = Inkscape::Renderer::Surface(argv[1]);
    auto size = src.dimensions();
    auto sample = PixelPatch::get_patch_scale(size[Geom::X], size[Geom::Y]);

    auto patch = src.run_pixel_filter(PixelPatch(method, sample.first, sample.second, true));
    std::cout << "int width = " << src.dimensions()[Geom::X] << ";\n"
              << "int height = " << src.dimensions()[Geom::Y] << ";\n"
              << "std::string expected =" << patch.pretty_print() << ";";
    //src.run_pixel_filter(PixelPaint());
    return 0;

}

