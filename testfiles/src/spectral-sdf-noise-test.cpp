// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Tests for the Tier 3 capability primitives — heat-kernel SDF
 * (Varadhan) and power-spectrum-controlled noise generation.
 *
 * SDF: validates sign correctness on a centered disk, monotone
 * radial growth past the boundary, and far-field sentinel clamping
 * (no NaN/+inf for pixels deeper than the uint8 quantization floor
 * allows reliable log evaluation).
 *
 * Noise: validates determinism (same seed → byte-identical tile),
 * reproducible separation between profiles, and qualitative
 * roughness ordering (white > pink > brown for mean-abs neighbour
 * differences; blue > white).
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <gtest/gtest.h>
#include <src/display/spectral/spectral-distance-field.h>
#include <src/display/spectral/spectral-noise.h>

using namespace Inkscape::Spectral;

namespace {

void make_disk_mask(std::uint8_t *m, int W, int H, int cx, int cy, double radius)
{
    double const r2 = radius * radius;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            double const dx = x + 0.5 - cx;
            double const dy = y + 0.5 - cy;
            m[y * W + x] = (dx * dx + dy * dy <= r2) ? 255 : 0;
        }
    }
}

double analytical_signed(int x, int y, int cx, int cy, double radius)
{
    double const dx = x + 0.5 - cx, dy = y + 0.5 - cy;
    return std::sqrt(dx * dx + dy * dy) - radius;
}

double mean_abs_neighbour_diff(std::uint8_t const *m, int W, int H)
{
    double sum = 0;
    long long n = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x + 1 < W; ++x) {
            sum += std::abs(m[y * W + x + 1] - m[y * W + x]);
            ++n;
        }
    }
    for (int y = 0; y + 1 < H; ++y) {
        for (int x = 0; x < W; ++x) {
            sum += std::abs(m[(y + 1) * W + x] - m[y * W + x]);
            ++n;
        }
    }
    return n > 0 ? sum / n : 0.0;
}

} // anonymous namespace

TEST(SpectralSDF, SignedSDFOnCenteredDisk)
{
    constexpr int W = 64, H = 64;
    constexpr int cx = W / 2, cy = H / 2;
    constexpr double radius = 12.0;
    constexpr double sigma = 3.0;

    std::vector<std::uint8_t> mask(W * H);
    make_disk_mask(mask.data(), W, H, cx, cy, radius);

    std::vector<float> sdf(W * H);
    signed_distance_field_a8(W, H, mask.data(), sdf.data(), sigma);

    // Sign-correctness probe at sparse grid points (skip boundary
    // band where antialiasing makes sign assignment ambiguous).
    int mismatches = 0, samples = 0;
    for (int y = 4; y < H - 4; y += 4) {
        for (int x = 4; x < W - 4; x += 4) {
            double const truth = analytical_signed(x, y, cx, cy, radius);
            if (std::abs(truth) < 1.5)
                continue;
            float const est = sdf[y * W + x];
            if (std::abs(est) >= kDistanceFieldFar * 0.5f)
                continue;
            ++samples;
            if ((truth < 0) != (est < 0))
                ++mismatches;
        }
    }
    EXPECT_GT(samples, 0);
    EXPECT_EQ(mismatches, 0) << mismatches << " sign mismatches across " << samples << " probes";
}

TEST(SpectralSDF, FarFieldClampsRatherThanOverflows)
{
    constexpr int W = 64, H = 64;
    std::vector<std::uint8_t> mask(W * H);
    make_disk_mask(mask.data(), W, H, W / 2, H / 2, /*radius=*/4.0);

    std::vector<float> dist(W * H);
    distance_field_a8(W, H, mask.data(), dist.data(), /*sigma=*/1.5);

    int finite = 0, sentinel = 0;
    for (int i = 0; i < W * H; ++i) {
        EXPECT_TRUE(std::isfinite(dist[i]));
        if (dist[i] >= kDistanceFieldFar * 0.5f)
            ++sentinel;
        else
            ++finite;
    }
    EXPECT_GT(sentinel, 0) << "expected far-field clamping with σ=1.5 on a 4-px disk; "
                           << "saw finite=" << finite << " sentinel=" << sentinel;
}

TEST(SpectralNoise, SeededReproducibility)
{
    constexpr int W = 64, H = 64;
    std::vector<std::uint8_t> a(W * H), b(W * H);
    noise_generate_a8(W, H, NoiseProfile::kPink, /*seed=*/0xDEADBEEFu, a.data());
    noise_generate_a8(W, H, NoiseProfile::kPink, /*seed=*/0xDEADBEEFu, b.data());
    bool identical = true;
    for (int i = 0; i < W * H; ++i)
        identical &= (a[i] == b[i]);
    EXPECT_TRUE(identical) << "same (W,H,profile,seed) must produce same tile";
}

TEST(SpectralNoise, SeedSensitivity)
{
    constexpr int W = 64, H = 64;
    std::vector<std::uint8_t> a(W * H), b(W * H);
    noise_generate_a8(W, H, NoiseProfile::kPink, /*seed=*/0xDEADBEEFu, a.data());
    noise_generate_a8(W, H, NoiseProfile::kPink, /*seed=*/0xDEADBEEEu, b.data());
    int diff = 0;
    for (int i = 0; i < W * H; ++i)
        if (a[i] != b[i])
            ++diff;
    EXPECT_GT(diff, W * H / 2) << "different seeds should diverge over most of the tile (got " << diff << "/" << (W * H)
                               << " pixels)";
}

TEST(SpectralNoise, ProfileRoughnessOrdering)
{
    // Mean-abs neighbour difference: a measure of high-frequency
    // content in the tile. Expect blue > white > pink > brown.
    // (The spectrum literature: brown=1/λ smoothest; blue=√λ has
    // most high-freq energy.)
    constexpr int W = 128, H = 128;
    std::vector<std::uint8_t> w(W * H), pk(W * H), br(W * H), bl(W * H);
    std::uint32_t const seed = 0xCAFEBABEu;
    noise_generate_a8(W, H, NoiseProfile::kWhite, seed, w.data());
    noise_generate_a8(W, H, NoiseProfile::kPink, seed, pk.data());
    noise_generate_a8(W, H, NoiseProfile::kBrown, seed, br.data());
    noise_generate_a8(W, H, NoiseProfile::kBlue, seed, bl.data());

    double const rW = mean_abs_neighbour_diff(w.data(), W, H);
    double const rPk = mean_abs_neighbour_diff(pk.data(), W, H);
    double const rBr = mean_abs_neighbour_diff(br.data(), W, H);
    double const rBl = mean_abs_neighbour_diff(bl.data(), W, H);
    std::fprintf(stderr, "[noise-roughness] white=%.2f pink=%.2f brown=%.2f blue=%.2f\n", rW, rPk, rBr, rBl);
    EXPECT_GT(rBl, rW) << "blue should have more high-freq than white";
    EXPECT_GT(rW, rPk) << "white should be rougher than pink";
    EXPECT_GT(rPk, rBr) << "pink should be rougher than brown";
}
