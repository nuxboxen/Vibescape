// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * feSpectralDistance filter primitive renderer.
 */

#include "display/nr-filter-spectral-distance.h"

#include <algorithm>
#include <cairo/cairo.h>
#include <cmath>
#include <cstdint>
#include <vector>

#include "display/cairo-utils.h"
#include "display/nr-filter-slot.h"
#include "display/spectral/spectral-distance-field.h"

namespace Inkscape::Filters {

FilterSpectralDistance::FilterSpectralDistance() = default;
FilterSpectralDistance::~FilterSpectralDistance() = default;

namespace {

// Quantize a float distance into 0..255 via a soft ramp:
//   inside (d < 0) → range below 128
//   boundary (d ≈ 0) → 128
//   outside (d > 0) → range above 128
// The soft saturation at ±4σ matches the regime where Varadhan's
// approximation is reliable.
inline std::uint8_t encode_signed(float d, float sigma)
{
    const float clamped = std::clamp(d, -4.0f * sigma, 4.0f * sigma);
    const float t = clamped / (8.0f * sigma) + 0.5f;  // [0, 1]
    return static_cast<std::uint8_t>(std::round(t * 255.0f));
}

inline std::uint8_t encode_unsigned(float d, float sigma)
{
    if (d >= Inkscape::Spectral::kDistanceFieldFar * 0.5f) return 255;
    const float clamped = std::clamp(d, 0.0f, 4.0f * sigma);
    return static_cast<std::uint8_t>(std::round((clamped / (4.0f * sigma)) * 255.0f));
}

} // anonymous namespace

void FilterSpectralDistance::render_cairo(FilterSlot &slot) const
{
    cairo_surface_t *input = slot.getcairo(_input);
    cairo_surface_t *out = ink_cairo_surface_create_same_size(input, CAIRO_CONTENT_COLOR_ALPHA);
    set_cairo_surface_ci(out, color_interpolation);

    cairo_surface_flush(input);
    const int W = cairo_image_surface_get_width(input);
    const int H = cairo_image_surface_get_height(input);
    if (W <= 0 || H <= 0 || _sigma <= 0.0) {
        slot.set(_output, out);
        cairo_surface_destroy(out);
        return;
    }

    const std::uint8_t *src_data = cairo_image_surface_get_data(input);
    const int src_stride = cairo_image_surface_get_stride(input);
    const cairo_format_t fmt = cairo_image_surface_get_format(input);

    // Extract alpha as a packed binary mask.
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(W) * H);
    if (fmt == CAIRO_FORMAT_A8) {
        for (int y = 0; y < H; ++y) {
            std::memcpy(mask.data() + static_cast<std::size_t>(y) * W,
                        src_data + y * src_stride, W);
        }
    } else {
        // ARGB32 native = BGRA on LE — alpha is byte index 3.
        for (int y = 0; y < H; ++y) {
            const std::uint8_t *row = src_data + y * src_stride;
            for (int x = 0; x < W; ++x) {
                mask[static_cast<std::size_t>(y) * W + x] = row[x * 4 + 3];
            }
        }
    }

    std::vector<float> dist(static_cast<std::size_t>(W) * H);
    if (_mode == SPECTRAL_DISTANCE_SIGNED) {
        Spectral::signed_distance_field_a8(W, H, mask.data(), dist.data(), _sigma);
    } else {
        Spectral::distance_field_a8(W, H, mask.data(), dist.data(), _sigma);
    }

    std::uint8_t *dst_data = cairo_image_surface_get_data(out);
    const int dst_stride = cairo_image_surface_get_stride(out);
    const float sigma_f = static_cast<float>(_sigma);
    for (int y = 0; y < H; ++y) {
        std::uint8_t *row = dst_data + y * dst_stride;
        const float *drow = dist.data() + static_cast<std::size_t>(y) * W;
        for (int x = 0; x < W; ++x) {
            const std::uint8_t v = (_mode == SPECTRAL_DISTANCE_SIGNED)
                ? encode_signed(drow[x], sigma_f)
                : encode_unsigned(drow[x], sigma_f);
            // Output: opaque grayscale visualisation. Premultiplied
            // BGRA (A=255 → values stay as is).
            row[x * 4 + 0] = v;
            row[x * 4 + 1] = v;
            row[x * 4 + 2] = v;
            row[x * 4 + 3] = 255;
        }
    }
    cairo_surface_mark_dirty(out);
    slot.set(_output, out);
    cairo_surface_destroy(out);
}

double FilterSpectralDistance::complexity(Geom::Affine const &) const
{
    // One heat-kernel pass + an O(W·H) log/sqrt loop. For the signed
    // variant: two heat-kernel passes (forward + inverted-mask).
    return _mode == SPECTRAL_DISTANCE_SIGNED ? 4.0 : 2.0;
}

} // namespace Inkscape::Filters
