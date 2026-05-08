// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Direct unit tests for the spectral substrate primitives:
 * radix-2 FFT, lattice DCT-II/III, and the SSoT heat-kernel apply
 * function. These verify the math primitives in isolation, not via
 * consumers, so a future reviewer can read this file and convince
 * themselves the substrate is well-defined before chasing its uses
 * through the rest of the codebase.
 *
 * Mirrors `tests/SkRadix2FFTTest.cpp` and `tests/LatticeDCTTest.cpp`
 * from the Skia spectral-faithful branch.
 */

#include <cmath>
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include <src/display/spectral/spectral-blur.h>
#include <src/display/spectral/spectral-dct.h>
#include <src/display/spectral/spectral-fft.h>

using namespace Inkscape::Spectral;

TEST(SpectralFFT, RoundTripExactToMachinePrecision)
{
    for (int N : {2, 4, 8, 16, 32, 64, 128}) {
        std::vector<double> origRe(N), origIm(N), workRe(N), workIm(N);
        for (int i = 0; i < N; ++i) {
            origRe[i] = std::sin(0.13 * i + 0.7);
            origIm[i] = std::cos(0.21 * i + 0.3);
            workRe[i] = origRe[i];
            workIm[i] = origIm[i];
        }
        radix2_fft(workRe.data(), workIm.data(), N, kForward);
        radix2_fft(workRe.data(), workIm.data(), N, kInverse);
        double maxErr = 0;
        for (int i = 0; i < N; ++i) {
            maxErr = std::max(maxErr, std::abs(workRe[i] - origRe[i]));
            maxErr = std::max(maxErr, std::abs(workIm[i] - origIm[i]));
        }
        EXPECT_LT(maxErr, 1e-12) << "FFT round-trip at N=" << N;
    }
}

TEST(SpectralFFT, ParsevalEnergyPreservation)
{
    for (int N : {16, 64, 256}) {
        std::vector<double> re(N), im(N);
        double timeEnergy = 0;
        for (int i = 0; i < N; ++i) {
            re[i] = std::sin(0.21 * i + 0.7);
            im[i] = 0;
            timeEnergy += re[i] * re[i];
        }
        radix2_fft(re.data(), im.data(), N, kForward);
        double freqEnergy = 0;
        for (int i = 0; i < N; ++i) {
            freqEnergy += re[i] * re[i] + im[i] * im[i];
        }
        double const ratio = freqEnergy / (N * timeEnergy);
        EXPECT_NEAR(ratio, 1.0, 1e-12) << "Parseval ratio at N=" << N;
    }
}

TEST(SpectralFFT, Linearity)
{
    constexpr int N = 64;
    constexpr double alpha = 1.7, beta = -0.4;
    std::vector<double> xRe(N), xIm(N), yRe(N), yIm(N);
    std::vector<double> sumRe(N), sumIm(N);
    for (int i = 0; i < N; ++i) {
        xRe[i] = std::sin(0.13 * i);
        xIm[i] = std::cos(0.13 * i);
        yRe[i] = std::sin(0.31 * i + 1.2);
        yIm[i] = std::cos(0.27 * i - 0.5);
        sumRe[i] = alpha * xRe[i] + beta * yRe[i];
        sumIm[i] = alpha * xIm[i] + beta * yIm[i];
    }
    radix2_fft(xRe.data(), xIm.data(), N, kForward);
    radix2_fft(yRe.data(), yIm.data(), N, kForward);
    radix2_fft(sumRe.data(), sumIm.data(), N, kForward);
    double maxErr = 0;
    for (int i = 0; i < N; ++i) {
        double const expectedRe = alpha * xRe[i] + beta * yRe[i];
        double const expectedIm = alpha * xIm[i] + beta * yIm[i];
        maxErr = std::max(maxErr, std::abs(sumRe[i] - expectedRe));
        maxErr = std::max(maxErr, std::abs(sumIm[i] - expectedIm));
    }
    EXPECT_LT(maxErr, 1e-11);
}

TEST(SpectralDCT, RoundTrip1D)
{
    for (int N : {2, 3, 5, 8, 16, 17, 32, 64, 100, 128}) {
        std::vector<double> orig(N), spec(N), back(N);
        for (int i = 0; i < N; ++i) {
            orig[i] = std::sin(0.31 * i + 1.1) + 0.3 * i;
        }
        dct2_1d(orig.data(), spec.data(), N);
        dct3_1d(spec.data(), back.data(), N);
        double maxErr = 0;
        for (int i = 0; i < N; ++i) {
            maxErr = std::max(maxErr, std::abs(orig[i] - back[i]));
        }
        // Direct O(N²) DCT accumulates O(N) FP error; loose bound at
        // larger N.
        EXPECT_LT(maxErr, 1e-10) << "DCT 1D round-trip at N=" << N;
    }
}

