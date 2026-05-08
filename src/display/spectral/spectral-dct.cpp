// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: lattice DCT-II / DCT-III with FFT acceleration.
 *
 * Two implementations selected by length:
 *
 *   - Pow-2 N >= 4: Makhoul method — real-input length-N DCT-II via
 *     length-(N/2) complex FFT plus a phase-correction pass. O(N log N).
 *     This is the production path for the heat-kernel blur, which
 *     pads to next-pow-2 internally.
 *
 *   - Non-pow-2 or N <= 2: direct O(N²) summation. Fallback used for
 *     unit tests at small or non-pow-2 sizes; never hit by the
 *     production blur consumer.
 *
 * Math is byte-identical to `src/core/SkLatticeDCT.cpp` from the Skia
 * spectral-faithful branch — that file carries the derivation comments
 * for the Makhoul reduction and the real-input FFT.
 */

#include "display/spectral/spectral-dct.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

#include "display/spectral/spectral-fft.h"

namespace Inkscape::Spectral {

namespace {

constexpr double kPi = 3.14159265358979323846;

inline bool is_pow2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

// ---------------- Direct O(N²) path (fallback for non-pow-2) ---------------

void direct_dct2_1d(double const *in, double *out, int N)
{
    double const a0 = std::sqrt(1.0 / N);
    double const ak = std::sqrt(2.0 / N);
    for (int k = 0; k < N; ++k) {
        double s = 0.0;
        for (int n = 0; n < N; ++n) {
            s += in[n] * std::cos(kPi * (n + 0.5) * k / N);
        }
        out[k] = (k == 0 ? a0 : ak) * s;
    }
}

void direct_dct3_1d(double const *in, double *out, int N)
{
    double const a0 = std::sqrt(1.0 / N);
    double const ak = std::sqrt(2.0 / N);
    for (int n = 0; n < N; ++n) {
        double s = a0 * in[0];
        for (int k = 1; k < N; ++k) {
            s += ak * in[k] * std::cos(kPi * (n + 0.5) * k / N);
        }
        out[n] = s;
    }
}

// ---------------- FFT-based path (Makhoul + length-N/2 complex FFT) -------

// Length-N real-input FFT via length-(N/2) complex FFT (forward).
//   in:                  length-N real input
//   out_re, out_im:      length-(N/2)+1 — conjugate-symmetric DFT (k = 0..N/2)
//   work_re, work_im:    length-(N/2) scratch
//   half_tw_re, half_tw_im:    twiddles for the length-(N/2) complex FFT
//                              (compute_twiddles(N/2, ...))
//   split_tw_re, split_tw_im:  twiddles W_N^k = exp(-i 2π k/N), k = 0..N/2-1
void fft_real_forward(double const *in, int N, double *out_re, double *out_im, double *work_re, double *work_im,
                      double const *half_tw_re, double const *half_tw_im, double const *split_tw_re,
                      double const *split_tw_im)
{
    assert(is_pow2(N) && N >= 4);
    int const half = N / 2;
    for (int m = 0; m < half; ++m) {
        work_re[m] = in[2 * m];
        work_im[m] = in[2 * m + 1];
    }
    radix2_fft_with_twiddles(work_re, work_im, half, half_tw_re, half_tw_im, kForward);

    int const mask = half - 1;
    for (int k = 0; k < half; ++k) {
        int const km = (half - k) & mask;
        double const yRe = work_re[k];
        double const yIm = work_im[k];
        double const ymRe = work_re[km];
        double const ymIm = work_im[km];
        double const eRe = 0.5 * (yRe + ymRe);
        double const eIm = 0.5 * (yIm - ymIm);
        double const oRe = 0.5 * (yIm + ymIm);
        double const oIm = -0.5 * (yRe - ymRe);
        double const wRe = split_tw_re[k];
        double const wIm = split_tw_im[k];
        double const woRe = wRe * oRe - wIm * oIm;
        double const woIm = wRe * oIm + wIm * oRe;
        out_re[k] = eRe + woRe;
        out_im[k] = eIm + woIm;
    }
    out_re[half] = work_re[0] - work_im[0];
    out_im[half] = 0.0;
}

// Length-N real-output IFFT given length-(N/2)+1 conjugate-symmetric input.
void fft_real_inverse(double const *in_re, double const *in_im, int N, double *out, double *work_re, double *work_im,
                      double const *half_tw_re, double const *half_tw_im, double const *split_tw_re,
                      double const *split_tw_im)
{
    assert(is_pow2(N) && N >= 4);
    int const half = N / 2;
    int const mask = half - 1;

    for (int k = 0; k < half; ++k) {
        int const km = (half - k) & mask;
        double xmRe, xmIm;
        if (k == 0) {
            xmRe = in_re[half];
            xmIm = in_im[half];
        } else {
            xmRe = in_re[km];
            xmIm = -in_im[km];
        }
        double const eRe = 0.5 * (in_re[k] + xmRe);
        double const eIm = 0.5 * (in_im[k] + xmIm);
        double const dRe = 0.5 * (in_re[k] - xmRe);
        double const dIm = 0.5 * (in_im[k] - xmIm);
        double const oRe = dRe * split_tw_re[k] + dIm * split_tw_im[k];
        double const oIm = dIm * split_tw_re[k] - dRe * split_tw_im[k];
        work_re[k] = eRe - oIm;
        work_im[k] = eIm + oRe;
    }

    radix2_fft_with_twiddles(work_re, work_im, half, half_tw_re, half_tw_im, kInverse);

    for (int m = 0; m < half; ++m) {
        out[2 * m] = work_re[m];
        out[2 * m + 1] = work_im[m];
    }
}

// Bundle of precomputed tables for one axis length N.
struct AxisTables
{
    std::vector<double> half_tw_re; // size N/4
    std::vector<double> half_tw_im;
    std::vector<double> split_re; // size N/2
    std::vector<double> split_im;
    std::vector<double> ph_re; // size N/2 + 1 — Makhoul phase
    std::vector<double> ph_im;
};

void build_axis_tables(int N, AxisTables *t)
{
    assert(is_pow2(N) && N >= 4);
    int const half = N / 2;
    t->half_tw_re.resize(half / 2);
    t->half_tw_im.resize(half / 2);
    compute_twiddles(half, t->half_tw_re.data(), t->half_tw_im.data());

    t->split_re.resize(half);
    t->split_im.resize(half);
    double const base_split = -2.0 * kPi / N;
    for (int k = 0; k < half; ++k) {
        double const ang = base_split * k;
        t->split_re[k] = std::cos(ang);
        t->split_im[k] = std::sin(ang);
    }

    t->ph_re.resize(half + 1);
    t->ph_im.resize(half + 1);
    double const base_ph = -kPi / (2.0 * N);
    for (int k = 0; k <= half; ++k) {
        double const ang = base_ph * k;
        t->ph_re[k] = std::cos(ang);
        t->ph_im[k] = std::sin(ang);
    }
}

void fft_dct2_1d(double const *in, double *out, int N, AxisTables const &t)
{
    assert(is_pow2(N) && N >= 4);
    int const half = N >> 1;
    std::vector<double> permuted(N);
    std::vector<double> z_re(half + 1), z_im(half + 1);
    std::vector<double> work_re(half), work_im(half);

    for (int n = 0; n < half; ++n) {
        permuted[n] = in[2 * n];
        permuted[N - 1 - n] = in[2 * n + 1];
    }

    fft_real_forward(permuted.data(), N, z_re.data(), z_im.data(), work_re.data(), work_im.data(), t.half_tw_re.data(),
                     t.half_tw_im.data(), t.split_re.data(), t.split_im.data());

    double const a0 = std::sqrt(1.0 / N);
    double const ak = std::sqrt(2.0 / N);
    for (int k = 0; k <= half; ++k) {
        double const pRe = t.ph_re[k];
        double const pIm = t.ph_im[k];
        double const re = pRe * z_re[k] - pIm * z_im[k];
        out[k] = (k == 0 ? a0 : ak) * re;
    }
    for (int k = half + 1; k < N; ++k) {
        int const j = N - k;
        double const pRe = t.ph_re[j];
        double const pIm = t.ph_im[j];
        double const im = pRe * z_im[j] + pIm * z_re[j];
        out[k] = ak * (-im);
    }
}

void fft_dct3_1d(double const *in, double *out, int N, AxisTables const &t)
{
    assert(is_pow2(N) && N >= 4);
    int const half = N >> 1;
    std::vector<double> z_re(half + 1), z_im(half + 1);
    std::vector<double> work_re(half), work_im(half);
    std::vector<double> permuted(N);

    double const a0 = std::sqrt(1.0 / N);
    double const ak = std::sqrt(2.0 / N);
    auto U = [&](int k) -> double {
        if (k == 0)
            return in[0] / a0;
        if (k >= N)
            return 0.0;
        return in[k] / ak;
    };

    for (int k = 0; k <= half; ++k) {
        double const Uk = U(k);
        double const UnK = U(N - k);
        double const pRe = t.ph_re[k];
        double const pIm = t.ph_im[k];
        z_re[k] = pRe * Uk - pIm * UnK;
        z_im[k] = -pIm * Uk - pRe * UnK;
    }
    z_im[half] = 0.0;

    fft_real_inverse(z_re.data(), z_im.data(), N, permuted.data(), work_re.data(), work_im.data(), t.half_tw_re.data(),
                     t.half_tw_im.data(), t.split_re.data(), t.split_im.data());

    for (int n = 0; n < half; ++n) {
        out[2 * n] = permuted[n];
        out[2 * n + 1] = permuted[N - 1 - n];
    }
}

// Length-2 closed form (Makhoul wants N >= 4).
void dct2_n2(double const *in, double *out)
{
    double const a0 = std::sqrt(0.5);
    double const a1 = 1.0;
    out[0] = a0 * (in[0] + in[1]);
    out[1] = a1 * std::cos(kPi * 0.25) * (in[0] - in[1]);
}

void dct3_n2(double const *in, double *out)
{
    double const a0 = std::sqrt(0.5);
    double const a1 = 1.0;
    double const c = std::cos(kPi * 0.25);
    out[0] = a0 * in[0] + a1 * c * in[1];
    out[1] = a0 * in[0] - a1 * c * in[1];
}

} // anonymous namespace

void dct2_1d(double const *in, double *out, int N)
{
    assert(in != nullptr && out != nullptr && in != out);
    assert(N > 0);
    if (N == 1) {
        out[0] = in[0];
        return;
    }
    if (N == 2) {
        dct2_n2(in, out);
        return;
    }
    if (!is_pow2(N)) {
        direct_dct2_1d(in, out, N);
        return;
    }
    AxisTables t;
    build_axis_tables(N, &t);
    fft_dct2_1d(in, out, N, t);
}

void dct3_1d(double const *in, double *out, int N)
{
    assert(in != nullptr && out != nullptr && in != out);
    assert(N > 0);
    if (N == 1) {
        out[0] = in[0];
        return;
    }
    if (N == 2) {
        dct3_n2(in, out);
        return;
    }
    if (!is_pow2(N)) {
        direct_dct3_1d(in, out, N);
        return;
    }
    AxisTables t;
    build_axis_tables(N, &t);
    fft_dct3_1d(in, out, N, t);
}

namespace {

// 2D row-major helper. Apply a 1D transform along each row, then
// each column. `in` and `out` may alias.
template <typename Op1D>
void apply_separable_2d(double const *in, double *out, int W, int H, Op1D op)
{
    std::vector<double> rowSrc(W);
    std::vector<double> rowDst(W);
    std::vector<double> work(static_cast<size_t>(W) * static_cast<size_t>(H));

    for (int y = 0; y < H; ++y) {
        std::memcpy(rowSrc.data(), in + y * W, sizeof(double) * W);
        op(rowSrc.data(), rowDst.data(), W);
        std::memcpy(work.data() + y * W, rowDst.data(), sizeof(double) * W);
    }

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

void dct2_2d(double const *in, double *out, int W, int H)
{
    apply_separable_2d(in, out, W, H, dct2_1d);
}

void dct3_2d(double const *in, double *out, int W, int H)
{
    apply_separable_2d(in, out, W, H, dct3_1d);
}

void apply_lattice_heat_kernel(double *dct_coeffs, int W, int H, double sigma_x, double sigma_y)
{
    assert(dct_coeffs != nullptr);
    assert(W > 0 && H > 0);
    assert(sigma_x >= 0.0 && sigma_y >= 0.0);

    std::vector<double> decayX(W), decayY(H);
    double const half_sx2 = 0.5 * sigma_x * sigma_x;
    double const half_sy2 = 0.5 * sigma_y * sigma_y;
    for (int k = 0; k < W; ++k) {
        double const lambda = 2.0 * (1.0 - std::cos(kPi * k / W));
        decayX[k] = std::exp(-half_sx2 * lambda);
    }
    for (int l = 0; l < H; ++l) {
        double const lambda = 2.0 * (1.0 - std::cos(kPi * l / H));
        decayY[l] = std::exp(-half_sy2 * lambda);
    }

    for (int l = 0; l < H; ++l) {
        double const dy = decayY[l];
        double *row = dct_coeffs + l * W;
        for (int k = 0; k < W; ++k) {
            row[k] *= decayX[k] * dy;
        }
    }
}

} // namespace Inkscape::Spectral
