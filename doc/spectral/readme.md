[Inkscape Developer Documentation](../readme.md) /

# Spectral filter primitives — design notebook

This subdirectory holds the design rationale, math derivations,
empirical findings, and methodology notes for the
**spectral filter primitives** contributed to Inkscape on the
`spectral-faithful` branch. The contribution adds three new SVG
filter primitives —  `feSpectralBilateral`, `feSpectralDistance`,
`feSpectralNoise` — wired through the existing filter pipeline
(renderer + parser + element registration + GUI editor + rendering
tests + icons).

The notebook is structured for a reviewer who wants to verify the
work, not just read about it. Recommended reading order:

| Document | What                                                                    |
|----------|-------------------------------------------------------------------------|
| [progress.md](progress.md) | The main design notebook. §−1 names the methodology (Mathematical Provenance Method) and lists six screening criteria; §§0–4 cover provenance, math, and self-screening; §5 carries the bench data that turned the σ-threshold dispatch into a `[-]` decision; §6 covers the self-portrait icons; §7 is the spectral-SVG experiment breadcrumb. |
| [todo.md](todo.md) | Tier-by-tier completion checklist with commit hashes. `[x]` done, `[-]` tested-and-rejected with inline reasoning, `[ ]` future work. |
| [gemini_failure_mode.md](gemini_failure_mode.md) | The diagnostic counterpart of the Mathematical Provenance Method — characterizes the LLM-generated "vocabulary-match" failure pattern that MPM is designed to catch. Drawn from two real attempts (one Skia, one Inkscape). The six MPM screens in `progress.md` §−1 derive directly from the failure modes documented here. |
| [svg_compression_experiment.md](svg_compression_experiment.md) | Speculative breadcrumb — does the framework's eigenbasis-projection + FFT-residual-recovery pattern apply to raster image compression? Short answer: yes for vector-graphics content, no for natural images. Recorded for any future contributor wanting to follow up on a hypothetical "spectral-SVG" file format. |
| [mr_description.md](mr_description.md) | Prepared description for the upstream merge request, ready to paste into GitLab. Title, scope, methodology pointer, removal map at three granularities, file inventory, local-verification instructions. |
| [icons/](icons/) | PNG renders of the three self-portrait scalable icons, embedded in `progress.md` §6. Useful for readers viewing this notebook outside an Inkscape build (the SVG icons require Inkscape's renderer to display correctly — they use the spectral primitives themselves to render). |

## Disposition

If carrying any of this isn't a fit, every public surface ships
with explicit removal notes. The substrate in
`src/display/spectral/` can stay linked for any internal consumer
regardless of whether the public APIs land. See
[mr_description.md](mr_description.md) for the three-level removal
map (revert all / keep substrate, drop public APIs / drop
breadcrumbs only).

The contributor is one-and-done; no offense will be taken in any
decision, including outright decline.

## AI assistance disclosure

This branch was authored with extensive use of **Claude Code**
(Anthropic's coding assistant, primary model: Claude Opus 4.7).
The human contributor directed the work, supplied the
antikythera-maths framework context, made all scope/methodology
decisions, and reviewed every commit. See
[mr_description.md](mr_description.md) for the full disclosure
and the connection to the Mathematical Provenance Method (which
was designed in part to catch AI-characteristic failure modes —
see [gemini_failure_mode.md](gemini_failure_mode.md)).

## Out-of-tree provenance

The mathematical substrate this branch ports comes from the
mlehaptics / antikythera-maths spectral framework. Four external
resources worth pointing at:

- **Antikythera-maths spectral notebook** —
  https://mlehaptics.readthedocs.io/en/latest/antikythera-maths/
  — the foundational document describing the lattice-Laplacian
  heat kernel, DCT eigenbasis, Phase-9 BIP residue vectors, etc.
- **Doom93 spectral research notebook** —
  https://mlehaptics.readthedocs.io/en/latest/antikythera-maths/doom_spectral_research_notebook/
  — the framework's first end-to-end primitive-replacement
  exercise (eight FPU-bound subsystems of id Tech 1 translated to
  graph-Laplacian primitives). Methodologically the closest
  precedent to this work — also a "replace where it maps cleanly,
  leave alone where it doesn't" discipline.
- **Ephemerides spectral research notebook** —
  https://mlehaptics.readthedocs.io/en/latest/antikythera-maths/ephemerides_spectral_research_notebook/
  — application of the same framework to celestial mechanics; the
  FFT-residual-coupling pattern in `svg_compression_experiment.md`
  has its native habitat there.
- **`ephemerides-spectral` PyPI package** —
  https://pypi.org/project/ephemerides-spectral/ — native-C
  reference implementation of the framework's BIP and Fiedler-
  partition primitives.

## Sibling work

A parallel Skia integration is preserved at
https://github.com/lemonforest/spectral-skai/tree/spectral-faithful.
Math primitives are byte-identical between Skia and Inkscape; only
the integration glue differs. The Skia branch closed with the same
shape of finding (vanilla blur wins; capability primitives are the
contribution) at smaller margin (5–13× there vs 22–50× here, since
Inkscape's IIR is more aggressively optimized than Skia's separable
Gaussian).
