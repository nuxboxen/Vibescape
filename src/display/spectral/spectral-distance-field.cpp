// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: Varadhan distance field on the heat-kernel
 * SSoT. See header for the math.
 *
 * Ported from `src/core/SkSpectralDistanceField.cpp` of the Skia
 * spectral-faithful branch.
 */

#include "display/spectral/spectral-distance-field.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

#include "display/spectral/spectral-blur.h"

namespace Inkscape::Spectral {

namespace {

constexpr double kLogFloor = 1.0 / 255.0;

inline float distance_from_diffused(std::uint8_t diffused, double sigma_spatial)
{
    double const u_norm = static_cast<double>(diffused) / 255.0;
    if (u_norm <= kLogFloor) {
        return kDistanceFieldFar;
    }
    return static_cast<float>(sigma_spatial * std::sqrt(-2.0 * std::log(u_norm)));
}

} // anonymous namespace

void distance_field_a8(int W, int H, std::uint8_t const *binary_mask, float *out_distance, double sigma_spatial)
{
    assert(binary_mask && out_distance);
    assert(W > 0 && H > 0);
    assert(sigma_spatial > 0);

    std::size_t const N = static_cast<std::size_t>(W) * H;
    std::vector<std::uint8_t> diffused(N);
    std::memcpy(diffused.data(), binary_mask, N);
    apply_heat_kernel_a8(W, H, diffused.data(), sigma_spatial, sigma_spatial);

    for (std::size_t i = 0; i < N; ++i) {
        out_distance[i] = distance_from_diffused(diffused[i], sigma_spatial);
    }
}

void signed_distance_field_a8(int W, int H, std::uint8_t const *binary_mask, float *out_distance, double sigma_spatial)
{
    assert(binary_mask && out_distance);
    assert(W > 0 && H > 0);
    assert(sigma_spatial > 0);

    std::size_t const N = static_cast<std::size_t>(W) * H;

    std::vector<float> d_out(N);
    distance_field_a8(W, H, binary_mask, d_out.data(), sigma_spatial);

    std::vector<std::uint8_t> inverted(N);
    for (std::size_t i = 0; i < N; ++i) {
        inverted[i] = static_cast<std::uint8_t>(255 - binary_mask[i]);
    }
    std::vector<float> d_in(N);
    distance_field_a8(W, H, inverted.data(), d_in.data(), sigma_spatial);

    for (std::size_t i = 0; i < N; ++i) {
        bool const inside = binary_mask[i] >= 128;
        out_distance[i] = inside ? -d_in[i] : d_out[i];
    }
}

} // namespace Inkscape::Spectral
