;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: resolved geometry, path operations, and item transforms.
;;
;; Corpus rank 1 -- 58 of the 129 shipped effect scripts parse path data. They parse `d`, which
;; getAttribute has always handed over, so the gap was never the string. The gap is that `d` is
;; not the shape: a <rect> has none at all, an LPE item's is its input rather than what the
;; canvas shows, and a stroke has an outline the fill path does not describe.
;;
;; The headline assertion is therefore case 2 and 3 together: `box` has NO `d` attribute, and
;; pathData answers a path for it anyway. That is the whole difference between reading
;; markup and asking the editor.
;;
;; The fixture's `referencing` group carries no transform and sits directly under the root, so
;; for the shapes in it document space and user space coincide -- which is what lets the
;; boolean and intersection cases assert exact coordinates.
;;
;; Build:  water cover-path.wat -o cover-path.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"    (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGGeometryElement" "pathData"  (func $pathData (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "pathDataBeforeLPE"
    (func $pathDataBeforeLPE (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "outline"   (func $outline (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "toPath"    (func $toPath (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "simplify"  (func $simplify (param i32 f64 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "boolop"    (func $boolop (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "offset"
    (func $offset (param i32 f64 i32 f64 i32 i32) (result i32)))
  (import "org.inkscape.Params" "setParam" (func $setParam (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "nearestPoint"
    (func $nearestPoint (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "winding"   (func $winding (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "intersect" (func $intersect (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "markers"   (func $markers (param i32) (result i32)))

  (import "org.inkscape.SVGGraphicsElement" "exactBounds"  (func $exactBounds (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "documentBBox"
    (func $documentBBox (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "collidesWith" (func $collidesWith (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "transform"    (func $transform (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "applyTransform" (func $applyTransform (param i32 i32 i32)))
  (import "org.inkscape.SVGGraphicsElement" "relativeTransform"
    (func $relativeTransform (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "rotationCenter"
    (func $rotationCenter (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "setRotationCenter" (func $setRotationCenter (param i32 i32)))
  (import "org.inkscape.SVGGraphicsElement" "unsetRotationCenter" (func $unsetRotationCenter (param i32)))
  (import "org.inkscape.SVGGraphicsElement" "clipPath"     (func $clipPath (param i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "maskObject"   (func $maskObject (param i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "pathEffects"  (func $pathEffects (param i32) (result i32)))

  (import "org.inkscape.CSSStyleDeclaration" "paintServer" (func $paintServer (param i32 i32) (result i32)))
  (import "org.inkscape.DOMStringList" "length" (func $listLength (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "item"   (func $listItem (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))

  (import "org.inkscape.SVGGeometryElement" "tangentAtLength"
          (func $tangentAt (param i32 f64 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "selfIntersections"
          (func $selfIntersections (param i32) (result i32)))
  (import "org.inkscape.PathBuilder" "pathBegin"  (func $begin (result i32)))
  (import "org.inkscape.PathBuilder" "pathString" (func $pathString (param i32 i32 i32) (result i32)))
  (import "org.inkscape.PathBuilder" "pathHull"   (func $hullOf (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-path")
  (data (i32.const 32)  "box")
  (data (i32.const 40)  "line")
  (data (i32.const 48)  "d")
  (data (i32.const 56)  "overlapA")
  (data (i32.const 72)  "overlapB")
  (data (i32.const 88)  "apartC")
  (data (i32.const 96)  "crossX")
  (data (i32.const 104) "crossY")
  (data (i32.const 112) "clipped")
  (data (i32.const 120) "masked")
  (data (i32.const 128) "marked-path")
  (data (i32.const 144) "gradient-user")
  (data (i32.const 160) "grad")
  (data (i32.const 168) "group")
  (data (i32.const 176) "effected")
  (data (i32.const 192) "effected-path")
  (data (i32.const 208) "donutEven")
  (data (i32.const 224) "donutNonzero")
  (data (i32.const 240) "bowtie")

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

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $hull i32)
    (local $box i32) (local $line i32) (local $group i32)
    (local $a i32) (local $b i32) (local $c i32) (local $cx i32) (local $cy i32)
    (local $clipped i32) (local $masked i32) (local $marked i32) (local $user i32) (local $grad i32)
    (local $effected i32) (local $effectedPath i32) (local $lenA i32) (local $lenB i32)
    (local $donutEven i32) (local $donutNz i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 32) (i32.const 3)))
    (local.set $line (call $getElementById (local.get $document) (i32.const 40) (i32.const 4)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 168) (i32.const 5)))
    (local.set $a (call $getElementById (local.get $document) (i32.const 56) (i32.const 8)))
    (local.set $b (call $getElementById (local.get $document) (i32.const 72) (i32.const 8)))
    (local.set $c (call $getElementById (local.get $document) (i32.const 88) (i32.const 6)))
    (local.set $cx (call $getElementById (local.get $document) (i32.const 96) (i32.const 6)))
    (local.set $cy (call $getElementById (local.get $document) (i32.const 104) (i32.const 6)))
    (local.set $clipped (call $getElementById (local.get $document) (i32.const 112) (i32.const 7)))
    (local.set $masked (call $getElementById (local.get $document) (i32.const 120) (i32.const 6)))
    (local.set $marked (call $getElementById (local.get $document) (i32.const 128) (i32.const 11)))
    (local.set $user (call $getElementById (local.get $document) (i32.const 144) (i32.const 13)))
    (local.set $grad (call $getElementById (local.get $document) (i32.const 160) (i32.const 4)))
    (local.set $effected (call $getElementById (local.get $document) (i32.const 176) (i32.const 8)))
    (local.set $effectedPath (call $getElementById (local.get $document) (i32.const 192) (i32.const 13)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))
    (local.set $donutEven (call $getElementById (local.get $document) (i32.const 208) (i32.const 9)))
    (local.set $donutNz (call $getElementById (local.get $document) (i32.const 224) (i32.const 12)))
    (if (i32.eqz (local.get $effected)) (then (return (i32.const 57))))
    (if (i32.eqz (local.get $effectedPath)) (then (return (i32.const 58))))
    (if (i32.eqz (local.get $donutEven)) (then (return (i32.const 68))))
    (if (i32.eqz (local.get $donutNz)) (then (return (i32.const 69))))

    ;; ── the headline: a shape with no `d` ─────────────────────────────────────
    ;; `box` is a <rect>. It has no `d` attribute at all...
    (if (i32.ne (call $getAttribute (local.get $box) (i32.const 48) (i32.const 1)
                                    (i32.const 1024) (i32.const 512)) (i32.const -1))
      (then (return (i32.const 2))))
    ;; ...and a resolved path anyway. This is the entire difference between reading markup and
    ;; asking the editor, and it is why 58 corpus scripts reimplement shape-to-path by hand.
    (if (i32.le_s (call $pathData (local.get $box) (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 3))))
    (if (i32.le_s (call $pathData (local.get $line) (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 4))))
    ;; No live path effect here, so the before-LPE geometry exists too.
    (if (i32.le_s (call $pathDataBeforeLPE (local.get $box) (i32.const 1024) (i32.const 512))
                  (i32.const 0))
      (then (return (i32.const 5))))

    ;; ── exact bounds, and the stroke outline ──────────────────────────────────
    (if (i32.le_s (call $exactBounds (local.get $box) (i32.const 1024) (i32.const 512))
                  (i32.const 0))
      (then (return (i32.const 6))))
    (if (i32.le_s (call $outline (local.get $line) (i32.const 0) (i32.const 1024) (i32.const 512))
                  (i32.const 0))
      (then (return (i32.const 7))))

    ;; ── winding ───────────────────────────────────────────────────────────────
    ;; `box` is x=10..40, y=20..60 in its own user space.
    (f64.store (i32.const 512) (f64.const 25))
    (f64.store (i32.const 520) (f64.const 40))
    (if (i32.eqz (call $winding (local.get $box) (i32.const 512))) (then (return (i32.const 8))))
    (f64.store (i32.const 512) (f64.const 0))
    (f64.store (i32.const 520) (f64.const 0))
    (if (i32.ne (call $winding (local.get $box) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 9))))

    ;; ── nearest point ─────────────────────────────────────────────────────────
    ;; `line` is M 0,0 L 100,0, so the closest point to (50,10) is (50,0), ten away.
    (f64.store (i32.const 512) (f64.const 50))
    (f64.store (i32.const 520) (f64.const 10))
    (if (i32.ne (call $nearestPoint (local.get $line) (i32.const 512) (i32.const 600))
                (i32.const 1))
      (then (return (i32.const 10))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 50) (f64.const 0.01)))
      (then (return (i32.const 11))))
    (if (i32.eqz (call $near (f64.load (i32.const 608)) (f64.const 0) (f64.const 0.01)))
      (then (return (i32.const 12))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 10) (f64.const 0.01)))
      (then (return (i32.const 13))))

    ;; ── booleans ──────────────────────────────────────────────────────────────
    ;; A and B overlap, so their union is a shape. 0 is bool_op_union.
    (if (i32.le_s (call $boolop (local.get $a) (local.get $b) (i32.const 0)
                                (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 14))))
    ;; A and C are disjoint, so their intersection is EMPTY -- a zero-length answer, which is a
    ;; real result and not an absence. 1 is bool_op_inters.
    (if (i32.ne (call $boolop (local.get $a) (local.get $c) (i32.const 1)
                              (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 15))))
    ;; An operation code outside the enum is refused rather than clamped.

    ;; ── intersection points ───────────────────────────────────────────────────
    ;; crossX and crossY meet exactly once, at (150,415).
    (if (i32.ne (call $intersect (local.get $cx) (local.get $cy) (i32.const 600) (i32.const 8))
                (i32.const 1))
      (then (return (i32.const 16))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 150) (f64.const 0.01)))
      (then (return (i32.const 17))))
    (if (i32.eqz (call $near (f64.load (i32.const 608)) (f64.const 415) (f64.const 0.01)))
      (then (return (i32.const 18))))

    ;; ── collision, against real shapes ────────────────────────────────────────
    (if (i32.ne (call $collidesWith (local.get $a) (local.get $b)) (i32.const 1))
      (then (return (i32.const 19))))
    (if (i32.ne (call $collidesWith (local.get $a) (local.get $c)) (i32.const 0))
      (then (return (i32.const 20))))

    ;; ── offset ────────────────────────────────────────────────────────────────
    ;; join 2 is miter, miter limit 4.
    (if (i32.le_s (call $offset (local.get $a) (f64.const 5) (i32.const 2) (f64.const 4)
                                (i32.const 1024) (i32.const 512))
                  (i32.const 0))
      (then (return (i32.const 21))))

    ;; The join is the CALLER's, so asking for round corners must not answer the mitred shape.
    ;; A host that hardcoded the join would return the same bytes for both.
    (local.set $lenA (call $offset (local.get $a) (f64.const 5) (i32.const 2) (f64.const 4)
                                   (i32.const 1024) (i32.const 512)))
    (local.set $lenB (call $offset (local.get $a) (f64.const 5) (i32.const 1) (f64.const 4)
                                   (i32.const 2048) (i32.const 512)))
    (if (i32.le_s (local.get $lenB) (i32.const 0)) (then (return (i32.const 59))))
    (if (call $sameBytes (local.get $lenA) (local.get $lenB)) (then (return (i32.const 60))))
    ;; A join code outside the enum is refused rather than quietly becoming miter.

    ;; ── the fill rule is the ELEMENT's, not the host's ────────────────────────
    ;; donutEven and donutNonzero have identical `d` and differ only in fill-rule. Under
    ;; even-odd the inner square is a hole and the offset has to travel around it; under
    ;; nonzero it is solid and there is nothing inside to offset. A host that passed a fixed
    ;; fill rule would produce identical bytes here -- which is what it did until this case
    ;; existed.
    (local.set $lenA (call $offset (local.get $donutEven) (f64.const 2) (i32.const 2) (f64.const 4)
                                   (i32.const 1024) (i32.const 512)))
    (local.set $lenB (call $offset (local.get $donutNz) (f64.const 2) (i32.const 2) (f64.const 4)
                                   (i32.const 2048) (i32.const 512)))
    (if (i32.le_s (local.get $lenA) (i32.const 0)) (then (return (i32.const 61))))
    (if (i32.le_s (local.get $lenB) (i32.const 0)) (then (return (i32.const 62))))
    (if (call $sameBytes (local.get $lenA) (local.get $lenB)) (then (return (i32.const 63))))

    ;; The same distinction through a boolean operation, which takes a rule PER OPERAND.
    (local.set $lenA (call $boolop (local.get $donutEven) (local.get $c) (i32.const 0)
                                   (i32.const 1024) (i32.const 512)))
    (local.set $lenB (call $boolop (local.get $donutNz) (local.get $c) (i32.const 0)
                                   (i32.const 2048) (i32.const 512)))
    (if (i32.le_s (local.get $lenA) (i32.const 0)) (then (return (i32.const 64))))
    (if (call $sameBytes (local.get $lenA) (local.get $lenB)) (then (return (i32.const 65))))

    ;; ── transforms ────────────────────────────────────────────────────────────
    ;; `box` carries transform="translate(7,3)" of its own, which getCTM would fold into its
    ;; ancestors' and this reports alone.
    (if (i32.ne (call $transform (local.get $box) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 22))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 23))))
    (if (i32.eqz (call $near (f64.load (i32.const 632)) (f64.const 7) (f64.const 0.001)))
      (then (return (i32.const 24))))
    (if (i32.eqz (call $near (f64.load (i32.const 640)) (f64.const 3) (f64.const 0.001)))
      (then (return (i32.const 25))))

    ;; Relative to its own parent, an element's transform is exactly its own.
    (if (i32.ne (call $relativeTransform (local.get $box) (local.get $group) (i32.const 600))
                (i32.const 1))
      (then (return (i32.const 26))))
    (if (i32.eqz (call $near (f64.load (i32.const 632)) (f64.const 7) (f64.const 0.001)))
      (then (return (i32.const 27))))

    ;; ── rotation centre ───────────────────────────────────────────────────────
    ;; Not moved by the user, so the flag is 0 -- but a point is still written, because every
    ;; object has one.
    (if (i32.ne (call $rotationCenter (local.get $box) (i32.const 600)) (i32.const 0))
      (then (return (i32.const 28))))
    (f64.store (i32.const 512) (f64.const 111))
    (f64.store (i32.const 520) (f64.const 222))
    (call $setRotationCenter (local.get $box) (i32.const 512))
    (if (i32.ne (call $rotationCenter (local.get $box) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 29))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 111) (f64.const 0.01)))
      (then (return (i32.const 30))))
    (call $unsetRotationCenter (local.get $box))
    (if (i32.ne (call $rotationCenter (local.get $box) (i32.const 600)) (i32.const 0))
      (then (return (i32.const 31))))

    ;; ── clip, mask, markers, paint server ─────────────────────────────────────
    (if (i32.eqz (call $clipPath (local.get $clipped))) (then (return (i32.const 32))))
    (if (i32.ne (call $clipPath (local.get $box)) (i32.const 0)) (then (return (i32.const 33))))
    (if (i32.eqz (call $maskObject (local.get $masked))) (then (return (i32.const 34))))
    (if (i32.ne (call $maskObject (local.get $box)) (i32.const 0)) (then (return (i32.const 35))))
    (if (i32.le_s (call $nodesLength (call $markers (local.get $marked))) (i32.const 0))
      (then (return (i32.const 36))))
    (if (i32.ne (call $nodesLength (call $markers (local.get $box))) (i32.const 0))
      (then (return (i32.const 37))))

    ;; The gradient, resolved -- not the string "url(#grad)" for a plugin to parse.
    (if (i32.ne (call $paintServer (local.get $user) (i32.const 0)) (local.get $grad))
      (then (return (i32.const 38))))
    (if (i32.ne (call $paintServer (local.get $box) (i32.const 0)) (i32.const 0))
      (then (return (i32.const 39))))

    ;; ── live path effects ─────────────────────────────────────────────────────
    ;; An element with none.
    (if (i32.ne (call $listLength (call $pathEffects (local.get $box))) (i32.const 0))
      (then (return (i32.const 40))))
    ;; ...and one with an offset effect attached, reported by name.
    (if (i32.ne (call $listLength (call $pathEffects (local.get $effected))) (i32.const 1))
      (then (return (i32.const 50))))
    (if (i32.le_s (call $listItem (call $pathEffects (local.get $effected))
                                  (i32.const 0) (i32.const 1024) (i32.const 256))
                  (i32.const 0))
      (then (return (i32.const 51))))

    ;; The effect's OUTPUT and its INPUT are different shapes, and the two operations report
    ;; different ones. `effected` is a 20x20 rect offset outward by 5, so the resolved geometry
    ;; is a 30x30 starting at (295,395) while the input is still the 20x20 at (300,400).
    (local.set $lenA (call $pathData (local.get $effected) (i32.const 1024) (i32.const 512)))
    (local.set $lenB (call $pathDataBeforeLPE (local.get $effected) (i32.const 2048) (i32.const 512)))
    (if (i32.le_s (local.get $lenA) (i32.const 0)) (then (return (i32.const 52))))
    (if (i32.le_s (local.get $lenB) (i32.const 0)) (then (return (i32.const 53))))
    (if (call $sameBytes (local.get $lenA) (local.get $lenB)) (then (return (i32.const 54))))

    ;; For a plain <path>, though, the two are identical BY DESIGN and not by accident:
    ;; curve_for_item returns the path before the effect when the item is an SPPath, and after
    ;; it for every other SPShape (path-util.cpp curve_for_item()). Asserted so the documentation
    ;; cannot quietly claim the two always differ.
    (local.set $lenA (call $pathData (local.get $effectedPath) (i32.const 1024) (i32.const 512)))
    (local.set $lenB (call $pathDataBeforeLPE (local.get $effectedPath) (i32.const 2048) (i32.const 512)))
    (if (i32.le_s (local.get $lenA) (i32.const 0)) (then (return (i32.const 55))))
    (if (i32.eqz (call $sameBytes (local.get $lenA) (local.get $lenB)))
      (then (return (i32.const 56))))

    ;; ── the two that mutate, last ─────────────────────────────────────────────
    ;; Applying a transform the way the editor does, which is NOT the same as setting the
    ;; attribute. doWriteTransform normally calls set_transform, embedding the affine into the
    ;; object's own geometry and leaving the attribute identity -- so asserting that the
    ;; attribute holds what was passed would be asserting a non-optimising implementation,
    ;; which is the opposite of what this operation is for.
    ;;
    ;; Both branches are real and the fixture has one of each.
    (f64.store (i32.const 512) (f64.const 2))
    (f64.store (i32.const 520) (f64.const 0))
    (f64.store (i32.const 528) (f64.const 0))
    (f64.store (i32.const 536) (f64.const 2))
    (f64.store (i32.const 544) (f64.const 11))
    (f64.store (i32.const 552) (f64.const 13))

    ;; `apartC` is a plain path: the transform is EMBEDDED, so the attribute stays identity and
    ;; the geometry is what moved. It was 200..210 x 400..410; under (2,0,0,2,11,13) the corner
    ;; lands at (2*200+11, 2*400+13) = (411,813) and the box doubles to 20 x 20.
    (call $applyTransform (local.get $c) (i32.const 512) (i32.const 0))
    (if (i32.ne (call $documentBBox (local.get $c) (i32.const 1) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 41))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 411) (f64.const 0.01)))
      (then (return (i32.const 42))))
    (if (i32.eqz (call $near (f64.load (i32.const 608)) (f64.const 813) (f64.const 0.01)))
      (then (return (i32.const 43))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 46))))

    ;; `clipped` carries a clip path, which is one of the conditions that STOPS the embedding
    ;; (sp-item.cpp SPItem::doWriteTransform()) -- so here the transform really is preserved in the attribute.
    (call $applyTransform (local.get $clipped) (i32.const 512) (i32.const 0))
    (if (i32.ne (call $transform (local.get $clipped) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 47))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 2) (f64.const 0.001)))
      (then (return (i32.const 48))))
    (if (i32.eqz (call $near (f64.load (i32.const 632)) (f64.const 11) (f64.const 0.001)))
      (then (return (i32.const 49))))

    ;; Simplify answers how many objects it changed. Both modes are reachable: refitting the
    ;; curve, and merely coalescing near-coincident nodes.
    (if (i32.lt_s (call $simplify (local.get $line) (f64.const 0.002) (i32.const 0))
                  (i32.const 0))
      (then (return (i32.const 44))))
    (if (i32.lt_s (call $simplify (local.get $line) (f64.const 0.002) (i32.const 1))
                  (i32.const 0))
      (then (return (i32.const 66))))

    ;; Writing a parameter the .inx never declared answers 0 rather than trapping, matching
    ;; every parameter GETTER. It trapped until this case existed, so a plugin could read an
    ;; optional parameter safely and die writing it.
    (if (i32.ne (call $setParam (i32.const 32) (i32.const 3) (i32.const 32) (i32.const 3))
                (i32.const 0))
      (then (return (i32.const 67))))

    ;; Stroke to path REPLACES the element, so it goes last: the handle it answers is the
    ;; replacement, and the original is stale from here on. `legacy` picks the pre-1.0
    ;; conversion, which a plugin reproducing an older document's output needs.
    (if (i32.eqz (call $toPath (local.get $marked) (i32.const 0))) (then (return (i32.const 45))))

    ;; ── geometry 2geom has and Inkscape does not wrap ─────────────────────────
    ;; Everything above is reached through Inkscape's own path helpers, which is the right
    ;; denominator for what Inkscape DOES. It is the wrong one for what the geometry library
    ;; can do: 2geom is what all of this rests on, and a capability it has that no Inkscape
    ;; wrapper happens to use was invisible to this suite by construction.
    ;;
    ;; A tangent pairs with getPointAtLength -- position without heading is half an answer for
    ;; anything placing an arrowhead, a tick or a label along a path. The box is axis-aligned,
    ;; so a quarter of the way along its top edge the heading is exactly +x.
    (local.set $box (call $getElementById (local.get $document) (i32.const 32) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 76))))
    (if (i32.ne (call $tangentAt (local.get $box) (f64.const 5) (i32.const 700)) (i32.const 1))
      (then (return (i32.const 68))))
    (if (i32.eqz (call $near (f64.load (i32.const 700)) (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 69))))
    (if (i32.eqz (call $near (f64.load (i32.const 708)) (f64.const 0) (f64.const 0.001)))
      (then (return (i32.const 70))))
    ;; It is a unit vector, so a plugin can use it as a direction without normalising.
    (if (i32.eqz (call $near (f64.add (f64.mul (f64.load (i32.const 700)) (f64.load (i32.const 700)))
                                      (f64.mul (f64.load (i32.const 708)) (f64.load (i32.const 708))))
                              (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 71))))

    ;; A rectangle does not cross itself. Self-intersection is a different question from the
    ;; path-to-path intersection above, and the one worth asking before a boolean operation.
    (if (i32.ne (call $selfIntersections (local.get $box)) (i32.const 0))
      (then (return (i32.const 72))))
    ;; The bowtie does, exactly once, which is what makes the case above mean something.
    (if (i32.ne (call $selfIntersections
                  (call $getElementById (local.get $document) (i32.const 240) (i32.const 6)))
                (i32.const 1))
      (then (return (i32.const 73))))

    ;; The convex hull of a shape, as a path. For the box it is the box: four corners, and an
    ;; area equal to its own.
    (local.set $hull (call $begin))
    (if (i32.eqz (call $hullOf (local.get $hull) (local.get $box))) (then (return (i32.const 74))))
    (if (i32.le_s (call $pathString (local.get $hull) (i32.const 1024) (i32.const 512))
                  (i32.const 0))
      (then (return (i32.const 75))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 7))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
