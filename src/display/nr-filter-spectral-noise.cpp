// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralNoise filter primitive renderer.
 */

#include "display/nr-filter-spectral-noise.h"

#include <cstdint>
#include <vector>
#include <cairo/cairo.h>

#include "display/cairo-utils.h"
#include "display/nr-filter-slot.h"
#include "display/nr-filter-units.h"

namespace Inkscape::Filters {

FilterSpectralNoise::FilterSpectralNoise() = default;
FilterSpectralNoise::~FilterSpectralNoise() = default;

void FilterSpectralNoise::render_cairo(FilterSlot &slot) const
{
    cairo_surface_t *input = slot.getcairo(_input);
    cairo_surface_t *out = ink_cairo_surface_create_same_size(input, CAIRO_CONTENT_COLOR_ALPHA);
    set_cairo_surface_ci(out, color_interpolation);

    // Render at device-scale-1 like feTurbulence does.
    double x_scale = 1.0, y_scale = 1.0;
    cairo_surface_get_device_scale(input, &x_scale, &y_scale);
    int const width = static_cast<int>(std::ceil(cairo_image_surface_get_width(input) / x_scale / x_scale));
    int const height = static_cast<int>(std::ceil(cairo_image_surface_get_height(input) / y_scale / y_scale));
    if (width <= 0 || height <= 0) {
        slot.set(_output, out);
        cairo_surface_destroy(out);
        return;
    }

    cairo_surface_t *temp = cairo_surface_create_similar(input, CAIRO_CONTENT_COLOR_ALPHA, width, height);
    cairo_surface_set_device_scale(temp, 1, 1);
    cairo_surface_flush(temp);

    // Generate the A8 noise tile, then expand to ARGB32 (Cairo native
    // BGRA on little-endian) as opaque grayscale.
    std::vector<std::uint8_t> tile(static_cast<std::size_t>(width) * height);
    Spectral::noise_generate_a8(width, height, _profile, _seed, tile.data());

    std::uint8_t *dst = cairo_image_surface_get_data(temp);
    int const stride = cairo_image_surface_get_stride(temp);
    for (int y = 0; y < height; ++y) {
        std::uint8_t *row = dst + y * stride;
        std::uint8_t const *src_row = tile.data() + static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            std::uint8_t const v = src_row[x];
            // Cairo ARGB32 is premultiplied BGRA on LE. Opaque grayscale:
            // R = G = B = v, A = 255. Premultiplied = v (since A=1).
            row[x * 4 + 0] = v;
            row[x * 4 + 1] = v;
            row[x * 4 + 2] = v;
            row[x * 4 + 3] = 255;
        }
    }
    cairo_surface_mark_dirty(temp);

    cairo_t *ct = cairo_create(out);
    cairo_set_source_surface(ct, temp, 0, 0);
    cairo_paint(ct);
    cairo_destroy(ct);
    cairo_surface_destroy(temp);
    cairo_surface_mark_dirty(out);

    slot.set(_output, out);
    cairo_surface_destroy(out);
}

double FilterSpectralNoise::complexity(Geom::Affine const &) const
{
    // Per-pixel cost is ~one DCT eigenvalue table lookup + Box-Muller
    // sample plus the inverse DCT (O(N log N) of grid). On
    // production-typical 64-256² tiles the cost is negligible
    // compared to the IIR Gaussian baseline. Use 2.0 — twice
    // the unit-cost baseline.
    return 2.0;
}

} // namespace Inkscape::Filters
