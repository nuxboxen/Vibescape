[Inkscape Developer Documentation](../readme.md) / [Spectral effects](readme.md) /

# Spectral effects for Inkscape — TODO

Mirroring the Skia branch's tier structure. The Skia branch demonstrated
that the substrate is reusable across rasterizers; Inkscape's
contribution is the *integration* of that substrate into the SVG filter
pipeline.

**Checkbox convention:**
- `[ ]` — not yet attempted
- `[x]` — done; details in commit history or `progress.md`
- `[-]` — considered and decided against, with reasoning recorded
        inline. Distinguishes "looked at it, no" from "haven't gotten
        to it."

## Tier 1 — Substrate port

The Skia branch's mathematical primitives port directly. Strip
Skia-specific helpers (SkASSERT, AutoTMalloc, skvx, SkToSizeT) and
relicense to GPL-2+ to match Inkscape.

- [x] `src/display/spectral/spectral-fft.{h,cpp}` — radix-2 complex
      FFT, length-must-be-power-of-2. *Landed in `30c9021a6c`.*
- [x] `src/display/spectral/spectral-dct.{h,cpp}` — DCT-II / DCT-III
      with `apply_lattice_heat_kernel()` for the per-mode
      `exp(-(σ²/2)·λ)` multiply. Direct O(N²) initial port in
      `30c9021a6c`; Makhoul FFT-via-DCT optimization in
      `134ad24ae2` (30× speedup of substrate test suite).
- [x] `src/display/spectral/spectral-blur.{h,cpp}` — the SSoT
      operator `apply_heat_kernel_a8()` and an RGBA-channel
      wrapper `blur_bgra` for Cairo's native ARGB32. *Landed in
      `30c9021a6c`.*
- [x] Unit tests under `testfiles/src/spectral-substrate-test.cpp`
      — 9 tests covering FFT round-trip, FFT Parseval, FFT
      linearity, DCT 1D round-trip, DCT 2D round-trip, heat-kernel
      DC preservation, Dirac isotropy, σ=0 identity, anisotropic
      σ. *Landed in `32a2374c33`.*

## Tier 2 — feGaussianBlur σ-threshold dispatch — `[-]` (rejected)

- [-] **Spectral DCT third tier above some σ cutoff.**

      *Tested and rejected for perf reasons.* Full bench results
      under progress.md §5: spectral path is 22–50× slower
      than van-Vliet IIR at every (canvas, σ) combination measured
      across {512², 1024², 2048²} × {8, 16, 32, 64, 128}. No
      crossover exists in the production σ regime; extrapolation
      suggests crossover would land at σ ≈ 500+ on multi-thousand-pixel
      canvases, well past anything Inkscape rasterizes in practice.

      The constant-factor gap (IIR ~50 ns/px, spectral ~1600 ns/px)
      is structural — IIR's Triggs-Sdika fixed-point recursive filter
      vs spectral's double-precision DCT plus pad-to-pow-2 doubling.
      Substrate SIMD (Tier 5.x) would narrow but not close it.

      Disposition: dispatch site in `nr-filter-gaussian.cpp` left
      as `constexpr bool use_spectral = false` so the substrate
      stays linked for Tier 3 capability primitives. Removing the
      dispatch entirely is a clean cut.

      Parity test (`testfiles/src/spectral-parity-test.cpp`) and
      bench harness (`testfiles/src/spectral-pipeline-bench.cpp`)
      kept — they document the operator correctness and the perf
      reality respectively, and any future contributor evaluating a
      spectral substrate revisit needs both. *Recorded in
      `b520d2ddd0`.*

## Tier 3 — New SVG filter primitives (capability, not displacement)

These add functionality Inkscape doesn't currently have. The argument
isn't perf — it's filter primitives that don't exist today.

- [x] **Substrate operators ported.**
      - `bilateral_a8` + `bilateral_bgra` (Perona-Malik, joint-similarity
        RGBA) in `dc9bda109d`.
      - `distance_field_a8` + `signed_distance_field_a8` (Varadhan
        heat-kernel SDF) in `a8885322f1`.
      - `noise_generate_a8` (white/pink/brown/blue power-spectrum
        synthesis) in `a8885322f1`.
- [x] **`feSpectralNoise`** SVG filter primitive (renderer + parser
      + element registration). *Landed in `3cce97a440`.*
- [x] **`feSpectralBilateral`** SVG filter primitive. *Landed in
      `4e636a4f12`.*
- [x] **`feSpectralDistance`** SVG filter primitive. *Landed in
      `4e636a4f12`.*
- [x] **Rendering test fixtures** + golden PNGs under
      `testfiles/rendering_tests/` for all three primitives, with
      `add_rendering_test()` CMake wiring at FUZZ 0.05. *Landed in
      `f7cae7655c`.*
