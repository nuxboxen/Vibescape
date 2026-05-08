// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spectral substrate: minimal radix-2 complex FFT.
 *
 * Length must be a positive power of 2. SOA layout: separate `re` and
 * `im` double arrays.
 *
 * Forward direction (kForward) is the unnormalized DFT:
 *   X[k] = sum_{n=0}^{N-1} x[n] · exp(-i · 2π · k · n / N)
 * Inverse (kInverse) divides by N so ifft(fft(x)) == x.
 *
 * This is a straight port of `src/core/SkRadix2FFT.{h,cpp}` from the
 * Skia spectral-faithful branch
 * (https://github.com/lemonforest/spectral-skai/tree/spectral-faithful)
 * with skvx SIMD removed and Skia helpers replaced by std equivalents.
 * The math is byte-identical; the parity tests under the Skia branch
 * carry over.
 *
 * Authors:
 *   mlehaptics (originally landed in Skia under BSD-3)
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more
 * information.
 */

#ifndef INKSCAPE_DISPLAY_SPECTRAL_FFT_H
#define INKSCAPE_DISPLAY_SPECTRAL_FFT_H

namespace Inkscape::Spectral {

enum FFTDirection {
    kForward = -1,
    kInverse = +1,
};

// Convenience: in-place radix-2 FFT, computes twiddles internally.
void radix2_fft(double *re, double *im, int N, FFTDirection dir);

// Hot-path: in-place FFT. twiddleRe/twiddleIm must be at least N/2
// entries each, with twiddleRe[k] = cos(-2π k/N), twiddleIm[k] =
// sin(-2π k/N) for k = 0..N/2-1. Inverse direction reuses the same
// tables (sign flip on the imaginary lane is handled inside the
// butterfly).
void radix2_fft_with_twiddles(double *re, double *im, int N,
                               const double *twiddleRe,
                               const double *twiddleIm,
                               FFTDirection dir);

// Populate twiddle tables for length N.
void compute_twiddles(int N, double *twiddleRe, double *twiddleIm);

inline int next_pow2(int x)
{
    if (x <= 1) return 1;
    int p = 1;
    while (p < x) p <<= 1;
    return p;
}

} // namespace Inkscape::Spectral

#endif // INKSCAPE_DISPLAY_SPECTRAL_FFT_H
