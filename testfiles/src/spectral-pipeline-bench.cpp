// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Full-pipeline bench: spectral DCT path vs Inkscape's production
 * van-Vliet IIR (`gaussian_pass_IIR`) on the same Cairo surface at
 * matched σ values. Measures the actual crossover point that
 * `kSpectralCutoff` should land at.
 *
 * Methodology:
 *  - Allocate ARGB32 Cairo surface at canvas size W×H.
 *  - Fill with a deterministic checker pattern (so the optimizer can't
 *    elide the work).
 *  - Time `Inkscape::Spectral::blur_bgra(W, H, ..., σ, σ)` for the
 *    spectral path.
 *  - Time `gaussian_pass_IIR(X, σ, ..., pool); gaussian_pass_IIR(Y, σ, ...)`
 *    for the IIR path (matches what FilterGaussian::render_cairo does
 *    when `use_IIR` is true).
 *  - Report wall-clock per σ across {1024, 2048, 4096} canvases and
 *    σ ∈ {8, 16, 32, 64, 128}.
 *
 * Output goes to stderr and feeds the `kSpectralCutoff` decision in
 * doc/spectral/progress.md §N. The crossover σ where spectral_ms >
 * iir_ms is the cutoff. Below it, IIR wins; above it, spectral wins.
 */

#include <gtest/gtest.h>

#include <2geom/coord.h>
#include <cairo/cairo.h>

#include <src/display/dispatch-pool.h>
#include <src/display/spectral/spectral-blur.h>
#include <src/display/threading.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

// Forward declaration for the now-non-static IIR helper. Mirrors
// the prototype in nr-filter-gaussian.cpp; couldn't put a header in
// src/display/ without polluting the public API surface, so the
// linkage is by symbol-name only — this bench file is the sole
// non-production caller.
namespace Inkscape::Filters {
void gaussian_pass_IIR(Geom::Dim2 d, double deviation,
                        cairo_surface_t *src, cairo_surface_t *dest,
                        double **tmpdata, dispatch_pool &pool);
}

namespace {

cairo_surface_t* make_checker_argb32(int W, int H)
{
    auto *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    auto *data = cairo_image_surface_get_data(surf);
    const int stride = cairo_image_surface_get_stride(surf);
    for (int y = 0; y < H; ++y) {
        std::uint8_t *row = data + y * stride;
        for (int x = 0; x < W; ++x) {
            const bool dark = ((x >> 4) ^ (y >> 4)) & 1;
            std::uint8_t *p = row + x * 4;
            // Premultiplied: just use solid colors (alpha = 255).
            p[0] = dark ? 0x20 : 0x84;  // B
            p[1] = dark ? 0x40 : 0x44;  // G
            p[2] = dark ? 0x80 : 0x24;  // R
            p[3] = 0xFF;                 // A
        }
    }
    cairo_surface_mark_dirty(surf);
    return surf;
}

double time_spectral(int W, int H, double sigma)
{
    auto *src = make_checker_argb32(W, H);
    cairo_surface_flush(src);
    auto *data = cairo_image_surface_get_data(src);
    const int stride = cairo_image_surface_get_stride(src);
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    Inkscape::Spectral::blur_bgra(W, H, data, stride, data, stride,
                                    sigma, sigma);
    const auto t1 = Clock::now();
    cairo_surface_destroy(src);
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

double time_iir(int W, int H, double sigma,
                 std::shared_ptr<Inkscape::dispatch_pool> &pool)
{
    auto *src = make_checker_argb32(W, H);
    cairo_surface_flush(src);
    // Each thread in the dispatch pool needs its own scratch buffer.
    // FilterGaussian::render_cairo allocates these per-thread up front.
    const int threads = pool->size();
    const int max_dim = std::max(W, H);
    std::vector<std::vector<double>> bufs(threads);
    std::vector<double *> tmpdata_array(threads);
    for (int i = 0; i < threads; ++i) {
        bufs[i].resize(static_cast<size_t>(max_dim) * 4);
        tmpdata_array[i] = bufs[i].data();
    }
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    Inkscape::Filters::gaussian_pass_IIR(Geom::X, sigma, src, src,
                                           tmpdata_array.data(), *pool);
    Inkscape::Filters::gaussian_pass_IIR(Geom::Y, sigma, src, src,
                                           tmpdata_array.data(), *pool);
    const auto t1 = Clock::now();
    cairo_surface_destroy(src);
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

} // anonymous namespace

TEST(SpectralPipelineBench, IIRvsSpectralCrossover)
{
    auto pool = Inkscape::get_global_dispatch_pool();
    const int sizes[] = {512, 1024, 2048};
    const double sigmas[] = {8.0, 16.0, 32.0, 64.0, 128.0};

    std::fprintf(stderr,
        "\n[bench-pipeline] full-canvas wall-clock comparison\n");
    std::fprintf(stderr,
        "%-8s %-8s %-12s %-12s %-12s\n",
        "size", "sigma", "iir-ms", "spectral-ms", "spectral/iir");

    for (int N : sizes) {
        for (double s : sigmas) {
            const double iir_ms  = time_iir(N, N, s, pool);
            const double spec_ms = time_spectral(N, N, s);
            const double ratio   = spec_ms / iir_ms;
            std::fprintf(stderr, "%-8d %-8.0f %-12.3f %-12.3f %-12.2f\n",
                         N, s, iir_ms, spec_ms, ratio);
        }
    }
    SUCCEED();
}
