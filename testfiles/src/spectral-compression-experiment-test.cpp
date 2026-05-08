// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * EXPERIMENT — does the framework's eigenbasis-projection +
 * FFT-residual-recovery pattern, originally landed in the
 * mlehaptics ephemerides project for DE441 truncation residuals,
 * apply to *raster image* compression with our lattice-Laplacian
 * eigenbasis as the substrate?
 *
 * Hypothesis. When we DCT-II project a raster image, truncate to
 * the top-K coefficients by magnitude, and inverse-DCT to
 * reconstruct, the residual (original − reconstruction) is a
 * *structured* signal whose 2D FFT concentrates energy in a small
 * fraction of bins. If structured, we can store the truncated K
 * coefficients PLUS a small "patch" capturing the dominant
 * residual modes and recover near-lossless quality with far fewer
 * stored modes than full DCT. This would be the framework's
 * "patch-shrinks-residual" discipline applied to image
 * compression — the basis for a hypothetical "spectral-SVG"
 * format.
 *
 * If the residual's FFT is roughly flat (noise-like), the pattern
 * doesn't pay back and our eigenbasis offers no advantage over
 * standard 8×8-block DCT (JPEG).
 *
 * Test inputs. Four content types span the regime where we'd
 * expect different compressibility:
 *   1. Geometric — centered binary disk (sharp edge, flat inside/
 *      outside). Predicted: residual concentrated near the edge,
 *      highly recoverable.
 *   2. Step edge — half-and-half pattern. Same prediction.
 *   3. Pink noise — random-spectrum content from our noise generator.
 *      Predicted: residual flatter (no spatial structure to exploit
 *      in higher modes).
 *   4. Bilateral output — bilateral-filtered colour edge. Piecewise-
 *      flat with sparse high-frequency content. Predicted: BEST
 *      compression of all four.
 *
 * Method (per input):
 *   - Compute full DCT-II coefficients
 *   - For K ∈ {N/4, N/8, N/16, N/32, N/64, N/128} (N = W·H total):
 *       a. Truncate to top-K by magnitude
 *       b. Inverse DCT to reconstruct
 *       c. Measure PSNR against original
 *       d. Compute residual = original − reconstructed
 *       e. 2D FFT the residual, measure energy concentration in
 *          top-1% of FFT bins
 *       f. Patch experiment: keep top-J residual FFT bins
 *          (J = K/4), inverse FFT back to spatial, add to
 *          reconstruction, measure PSNR uplift
 *
 * Output. A structured tab-separated report to stderr that the
 * `report-builder` test concatenates into a markdown summary at
 * `docs/SPECTRAL_SVG_EXPERIMENT.md`.
 */

#include <gtest/gtest.h>

#include <src/display/spectral/spectral-bilateral.h>
#include <src/display/spectral/spectral-blur.h>
#include <src/display/spectral/spectral-dct.h>
#include <src/display/spectral/spectral-fft.h>
#include <src/display/spectral/spectral-noise.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>

using namespace Inkscape::Spectral;

namespace {

// =============================================================================
// Helpers
// =============================================================================

double mse(const std::vector<double> &a, const std::vector<double> &b)
{
    double s = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double d = a[i] - b[i];
        s += d * d;
    }
    return s / a.size();
}

double psnr_uint8(const std::vector<double> &original,
                   const std::vector<double> &reconstructed)
{
    const double e = mse(original, reconstructed);
    if (e <= 0) return 999.0;
    return 10.0 * std::log10((255.0 * 255.0) / e);
}

// Truncate `coeffs` to keep only the top-K by absolute magnitude;
// zero the rest. Returns the count actually retained.
int truncate_top_k(std::vector<double> &coeffs, int K)
{
    std::vector<int> idx(coeffs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + K, idx.end(),
        [&](int i, int j) { return std::abs(coeffs[i]) > std::abs(coeffs[j]); });
    std::vector<bool> keep(coeffs.size(), false);
    for (int i = 0; i < K; ++i) keep[idx[i]] = true;
    int kept = 0;
    for (std::size_t i = 0; i < coeffs.size(); ++i) {
        if (keep[i]) ++kept;
        else         coeffs[i] = 0;
    }
    return kept;
}

// 2D FFT helper (pads to pow-2 like the heat kernel does internally,
// though here we just require pow-2 inputs for the experiment).
struct ResidualSpectrum {
    double total_energy;     // L2² of the residual
    double top_1pct_energy;  // L2² of top-1% of FFT bins
    double top_5pct_energy;
    double concentration;    // top_1pct / total
};

