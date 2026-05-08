// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: minimal radix-2 complex FFT.
 *
 * Ported from `src/core/SkRadix2FFT.cpp` of the Skia spectral-faithful
 * branch with skvx SIMD removed (Tier 5 will reintroduce SIMD via
 * std::simd or hand-vectorized intrinsics; the scalar path is the
 * mathematical reference). Math is byte-identical to the Skia version.
 */

#include "display/spectral/spectral-fft.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace Inkscape::Spectral {

namespace {

constexpr double kPi = 3.14159265358979323846;

inline bool is_pow2(int n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

void bit_reverse(double *re, double *im, int N)
{
    int j = 0;
    for (int i = 1; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
}

// Scalar Cooley-Tukey butterfly stage.
void butterfly_scalar(double *re, double *im, int N, int len, double const *twRe, double const *twIm, double imSign)
{
    int const half = len >> 1;
    int const stride = N / len;
    for (int i = 0; i < N; i += len) {
        for (int k = 0; k < half; ++k) {
            int const idx = k * stride;
            double const wRe = twRe[idx];
            double const wIm = imSign * twIm[idx];
            double const aRe = re[i + k];
            double const aIm = im[i + k];
            double const bRe = re[i + k + half];
            double const bIm = im[i + k + half];
            double const tRe = wRe * bRe - wIm * bIm;
            double const tIm = wRe * bIm + wIm * bRe;
            re[i + k + half] = aRe - tRe;
            im[i + k + half] = aIm - tIm;
            re[i + k] = aRe + tRe;
            im[i + k] = aIm + tIm;
        }
    }
}

} // anonymous namespace

void compute_twiddles(int N, double *twRe, double *twIm)
{
    assert(is_pow2(N) && N >= 2);
    assert(twRe != nullptr && twIm != nullptr);
    double const base = -2.0 * kPi / N;
    for (int k = 0; k < N / 2; ++k) {
        double const ang = base * k;
        twRe[k] = std::cos(ang);
        twIm[k] = std::sin(ang);
    }
}

void radix2_fft_with_twiddles(double *re, double *im, int N, double const *twRe, double const *twIm, FFTDirection dir)
{
    assert(is_pow2(N));
    assert(dir == kForward || dir == kInverse);
    assert(re != nullptr && im != nullptr);
    assert(N == 1 || (twRe != nullptr && twIm != nullptr));

    if (N <= 1)
        return;
    bit_reverse(re, im, N);

    double const imSign = (dir == kForward) ? 1.0 : -1.0;
    for (int len = 2; len <= N; len <<= 1) {
        butterfly_scalar(re, im, N, len, twRe, twIm, imSign);
    }

    if (dir == kInverse) {
        double const inv = 1.0 / N;
        for (int i = 0; i < N; ++i) {
            re[i] *= inv;
            im[i] *= inv;
        }
    }
}

void radix2_fft(double *re, double *im, int N, FFTDirection dir)
{
    assert(is_pow2(N));
    if (N <= 1)
        return;
    std::vector<double> twRe(N / 2), twIm(N / 2);
    compute_twiddles(N, twRe.data(), twIm.data());
    radix2_fft_with_twiddles(re, im, N, twRe.data(), twIm.data(), dir);
}

} // namespace Inkscape::Spectral
