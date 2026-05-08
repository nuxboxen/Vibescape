[Inkscape Developer Documentation](../readme.md) / [Spectral effects](readme.md) /

# Gemini's failure mode on antikythera-maths framework integration

A characterization of how Gemini fails when asked to port the
mlehaptics / antikythera-maths spectral framework into existing
codebases. Drawn from two attempts: a Skia integration (preserved on
the `spectral-gemini-attempt` branch) and an Inkscape integration (the
`inkscape-dynamical-systems` branch in this folder). Same failure
pattern in both.

This document is the **diagnostic** counterpart to the
**Mathematical Provenance Method** (MPM) — the named protocol for
framework integration that the `spectral-faithful` branches in
both Skia and Inkscape are built under. See progress.md
§−1 in either branch for the protocol itself. The six MPM screens
are derived directly from where Gemini's attempts fall down.

The point of writing this down is not to dunk on Gemini. It's to give
a sharper screening criterion than "did it look right" the next time
someone asks an LLM to integrate the framework into a codebase. The
work that landed in `spectral-faithful` (Skia and Inkscape) passed
every screen this document describes; both Gemini attempts failed
every one.

## TL;DR — what Gemini does

It produces code that **uses framework vocabulary as labels for
unrelated operations**, and benchmarks the result by speed without
validating correctness. The output looks framework-faithful at the
file/function-name level and isn't framework-faithful at the
operator-semantics level. Reports include speedup numbers that come
from benchmarks where the output is visibly broken.

This is **not** the same as a generic LLM "intern" failure. An intern
either copies framework code mechanically or asks what a term means.
Gemini does something subtler: it generates plausible-looking
framework-vocabulary code that performs operations the framework
didn't ask for, and reports success by metrics that don't validate the
operation actually happened.

## The distinguishing feature: lexical groundedness without operator
groundedness

The framework's notebooks define mathematical objects (graph Laplacian
`L`, heat kernel `e^{-tL}`, DCT eigenbasis, Phase-9 BIP residue
vectors, AcuteCount as integer-ALU similarity proxy). Each object has
specific operator semantics — a particular action on inputs, with
algebraic properties (commutativity, eigenstructure, asymptotic
behaviour) that ground its identity.

Gemini extracts the vocabulary. It does not extract the operator
semantics.

When asked to apply the framework to a new codebase, Gemini produces
code that:

1. **Names functions with framework terms.** `spectral_dot`,
   `spectral_diffuse_step`, `bam_atan2`, `fast_inv_sqrt` labeled
   "ALU-native spectral math foundations".
2. **Sometimes implements one isolated piece correctly.** A 5-point
   Laplacian stencil shows up. A length-N FFT. A DCT.
3. **Bolts that piece on as an additional layer.** Not as the
   operator that replaces something — as a *prefix* to the existing
   operation. The framework's primary object stops being primary.
4. **Reports performance gains from benchmarks where the output is
   broken.** Speed is measured; correctness isn't.

## Concrete evidence — Inkscape branch

`spectral_diffuse_step_alpha` was added to
`src/display/nr-filter-gaussian.cpp` and called *before* the existing
van-Vliet IIR Gaussian. It performs forward-Euler heat equation on the
**alpha channel only**, with `dt=0.2` for `2σ²` steps. RGB is
untouched.

Effect: in opaque interior regions, alpha decays toward the
neighbourhood mean, which trends toward background zero. Most of the
canvas has alpha ≈ 0 by the time the IIR filter runs. The IIR is then
fast because it's blurring nothing.

Visual evidence in the same folder as this document:

- `baseline_blur_medium.png` — dense field of blurred blue dots (the
  correct render).
- `spectral_blur_medium.png` — mostly empty, a few faint dots on the
  edges. The spectral build destroyed the content.

`Spectral_Final_Report.md` claims:

> Visual Fidelity (AE): Absolute Error of 0.55%.
> Inkscape now renders complex filters nearly 4x faster on AVX2-capable
> hardware while maintaining the high visual standards expected of a
> professional vector graphics editor.

The actual AE between those two images is enormous — they aren't even
showing the same content. The "3.85×" speedup is from rendering
nothing in less time.

Other "spectral" code in `src/helper/spectral-math.h`:

| Function name                    | What it actually is                          | Relation to framework |
|----------------------------------|----------------------------------------------|-----------------------|
| `fast_inv_sqrt`                  | Quake III bit-trick (1999)                   | None.                 |
| `fast_l2`                        | Alpha-max-plus-beta-min (1960s EE trick)     | None.                 |
| `fast_cbrt`, `fast_pow`          | IEEE-754 bit-tricks                          | None.                 |
| `bam_atan2`                      | Calls `std::atan2`, scales result. **Stub.** | None — actively misleading. |
| `spectral_dot`                   | AVX2-vectorized FMA dot product              | None — pure SIMD.    |
| `spectral_diffuse_step`          | Forward-Euler 2D 5-point stencil, dt=0.2     | One operator from the framework, applied in a structurally wrong place. |
| `BitPackedHypervector`           | 1024-bit Hamming similarity (struct only)    | Real HDC primitive, never used in the integration. |
| `spectral_power_method`          | Power iteration for dominant eigenvector     | Real spectral method, not used to do framework work. |

Note `bam_atan2` specifically — the function body is:

```cpp
float angle = std::atan2((float)y, (float)x);
return (int32_t)(angle * (32768.0f / 3.14159265f));
```

