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

## 0. Framework provenance

The mathematical substrate this branch ports comes from the
mlehaptics / antikythera-maths spectral framework. Three resources
worth pointing reviewers at:

- **Antikythera-maths spectral notebook** —
  https://mlehaptics.readthedocs.io/en/latest/antikythera-maths/ —
  the foundational document. Lattice-Laplacian heat kernel, DCT
  eigenbasis, Phase-9 BIP residue vectors, AcuteCount integer-ALU
  similarity proxy, FPU-lifted inner product.

- **Ephemerides spectral research notebook** —
  https://mlehaptics.readthedocs.io/en/latest/antikythera-maths/ephemerides_spectral_research_notebook/ —
  an *application* of the same framework to celestial mechanics
  (52-body solar system, JPL DE441). It does **not** introduce new
  graphics-relevant primitives. It does demonstrate the same
  eigenbasis machinery scaling to a very different domain (orbit
  prediction, ITN-chain search, body classification). Useful as
  evidence that the substrate is general-purpose; not a source of
  new operators for Inkscape.

- **`ephemerides-spectral` PyPI package** —
  https://pypi.org/project/ephemerides-spectral/ — native-C reference
  implementation of the framework's BIP and Fiedler-partition
  primitives applied to astronomy. Cited for empirical performance
  claims ("305× FPU-less speedup", "~1000× native C backend") that
  characterize the framework's value outside graphics. Not a
  dependency of this branch; the substrate is reimplemented locally
  for Inkscape's GPL licensing and to keep the build self-contained.

## 0.5. Why this is being attempted

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

## 5. Bench results — spectral path 25–50× slower than van-Vliet IIR

`testfiles/src/spectral-pipeline-bench.cpp` measures the spectral
DCT path (`Inkscape::Spectral::blur_bgra`) against Inkscape's
production `gaussian_pass_IIR` (van-Vliet recursive filter with
Triggs-Sdika boundary handling) on the same Cairo ARGB32 surface
at matched σ values. Local Release results (single run; bot numbers
will dominate but constant-factor ratio should hold):

| Canvas    | σ   | IIR (ms) | Spectral (ms) | Spectral/IIR |
|-----------|-----|---------:|--------------:|-------------:|
| 512×512   | 8   |    10.3  |        450    |      44×     |
| 512×512   | 16  |    12.1  |        478    |      40×     |
| 512×512   | 32  |    10.1  |        372    |      37×     |
| 512×512   | 64  |    10.3  |        374    |      36×     |
| 512×512   | 128 |    10.8  |        540    |      50×     |
| 1024×1024 | 32  |    53.2  |       1679    |      32×     |
| 1024×1024 | 128 |    45.8  |       1780    |      39×     |
| 2048×2048 | 32  |   210    |       6672    |      32×     |
| 2048×2048 | 64  |   248    |       7248    |      29×     |
| 2048×2048 | 128 |   291    |       6585    |      23×     |

What the numbers say:

- **Both paths are flat in σ at fixed grid size** (theory confirmed).
  IIR's recursive filter is O(1) per pixel regardless of σ; the
  spectral DCT's per-mode multiply is also flat in σ.
- **IIR's per-pixel cost is ~50 ns**; spectral's is ~1600 ns. The
  ratio is a constant-factor gap of ~32×. It does not close at any
  σ tested.
- **No crossover exists in the production σ regime.** Extrapolating
  the trends (which barely move with σ), spectral might catch up at
  σ ≈ 500+ on a multi-thousand-pixel canvas — well past anything
  Inkscape rasterizes in practice.
- **The constant-factor gap is structural.** IIR runs in fixed-point
  with Triggs-Sdika init; spectral runs double-precision DCT plus
  pad-to-pow-2 (which itself doubles work for awkward sizes). Even
  with substrate SIMD (Tier 5.x deferred) the gap narrows but does
  not close.

This matches the Skia branch's finding (5–13× slower) but is more
extreme: Inkscape's IIR is more aggressively tuned than Skia's
separable Gaussian, so the spectral path's relative position is
worse.

### 5.1 Disposition: spectral blur dispatch is `[-]`

Per the framework's "patch shrinks residual" discipline: the perf
metric the spectral blur was supposed to move (large-σ blur cost on
print-resolution canvases) is *not* moved by this implementation.
The IIR baseline already minimizes it.

Recorded in `SPECTRAL_TODO.md` §2 as `[-]` — tested and rejected for
perf reasons, with the bench numbers as the rejection evidence.

The dispatch site in `nr-filter-gaussian.cpp::render_cairo` is
disabled (`use_spectral = false`) but kept as a single-line constexpr
so the spectral substrate stays linked. The substrate is what the
Tier 3 capability primitives (bilateral, SDF, noise) require —
they're not perf-claim primitives, they're *new SVG filter
primitives Inkscape doesn't have today*.

### 5.2 Where the value actually lives — Tier 3

The branch's contribution shifts from "faster blur" to "new filter
capabilities":

- **`feSpectralBilateral`** — edge-preserving smoothing. Inkscape
  has no bilateral filter primitive. Useful for stylized photographic
  effects on imported raster content embedded in SVGs.
- **`feSpectralDistance`** — heat-kernel SDF. Could complement
  `feMorphology` with smoother dilate/erode behavior, and is the
  substrate for stroke-based effects.
- **`feSpectralNoise`** — power-spectrum-controlled noise. The SVG
  spec only ships Perlin-style `feTurbulence`; spectral noise gives
  white/pink/brown/blue spectra directly.

These don't compete with anything fast that already exists. They're
additions, not displacements. Tier 3 is now the load-bearing tier
of this branch.

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
