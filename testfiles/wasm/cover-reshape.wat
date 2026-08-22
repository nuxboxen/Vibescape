;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: reshaping the selection -- booleans, stroke to path, simplify, transforms.
;;
;; Every case here measures the RESULT rather than checking the call returned, because all of
;; these are operations whose only evidence is the geometry afterwards. A boolean that ran and
;; did nothing, or a rotate that wrote a transform the renderer ignores, both look like success
;; from the return value.
;;
;; Measurements are in document coordinates (documentBBox, selectionBounds), not
;; getBBox: several of these operations cannot fold their result into a shape's own attributes
;; and write a transform instead, and getBBox answers in the element's own user space where
;; that transform does not appear.
;;
;; Build:  water cover-reshape.wat -o cover-reshape.wasm

(module
  (import "org.inkscape.Document" "documentElement"      (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"        (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"       (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"          (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"         (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"         (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGGraphicsElement" "documentBBox" (func $docBBox (param i32 i32 i32) (result i32)))

  (import "org.inkscape.Selection" "selectionClear"  (func $selClear))
  (import "org.inkscape.Selection" "selectionAdd"    (func $selAdd (param i32)))
  (import "org.inkscape.Selection" "selectionBounds" (func $selBounds (param i32 i32) (result i32)))

  (import "org.inkscape.Selection" "selectionBoolop"   (func $boolop (param i32)))
  (import "org.inkscape.Selection" "strokesToPaths"    (func $strokesToPaths (param i32) (result i32)))
  (import "org.inkscape.Selection" "simplifyPaths"     (func $simplify (result i32)))
  (import "org.inkscape.Selection" "selectionMove"     (func $move (param f64 f64)))
  (import "org.inkscape.Selection" "selectionRotate"   (func $rotate (param f64 f64 f64)))
  (import "org.inkscape.Selection" "selectionScale"    (func $scale (param f64 f64 f64 f64)))
  (import "org.inkscape.Selection" "selectionSkew"     (func $skew (param f64 f64 f64 f64)))
  (import "org.inkscape.Selection" "selectionApplyAffine"
                                  (func $applyAffine (param f64 f64 f64 f64 f64 f64 i32)))
  (import "org.inkscape.Selection" "removeTransform"   (func $removeTransform))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-reshape")
  (data (i32.const 32)  "svg:rect")
  (data (i32.const 40)  "svg:path")
  (data (i32.const 48)  "x")
  (data (i32.const 56)  "y")
  (data (i32.const 64)  "width")
  (data (i32.const 72)  "height")
  (data (i32.const 80)  "d")
  (data (i32.const 88)  "style")
  (data (i32.const 96)  "a1")
  (data (i32.const 104) "a2")
  (data (i32.const 112) "a3")
  (data (i32.const 120) "a4")
  (data (i32.const 128) "b1")
  (data (i32.const 136) "t1")
  (data (i32.const 144) "s1")
  (data (i32.const 152) "gg")
  (data (i32.const 160) "300")
  (data (i32.const 168) "320")
  (data (i32.const 176) "100")
  (data (i32.const 184) "200")
  (data (i32.const 192) "040")
  (data (i32.const 200) "020")
  (data (i32.const 208) "hold2")
  (data (i32.const 216) "a5")
  (data (i32.const 224) "a6")
  (data (i32.const 232) "300")
  (data (i32.const 320) "stroke:#000000;stroke-width:4;fill:none")
  (data (i32.const 400) "M 0,0 L 1,0 L 2,0 L 3,0 L 4,0 L 5,0 L 6,0 L 7,0 L 8,0 L 9,0 L 10,0")

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  (func $mkrect (param $document i32) (param $parent i32) (param $id_off i32)
                (param $x_off i32) (param $y_off i32) (param $w_off i32) (param $h_off i32)
                (result i32)
    (local $e i32)
    (local.set $e (call $createElement (local.get $document) (i32.const 32) (i32.const 8)))
    (call $setAttribute (local.get $e) (i32.const 8) (i32.const 2) (local.get $id_off) (i32.const 2))
    (call $setAttribute (local.get $e) (i32.const 48) (i32.const 1) (local.get $x_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 56) (i32.const 1) (local.get $y_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 64) (i32.const 5) (local.get $w_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 72) (i32.const 6) (local.get $h_off) (i32.const 3))
    (drop (call $appendChild (local.get $parent) (local.get $e)))
    (local.get $e))

  (func $selectTwo (param $a i32) (param $b i32)
    (call $selClear)
    (call $selAdd (local.get $a))
    (call $selAdd (local.get $b)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $hold i32) (local $gg i32)
    (local $a1 i32) (local $a2 i32) (local $a3 i32) (local $a4 i32) (local $a5 i32) (local $a6 i32)
    (local $b1 i32) (local $t1 i32) (local $s1 i32)
    (local $w f64) (local $x f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $hold (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $hold) (i32.const 8) (i32.const 2) (i32.const 208) (i32.const 5))
    (drop (call $appendChild (local.get $root) (local.get $hold)))

    ;; ── booleans ──────────────────────────────────────────────────────────────
    ;; Two 40-wide rects overlapping by 20. Union spans 60, difference leaves 20 -- so the op
    ;; code is doing the choosing, which a single case with a single code could not show.
    (local.set $a1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 96)
                                 (i32.const 160) (i32.const 176) (i32.const 192) (i32.const 192)))
    (local.set $a2 (call $mkrect (local.get $document) (local.get $hold) (i32.const 104)
                                 (i32.const 168) (i32.const 176) (i32.const 192) (i32.const 192)))
    (call $selectTwo (local.get $a1) (local.get $a2))
    (call $boolop (i32.const 0))
    (if (i32.ne (call $selBounds (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 1))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 60) (f64.const 0.01)))
      (then (return (i32.const 2))))

    (local.set $a3 (call $mkrect (local.get $document) (local.get $hold) (i32.const 112)
                                 (i32.const 160) (i32.const 184) (i32.const 192) (i32.const 192)))
    (local.set $a4 (call $mkrect (local.get $document) (local.get $hold) (i32.const 120)
                                 (i32.const 168) (i32.const 184) (i32.const 192) (i32.const 192)))
    (call $selectTwo (local.get $a3) (local.get $a4))
    (call $boolop (i32.const 2))
    (if (i32.ne (call $selBounds (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 4))))
    ;; Difference keeps the LEFT 20, starting where the lower rect starts.
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 300) (f64.const 0.01)))
      (then (return (i32.const 24))))

    ;; Intersection is also 20 wide and is somewhere else entirely, which is what separates it
    ;; from difference -- a width-only assertion would accept either.
    (local.set $a5 (call $mkrect (local.get $document) (local.get $hold) (i32.const 216)
                                 (i32.const 160) (i32.const 232) (i32.const 192) (i32.const 192)))
    (local.set $a6 (call $mkrect (local.get $document) (local.get $hold) (i32.const 224)
                                 (i32.const 168) (i32.const 232) (i32.const 192) (i32.const 192)))
    (call $selectTwo (local.get $a5) (local.get $a6))
    (call $boolop (i32.const 1))
    (if (i32.ne (call $selBounds (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 25))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 26))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 320) (f64.const 0.01)))
      (then (return (i32.const 27))))

    ;; ── stroke to path ────────────────────────────────────────────────────────
    ;; legacy refuses groups outright (path-outline.cpp item_to_paths()), so the same selection answers
    ;; false one way and true the other. That is the whole of what the flag does here.
    (local.set $gg (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $gg) (i32.const 8) (i32.const 2) (i32.const 152) (i32.const 2))
    (drop (call $appendChild (local.get $hold) (local.get $gg)))
    (local.set $b1 (call $mkrect (local.get $document) (local.get $gg) (i32.const 128)
                                 (i32.const 160) (i32.const 176) (i32.const 192) (i32.const 192)))
    (call $setAttribute (local.get $b1) (i32.const 88) (i32.const 5) (i32.const 320) (i32.const 39))

    (call $selClear)
    (call $selAdd (local.get $gg))
    (if (i32.ne (call $strokesToPaths (i32.const 1)) (i32.const 0)) (then (return (i32.const 5))))
    (call $selClear)
    (call $selAdd (local.get $gg))
    (if (i32.eqz (call $strokesToPaths (i32.const 0))) (then (return (i32.const 6))))

    ;; ── simplify ──────────────────────────────────────────────────────────────
    ;; Eleven collinear points, which is exactly what simplification is for. The `d` afterwards
    ;; must be shorter, and shorter is the only claim made -- how much is Inkscape's threshold
    ;; preference to decide, not this suite's.
    (local.set $s1 (call $createElement (local.get $document) (i32.const 40) (i32.const 8)))
    (call $setAttribute (local.get $s1) (i32.const 8) (i32.const 2) (i32.const 144) (i32.const 2))
    (call $setAttribute (local.get $s1) (i32.const 80) (i32.const 1) (i32.const 400) (i32.const 66))
    (drop (call $appendChild (local.get $hold) (local.get $s1)))
    (call $selClear)
    (call $selAdd (local.get $s1))
    (if (i32.eqz (call $simplify)) (then (return (i32.const 7))))
    (local.set $s1 (call $getElementById (local.get $document) (i32.const 144) (i32.const 2)))
    (if (i32.eqz (local.get $s1)) (then (return (i32.const 8))))
    (if (i32.ge_s (call $getAttribute (local.get $s1) (i32.const 80) (i32.const 1)
                                      (i32.const 700) (i32.const 256))
                  (i32.const 66))
      (then (return (i32.const 9))))

    ;; ── transforms ────────────────────────────────────────────────────────────
    ;; A 40 x 20 rect, so rotating it is visible: a square would measure the same either way.
    (local.set $t1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 136)
                                 (i32.const 160) (i32.const 184) (i32.const 192) (i32.const 200)))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 10))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 300) (f64.const 0.01)))
      (then (return (i32.const 11))))

    (call $selClear)
    (call $selAdd (local.get $t1))
    (call $move (f64.const 10) (f64.const 0))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 12))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 310) (f64.const 0.01)))
      (then (return (i32.const 13))))

    ;; Doubling the width about the box's own left edge.
    (call $scale (f64.const 310) (f64.const 200) (f64.const 2) (f64.const 1))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 14))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 80) (f64.const 0.01)))
      (then (return (i32.const 15))))

    ;; A quarter turn swaps the extents: 80 x 20 measures 20 x 80.
    (call $rotate (f64.const 310) (f64.const 200) (f64.const 90))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 16))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 17))))

    ;; Skewing widens it again, by an amount this does not pin down -- only that the shear
    ;; reached the geometry at all.
    (local.set $w (f64.load (i32.const 616)))
    (call $skew (f64.const 310) (f64.const 200) (f64.const 0.5) (f64.const 0))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 18))))
    (if (f64.le (f64.load (i32.const 616)) (local.get $w)) (then (return (i32.const 19))))

    ;; An affine given directly: translate by 25 along x and nothing else.
    (local.set $x (f64.load (i32.const 600)))
    (call $applyAffine (f64.const 1) (f64.const 0) (f64.const 0) (f64.const 1)
                       (f64.const 25) (f64.const 0) (i32.const 1))
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 20))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.add (local.get $x) (f64.const 25))
                             (f64.const 0.01)))
      (then (return (i32.const 21))))

    ;; Dropping the transform leaves the shape where its own attributes say it is, which after
    ;; all of the above is not where it currently sits.
    (local.set $x (f64.load (i32.const 600)))
    (call $removeTransform)
    (if (i32.ne (call $docBBox (local.get $t1) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 22))))
    (if (call $near (f64.load (i32.const 600)) (local.get $x) (f64.const 0.01))
      (then (return (i32.const 23))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 10))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
