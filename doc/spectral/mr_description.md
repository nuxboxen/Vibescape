[Inkscape Developer Documentation](../readme.md) / [Spectral effects](readme.md) /

# Merge request description (draft)

This file holds the prepared description for the upstream merge
request, ready to paste into GitLab's MR form. It's checked in so
the contributor and any future reader can see exactly what claim
is being made to upstream.

---

## Title

`Spectral filter primitives (feSpectralBilateral, feSpectralDistance, feSpectralNoise) — Inkscape extensions`

## Description

### What this MR adds

Three new SVG filter primitives, wired end-to-end through
Inkscape's filter pipeline:

- **`<feSpectralBilateral>`** — edge-preserving smoothing via
  Perona-Malik anisotropic diffusion. Smooths flat regions while
  preserving sharp edges. Useful for stylized photographic effects
  on imported raster content.
- **`<feSpectralDistance>`** — heat-kernel signed distance field
  via Varadhan's classical asymptotic
  (`d ≈ σ · √(-2 · log u_norm)`). Visualizes proximity to the
  input's alpha-mask boundary. Available in signed and unsigned
  modes.
- **`<feSpectralNoise>`** — power-spectrum-controlled synthetic
  noise (white, pink, brown, blue) as an alternative to
  `feTurbulence`'s Perlin. The caller specifies the spectrum
  directly via the eigenmode framing.

All three are reachable from the Filter Effects dialog (with
self-portrait icons rendered by the primitives themselves) or by
writing the corresponding XML directly. None of them replace
existing Inkscape functionality — they're capability additions.

### What this MR does NOT add

Initially the branch attempted a σ-threshold dispatch in
`feGaussianBlur` to route large-σ blurs through a spectral path.
**Bench data falsified the perf claim**: the spectral path is
22-50× slower than the existing van-Vliet IIR at every production
σ on the test machine. That dispatch is disabled
(`constexpr bool use_spectral = false`) and the decision recorded
as `[-]` with full bench numbers in `progress.md` §5.
The substrate stays linked because the Tier 3 capability
primitives use it; the dispatch site is a single line that can
be removed entirely if reviewers prefer.

**Hardware-dependence caveat (§5.3).** The bench was run on an
Intel Xeon E5530 (Nehalem, 2009) — SSE4.2 only, no AVX/AVX2/
AVX-512/FMA. Rebuilding with `-march=native -ffast-math` on the
same hardware narrowed the ratio from 22-50× to 18-32× (spectral
auto-vectorizes; van-Vliet IIR's recursive form mostly doesn't).
Estimated AVX-512 impact: another ~3-4× for spectral, ~1.5-2× for
IIR; final ratio probably **3-8×, still favoring IIR**. The
structural floor is the asymptotic difference (van-Vliet O(1)
per pixel vs spectral O(log N) per pixel) — SIMD shifts the
constant; the exponent is what wins.

**GPU compute-shader FFT (§5.4).** Recorded as future-work
breadcrumb. Not feasible for Inkscape in any practical timeframe
— the renderer is Cairo-only with zero GPU compute infrastructure
in the codebase. Tractable on the sibling Skia branch (which has
the substrate); estimated 2-week experiment if a future
contributor wants to test it.

### Methodology — Mathematical Provenance Method

This branch is built under a documented protocol called the
**Mathematical Provenance Method** (`progress.md` §−1).
The protocol exists because LLM-generated framework integrations
routinely produce vocabulary-match code that uses framework names
without implementing the framework operators. MPM is six screens
each commit must pass:

1. Bit-equivalence under benchmark
2. Parity test against reference operator
3. Operator algebra (round-trip, identity, asymptotic limits)
4. Asymptotic profile (cost matches the math prediction)
5. Honest slow-where-slow accounting
6. Recorded structural-defect decisions (`[-]` entries with reasoning)

The Tier 2 dispatch `[-]` is itself an example of MPM working: the
bench falsified the perf claim and the branch pivoted honestly.
The diagnostic counterpart of MPM —
`gemini_failure_mode.md` — characterizes the failure pattern this
discipline is designed to catch.

### Math substrate

Lattice-Laplacian heat kernel on the DCT eigenbasis (Neumann BC,
pad-to-pow-2). The substrate ports cleanly between rasterizers; a
parallel Skia integration is preserved at
`github.com/lemonforest/spectral-skai/tree/spectral-faithful` for
reference. Math primitives are byte-identical between Skia and
Inkscape; only the integration glue differs.

Operators in `src/display/spectral/`:

| File | Provides |
|------|----------|
| `spectral-fft.{h,cpp}` | Radix-2 complex FFT (substrate for Makhoul DCT) |
| `spectral-dct.{h,cpp}` | DCT-II/III with FFT-via-DCT for pow-2 N + per-mode heat-kernel transfer |
| `spectral-blur.{h,cpp}` | `apply_heat_kernel_a8` — the SSoT operator. Used by all three primitives. |
| `spectral-bilateral.{h,cpp}` | Perona-Malik with state-dependent W_ij weights |
| `spectral-distance-field.{h,cpp}` | Varadhan SDF |
| `spectral-noise.{h,cpp}` | Power-spectrum-controlled noise generation |