TEST(SpectralDCT, RoundTrip2D)
{
    for (int W : {4, 8, 16, 32}) {
        for (int H : {4, 8, 16, 32}) {
            std::vector<double> orig(W * H), spec(W * H), back(W * H);
            for (int i = 0; i < W * H; ++i) {
                orig[i] = std::sin(0.07 * i) + 0.5;
            }
            dct2_2d(orig.data(), spec.data(), W, H);
            dct3_2d(spec.data(), back.data(), W, H);
            double maxErr = 0;
            for (int i = 0; i < W * H; ++i) {
                maxErr = std::max(maxErr, std::abs(orig[i] - back[i]));
            }
            EXPECT_LT(maxErr, 1e-10) << "DCT 2D round-trip at " << W << "x" << H;
        }
    }
}

TEST(SpectralHeatKernel, DCInputPreservedExactly)
{
    // Pure DC (constant) signal must survive the heat kernel unchanged
    // — the DC mode has eigenvalue λ=0, so exp(-(σ²/2)·0) = 1.
    constexpr int W = 32, H = 32;
    std::vector<std::uint8_t> buf(W * H, 200);
    apply_heat_kernel_a8(W, H, buf.data(), 3.0, 3.0);
    int maxDelta = 0;
    for (int i = 0; i < W * H; ++i) {
        maxDelta = std::max(maxDelta, std::abs(buf[i] - 200));
    }
    EXPECT_LE(maxDelta, 1) << "DC mass preservation (rounding ok)";
}

TEST(SpectralHeatKernel, DiracImpulseSpreadsIsotropically)
{
    // Single-pixel impulse at the center, blurred with σ_x = σ_y,
    // produces an isotropic spread (4-fold symmetry around center).
    constexpr int W = 64, H = 64;
    std::vector<std::uint8_t> buf(W * H, 0);
    buf[(H / 2) * W + (W / 2)] = 255;
    apply_heat_kernel_a8(W, H, buf.data(), 3.0, 3.0);

    int const center = buf[(H / 2) * W + (W / 2)];
    EXPECT_GT(center, 0);
    EXPECT_LT(center, 255);

    int const left = buf[(H / 2) * W + (W / 2 - 3)];
    int const right = buf[(H / 2) * W + (W / 2 + 3)];
    int const up = buf[(H / 2 - 3) * W + (W / 2)];
    int const down = buf[(H / 2 + 3) * W + (W / 2)];
    EXPECT_LE(std::abs(left - right), 1) << "horizontal symmetry";
    EXPECT_LE(std::abs(up - down), 1) << "vertical symmetry";
    EXPECT_LE(std::abs(left - up), 2) << "isotropy at sigma_x == sigma_y";
}

TEST(SpectralHeatKernel, ZeroSigmaIsIdentity)
{
    // sigma=0 must be a no-op.
    constexpr int W = 16, H = 16;
    std::vector<std::uint8_t> buf(W * H);
    for (int i = 0; i < W * H; ++i) {
        buf[i] = static_cast<std::uint8_t>(i & 0xFF);
    }
    std::vector<std::uint8_t> orig = buf;
    apply_heat_kernel_a8(W, H, buf.data(), 0.0, 0.0);
    for (int i = 0; i < W * H; ++i) {
        EXPECT_EQ(buf[i], orig[i]);
    }
}

TEST(SpectralHeatKernel, AnisotropicSigmaSpreadsCorrespondingly)
{
    // sigma_x >> sigma_y should spread an impulse much further
    // horizontally than vertically.
    constexpr int W = 64, H = 64;
    std::vector<std::uint8_t> buf(W * H, 0);
    buf[(H / 2) * W + (W / 2)] = 255;
    apply_heat_kernel_a8(W, H, buf.data(), 8.0, 2.0);

    int horizExtent = 0, vertExtent = 0;
    for (int dx = 0; dx < W / 2; ++dx) {
        if (buf[(H / 2) * W + (W / 2 + dx)] > 1)
            horizExtent = dx;
    }
    for (int dy = 0; dy < H / 2; ++dy) {
        if (buf[(H / 2 + dy) * W + (W / 2)] > 1)
            vertExtent = dy;
    }
    EXPECT_GT(horizExtent, 2 * vertExtent) << "horizontal extent should be substantially larger "
                                           << "than vertical when sigma_x = 4 * sigma_y "
                                           << "(horiz=" << horizExtent << " vert=" << vertExtent << ")";
}