- [x] **Pedantic determinism + cross-validation tests** —
      `testfiles/src/spectral-pipeline-determinism-test.cpp`. 10
      tests covering byte-determinism across operators, edge-case
      grids (1×1, 1×N, non-pow-2), bilateral ↔ heat-kernel
      Gaussian-limit cross-validation, SDF radial monotonicity.
      *Landed in `80a77cd7d3`.*
- [x] **Filter-effects-dialog GUI integration.** Three new entries
      in the popup menu and three settings blocks for the parameter
      widgets. *Landed in `8be41f8b04`.*
- [x] **Self-portrait icons.** Each spectral primitive's scalable
      icon uses the primitive itself to render the icon — Inkscape's
      renderer dispatches through our spectral filter pipeline at
      icon-load time. Symbolic 16×16 versions are abstract single-
      color glyphs. *Landed in `3bedf0493d`.*

## Tier 4 — Tests + bench

- [x] **Visual regression test fixtures** under
      `testfiles/rendering_tests/`. *Landed in `f7cae7655c`.*
- [x] **Bench harness** measuring vanilla IIR vs spectral. The
      result became the Tier 2 `[-]` decision. *Landed in
      `b520d2ddd0`.*
- [x] **Parity test** vs continuous Gaussian. Three cases (disk
      σ=8, disk σ=16, step-edge σ=16) all max-abs ≤ 1 within the
      4σ-margin interior. *Landed in `a8851f6977`.*

## Tier 5 — Substrate features that don't gate the first land

These are deferrable. Listed here so future work has a place to land
without losing the SSoT story.

- [x] **Twiddle cache + Makhoul real-input FFT.** Landed in
      `134ad24ae2`. 30× speedup on the substrate test suite (155 ms
      → 5 ms). The deeper Skia-style optimizations (separate
      twiddle pre-compute keyed on N, SIMD on butterflies past the
      final stage) are not yet ported.
- [ ] **Substrate SIMD** (skvx-equivalent or std::simd) for the FFT
      butterflies. The Skia branch had this; ported version starts
      scalar. Would close some of the constant-factor gap to IIR
      noted in Tier 2 but not enough to make spectral blur
      competitive.
- [x] **Anisotropic σ_x ≠ σ_y in the spectral path.** Already
      supported — `apply_lattice_heat_kernel` and
      `apply_heat_kernel_a8` both take per-axis sigmas and use
      independent decay tables. Tested by the
      `AnisotropicSigmaSpreadsCorrespondingly` substrate test.

## Tier 6 — Tested-and-rejected decisions

Every `[-]` carries the math reasoning that justifies the rejection,
so future contributors can see what's been ruled out and why.

- [-] **Spectral DCT dispatch in feGaussianBlur.** See Tier 2
      above. Bench shows 22–50× slower than van-Vliet IIR at every
      production σ; recorded with full numbers as evidence.

(Future entries may include: BIP joint-similarity for RGBA
bilateral, periodic-boundary FFT instead of pad-to-pow-2 DCT, etc.
None tested yet on this branch.)

## Out of scope for this branch

- The `libcola` AVX2 dot product — that's Inkscape graph layout perf,
  legitimate but unrelated to the spectral framework. If someone wants
  it, it's a separate CL.
- Any modification to `alignment-snapper.cpp`, color-space conversions,
  Quake-style fast-inverse-sqrt, BAM atan2 — none of these are
  spectral methods. The Gemini branch's claims that they were are
  documented in `gemini_failure_mode.md`.
- `gradient_projection.cpp` and other libcola changes — same reasoning.

## Polish bucket — pre-MR checklist

These are bookkeeping/finishing items remaining before opening a
merge request. None gate the technical work; all are about
presentation:

- [ ] **`NEWS.md` entry** mentioning the three new SVG filter
      primitives and noting them as Inkscape extensions (not
      standard SVG 1.1).
- [ ] **MR description draft** that surfaces: the MPM screening
      protocol, the bench finding (spectral blur disabled with
      data), the capability primitives as the actual contribution,
      the spectral-SVG experiment as a breadcrumb, and the
      removal map for any subset reviewers don't want.
- [ ] **Final notebook polish** — read-cold reviewer summary
      similar to the Skia branch's leading note. Most of the
      content already exists in `progress.md`; the polish
      is making the entry-point obvious.

## Speculative — the spectral-SVG breadcrumb

Recorded in `svg_compression_experiment.md` (commit
`61fa03d6e6`). Out of scope for this branch; left for whoever
picks it up. See progress.md §7 for the headline finding
and pointer.
