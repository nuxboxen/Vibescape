// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: Perona-Malik bilateral / state-dependent
 * diffusion. See header for operator and parameterization.
 *
 * Ported from `src/core/SkSpectralBilateral.cpp` of the Skia
 * spectral-faithful branch with the Neumann BC adjustment described
 * in the header.
 */

#include "display/spectral/spectral-bilateral.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace Inkscape::Spectral {

namespace {

constexpr double kDeltaT = 0.25;

inline int passes_for_sigma(double sigma_spatial)
{
    const int p = static_cast<int>(std::ceil(2.0 * sigma_spatial * sigma_spatial));
    return std::max(p, 1);
}

// 511-entry weight table indexed by (Δ + 255) for Δ ∈ [-255, +255].
// One fill per blur (cheap — 511 std::exp calls).
void make_weight_table(double sigma_range, double *table)
{
    assert(sigma_range > 0);
    const double inv2 = 1.0 / (2.0 * sigma_range * sigma_range);
    for (int d = -255; d <= 255; ++d) {
        table[d + 255] = std::exp(-static_cast<double>(d * d) * inv2);
    }
}

// One Forward Euler pass for single channel A8.
//   u_new = u + Δt · ∑_n W_n · (u_n - u),  W_n from table[Δ+255]
// Neumann BC: missing neighbour replaced by self.
void bilateral_pass_a8(const std::uint8_t *src, std::uint8_t *dst,
                        int W, int H,
                        const double *weight_table)
{
    assert(src && dst && src != dst);
    for (int y = 0; y < H; ++y) {
        const std::uint8_t *up   = (y > 0)     ? src + (y - 1) * W : src + y * W;
        const std::uint8_t *dn   = (y < H - 1) ? src + (y + 1) * W : src + y * W;
        const std::uint8_t *cur  = src + y * W;
        std::uint8_t *dst_row    = dst + y * W;

        for (int x = 0; x < W; ++x) {
            const int u_i  = cur[x];
            const int u_up = up[x];
            const int u_dn = dn[x];
            const int u_lf = (x > 0)     ? cur[x - 1] : u_i;
            const int u_rt = (x < W - 1) ? cur[x + 1] : u_i;

            const double w_up = weight_table[u_up - u_i + 255];
            const double w_dn = weight_table[u_dn - u_i + 255];
            const double w_lf = weight_table[u_lf - u_i + 255];
            const double w_rt = weight_table[u_rt - u_i + 255];

            const double flux = w_up * (u_up - u_i)
                              + w_dn * (u_dn - u_i)
                              + w_lf * (u_lf - u_i)
                              + w_rt * (u_rt - u_i);

            const double v = u_i + kDeltaT * flux;
            const int iv = (v < 0.0) ? 0 : (v > 255.0) ? 255
                         : static_cast<int>(std::round(v));
            dst_row[x] = static_cast<std::uint8_t>(iv);
        }
    }
}

struct Pixel { std::uint8_t b, g, r, a; };

inline Pixel load_pixel(const std::uint8_t *row, int x)
{
    const std::uint8_t *p = row + x * 4;
    return Pixel{ p[0], p[1], p[2], p[3] };
}

inline void store_pixel(std::uint8_t *row, int x, Pixel p)
{
    std::uint8_t *d = row + x * 4;
    d[0] = p.b; d[1] = p.g; d[2] = p.r; d[3] = p.a;
}

inline int dist2(Pixel a, Pixel b)
{
    const int db = int(a.b) - int(b.b);
    const int dg = int(a.g) - int(b.g);
    const int dr = int(a.r) - int(b.r);
    const int da = int(a.a) - int(b.a);
    return db*db + dg*dg + dr*dr + da*da;
}

// One Forward Euler pass for RGBA. Joint similarity:
//   W = exp(-Σ_c (c_i - c_j)² / (2 σ_range²))
// One scalar weight per neighbour, applied to all 4 channel deltas.
void bilateral_pass_rgba(int W, int H,
                          const std::uint8_t *src, std::size_t src_stride,
                          std::uint8_t *dst,       std::size_t dst_stride,
                          double inv2sigmaR2)
{
    assert(src && dst && src != dst);
    for (int y = 0; y < H; ++y) {
        const std::uint8_t *s_row    = src + y * src_stride;
        const std::uint8_t *s_up     = (y > 0)     ? src + (y - 1) * src_stride : s_row;
        const std::uint8_t *s_dn     = (y < H - 1) ? src + (y + 1) * src_stride : s_row;
        std::uint8_t *d_row          = dst + y * dst_stride;

        for (int x = 0; x < W; ++x) {
            const Pixel pi = load_pixel(s_row, x);
            const Pixel pUp = load_pixel(s_up, x);
            const Pixel pDn = load_pixel(s_dn, x);
            const Pixel pLf = (x > 0)     ? load_pixel(s_row, x - 1) : pi;
            const Pixel pRt = (x < W - 1) ? load_pixel(s_row, x + 1) : pi;

            const double wU = std::exp(-dist2(pUp, pi) * inv2sigmaR2);
            const double wD = std::exp(-dist2(pDn, pi) * inv2sigmaR2);
            const double wL = std::exp(-dist2(pLf, pi) * inv2sigmaR2);
            const double wR = std::exp(-dist2(pRt, pi) * inv2sigmaR2);

            auto ch_flux = [&](int ci, int cU, int cD, int cL, int cR) {
                return wU * (cU - ci) + wD * (cD - ci)
                     + wL * (cL - ci) + wR * (cR - ci);
            };
            const double fB = ch_flux(pi.b, pUp.b, pDn.b, pLf.b, pRt.b);
            const double fG = ch_flux(pi.g, pUp.g, pDn.g, pLf.g, pRt.g);
            const double fR = ch_flux(pi.r, pUp.r, pDn.r, pLf.r, pRt.r);
            const double fA = ch_flux(pi.a, pUp.a, pDn.a, pLf.a, pRt.a);

            auto clamp_round = [](double v) -> std::uint8_t {
                const double r = std::round(v);
                return r < 0.0 ? 0 : r > 255.0 ? 255 : static_cast<std::uint8_t>(r);
            };
            store_pixel(d_row, x, Pixel{
                clamp_round(pi.b + kDeltaT * fB),
                clamp_round(pi.g + kDeltaT * fG),
                clamp_round(pi.r + kDeltaT * fR),
                clamp_round(pi.a + kDeltaT * fA),
            });
        }
    }
}

} // anonymous namespace

