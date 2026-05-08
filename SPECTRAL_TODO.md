# Spectral effects for Inkscape — TODO

Mirroring the Skia branch's tier structure. The Skia branch demonstrated
that the substrate is reusable across rasterizers; Inkscape's
contribution is the *integration* of that substrate into the SVG filter
pipeline.

**Checkbox convention:**
- `[ ]` — not yet attempted
- `[x]` — done; details in commit history or `SPECTRAL_PROGRESS.md`
- `[-]` — considered and decided against, with reasoning recorded
        inline. Distinguishes "looked at it, no" from "haven't gotten
        to it."

## Tier 1 — Substrate port

The Skia branch's mathematical primitives port directly. Strip
Skia-specific helpers (SkASSERT, AutoTMalloc, skvx, SkToSizeT) and
relicense to GPL-2+ to match Inkscape.

- [ ] `src/display/spectral/spectral-fft.{h,cpp}` — radix-2 complex
      FFT, length-must-be-power-of-2.
- [ ] `src/display/spectral/spectral-dct.{h,cpp}` — DCT-II / DCT-III
      via Makhoul (length-N/2 complex FFT for real input). 1D and 2D.
      `apply_lattice_heat_kernel()` for the per-mode `exp(-(σ²/2)·λ)`
      multiply.
- [ ] `src/display/spectral/spectral-blur.{h,cpp}` — the SSoT
      operator `apply_spectral_heat_kernel_a8()` and an RGBA-channel
      wrapper. This is the function a future bilateral / SDF /
      drop-shadow primitive would call into.
- [ ] Unit tests under `testfiles/src/display/` — DCT round-trip,
      FFT round-trip + Parseval + linearity, heat-kernel composition.
      Mirrors `tests/SkRadix2FFTTest.cpp` and `tests/LatticeDCTTest.cpp`
      from the Skia branch.

## Tier 2 — feGaussianBlur σ-threshold dispatch — `[-]` (rejected)

- [-] **Spectral DCT third tier above some σ cutoff.**

      *Tested and rejected for perf reasons.* Full bench results
      under SPECTRAL_PROGRESS.md §5: spectral path is 22–50× slower
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
      spectral substrate revisit needs both.

## Tier 3 — New SVG filter primitives (capability, not displacement)

These add functionality Inkscape doesn't currently have. The argument
isn't perf — it's filter primitives that don't exist today.

- [ ] **`feSpectralBilateral`** — non-standard SVG filter for
      edge-preserving smoothing. Useful for stylized photographic
      effects on vectorized photos. Powered by
      `SkSpectralBilateralBlurRGBA8888` ported from the Skia branch.
- [ ] **`feSpectralDistance`** — heat-kernel SDF. Could replace or
      complement `feMorphology` with smoother dilate/erode behavior.
- [ ] **`feSpectralNoise`** — power-spectrum-controlled noise as
      alternative to `feTurbulence`'s Perlin. Same four profiles as
      Skia: white, pink, brown, blue.
- [ ] Each of these requires SVG parser registration in
      `src/object/sp-fe*.cpp` plus filter-primitive subclass under
      `src/display/`.

## Tier 4 — Tests + GMs + bench

- [ ] DM-equivalent visual regression test fixtures under
      `testfiles/rendering_tests/` — one SVG per spectral primitive,
      golden PNG generated on first run.
- [ ] Bench harness measuring large-σ blur on print-resolution
      canvases: 4000×4000 pixel render, σ ∈ {32, 64, 128}. Vanilla
      IIR vs spectral DCT, side by side. Report wall-clock and
      establishes `kSpectralCutoff`.
- [ ] CLI cross-check: `inkscape --export-png` produces identical
      output between IIR and spectral paths above the cutoff (within
      quantization noise).

## Tier 5 — Substrate features that don't gate the first land

These are deferrable. Listed here so future work has a place to land
without losing the SSoT story.

- [ ] Substrate SIMD (skvx-equivalent or std::simd) for the FFT
      butterflies. The Skia branch had this; ported version starts
      scalar.
- [ ] Twiddle cache + Makhoul real-input FFT. Skia branch landed this
      as a 2-3× speedup; ported version starts with the simpler
      length-2N approach and adds the cache when bench shows it
      mattering.
- [ ] Anisotropic σ_x ≠ σ_y in the spectral path. Skia branch
      handles it via per-axis decay tables. Same approach should port.

## Tier 6 — Tested-and-rejected decisions

This section will populate as work lands. Every `[-]` carries the
math reasoning that justifies the rejection, so future contributors
can see what's been ruled out and why.

(empty until first rejection — but expect entries similar to Skia's:
Bluestein, BIP for joint-RGBA, GPU multi-pass, etc.)

## Out of scope for this branch

- The `libcola` AVX2 dot product — that's Inkscape graph layout perf,
  legitimate but unrelated to the spectral framework. If someone wants
  it, it's a separate CL.
- Any modification to `alignment-snapper.cpp`, color-space conversions,
  Quake-style fast-inverse-sqrt, BAM atan2 — none of these are
  spectral methods. The Gemini branch's claims that they were are
  documented in `GEMINI_FAILURE_MODE.md`.
- `gradient_projection.cpp` and other libcola changes — same reasoning.
