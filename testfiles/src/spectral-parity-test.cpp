// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Parity tests for the spectral heat-kernel blur against a reference
 * continuous separable Gaussian. The reference is computed at full
 * double precision with a 4σ-radius truncated kernel — slow but
 * trustworthy.
 *
 * What this proves:
 *
 *  - The spectral path is *not* a different operator dressed in
 *    framework vocabulary. It really does compute Gaussian-convolved
 *    output, bounded by quantization noise + boundary-condition drift.
 *
 *  - The pad-to-pow-2 boundary handling stays bounded: Neumann
 *    reflection at the padded boundary causes a small drift vs the
 *    natural-grid Laplacian, but for inputs surrounded by sufficient
 *    zero halo the drift is dominated by uint8 rounding.
 *
 * Mirrors `tests/SpectralBlurParityTest.cpp` from the Skia
 * spectral-faithful branch — same patterns (centered disk, step edge),
 * same tolerance bounds.
 *
 * Also includes a parity timing diagnostic: the same operator timed at
 * print-resolution sizes so the bench-tuned σ-cutoff has data to land
 * against. Output goes to stderr; the assertions only gate
 * correctness, not speed.
 */

#include <gtest/gtest.h>

#include <src/display/spectral/spectral-blur.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace Inkscape::Spectral;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Reference: continuous separable Gaussian, double precision, 4σ kernel
// truncated and renormalized. Slow (O(W·H·σ)) but the math ground truth.
void reference_gaussian_a8(const std::uint8_t *in, int W, int H,
                            std::uint8_t *out, double sigma)
{
    if (sigma <= 0) {
        std::memcpy(out, in, static_cast<std::size_t>(W) * H);
        return;
    }
    const int radius = static_cast<int>(std::ceil(4.0 * sigma));
    const double k = 1.0 / (2.0 * sigma * sigma);
    std::vector<double> kernel(2 * radius + 1);
    double sum = 0;
    for (int t = -radius; t <= radius; ++t) {
        kernel[t + radius] = std::exp(-static_cast<double>(t * t) * k);
        sum += kernel[t + radius];
    }
    for (auto &v : kernel) v /= sum;

    auto src_at = [&](int x, int y) -> double {
        if (x < 0 || x >= W || y < 0 || y >= H) return 0.0;
        return static_cast<double>(in[y * W + x]);
    };

    std::vector<double> tmp(W * H, 0.0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double s = 0;
            for (int t = -radius; t <= radius; ++t) {
                s += kernel[t + radius] * src_at(x - t, y);
            }
            tmp[y * W + x] = s;
        }
    }
    std::vector<double> col(W * H, 0.0);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double s = 0;
            for (int t = -radius; t <= radius; ++t) {
                const int yy = y - t;
                if (yy >= 0 && yy < H) {
                    s += kernel[t + radius] * tmp[yy * W + x];
                }
            }
            col[y * W + x] = s;
        }
    }
    for (int i = 0; i < W * H; ++i) {
        const double v = std::round(col[i]);
        out[i] = (v < 0) ? 0 : (v > 255) ? 255 : static_cast<std::uint8_t>(v);
    }
}

void make_centered_disk(std::uint8_t *m, int W, int H,
                          double radius_pixels)
{
    const double cx = W * 0.5, cy = H * 0.5;
    const double r2 = radius_pixels * radius_pixels;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const double dx = x + 0.5 - cx;
            const double dy = y + 0.5 - cy;
            m[y * W + x] = (dx * dx + dy * dy <= r2) ? 255 : 0;
        }
    }
}

// Step in the middle of a canvas with `halo_pixels` of zero margin
// on both sides. Both spectral and reference see zeros at the actual
// canvas edges, so their boundary conditions don't differ in the
// region where both are zero. The step is only in the interior.
void make_centered_step(std::uint8_t *m, int W, int H, int halo_pixels)
{
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            m[y * W + x] = (x >= W / 2 && x < W - halo_pixels) ? 255 : 0;
        }
    }
}

struct ParityStats {
    int    max_abs;
    double mean_abs;
    int    sample_count;
};

// Compare only pixels at least `margin` away from any edge. The
// spectral path uses Neumann BC at the padded boundary; the reference
// uses zero BC. Within `margin = 4*sigma` of either, the BC mismatch
// dominates the diff and makes comparison meaningless. Cropping to
// the interior measures the operator agreement, not the BC mismatch.
ParityStats compare_interior(const std::uint8_t *a, const std::uint8_t *b,
                              int W, int H, int margin)
{
    int max_a = 0;
    long long sum = 0;
    int n = 0;
    for (int y = margin; y < H - margin; ++y) {
        for (int x = margin; x < W - margin; ++x) {
            const int d = std::abs(a[y * W + x] - b[y * W + x]);
            max_a = std::max(max_a, d);
            sum += d;
            ++n;
        }
    }
    return {max_a, n > 0 ? static_cast<double>(sum) / n : 0.0, n};
}

} // anonymous namespace