void bilateral_a8(int W, int H,
                   const std::uint8_t *src, std::uint8_t *dst,
                   double sigma_spatial, double sigma_range)
{
    assert(src && dst);
    assert(W > 0 && H > 0);
    assert(sigma_spatial > 0 && sigma_range > 0);

    double weight_table[511];
    make_weight_table(sigma_range, weight_table);

    const std::size_t N = static_cast<std::size_t>(W) * H;
    std::vector<std::uint8_t> bufA(N), bufB(N);
    std::memcpy(bufA.data(), src, N);

    std::uint8_t *a = bufA.data();
    std::uint8_t *b = bufB.data();
    const int passes = passes_for_sigma(sigma_spatial);
    for (int p = 0; p < passes; ++p) {
        bilateral_pass_a8(a, b, W, H, weight_table);
        std::swap(a, b);
    }
    std::memcpy(dst, a, N);
}

void bilateral_bgra(int W, int H,
                     const std::uint8_t *src, std::size_t src_stride,
                     std::uint8_t *dst,       std::size_t dst_stride,
                     double sigma_spatial, double sigma_range)
{
    assert(src && dst);
    assert(W > 0 && H > 0);
    assert(sigma_spatial > 0 && sigma_range > 0);

    const double inv2sigmaR2 = 1.0 / (2.0 * sigma_range * sigma_range);
    const std::size_t bytes = static_cast<std::size_t>(W) * 4 * H;
    std::vector<std::uint8_t> bufA(bytes), bufB(bytes);
    const std::size_t stride = static_cast<std::size_t>(W) * 4;
    for (int y = 0; y < H; ++y) {
        std::memcpy(bufA.data() + y * stride, src + y * src_stride, stride);
    }

    std::uint8_t *a = bufA.data();
    std::uint8_t *b = bufB.data();
    const int passes = passes_for_sigma(sigma_spatial);
    for (int p = 0; p < passes; ++p) {
        bilateral_pass_rgba(W, H, a, stride, b, stride, inv2sigmaR2);
        std::swap(a, b);
    }
    for (int y = 0; y < H; ++y) {
        std::memcpy(dst + y * dst_stride, a + y * stride, stride);
    }
}

} // namespace Inkscape::Spectral