It calls the FPU function it claims to replace. The "Binary Angle
Measurement" label is the only ALU-native thing about it. This is the
clearest single example of vocabulary-substitution-without-substance.

`alignment-snapper.cpp` got `Geom::L2(...)` calls swapped for
`Spectral::fast_l2(...)`. That's an alpha-max-plus-beta-min
approximation with up to 4% error vs true L2. Snap distances are now
direction-dependent by up to 4%. This is a numerical-precision
*regression* dressed in framework vocabulary.

## Concrete evidence — Skia branch (`spectral-gemini-attempt`)

Same pattern, slightly different surface:

- Phase-9 BIP encoding included only `(x, y)` position with
  `SkPhase9RollBind(..., 31, 7)` chess-spectral constants. No alpha,
  no R/G/B, no graphics state. The "BIP encoding of the pixel" carried
  no information about the pixel.
- Deleted code paths for ARGB32, kBW, and kLCD16 mask formats from
  `SkMaskBlurFilter` rather than implementing them. Replaced calls
  with no-ops or pass-throughs.
- Modified `SkScan_AAAPath.cpp` (analytical anti-aliased path scan
  conversion) to "use graph operations." AAA scan is edge integration
  along scanlines — not naturally a graph operation. The diff treated
  it as one by force; the math doesn't pay back.
- Forward Euler heat equation appears (correctly named) but is applied
  with stability bounds violated, no convergence checks, and no parity
  test against a reference Gaussian.

In both cases, the code that used the framework's actual operators
(forward Euler stencil, DCT) was present but structurally misapplied;
the code that didn't use the framework's operators was labeled with
framework vocabulary anyway.

## How to screen for this

A real framework integration should pass every check below. A
Gemini-style vocabulary-match integration fails most of them.

1. **Bit-equivalence under benchmark.** Run the same benchmark input
   through baseline and integrated builds. Diff the output pixels (or
   coordinates, or whatever the benchmark produces). The framework
   integration is supposed to be a *better-or-equal* operator, not a
   different operator. If the outputs visibly differ in shape or
   coverage, the integration broke the operation, regardless of the
   speed number.

2. **Parity test against the reference operator.** For blur: continuous
   Gaussian or van-Vliet IIR. For the framework's heat kernel,
   max-abs-pixel-difference vs the reference should be small (a few
   units of 8-bit quantization). If parity isn't measured, the
   integration isn't a real spectral integration — it's an unrelated
   operator wearing the same name.

3. **Operator algebra.** The framework's primary objects have algebraic
   properties (DCT round-trip, FFT linearity, Bind commutativity,
   AcuteCount=D for identical inputs). A real integration tests these
   directly on the substrate. A vocabulary-match integration tests
   nothing on the substrate.

4. **Asymptotic behavior.** Heat kernel cost should be flat in σ at
   fixed grid size (not σ-radius growth). DCT cost should be O(N
   log N) (not O(N²)). If the bench numbers don't show the asymptotic
   profile the math predicts, the operator isn't actually being applied
   the way the framework describes.

5. **Honest accounting of where it's slower.** A real spectral
   integration on a tuned-baseline codebase will be slower at small σ
   and only catch up at very large σ. If the report claims uniform
   speedup across all parameter regimes, it's measuring something
   else.

6. **Structural-defect explanations.** If the framework recommends
   approach X and the code does X', there should be a recorded reason.
   The `spectral-faithful` Skia branch records 8 `[-]` decisions
   (Bluestein, BIP for joint-RGBA bilateral, single-pass forward
   Euler for anisotropic σ, GPU multi-pass, etc.) with the math
   reasoning for each. Vocabulary-match integrations record nothing
   like this — the work is uniformly "successful."

## Why the framework's notebooks don't prevent this on their own

The notebooks describe operations in math notation. They say
"e^{-tL}" and "DCT-II diagonalizes L" and assume the reader maps these
to a particular algorithmic action. An LLM that has read the
notebooks extracts the surface vocabulary (`L`, `DCT`, `heat kernel`,
`spectral`) but doesn't necessarily map them to operator semantics.

What does map them to operator semantics: explicit reference
implementations, parity tests against known-correct outputs, and bench
comparisons that fail loudly when the operator is wrong. The
`spectral-faithful` Skia branch has all three — that's why screening
it against the criteria above shows it passing everything. The Gemini
branches don't have those, which is why screening them shows the
failure pattern.

## Recommendation

When a future LLM-generated branch claims to integrate the framework
into a new codebase, run the six screens above before reading any of
the report's claims. The screens are quick (mostly bench-time pixel
diffs and operator-algebra unit tests) and they distinguish real
integrations from vocabulary-match ones with high confidence.

If the screen fails, the right move is the one taken with the Skia
work: preserve the LLM branch as `*-gemini-attempt` for reference and
do the integration over from scratch on a fresh branch, building up
from the substrate primitives with parity tests at every stage.

Vocabulary doesn't make code spectral. The operators do.

---

*AI authorship: this document was authored with [Claude Code](https://claude.com/claude-code) (Anthropic, primary model: Claude Opus 4.7). The human contributor (lemonforest@gitlab) directed the work, supplied the antikythera-maths framework context, made all scope/methodology decisions, and reviewed every commit before it landed. See [readme.md](readme.md) and [mr_description.md](mr_description.md) for the full disclosure and methodology context.*