TEST(SpectralParity, DiskAtSigma8)
{
    // 256x256 canvas with disk of radius 32 → 96 pixels of background
    // halo around it. 4σ=32, so 2σ of clean halo even at the
    // tightest. Comparison interior crop margin is 4σ=32 to avoid
    // the BC mismatch zone.
    constexpr int W = 256, H = 256;
    constexpr double sigma = 8.0;
    constexpr int margin = static_cast<int>(4.0 * sigma);

    std::vector<std::uint8_t> src(W * H);
    make_centered_disk(src.data(), W, H, /*radius_pixels=*/32.0);

    std::vector<std::uint8_t> spec(src);
    apply_heat_kernel_a8(W, H, spec.data(), sigma, sigma);

    std::vector<std::uint8_t> ref(W * H);
    reference_gaussian_a8(src.data(), W, H, ref.data(), sigma);

    const auto s = compare_interior(spec.data(), ref.data(), W, H, margin);
    std::fprintf(stderr,
        "[parity disk sigma=8 %dx%d interior] max-abs=%d mean-abs=%.3f n=%d\n",
        W, H, s.max_abs, s.mean_abs, s.sample_count);
    EXPECT_LE(s.max_abs, 3);
    EXPECT_LT(s.mean_abs, 0.5);
}

TEST(SpectralParity, DiskAtSigma16)
{
    // 512x512 canvas with disk of radius 64 → 192 pixels of background
    // halo around it. 4σ=64, comparison interior margin=64.
    constexpr int W = 512, H = 512;
    constexpr double sigma = 16.0;
    constexpr int margin = static_cast<int>(4.0 * sigma);

    std::vector<std::uint8_t> src(W * H);
    make_centered_disk(src.data(), W, H, /*radius_pixels=*/64.0);

    std::vector<std::uint8_t> spec(src);
    apply_heat_kernel_a8(W, H, spec.data(), sigma, sigma);

    std::vector<std::uint8_t> ref(W * H);
    reference_gaussian_a8(src.data(), W, H, ref.data(), sigma);

    const auto s = compare_interior(spec.data(), ref.data(), W, H, margin);
    std::fprintf(stderr,
        "[parity disk sigma=16 %dx%d interior] max-abs=%d mean-abs=%.3f n=%d\n",
        W, H, s.max_abs, s.mean_abs, s.sample_count);
    EXPECT_LE(s.max_abs, 3);
    EXPECT_LT(s.mean_abs, 0.5);
}

TEST(SpectralParity, StepEdgeAtSigma16)
{
    // Step edge in a centered window with 128 pixels of zero halo on
    // each side. The actual edges of the canvas are zero either way,
    // so spectral (Neumann) and reference (zero) BCs both see "0
    // bordered by 0" → they don't diverge at the canvas edges. The
    // interior step is what we measure.
    constexpr int W = 512, H = 256;
    constexpr double sigma = 16.0;
    constexpr int halo = 128;
    constexpr int margin = static_cast<int>(4.0 * sigma);

    std::vector<std::uint8_t> src(W * H);
    make_centered_step(src.data(), W, H, halo);

    std::vector<std::uint8_t> spec(src);
    apply_heat_kernel_a8(W, H, spec.data(), sigma, sigma);

    std::vector<std::uint8_t> ref(W * H);
    reference_gaussian_a8(src.data(), W, H, ref.data(), sigma);

    const auto s = compare_interior(spec.data(), ref.data(), W, H, margin);
    std::fprintf(stderr,
        "[parity step sigma=16 %dx%d interior] max-abs=%d mean-abs=%.3f n=%d\n",
        W, H, s.max_abs, s.mean_abs, s.sample_count);
    EXPECT_LE(s.max_abs, 4);
    EXPECT_LT(s.mean_abs, 0.5);
}

// Timing diagnostic — emits wall-clock per-pixel for the spectral
// path at print-resolution sizes. Not gated by assertions; the
// numbers feed into kSpectralCutoff tuning.
TEST(SpectralBench, HeatKernelAcrossSizesAndSigmas)
{
    using Clock = std::chrono::steady_clock;
    const int sizes[] = {256, 512, 1024, 2048};
    const double sigmas[] = {8, 16, 32, 64, 128};

    std::fprintf(stderr,
        "\n[bench] spectral apply_heat_kernel_a8 wall-clock\n");
    std::fprintf(stderr,
        "%-10s %-8s %-12s %-12s\n",
        "size", "sigma", "wall-ms", "ns/px");

    for (int N : sizes) {
        std::vector<std::uint8_t> src(N * N);
        make_centered_disk(src.data(), N, N, /*radius_pixels=*/N * 0.25);
        for (double s : sigmas) {
            std::vector<std::uint8_t> buf(src);
            const auto t0 = Clock::now();
            apply_heat_kernel_a8(N, N, buf.data(), s, s);
            const auto t1 = Clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double ns_per_px = ms * 1.0e6 / (static_cast<double>(N) * N);
            std::fprintf(stderr, "%-10d %-8.0f %-12.3f %-12.2f\n",
                         N, s, ms, ns_per_px);
        }
    }
    SUCCEED();
}