ResidualSpectrum analyze_residual_spectrum(const std::vector<double> &residual,
                                             int W, int H)
{
    // Pad to next pow-2 separately for each axis.
    const int padW = next_pow2(W);
    const int padH = next_pow2(H);
    std::vector<double> work_re(padW * padH, 0.0);
    std::vector<double> work_im(padW * padH, 0.0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            work_re[y * padW + x] = residual[y * W + x];

    // Row pass.
    for (int y = 0; y < padH; ++y) {
        radix2_fft(work_re.data() + y * padW,
                    work_im.data() + y * padW, padW, kForward);
    }
    // Column pass — extract column, FFT, write back.
    std::vector<double> col_re(padH), col_im(padH);
    for (int x = 0; x < padW; ++x) {
        for (int y = 0; y < padH; ++y) {
            col_re[y] = work_re[y * padW + x];
            col_im[y] = work_im[y * padW + x];
        }
        radix2_fft(col_re.data(), col_im.data(), padH, kForward);
        for (int y = 0; y < padH; ++y) {
            work_re[y * padW + x] = col_re[y];
            work_im[y * padW + x] = col_im[y];
        }
    }

    // Magnitude per bin.
    std::vector<double> mag(padW * padH);
    for (std::size_t i = 0; i < mag.size(); ++i) {
        mag[i] = work_re[i] * work_re[i] + work_im[i] * work_im[i];
    }
    const double total = std::accumulate(mag.begin(), mag.end(), 0.0);

    // Sort descending and accumulate top-N percentages.
    std::sort(mag.begin(), mag.end(), std::greater<double>());
    const int n_1pct = std::max(1, static_cast<int>(mag.size()) / 100);
    const int n_5pct = std::max(1, static_cast<int>(mag.size()) / 20);
    const double e_1 = std::accumulate(mag.begin(), mag.begin() + n_1pct, 0.0);
    const double e_5 = std::accumulate(mag.begin(), mag.begin() + n_5pct, 0.0);

    return ResidualSpectrum{
        total, e_1, e_5,
        total > 0 ? e_1 / total : 0.0
    };
}

// Patch experiment: take residual, FFT, keep top-J bins, inverse FFT,
// add back to reconstruction. Returns PSNR after patching.
double psnr_after_residual_patch(const std::vector<double> &original,
                                   const std::vector<double> &reconstructed,
                                   int W, int H, int J)
{
    std::vector<double> residual(W * H);
    for (int i = 0; i < W * H; ++i) residual[i] = original[i] - reconstructed[i];

    const int padW = next_pow2(W);
    const int padH = next_pow2(H);
    std::vector<double> work_re(padW * padH, 0.0);
    std::vector<double> work_im(padW * padH, 0.0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            work_re[y * padW + x] = residual[y * W + x];

    // Forward 2D FFT.
    for (int y = 0; y < padH; ++y) {
        radix2_fft(work_re.data() + y * padW,
                    work_im.data() + y * padW, padW, kForward);
    }
    std::vector<double> col_re(padH), col_im(padH);
    for (int x = 0; x < padW; ++x) {
        for (int y = 0; y < padH; ++y) {
            col_re[y] = work_re[y * padW + x];
            col_im[y] = work_im[y * padW + x];
        }
        radix2_fft(col_re.data(), col_im.data(), padH, kForward);
        for (int y = 0; y < padH; ++y) {
            work_re[y * padW + x] = col_re[y];
            work_im[y * padW + x] = col_im[y];
        }
    }

    // Keep top-J bins by magnitude.
    std::vector<int> idx(padW * padH);
    std::iota(idx.begin(), idx.end(), 0);
    std::partial_sort(idx.begin(), idx.begin() + J, idx.end(),
        [&](int a, int b) {
            const double ma = work_re[a]*work_re[a] + work_im[a]*work_im[a];
            const double mb = work_re[b]*work_re[b] + work_im[b]*work_im[b];
            return ma > mb;
        });
    std::vector<bool> keep(padW * padH, false);
    for (int i = 0; i < J; ++i) keep[idx[i]] = true;
    for (std::size_t i = 0; i < work_re.size(); ++i) {
        if (!keep[i]) { work_re[i] = 0; work_im[i] = 0; }
    }

    // Inverse FFT to spatial.
    for (int y = 0; y < padH; ++y) {
        radix2_fft(work_re.data() + y * padW,
                    work_im.data() + y * padW, padW, kInverse);
    }
    for (int x = 0; x < padW; ++x) {
        for (int y = 0; y < padH; ++y) {
            col_re[y] = work_re[y * padW + x];
            col_im[y] = work_im[y * padW + x];
        }
        radix2_fft(col_re.data(), col_im.data(), padH, kInverse);
        for (int y = 0; y < padH; ++y) {
            work_re[y * padW + x] = col_re[y];
            work_im[y * padW + x] = col_im[y];
        }
    }

    // Patched reconstruction = reconstructed + recovered_residual.
    std::vector<double> patched(W * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            patched[y * W + x] = reconstructed[y * W + x] + work_re[y * padW + x];

    return psnr_uint8(original, patched);
}

// =============================================================================
// Test inputs
// =============================================================================

std::vector<double> input_disk(int W, int H)
{
    std::vector<double> out(W * H, 0);
    const double cx = W / 2.0, cy = H / 2.0;
    const double r = std::min(W, H) * 0.25;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            out[y * W + x] = (dx*dx + dy*dy <= r*r) ? 255.0 : 0.0;
        }
    return out;
}

std::vector<double> input_step(int W, int H)
{
    std::vector<double> out(W * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            out[y * W + x] = (x < W / 2) ? 0.0 : 255.0;
    return out;
}

std::vector<double> input_pink_noise(int W, int H)
{
    std::vector<std::uint8_t> tile(W * H);
    noise_generate_a8(W, H, NoiseProfile::kPink, /*seed=*/0xCAFEBABEu, tile.data());
    std::vector<double> out(W * H);
    for (int i = 0; i < W * H; ++i) out[i] = static_cast<double>(tile[i]);
    return out;
}

std::vector<double> input_bilateral_edge(int W, int H)
{
    // Step edge with embedded jitter, then bilateral-smoothed.
    // This is the case the framework's edge-preservation should
    // compress *especially* well.
    std::vector<std::uint8_t> src(W * H);
    std::uint32_t s = 0x5EEDu;
    auto next = [&]() { s = s * 1664525u + 1013904223u; return (s >> 24) & 0x1F; };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int j = next();
            src[y * W + x] = static_cast<std::uint8_t>(
                (x < W / 2 ? 60 : 200) + j - 16);
        }
    std::vector<std::uint8_t> dst(W * H);
    bilateral_a8(W, H, src.data(), dst.data(), /*sigma_spatial=*/3.0,
                  /*sigma_range=*/16.0);
    std::vector<double> out(W * H);
    for (int i = 0; i < W * H; ++i) out[i] = static_cast<double>(dst[i]);
    return out;
}

// =============================================================================
// Experimental measurement
// =============================================================================

struct Trial {
    const char *name;
    int         W, H;
    int         total_coeffs;
    int         K;
    double      ratio;            // K / total
    double      psnr_truncated;
    double      psnr_patched;
    double      patched_uplift;   // psnr_patched - psnr_truncated
    double      residual_concentration_top1pct;
};

void run_trial(const char *name, const std::vector<double> &original,
                int W, int H, int K, std::vector<Trial> &results)
{
    // Forward DCT-II.
    std::vector<double> coeffs(W * H);
    dct2_2d(original.data(), coeffs.data(), W, H);

    // Truncate to top-K by magnitude.
    std::vector<double> truncated_coeffs = coeffs;
    truncate_top_k(truncated_coeffs, K);

    // Inverse DCT-III.
    std::vector<double> reconstructed(W * H);
    dct3_2d(truncated_coeffs.data(), reconstructed.data(), W, H);

    const double psnr_t = psnr_uint8(original, reconstructed);

    // Residual spectrum analysis.
    std::vector<double> residual(W * H);
    for (int i = 0; i < W * H; ++i) residual[i] = original[i] - reconstructed[i];
    const ResidualSpectrum spec = analyze_residual_spectrum(residual, W, H);

    // Patch experiment: keep top-J residual modes (J = K / 4).
    const int J = std::max(1, K / 4);
    const double psnr_p = psnr_after_residual_patch(original, reconstructed, W, H, J);

    results.push_back(Trial{
        name, W, H, W * H, K,
        static_cast<double>(K) / (W * H),
        psnr_t, psnr_p, psnr_p - psnr_t, spec.concentration
    });
}

void run_all_trials(const char *name, std::vector<double> input,
                     int W, int H, std::vector<Trial> &results)
{
    const int total = W * H;
    for (double r : {0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625}) {
        const int K = std::max(1, static_cast<int>(total * r));
        run_trial(name, input, W, H, K, results);
    }
}

void print_report_row(const Trial &t)
{
    std::fprintf(stderr,
        "| %-20s | %5dx%-5d | %8d | %8.2f%% | %7.2f | %7.2f | %+7.2f | %7.2f%% |\n",
        t.name, t.W, t.H, t.K,
        t.ratio * 100.0,
        t.psnr_truncated, t.psnr_patched, t.patched_uplift,
        t.residual_concentration_top1pct * 100.0);
}

} // anonymous namespace

// =============================================================================
// gtest cases — one per content type, plus one consolidated report.
// =============================================================================

class SpectralCompressionExperiment : public ::testing::Test {};

TEST_F(SpectralCompressionExperiment, GeometricDisk)
{
    constexpr int W = 128, H = 128;
    auto input = input_disk(W, H);
    std::vector<Trial> results;
    run_all_trials("geometric-disk", input, W, H, results);
    std::fprintf(stderr, "\n[experiment: geometric-disk]\n");
    for (auto const &t : results) print_report_row(t);
    SUCCEED();
}

TEST_F(SpectralCompressionExperiment, StepEdge)
{
    constexpr int W = 128, H = 128;
    auto input = input_step(W, H);
    std::vector<Trial> results;
    run_all_trials("step-edge", input, W, H, results);
    std::fprintf(stderr, "\n[experiment: step-edge]\n");
    for (auto const &t : results) print_report_row(t);
    SUCCEED();
}

TEST_F(SpectralCompressionExperiment, PinkNoise)
{
    constexpr int W = 128, H = 128;
    auto input = input_pink_noise(W, H);
    std::vector<Trial> results;
    run_all_trials("pink-noise", input, W, H, results);
    std::fprintf(stderr, "\n[experiment: pink-noise]\n");
    for (auto const &t : results) print_report_row(t);
    SUCCEED();
}

TEST_F(SpectralCompressionExperiment, BilateralEdge)
{
    constexpr int W = 128, H = 128;
    auto input = input_bilateral_edge(W, H);
    std::vector<Trial> results;
    run_all_trials("bilateral-edge", input, W, H, results);
    std::fprintf(stderr, "\n[experiment: bilateral-edge]\n");
    for (auto const &t : results) print_report_row(t);
    SUCCEED();
}

TEST_F(SpectralCompressionExperiment, FullReportTable)
{
    constexpr int W = 128, H = 128;
    std::vector<Trial> results;
    run_all_trials("geometric-disk",     input_disk(W, H),            W, H, results);
    run_all_trials("step-edge",          input_step(W, H),            W, H, results);
    run_all_trials("pink-noise",         input_pink_noise(W, H),      W, H, results);
    run_all_trials("bilateral-edge",     input_bilateral_edge(W, H),  W, H, results);

    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "================================================================\n");
    std::fprintf(stderr, "SPECTRAL-SVG EXPERIMENT — FULL REPORT (markdown-formatted)\n");
    std::fprintf(stderr, "================================================================\n\n");
    std::fprintf(stderr,
        "| Input                | Grid       | K        | K/total   | PSNR-T  | PSNR-P  | uplift  | top-1%%   |\n");
    std::fprintf(stderr,
        "|----------------------|------------|----------|-----------|---------|---------|---------|----------|\n");
    for (auto const &t : results) print_report_row(t);
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "Legend:\n");
    std::fprintf(stderr, "  PSNR-T   = PSNR of plain top-K DCT truncation\n");
    std::fprintf(stderr, "  PSNR-P   = PSNR after FFT-residual-patch (J = K/4 modes)\n");
    std::fprintf(stderr, "  uplift   = PSNR-P − PSNR-T (dB recovered by the patch)\n");
    std::fprintf(stderr, "  top-1%%   = fraction of residual FFT energy in the top 1%% of bins.\n");
    std::fprintf(stderr, "             >50%% = highly structured (well-recoverable);\n");
    std::fprintf(stderr, "             <30%% = noise-like (eigenbasis offers no advantage).\n");
    SUCCEED();
}