### Test coverage

- **9 substrate unit tests** — FFT round-trip, Parseval, linearity,
  DCT 1D/2D round-trip, heat-kernel DC preservation, Dirac
  isotropy, σ=0 identity, anisotropic σ.
- **3 parity tests** — disk σ=8/16, step-edge σ=16, all max-abs ≤ 1
  vs continuous Gaussian reference. (Plus one bench fixture in
  the same binary.)
- **5 bilateral tests** — flat-region invariance, edge preservation
  at small σ_range, Gaussian-limit at large σ_range, RGBA flat
  invariance, RGBA colour-edge preservation.
- **5 SDF/noise tests** — SDF sign correctness on disk, SDF
  far-field sentinel clamping, noise reproducibility, noise seed
  sensitivity, noise profile roughness ordering.
- **10 pedantic determinism tests** — byte-determinism across
  noise/heat/bilateral/SDF (4); edge-case grids and parameter
  bounds (4); cross-validation (bilateral → heat-limit, SDF radial
  monotonicity — 2).
- **3 rendering tests** under `testfiles/rendering_tests/` — golden
  PNGs diffed via ImageMagick `compare` at FUZZ 0.05.
- **1 bench harness** — IIR vs spectral wall-clock comparison
  (the data behind the Tier 2 `[-]`).
- **1 compression experiment** — 4 per-input cases + 1 full-report
  case; speculative spectral-SVG study, recorded as breadcrumb.
  See `svg_compression_experiment.md`.

Total: **39 unit-test cases across 6 binaries + 3 rendering tests
+ 1 bench harness + 1 compression experiment** (with 5 cases
inside). All green in `ctest -R spectral`. Run time ~60s on the
test machine, dominated by the pipeline bench (~37s of the 60s).

### Removal map

Three removal granularities depending on reviewer preference:

1. **Reject everything** — revert all 16 commits. Substrate, GUI
   integration, tests, icons, notebook all go away. Net change:
   zero files modified outside the branch's diff.
2. **Keep substrate, drop public APIs** — delete the
   `nr-filter-spectral-*.{h,cpp}`, `object/filters/spectral-*.{h,cpp}`,
   the `NR_FILTER_SPECTRAL_*` enum entries, the dialog wiring, and
   the icons. Substrate in `src/display/spectral/` stays linked
   for any internal consumer. Each public header carries a
   `Removal note for upstream maintainers` block describing the
   exact cleanup map.
3. **Keep everything except the spectral-SVG experiment breadcrumb**
   — delete `svg_compression_experiment.md` and
   `testfiles/src/spectral-compression-experiment-test.cpp`.
   Nothing else depends on either. Net change: −2 files, −~750 lines.

The contributor is one-and-done on Inkscape; no offense will be
taken in any decision, including outright decline. The math, the
tests, and the work record stand on their own.

### Files modified vs added

```
Substrate:
  src/display/spectral/spectral-{fft,dct,blur,bilateral,distance-field,noise}.{h,cpp}
                                                       (new, 6 module pairs)
  src/display/CMakeLists.txt                           (added entries)

SVG filter primitives:
  src/display/nr-filter-spectral-{noise,bilateral,distance}.{h,cpp}
                                                       (new, 3 module pairs)
  src/object/filters/spectral-{noise,bilateral,distance}.{h,cpp}
                                                       (new, 3 module pairs)
  src/object/filters/CMakeLists.txt                    (added entries)
  src/object/sp-factory.cpp                            (3 new svg:fe* tags)
  src/object/tags.h                                    (3 new SPFe* tags)
  src/display/nr-filter-types.h                        (3 new enum values)
  src/attributes.{h,cpp}                               (4 new SVG attributes)
  src/filter-enums.{h,cpp}                             (2 new EnumDataConverters)

Existing-pipeline integration:
  src/display/nr-filter-gaussian.cpp                   (added spectral dispatch
                                                        site, set false; un-statics
                                                        gaussian_pass_IIR for bench)
  src/ui/dialog/filter-effects-dialog.cpp              (3 menu entries, 3 widget
                                                        blocks)

Icons:
  share/icons/hicolor/scalable/actions/feSpectral{Noise,Bilateral,Distance}-icon.svg
                                                       (new, 3 self-portrait SVGs)
  share/icons/hicolor/symbolic/actions/feSpectral*-icon-symbolic.svg
                                                       (new, 3 abstract glyphs)

Tests:
  testfiles/CMakeLists.txt                             (added 6 entries)
  testfiles/src/spectral-{substrate,parity,bilateral,sdf-noise,
    pipeline-bench,pipeline-determinism,compression-experiment}-test.cpp
                                                       (new, 7 test files)
  testfiles/rendering_tests/CMakeLists.txt             (added 3 entries)
  testfiles/rendering_tests/test-spectral-{noise,bilateral,distance}.svg
                                                       (new, 3 SVG fixtures)
  testfiles/rendering_tests/expected_rendering/test-spectral-*.png
                                                       (new, 3 golden PNGs)

Documentation (under doc/spectral/, conforming to
doc/documentation_style.md — `readme.md` index, back-link headers,
lowercase + underscore filenames):
  doc/spectral/readme.md                               (index)
  doc/spectral/progress.md                             (design notebook)
  doc/spectral/todo.md                                 (tier checklist)
  doc/spectral/svg_compression_experiment.md           (breadcrumb)
  doc/spectral/gemini_failure_mode.md                  (MPM diagnostic)
  doc/spectral/mr_description.md                       (this file)
  doc/spectral/icons/*.png                             (3 self-portrait PNGs)
  doc/readme.md                                        (linked spectral/)
  NEWS.md                                              (one section added)

Total: ~50 files. ~5000 lines of code + ~1500 lines of documentation.
```

