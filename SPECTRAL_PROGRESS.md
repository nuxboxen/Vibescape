# Spectral effects for Inkscape — progress notebook

> **For Inkscape reviewers:** this notebook is the design rationale and
> work record for porting the antikythera-maths spectral framework into
> Inkscape's filter rendering pipeline. The contribution is staged on
> the `spectral-faithful` branch off Inkscape `master`. Reading it cold:
>
> - **§1** is the chronological commit summary.
> - **§2** is the math: lattice-Laplacian heat kernel `e^{-tL}` as the
>   single source of truth (SSoT) operator behind blur, RGBA blur,
>   bilateral, distance fields, and noise synthesis.
> - **§3** documents the σ-threshold dispatch — Inkscape's existing
>   van-Vliet IIR is *not* displaced; the spectral path is added as a
>   third dispatch tier above some empirically-determined σ threshold
>   where the heat kernel's flat-in-σ cost becomes competitive.
> - **§4** is the screening protocol from `GeminiPlayground/GEMINI_FAILURE_MODE.md`,
>   applied to every commit on this branch.
> - **§N** (last) carries the bench numbers paired against vanilla
>   Inkscape so reviewers can see the tradeoff explicitly.
>
> This work has a sibling on Skia (preserved at `lemonforest/spectral-skai`
> on GitHub), where the same operator family was integrated against a
> different rasterizer. The math primitives are byte-identical
> between the two; only the integration glue differs. The Skia work
> closed with a candid finding that spectral blur is 5–13× *slower*
> than vanilla in the typical-σ production regime and only catches up
> at σ ≈ 100. The Inkscape work targets the regime where that
> tradeoff actually flips: large-σ blurs on print-resolution canvases
> (the case Inkscape is currently slowest at), plus capability
> additions (bilateral, SDF, spectral noise) that are new SVG filter
> primitives, not displacements of existing ones.
>
> If carrying any of this isn't a fit, every public surface ships with
> a "removal note" describing the clean cut. The contributor is
> one-and-done; no offense will be taken in any decision, including
> outright decline. The math, the tests, and the work record stand on
> their own.

## 0. Why this is being attempted

A prior Gemini-generated branch (in `~/gitlab/GeminiPlayground/inkscape`)
claimed a 3.85× speedup on `14-filters.svg`. On inspection:

- The "spectral diffuse step" was added as forward-Euler iteration on
  the **alpha channel only**, run *before* the existing van-Vliet IIR
  Gaussian. Most of the canvas's alpha decayed to zero before the IIR
  ran. The "speedup" was measured against a build that rendered
  almost nothing.
- `bam_atan2()` literally calls `std::atan2()` and scales the result;
  the "Binary Angle Measurement" label is the only ALU-native thing
  about it.
- `fast_l2()` (alpha-max-plus-beta-min, ~4% error) replaced exact L2
  in `alignment-snapper.cpp`. Snap distances are now direction-dependent.
- AVX2-vectorized dot product in `libcola/conjugate_gradient.cpp` is
  legitimate perf work but is not "spectral" — it's just FMA
  vectorization with a relabel.

Full triage in `~/gitlab/GeminiPlayground/GEMINI_FAILURE_MODE.md`. That
document characterizes Gemini's failure mode (lexical groundedness
without operator groundedness) and gives the six screening criteria
this branch holds itself to.

## 1. Commit log

(filled in as work lands)

## 2. The math: one operator, several primitives

(populated when substrate ports)

## 3. Dispatch design — σ threshold above the IIR tier

Inkscape's `nr-filter-gaussian.cpp::render_cairo` already dispatches
between two implementations based on σ:

```cpp
bool use_IIR_x = deviation_x > 3;   // van-Vliet recursive filter
bool use_IIR_y = deviation_y > 3;
// else: FIR (boxed convolution)
```

For σ ≤ 3, FIR is dominant (cheap at small kernels). For 3 < σ ≤ N,
van-Vliet IIR is the production path — *very* well-tuned, recursive,
O(1) per pixel independent of σ.

The spectral path is added as a **third tier** above some
empirically-determined cutoff `kSpectralCutoff`:

```cpp
bool use_spectral_x = deviation_x > kSpectralCutoff;  // ~30-50?
```

The cutoff is set by the bench harness — it's the σ where the spectral
DCT path's flat-in-σ cost crosses the van-Vliet IIR cost. Below the
cutoff, IIR remains the canonical implementation; the spectral path
exists for the σ regime IIR doesn't dominate (large blurs on
print-resolution canvases).

This is *additive*: nothing existing is displaced. Removing the
spectral path is a clean revert.

## 4. Self-screening against the Gemini failure pattern

Every commit on this branch must pass the six screens described in
`~/gitlab/GeminiPlayground/GEMINI_FAILURE_MODE.md`. Reproduced here
for convenience:

1. **Bit-equivalence under benchmark.** Spectral output diff'd
   against IIR output at the threshold σ. Max-abs-pixel-diff bounded
   by quantization noise (a few units of 8-bit).
2. **Parity test against the reference.** Continuous Gaussian as
   ground truth. Max error in the spectral path bounded.
3. **Operator algebra.** DCT round-trip, FFT linearity, heat-kernel
   composition (`e^{-(t1+t2)L} == e^{-t1 L} e^{-t2 L}`) — all tested
   directly on the substrate, not just via consumers.
4. **Asymptotic profile.** Bench numbers must show flat-in-σ cost at
   fixed grid size. If the cost curve doesn't match the math
   prediction, the operator isn't being applied correctly.
5. **Honest slow-where-slow accounting.** Bench results must include
   the σ regime where vanilla IIR wins, not just the regime where
   spectral wins. The crossover is the point of the integration.
6. **Recorded structural-defect decisions.** Every `[-]` in
   SPECTRAL_TODO.md (tested-and-rejected) carries an inline reasoning
   block, just like the Skia work did.

A commit that doesn't pass these screens does not land.
