// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralBilateral filter primitive renderer.
 */

#include "display/nr-filter-spectral-bilateral.h"

#include <cairo/cairo.h>
#include <cstdint>
#include <vector>

#include "display/cairo-utils.h"
#include "display/nr-filter-slot.h"
#include "display/spectral/spectral-bilateral.h"

namespace Inkscape::Filters {

FilterSpectralBilateral::FilterSpectralBilateral() = default;
FilterSpectralBilateral::~FilterSpectralBilateral() = default;

void FilterSpectralBilateral::render_cairo(FilterSlot &slot) const
{
    cairo_surface_t *input = slot.getcairo(_input);
    cairo_surface_t *out = ink_cairo_surface_create_identical(input);
    copy_cairo_surface_ci(input, out);

    if (_sigma_spatial <= 0.0 || _sigma_range <= 0.0) {
        // No-op — copy and exit.
        cairo_t *ct = cairo_create(out);
        cairo_set_source_surface(ct, input, 0, 0);
        cairo_paint(ct);
        cairo_destroy(ct);
        cairo_surface_mark_dirty(out);
        slot.set(_output, out);
        cairo_surface_destroy(out);
        return;
    }

    cairo_surface_flush(input);
    const int W = cairo_image_surface_get_width(input);
    const int H = cairo_image_surface_get_height(input);
    const int src_stride = cairo_image_surface_get_stride(input);
    const int dst_stride = cairo_image_surface_get_stride(out);
    const std::uint8_t *src_data = cairo_image_surface_get_data(input);
    std::uint8_t *dst_data       = cairo_image_surface_get_data(out);

    if (W <= 0 || H <= 0) {
        slot.set(_output, out);
        cairo_surface_destroy(out);
        return;
    }

    if (cairo_image_surface_get_format(input) == CAIRO_FORMAT_A8) {
        // Pack rows tightly (Cairo aligns A8 rows to 4 bytes).
        std::vector<std::uint8_t> packed_in(static_cast<std::size_t>(W) * H);
        std::vector<std::uint8_t> packed_out(static_cast<std::size_t>(W) * H);
        for (int y = 0; y < H; ++y) {
            std::memcpy(packed_in.data() + static_cast<std::size_t>(y) * W,
                        src_data + y * src_stride, W);
        }
        Spectral::bilateral_a8(W, H, packed_in.data(), packed_out.data(),
                                _sigma_spatial, _sigma_range);
        for (int y = 0; y < H; ++y) {
            std::memcpy(dst_data + y * dst_stride,
                        packed_out.data() + static_cast<std::size_t>(y) * W, W);
        }
    } else {
        Spectral::bilateral_bgra(W, H,
                                  src_data, src_stride,
                                  dst_data, dst_stride,
                                  _sigma_spatial, _sigma_range);
    }

    cairo_surface_mark_dirty(out);
    slot.set(_output, out);
    cairo_surface_destroy(out);
}

double FilterSpectralBilateral::complexity(Geom::Affine const &) const
{
    // Pass count grows quadratically with σ_spatial. Each pass is a
    // 5-point stencil with state-dependent weights — comparable to a
    // single Gaussian convolution pass per σ²/2 of integration time.
    const double passes = std::max(1.0,
        std::ceil(2.0 * _sigma_spatial * _sigma_spatial));
    return passes;
}

} // namespace Inkscape::Filters
