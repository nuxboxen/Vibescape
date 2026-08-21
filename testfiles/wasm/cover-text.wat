;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: text metrics -- the operations inkex's own source calls impossible.
;;
;; Its Text.shape_box() carries the comment "Returns a horrible bounding box that just
;; contains the coord points of the text without width or height (which is impossible to
;; calculate)" and the line `x2 = self.x + 0  # XXX This is impossible to calculate!`. The
;; 1.2-era workaround spawns a second Inkscape to ask.
;;
;; It is not impossible; it needs the shaped layout, which is in this process. The fixture's
;; text is "Hello" at 16px, so the assertions are about shaping actually having happened:
;; five characters, a positive advance width, and per-character extents that progress across
;; the run.
;;
;; Build:  water cover-text.wat -o cover-text.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGTextContentElement" "getNumberOfChars"
    (func $getNumberOfChars (param i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getComputedTextLength"
    (func $getComputedTextLength (param i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getSubStringLength"
    (func $getSubStringLength (param i32 i32 i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getStartPositionOfChar"
    (func $getStartPositionOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getEndPositionOfChar"
    (func $getEndPositionOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getExtentOfChar"
    (func $getExtentOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getRotationOfChar"
    (func $getRotationOfChar (param i32 i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getCharNumAtPosition"
    (func $getCharNumAtPosition (param i32 f64 f64) (result i32)))

  (import "org.inkscape.SVGTextContentElement" "toPath"
    (func $textToPath (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "textString"
    (func $textString (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "styleAtPosition"
    (func $styleAtPosition (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "baselines"
    (func $baselines (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "lineCount"
    (func $lineCount (param i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "fontFamily"
    (func $fontFamily (param i32 i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "label")
  (data (i32.const 8)  "box")
  (data (i32.const 16) "svg:g")
  (data (i32.const 24) "id")
  (data (i32.const 32) "ok-text")
  (data (i32.const 40) "Hello")
  (data (i32.const 48) "multiline")

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  ;; Are the $lenA bytes at 1024 the same as the $lenB bytes at 2048?
  (func $sameBytes (param $lenA i32) (param $lenB i32) (result i32)
    (local $i i32)
    (if (i32.ne (local.get $lenA) (local.get $lenB)) (then (return (i32.const 0))))
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $lenA)))
        (if (i32.ne (i32.load8_u (i32.add (i32.const 1024) (local.get $i)))
                    (i32.load8_u (i32.add (i32.const 2048) (local.get $i))))
          (then (return (i32.const 0))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 1))

  ;; Are the $len bytes at 1024 the literal at $at?
  (func $bytesAre (param $at i32) (param $len i32) (result i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $len)))
        (if (i32.ne (i32.load8_u (i32.add (i32.const 1024) (local.get $i)))
                    (i32.load8_u (i32.add (local.get $at) (local.get $i))))
          (then (return (i32.const 0))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 1))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $label i32) (local $box i32) (local $marker i32) (local $multi i32)
    (local $lenA i32) (local $lenB i32)
    (local $whole f64) (local $part f64) (local $x0 f64) (local $x1 f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $label (call $getElementById (local.get $document) (i32.const 0) (i32.const 5)))
    (local.set $multi (call $getElementById (local.get $document) (i32.const 48) (i32.const 9)))
    (if (i32.eqz (local.get $multi)) (then (return (i32.const 53))))
    (local.set $box (call $getElementById (local.get $document) (i32.const 8) (i32.const 3)))
    (if (i32.eqz (local.get $label)) (then (return (i32.const 1))))

    ;; "Hello" is five characters.
    (if (i32.ne (call $getNumberOfChars (local.get $label)) (i32.const 5))
      (then (return (i32.const 2))))

    ;; A shaped run has a real width. inkex reports nothing here at all.
    (local.set $whole (call $getComputedTextLength (local.get $label)))
    (if (f64.le (local.get $whole) (f64.const 0)) (then (return (i32.const 3))))

    ;; Two characters are narrower than five, and not zero.
    (local.set $part (call $getSubStringLength (local.get $label) (i32.const 0) (i32.const 2)))
    (if (f64.le (local.get $part) (f64.const 0)) (then (return (i32.const 4))))
    (if (f64.ge (local.get $part) (local.get $whole)) (then (return (i32.const 5))))

    ;; Character positions advance along the run.
    (if (i32.ne (call $getStartPositionOfChar (local.get $label) (i32.const 0) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 6))))
    (local.set $x0 (f64.load (i32.const 512)))
    (if (i32.ne (call $getStartPositionOfChar (local.get $label) (i32.const 3) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 7))))
    (local.set $x1 (f64.load (i32.const 512)))
    (if (f64.le (local.get $x1) (local.get $x0)) (then (return (i32.const 8))))

    ;; The end of a character is past its start.
    (if (i32.ne (call $getEndPositionOfChar (local.get $label) (i32.const 0) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 9))))
    (if (f64.le (f64.load (i32.const 512)) (local.get $x0)) (then (return (i32.const 10))))

    ;; A glyph has a real extent -- width and height both non-zero, which is exactly what
    ;; "impossible to calculate" refers to.
    (if (i32.ne (call $getExtentOfChar (local.get $label) (i32.const 0) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 11))))
    (if (f64.le (f64.load (i32.const 528)) (f64.const 0)) (then (return (i32.const 12))))
    (if (f64.le (f64.load (i32.const 536)) (f64.const 0)) (then (return (i32.const 13))))

    ;; Unrotated text reports no rotation.
    (if (f64.ne (call $getRotationOfChar (local.get $label) (i32.const 0)) (f64.const 0))
      (then (return (i32.const 14))))

    ;; A point on the first glyph names a character in range.
    (if (i32.lt_s (call $getCharNumAtPosition (local.get $label) (local.get $x0) (f64.const 150))
                  (i32.const 0))
      (then (return (i32.const 15))))

    ;; A rect is not a text element: the metrics report emptiness rather than trapping, so a
    ;; plugin can ask without first having to know what it is holding.
    (if (i32.ne (call $getNumberOfChars (local.get $box)) (i32.const 0))
      (then (return (i32.const 16))))

    ;; ── text as outlines ──────────────────────────────────────────────────────
    ;; The glyph outlines of the shaped run, as path data. hershey.py vendors a complete
    ;; stroke-font engine because no such operation exists, and every text-to-path extension
    ;; either does the same or shells out to a second Inkscape.
    (if (i32.le_s (call $textToPath (local.get $label) (i32.const 1024) (i32.const 900))
                  (i32.const 0))
      (then (return (i32.const 40))))
    ;; A rect has no glyphs, so it answers absent rather than an empty path.
    (if (i32.ne (call $textToPath (local.get $box) (i32.const 1024) (i32.const 900))
                (i32.const -1))
      (then (return (i32.const 41))))

    ;; ── the characters, as a string ───────────────────────────────────────────
    (if (i32.ne (call $textString (local.get $label) (i32.const 1024) (i32.const 256))
                (i32.const 5))
      (then (return (i32.const 42))))
    (if (i32.eqz (call $bytesAre (i32.const 40) (i32.const 5))) (then (return (i32.const 43))))
    ;; Across two lines the text is joined, not truncated to the first.
    (if (i32.ne (call $textString (local.get $multi) (i32.const 1024) (i32.const 256))
                (i32.const 7))
      (then (return (i32.const 44))))

    ;; ── line structure ────────────────────────────────────────────────────────
    (if (i32.ne (call $lineCount (local.get $label)) (i32.const 1)) (then (return (i32.const 45))))
    (if (i32.ne (call $lineCount (local.get $multi)) (i32.const 2)) (then (return (i32.const 46))))
    (if (i32.ne (call $lineCount (local.get $box)) (i32.const 0)) (then (return (i32.const 47))))

    ;; One baseline per line, each a segment of four doubles.
    (if (i32.ne (call $baselines (local.get $label) (i32.const 512) (i32.const 8))
                (i32.const 1))
      (then (return (i32.const 48))))
    (if (i32.ne (call $baselines (local.get $multi) (i32.const 512) (i32.const 8))
                (i32.const 2))
      (then (return (i32.const 49))))
    ;; The two baselines are at different heights, which is what makes them two lines.
    (if (call $near (f64.load (i32.const 520)) (f64.load (i32.const 552)) (f64.const 0.001))
      (then (return (i32.const 50))))

    ;; ── resolved style and font ───────────────────────────────────────────────
    ;; The style in force AT a character. The resolved style has no locally-set properties, so
    ;; a host writing only what is set answers an empty string.
    (if (i32.le_s (call $styleAtPosition (local.get $label) (i32.const 0)
                                         (i32.const 1024) (i32.const 900)) (i32.const 0))
      (then (return (i32.const 51))))

    ;; The second line of `multiline` overrides fill, so the style at a character in it is not
    ;; the style at a character in the first. A host ignoring the position would return the
    ;; same bytes for both.
    (local.set $lenA (call $styleAtPosition (local.get $multi) (i32.const 0)
                                            (i32.const 1024) (i32.const 900)))
    (local.set $lenB (call $styleAtPosition (local.get $multi) (i32.const 5)
                                            (i32.const 2048) (i32.const 900)))
    (if (i32.le_s (local.get $lenA) (i32.const 0)) (then (return (i32.const 54))))
    (if (i32.le_s (local.get $lenB) (i32.const 0)) (then (return (i32.const 55))))
    (if (call $sameBytes (local.get $lenA) (local.get $lenB)) (then (return (i32.const 56))))
    ;; The family actually SHAPED with, which is the end of the font-family fallback list and
    ;; not necessarily anything the document names.
    (if (i32.le_s (call $fontFamily (local.get $label) (i32.const 0)
                                    (i32.const 1024) (i32.const 256)) (i32.const 0))
      (then (return (i32.const 52))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 16) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 24) (i32.const 2) (i32.const 32) (i32.const 7))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
