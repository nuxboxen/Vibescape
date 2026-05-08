// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: SSoT heat-kernel blur operator.
 *
 * Ported from `src/core/SkSpectralBlur.cpp` of the Skia
 * spectral-faithful branch. Skia helpers replaced with std equivalents;
 * Cairo BGRA pixel layout used in the 4-channel wrapper.
 */

#include "display/spectral/spectral-blur.h"

#include "display/spectral/spectral-dct.h"
#include "display/spectral/spectral-fft.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace Inkscape::Spectral {

void apply_heat_kernel_a8(int W, int H, std::uint8_t *buf,
                           double sigma_x, double sigma_y)
{
    assert(buf != nullptr);
    assert(W > 0 && H > 0);
    assert(sigma_x >= 0.0 && sigma_y >= 0.0);

    if (sigma_x <= 0.0 && sigma_y <= 0.0) {
        return; // no-blur fast path
    }

    const int padW = next_pow2(W);
    const int padH = next_pow2(H);
    const std::size_t N = static_cast<std::size_t>(padW) *
                          static_cast<std::size_t>(padH);
    std::vector<double> work(N, 0.0);
    std::vector<double> spec(N, 0.0);

    // Lift uint8 → double into the top-left of the padded grid.
    for (int y = 0; y < H; ++y) {
        const std::uint8_t *src_row = buf + y * W;
        double *dst_row = work.data() + y * padW;
        for (int x = 0; x < W; ++x) {
            dst_row[x] = static_cast<double>(src_row[x]);
        }
    }

    dct2_2d(work.data(), spec.data(), padW, padH);
    apply_lattice_heat_kernel(spec.data(), padW, padH, sigma_x, sigma_y);
    dct3_2d(spec.data(), work.data(), padW, padH);

    // Crop back to (W × H), clamping to uint8.
    for (int y = 0; y < H; ++y) {
        std::uint8_t *dst_row = buf + y * W;
        const double *src_row = work.data() + y * padW;
        for (int x = 0; x < W; ++x) {
            const double v = std::round(src_row[x]);
            const int iv = (v < 0.0)   ? 0
                         : (v > 255.0) ? 255
                                       : static_cast<int>(v);
            dst_row[x] = static_cast<std::uint8_t>(iv);
        }
    }
}

void blur_bgra(int W, int H,
                const std::uint8_t *src, std::size_t src_stride,
                std::uint8_t *dst, std::size_t dst_stride,
                double sigma_x, double sigma_y)
{
    assert(src != nullptr && dst != nullptr);
    assert(W > 0 && H > 0);
    assert(src_stride >= static_cast<std::size_t>(W) * 4);
    assert(dst_stride >= static_cast<std::size_t>(W) * 4);

    if (sigma_x <= 0.0 && sigma_y <= 0.0) {
        for (int y = 0; y < H; ++y) {
            std::memcpy(dst + y * dst_stride,
                        src + y * src_stride,
                        static_cast<std::size_t>(W) * 4);
        }
        return;
    }

    const std::size_t plane = static_cast<std::size_t>(W) *
                              static_cast<std::size_t>(H);
    std::vector<std::uint8_t> b(plane), g(plane), r(plane), a(plane);

    // Deinterleave (Cairo native BGRA on little-endian).
    for (int y = 0; y < H; ++y) {
        const std::uint8_t *s_row = src + y * src_stride;
        std::uint8_t *b_row = b.data() + y * W;
        std::uint8_t *g_row = g.data() + y * W;
        std::uint8_t *r_row = r.data() + y * W;
        std::uint8_t *a_row = a.data() + y * W;
        for (int x = 0; x < W; ++x) {
            const std::uint8_t *p = s_row + x * 4;
            b_row[x] = p[0];
            g_row[x] = p[1];
            r_row[x] = p[2];
            a_row[x] = p[3];
        }
    }

    apply_heat_kernel_a8(W, H, b.data(), sigma_x, sigma_y);
    apply_heat_kernel_a8(W, H, g.data(), sigma_x, sigma_y);
    apply_heat_kernel_a8(W, H, r.data(), sigma_x, sigma_y);
    apply_heat_kernel_a8(W, H, a.data(), sigma_x, sigma_y);

    // Reinterleave.
    for (int y = 0; y < H; ++y) {
        const std::uint8_t *b_row = b.data() + y * W;
        const std::uint8_t *g_row = g.data() + y * W;
        const std::uint8_t *r_row = r.data() + y * W;
        const std::uint8_t *a_row = a.data() + y * W;
        std::uint8_t *d_row = dst + y * dst_stride;
        for (int x = 0; x < W; ++x) {
            std::uint8_t *p = d_row + x * 4;
            p[0] = b_row[x];
            p[1] = g_row[x];
            p[2] = r_row[x];
            p[3] = a_row[x];
        }
    }
}

} // namespace Inkscape::Spectral