### AI assistance disclosure

In the spirit of full transparency to reviewers: this branch was
authored with extensive use of **Claude Code** (Anthropic's coding
assistant, primary model: Claude Opus 4.7). The human contributor
directed the work, supplied the antikythera-maths framework
context, made all decisions about scope and methodology, and
reviewed every commit before it landed. The code was generated
incrementally — typically: human asks a focused question or
identifies a problem; AI proposes implementation or analysis;
human reviews, accepts, modifies, or rejects.

Why this is recorded openly:

1. **Reviewer fairness** — readers should know the author profile
   when calibrating critique. AI-generated code can fail in
   characteristic ways (the `gemini_failure_mode.md` document in
   this notebook is exactly such a characterization). The
   Mathematical Provenance Method (`progress.md` §−1) was designed
   in part to catch those failure modes; this branch holds itself
   to the protocol but reviewers shouldn't have to take that on
   faith.
2. **Reproducibility** — the work record (`progress.md`,
   `todo.md`, the bench data in §5.3, the spectral-SVG experiment
   report) is structured so a human or another AI could
   independently verify each claim. No "trust me, the math
   works" — every operator has parity tests, every
   design decision has rejection criteria with bench numbers.
3. **Inkscape's contribution policy** — CONTRIBUTING.md doesn't
   address AI-assisted contributions explicitly. Disclosing
   prominently is the conservative reading.

If reviewers prefer different conventions for AI disclosure, the
contributor will follow whatever guidance Inkscape adopts.

### How to verify locally

```bash
# Build
cmake -B build -GNinja -DBUILD_TESTING=ON
ninja -C build

# All spectral tests (32 individual tests across 6 binaries + 3 render)
ctest -R 'spectral|render_test-spectral' --test-dir build --output-on-failure

# Smoke-test rendering by exporting a test SVG to PNG
./build/bin/inkscape --export-type=png \
  --export-filename=/tmp/out.png \
  testfiles/rendering_tests/test-spectral-bilateral.svg

# Read the design rationale
less progress.md     # leading note has the orientation
less todo.md         # tier-by-tier completion checklist
```

### Reviewers — questions worth asking

1. Is "non-standard SVG filter primitives via the `svg:` namespace"
   acceptable? Alternatives: `inkscape:` namespace (standards-clean
   but visible-only-in-Inkscape), or wait for upstream W3C
   standardization (likely never). The current branch uses `svg:`
   matching feTurbulence's existing precedent for primitives that
   originated in SVG 1.1 spec but have non-standard parameters.

2. The bench shows IIR wins by 22-50×. The branch keeps the
   spectral substrate compiled in for the capability primitives.
   Is the substrate's binary-size cost (~30KB compiled) acceptable
   for the three new filter primitives? Removal of substrate is
   only possible if all three primitives are also removed.

3. The icons are self-portraits (the icon is rendered by the
   primitive it represents). They render correctly in Inkscape
   and *do not* render in any other SVG viewer. Is this elegant
   or confusing? An alternative is conventional iconography that
   doesn't depend on the primitives — would mean writing custom
   icons by hand.

4. The spectral-SVG compression experiment
   (`svg_compression_experiment.md`) is breadcrumb material for a
   future research direction. Reviewers can ask for it to be
   dropped (option 3 in the removal map above) without affecting
   any production code.

---

*AI authorship: this document was authored with [Claude Code](https://claude.com/claude-code) (Anthropic, primary model: Claude Opus 4.7). The human contributor (lemonforest@gitlab) directed the work, supplied the antikythera-maths framework context, made all scope/methodology decisions, and reviewed every commit before it landed. See [readme.md](readme.md) and [mr_description.md](mr_description.md) for the full disclosure and methodology context.*
