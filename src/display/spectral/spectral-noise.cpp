// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: power-spectrum-controlled noise generation.
 *
 * Ported from `src/core/SkSpectralNoise.cpp` of the Skia
 * spectral-faithful branch. The shader-factory variant is not
 * ported — Inkscape's equivalent (an feSpectralNoise SVG filter
 * primitive) is wired separately at the filter-primitive layer.
 */

#include "display/spectral/spectral-noise.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "display/spectral/spectral-dct.h"

namespace Inkscape::Spectral {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Numerical Recipes LCG. Visualization-grade; not cryptographic.
struct LCG
{
    std::uint32_t s;
    explicit LCG(std::uint32_t seed)
        : s(seed)
    {}
    std::uint32_t next()
    {
        s = s * 1664525u + 1013904223u;
        return s;
    }
    // Uniform in (0, 1] — exclude 0 so log() is safe in Box-Muller.
    double uniform() { return ((next() >> 8) + 1u) / static_cast<double>(1u << 24); }
};

double next_normal(LCG *rng)
{
    double const u1 = rng->uniform();
    double const u2 = rng->uniform();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
}

double power_at_lambda(NoiseProfile profile, double lambda)
{
    if (lambda <= 0.0)
        return 0.0; // DC coefficient — explicit zero
    switch (profile) {
        case NoiseProfile::kWhite:
            return 1.0;
        case NoiseProfile::kPink:
            return 1.0 / std::sqrt(lambda);
        case NoiseProfile::kBrown:
            return 1.0 / lambda;
        case NoiseProfile::kBlue:
            return std::sqrt(lambda);
    }
    return 0.0;
}

void normalize_to_uint8(double const *in, std::uint8_t *out, int n)
{
    double minV = in[0], maxV = in[0];
    for (int i = 1; i < n; ++i) {
        if (in[i] < minV)
            minV = in[i];
        if (in[i] > maxV)
            maxV = in[i];
    }
    double const range = maxV - minV;
    double const inv = (range > 1e-12) ? 1.0 / range : 0.0;
    for (int i = 0; i < n; ++i) {
        double const v = (in[i] - minV) * inv * 255.0;
        int const iv = (v < 0.0) ? 0 : (v > 255.0) ? 255 : static_cast<int>(v + 0.5);
        out[i] = static_cast<std::uint8_t>(iv);
    }
}

} // anonymous namespace

void noise_generate_a8(int W, int H, NoiseProfile profile, std::uint32_t seed, std::uint8_t *out)
{
    assert(W > 0 && H > 0);
    assert(out != nullptr);

    std::vector<double> lambda_x(W), lambda_y(H);
    for (int k = 0; k < W; ++k) {
        lambda_x[k] = 2.0 * (1.0 - std::cos(kPi * k / W));
    }
    for (int l = 0; l < H; ++l) {
        lambda_y[l] = 2.0 * (1.0 - std::cos(kPi * l / H));
    }

    std::size_t const N = static_cast<std::size_t>(W) * H;
    std::vector<double> coeffs(N), spatial(N);

    LCG rng(seed);
    for (int l = 0; l < H; ++l) {
        for (int k = 0; k < W; ++k) {
            double const lambda = lambda_x[k] + lambda_y[l];
            double const power = power_at_lambda(profile, lambda);
            coeffs[l * W + k] = std::sqrt(power) * next_normal(&rng);
        }
    }

    dct3_2d(coeffs.data(), spatial.data(), W, H);
    normalize_to_uint8(spatial.data(), out, static_cast<int>(N));
}

} // namespace Inkscape::Spectral
