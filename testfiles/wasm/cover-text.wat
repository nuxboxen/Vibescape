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

  (memory (export "memory") 1)

  (data (i32.const 0)  "label")
  (data (i32.const 8)  "box")
  (data (i32.const 16) "svg:g")
  (data (i32.const 24) "id")
  (data (i32.const 32) "ok-text")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $label i32) (local $box i32) (local $marker i32)
    (local $whole f64) (local $part f64) (local $x0 f64) (local $x1 f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $label (call $getElementById (local.get $document) (i32.const 0) (i32.const 5)))
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

    (local.set $marker (call $createElement (local.get $document) (i32.const 16) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 24) (i32.const 2) (i32.const 32) (i32.const 7))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
