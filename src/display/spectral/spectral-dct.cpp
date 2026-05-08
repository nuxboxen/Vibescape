// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: direct (O(N²)) lattice DCT-II / DCT-III, plus
 * the heat-kernel transfer function in eigenmode space.
 *
 * The direct implementation is the mathematical reference; an
 * FFT-accelerated path will land later (Tier 5) when bench numbers
 * justify the complexity.
 */

#include "display/spectral/spectral-dct.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace Inkscape::Spectral {

namespace {

constexpr double kPi = 3.14159265358979323846;

} // anonymous namespace

void dct2_1d(const double *in, double *out, int N)
{
    assert(in != nullptr && out != nullptr && in != out);
    assert(N > 0);
    const double a0 = std::sqrt(1.0 / N);
    const double ak = std::sqrt(2.0 / N);
    for (int k = 0; k < N; ++k) {
        double s = 0.0;
        for (int n = 0; n < N; ++n) {
            s += in[n] * std::cos(kPi * (n + 0.5) * k / N);
        }
        out[k] = (k == 0 ? a0 : ak) * s;
    }
}

void dct3_1d(const double *in, double *out, int N)
{
    assert(in != nullptr && out != nullptr && in != out);
    assert(N > 0);
    const double a0 = std::sqrt(1.0 / N);
    const double ak = std::sqrt(2.0 / N);
    for (int n = 0; n < N; ++n) {
        // k=0 term is constant; remaining terms have α_k = ak.
        double s = a0 * in[0];
        for (int k = 1; k < N; ++k) {
            s += ak * in[k] * std::cos(kPi * (n + 0.5) * k / N);
        }
        out[n] = s;
    }
}

namespace {

// 2D row-major helper. Apply a 1D transform op(in, out, N) along each
// row, then along each column. Internal scratch ping-pongs between
// `out` and a working buffer so callers can pass aliased in/out.
template <typename Op>
void apply_separable_2d(const double *in, double *out, int W, int H, Op op)
{
    std::vector<double> rowSrc(W);
    std::vector<double> rowDst(W);
    std::vector<double> work(static_cast<size_t>(W) * static_cast<size_t>(H));

    // Row pass: in → work.
    for (int y = 0; y < H; ++y) {
        std::memcpy(rowSrc.data(), in + y * W, sizeof(double) * W);
        op(rowSrc.data(), rowDst.data(), W);
        std::memcpy(work.data() + y * W, rowDst.data(), sizeof(double) * W);
    }

    // Column pass: work → out.
    std::vector<double> colSrc(H);
    std::vector<double> colDst(H);
    for (int x = 0; x < W; ++x) {
        for (int y = 0; y < H; ++y) {
            colSrc[y] = work[y * W + x];
        }
        op(colSrc.data(), colDst.data(), H);
        for (int y = 0; y < H; ++y) {
            out[y * W + x] = colDst[y];
        }
    }
}

} // anonymous namespace

void dct2_2d(const double *in, double *out, int W, int H)
{
    apply_separable_2d(in, out, W, H, dct2_1d);
}

void dct3_2d(const double *in, double *out, int W, int H)
{
    apply_separable_2d(in, out, W, H, dct3_1d);
}

void apply_lattice_heat_kernel(double *dct_coeffs,
                                int W, int H,
                                double sigma_x, double sigma_y)
{
    assert(dct_coeffs != nullptr);
    assert(W > 0 && H > 0);
    assert(sigma_x >= 0.0 && sigma_y >= 0.0);

    // Per-axis decay tables: exp(-(σ² / 2) · λ_k). Precomputed once per
    // call so the inner loop is a multiply rather than a transcendental.
    std::vector<double> decayX(W), decayY(H);
    const double half_sx2 = 0.5 * sigma_x * sigma_x;
    const double half_sy2 = 0.5 * sigma_y * sigma_y;
    for (int k = 0; k < W; ++k) {
        const double lambda = 2.0 * (1.0 - std::cos(kPi * k / W));
        decayX[k] = std::exp(-half_sx2 * lambda);
    }
    for (int l = 0; l < H; ++l) {
        const double lambda = 2.0 * (1.0 - std::cos(kPi * l / H));
        decayY[l] = std::exp(-half_sy2 * lambda);
    }

    for (int l = 0; l < H; ++l) {
        const double dy = decayY[l];
        double *row = dct_coeffs + l * W;
        for (int k = 0; k < W; ++k) {
            row[k] *= decayX[k] * dy;
        }
    }
}

} // namespace Inkscape::Spectral
