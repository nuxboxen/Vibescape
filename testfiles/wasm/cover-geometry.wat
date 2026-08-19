;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the derived queries. This is the half that cannot be answered from the markup,
;; and the reason the whole exercise exists.
;;
;; The fixture's rect is 30x40 with a 4-wide stroke, so its geometric box is 30x40 and its
;; visual box is 34x44. An extension holding serialised SVG can compute the first and cannot
;; compute the second -- inkex's shape_box() is the geometric one, and it has no way to reach
;; the other. Asserting both, and that they differ by the stroke, is the claim made concrete.
;;
;; Build:  water cover-geometry.wat -o cover-geometry.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGGraphicsElement" "getBBox"       (func $getBBox (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getCTM"        (func $getCTM (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getScreenCTM"  (func $getScreenCTM (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "getTotalLength"   (func $getTotalLength (param i32) (result f64)))
  (import "org.inkscape.SVGGeometryElement" "getPointAtLength" (func $getPointAtLength (param i32 f64 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "isPointInFill"    (func $isPointInFill (param i32 f64 f64) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "isPointInStroke"  (func $isPointInStroke (param i32 f64 f64) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "box")
  (data (i32.const 8)  "line")
  (data (i32.const 16) "svg:g")
  (data (i32.const 24) "id")
  (data (i32.const 32) "ok-geometry")

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $line i32) (local $marker i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (local.set $line (call $getElementById (local.get $document) (i32.const 8) (i32.const 4)))
    (if (i32.or (i32.eqz (local.get $box)) (i32.eqz (local.get $line)))
      (then (return (i32.const 1))))

    ;; Geometric box: fill only. The rect is declared at x=10 y=20, 30 x 40, and sits under
    ;; two translations -- its own and its group's. getBBox is defined in the element's own
    ;; user space, so it reports the declared position: neither transform is applied. An
    ;; implementation returning Inkscape's document bounds would answer 117,73 here, which is
    ;; where the rect is drawn rather than where it is.
    (if (i32.ne (call $getBBox (local.get $box) (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 2))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.01)))
      (then (return (i32.const 18))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 19))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 30) (f64.const 0.01)))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 40) (f64.const 0.01)))
      (then (return (i32.const 4))))

    ;; Visual box: fill + stroke. The 4-wide stroke straddles the outline, so each dimension
    ;; grows by exactly the stroke width. This is the number a subprocess cannot produce.
    (if (i32.ne (call $getBBox (local.get $box) (i32.const 3) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 5))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 34) (f64.const 0.01)))
      (then (return (i32.const 6))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 44) (f64.const 0.01)))
      (then (return (i32.const 7))))

    ;; getCTM is the other side of the same coin: it carries the transforms getBBox leaves
    ;; out, so the two compose to where the element is drawn. The group scales by 2 and
    ;; translates by 100,50; the rect adds translate(7,3) inside that scale, giving
    ;; 100 + 7*2 = 114 and 50 + 3*2 = 56.
    (if (i32.ne (call $getCTM (local.get $box) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 8))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 2) (f64.const 0.01)))
      (then (return (i32.const 9))))
    (if (i32.eqz (call $near (f64.load (i32.const 544)) (f64.const 114) (f64.const 0.01)))
      (then (return (i32.const 20))))
    (if (i32.eqz (call $near (f64.load (i32.const 552)) (f64.const 56) (f64.const 0.01)))
      (then (return (i32.const 21))))

    ;; getScreenCTM answers absent with no desktop, which is the honest result rather than
    ;; an identity matrix standing in for "do not know".
    (if (i32.ne (call $getScreenCTM (local.get $box) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 10))))

    ;; The path is "M 0,0 L 100,0", so its length is 100 and its midpoint is (50, 0). Path
    ;; time is not arc length, so a wrong parameterisation shows up here immediately.
    (if (i32.eqz (call $near (call $getTotalLength (local.get $line)) (f64.const 100) (f64.const 0.01)))
      (then (return (i32.const 11))))
    (if (i32.ne (call $getPointAtLength (local.get $line) (f64.const 50) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 12))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 50) (f64.const 0.5)))
      (then (return (i32.const 13))))

    ;; A point inside the rect is in its fill; one far outside is not.
    (if (i32.ne (call $isPointInFill (local.get $box) (f64.const 25) (f64.const 40)) (i32.const 1))
      (then (return (i32.const 14))))
    (if (i32.ne (call $isPointInFill (local.get $box) (f64.const 500) (f64.const 500)) (i32.const 0))
      (then (return (i32.const 15))))

    ;; A point on the outline is in the stroke -- an answer that needs the stroke geometry,
    ;; not just the path data.
    (if (i32.ne (call $isPointInStroke (local.get $line) (f64.const 50) (f64.const 0)) (i32.const 1))
      (then (return (i32.const 16))))
    (if (i32.ne (call $isPointInStroke (local.get $line) (f64.const 50) (f64.const 50)) (i32.const 0))
      (then (return (i32.const 17))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 16) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 24) (i32.const 2) (i32.const 32) (i32.const 11))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
