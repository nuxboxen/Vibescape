// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Tests for the bilateral / state-dependent diffusion operator.
 *
 * Three properties asserted:
 *
 *  1. Flat-region invariance — uniform input is a fixed point of
 *     the operator. With identical neighbours every weight = 1 but
 *     every (u_n - u_i) is zero, so the flux is exactly zero and
 *     the output equals the input pixel-for-pixel.
 *
 *  2. Gaussian-limit behavior — at very large σ_range, every weight
 *     ≈ 1 and the operator reduces to plain forward-Euler heat
 *     equation. A step edge under that regime smears like a Gaussian
 *     would (max gradient drops substantially from the input's 255).
 *
 *  3. Edge preservation — at small σ_range, the weight collapses
 *     across a 255-step and the edge is preserved within rounding.
 *     The interior of each region smooths.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <gtest/gtest.h>
#include <src/display/spectral/spectral-bilateral.h>

using namespace Inkscape::Spectral;

namespace {

void make_uniform(std::uint8_t *m, int W, int H, std::uint8_t v)
{
    std::fill(m, m + W * H, v);
}

void make_step(std::uint8_t *m, int W, int H)
{
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            m[y * W + x] = (x < W / 2) ? 0 : 255;
        }
    }
}

int max_horiz_gradient(std::uint8_t const *m, int W, int H)
{
    int g = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x + 1 < W; ++x) {
            int const d = std::abs(m[y * W + x + 1] - m[y * W + x]);
            g = std::max(g, d);
        }
    }
    return g;
}

} // anonymous namespace

TEST(SpectralBilateral, FlatRegionIsFixedPoint)
{
    constexpr int W = 32, H = 32;
    std::vector<std::uint8_t> src(W * H);
    std::vector<std::uint8_t> dst(W * H);
    make_uniform(src.data(), W, H, 128);

    bilateral_a8(W, H, src.data(), dst.data(),
                 /*sigma_spatial=*/4.0, /*sigma_range=*/16.0);

    int max_drift = 0;
    for (int i = 0; i < W * H; ++i) {
        max_drift = std::max(max_drift, std::abs(dst[i] - 128));
    }
    EXPECT_EQ(max_drift, 0) << "uniform input must be a fixed point";
}

TEST(SpectralBilateral, EdgePreservedAtSmallSigmaRange)
{
    // σ_range = 16 → weight at Δ=255 is exp(-255²/(2·16²)) ≈ 1e-55,
    // effectively zero. Neighbouring 0/255 pixels stop diffusing
    // across the seam, so the step edge survives.
    constexpr int W = 64, H = 16;
    std::vector<std::uint8_t> src(W * H);
    std::vector<std::uint8_t> dst(W * H);
    make_step(src.data(), W, H);

    bilateral_a8(W, H, src.data(), dst.data(),
                 /*sigma_spatial=*/4.0, /*sigma_range=*/16.0);

    int const src_step = max_horiz_gradient(src.data(), W, H);
    int const dst_step = max_horiz_gradient(dst.data(), W, H);
    std::fprintf(stderr, "[bilateral edge sigma_r=16] src-step=%d dst-step=%d\n", src_step, dst_step);
    EXPECT_EQ(src_step, 255);
    EXPECT_GE(dst_step, 240) << "edge should be preserved within rounding";
}

TEST(SpectralBilateral, LargeSigmaRangeApproachesPlainHeat)
{
    // σ_range = 1e6 → every weight ≈ 1, regardless of pixel delta.
    // Operator reduces to plain forward-Euler heat. A step edge
    // under that regime smears toward a smooth ramp.
    constexpr int W = 64, H = 16;
    std::vector<std::uint8_t> src(W * H);
    std::vector<std::uint8_t> dst(W * H);
    make_step(src.data(), W, H);

    bilateral_a8(W, H, src.data(), dst.data(),
                 /*sigma_spatial=*/4.0, /*sigma_range=*/1.0e6);

    int const dst_step = max_horiz_gradient(dst.data(), W, H);
    std::fprintf(stderr, "[bilateral edge sigma_r=1e6] dst-step=%d (should be << 255)\n", dst_step);
    EXPECT_LT(dst_step, 100) << "Gaussian-limit (sigma_range=1e6) should smear the edge";
}

TEST(SpectralBilateral, RGBAFlatRegionInvariant)
{
    constexpr int W = 32, H = 32;
    std::vector<std::uint8_t> src(W * H * 4);
    std::vector<std::uint8_t> dst(W * H * 4);
    for (int i = 0; i < W * H; ++i) {
        src[i * 4 + 0] = 100;
        src[i * 4 + 1] = 150;
        src[i * 4 + 2] = 200;
        src[i * 4 + 3] = 255;
    }
    bilateral_bgra(W, H, src.data(), W * 4, dst.data(), W * 4,
                   /*sigma_spatial=*/4.0, /*sigma_range=*/16.0);
    int max_drift = 0;
    for (int i = 0; i < W * H; ++i) {
        max_drift = std::max(max_drift, std::abs(dst[i * 4 + 0] - 100));
        max_drift = std::max(max_drift, std::abs(dst[i * 4 + 1] - 150));
        max_drift = std::max(max_drift, std::abs(dst[i * 4 + 2] - 200));
        max_drift = std::max(max_drift, std::abs(dst[i * 4 + 3] - 255));
    }
    EXPECT_EQ(max_drift, 0);
}

TEST(SpectralBilateral, RGBAColorEdgePreserved)
{
    // Red-vs-blue colour edge with joint-similarity weight.
    // Per-channel max delta is 255 (red 255→0, blue 0→255). Joint
    // distance² is 255²·2 = 130050. With σ_range=16 the weight is
    // exp(-130050 / 512) ≈ 5e-111 — effectively zero. Edge survives.
    constexpr int W = 64, H = 16;
    std::vector<std::uint8_t> src(W * H * 4);
    std::vector<std::uint8_t> dst(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            std::uint8_t *p = src.data() + (y * W + x) * 4;
            // BGRA: B G R A
            if (x < W / 2) {
                p[0] = 0;
                p[1] = 0;
                p[2] = 255;
                p[3] = 255;
            } else {
                p[0] = 255;
                p[1] = 0;
                p[2] = 0;
                p[3] = 255;
            }
        }
    }
    bilateral_bgra(W, H, src.data(), W * 4, dst.data(), W * 4,
                   /*sigma_spatial=*/4.0, /*sigma_range=*/16.0);

    // Find the max horizontal R-channel gradient in dst.
    int max_r_grad = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x + 1 < W; ++x) {
            int const r0 = dst[(y * W + x) * 4 + 2];
            int const r1 = dst[(y * W + x + 1) * 4 + 2];
            max_r_grad = std::max(max_r_grad, std::abs(r1 - r0));
        }
    }
    std::fprintf(stderr, "[bilateral colour-edge sigma_r=16] max R-gradient=%d\n", max_r_grad);
    EXPECT_GE(max_r_grad, 240) << "joint colour edge should be preserved";
}
