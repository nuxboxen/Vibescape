;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: document metrics, and the document-space bounding box.
;;
;; `Grid::effect()` opens with preferredBounds() and getDocumentScale(). Until these existed a
;; wasm plugin could not be a peer of that builtin, which is the claim the whole project rests
;; on -- so this module is the parity gate's first half made checkable.
;;
;; The fixture is 400 x 400 with a matching viewBox, so the document scale is 1. That is
;; deliberately the boring case here: the interesting one is documentBBox, where the
;; fixture's group carries translate(100,50) scale(2) and the rect adds translate(7,3) of its
;; own, so the document-space answer is nowhere near the declared one.
;;
;; Build:  water cover-metrics.wat -o cover-metrics.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGSVGElement" "viewBox"            (func $viewBox (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "preferredBounds" (func $preferredBounds (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "pageBounds"      (func $pageBounds (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "documentScale"   (func $documentScale (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "documentSize"    (func $documentSize (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "displayUnit"     (func $displayUnit (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Selection" "convertUnit"
          (func $convertUnit (param f64 i32 i32 i32 i32) (result f64)))
  (import "org.inkscape.SVGGraphicsElement" "documentBBox"
    (func $documentBBox (param i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "box")
  (data (i32.const 8)  "svg:g")
  (data (i32.const 16) "id")
  (data (i32.const 24) "ok-metrics")
  (data (i32.const 40) "in")
  (data (i32.const 48) "px")
  (data (i32.const 56) "mm")
  (data (i32.const 64) "furlong")

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $marker i32) (local $bad f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (if (i32.or (i32.eqz (local.get $root)) (i32.eqz (local.get $box)))
      (then (return (i32.const 1))))

    ;; ── size, in user units ───────────────────────────────────────────────────
    (if (i32.ne (call $documentSize (local.get $root) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 2))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 400) (f64.const 0.01)))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 400) (f64.const 0.01)))
      (then (return (i32.const 4))))

    ;; ── viewBox ───────────────────────────────────────────────────────────────
    (if (i32.ne (call $viewBox (local.get $root) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 5))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 400) (f64.const 0.01)))
      (then (return (i32.const 6))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 400) (f64.const 0.01)))
      (then (return (i32.const 7))))

    ;; ── scale: viewBox matches the size, so 1 in both axes ────────────────────
    (if (i32.ne (call $documentScale (local.get $root) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 8))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 9))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 10))))

    ;; ── the bounds Grid measures ──────────────────────────────────────────────
    (if (i32.ne (call $preferredBounds (local.get $root) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 11))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 400) (f64.const 0.01)))
      (then (return (i32.const 12))))
    (if (i32.ne (call $pageBounds (local.get $root) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 13))))

    ;; ── the unit the fixture declares ─────────────────────────────────────────
    (if (i32.ne (call $displayUnit (local.get $root) (i32.const 600) (i32.const 32)) (i32.const 2))
      (then (return (i32.const 14))))
    (if (i32.ne (i32.load8_u (i32.const 600)) (i32.const 109))   ;; 'm'
      (then (return (i32.const 15))))
    (if (i32.ne (i32.load8_u (i32.const 601)) (i32.const 109))   ;; 'm'
      (then (return (i32.const 16))))

    ;; ── document-space bounds ─────────────────────────────────────────────────
    ;; The rect is declared at 10,20 and is 30 x 40. Its own transform adds (7,3), putting it
    ;; at 17,23 in its group; the group is translate(100,50) scale(2), so a point p lands at
    ;; (100 + 2px, 50 + 2py) and the box lands at 134,96 measuring 60 x 80.
    ;;
    ;; getBBox answers 10,20,30,40 for the same element, because SVG defines it in the
    ;; element's own user space. Both are right and they are not interchangeable, which is
    ;; why both exist -- and why a plugin comparing two elements under different groups needs
    ;; this one.
    (if (i32.ne (call $documentBBox (local.get $box) (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 17))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 134) (f64.const 0.01)))
      (then (return (i32.const 18))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 96) (f64.const 0.01)))
      (then (return (i32.const 19))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 60) (f64.const 0.01)))
      (then (return (i32.const 20))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 80) (f64.const 0.01)))
      (then (return (i32.const 21))))

    ;; ── converting between units ──────────────────────────────────────────────
    ;; 27 of the 166 shipped Python extensions call unittouu or viewport_to_unit, and it is
    ;; almost always the FIRST line of work: a parameter arrives as a number and a unit name
    ;; ("5", "mm") and has to become user units before anything can be done with it.
    ;;
    ;; One inch is 96 user units by definition, so this is a fact rather than a measurement.
    (if (i32.eqz (call $near (call $convertUnit (f64.const 1) (i32.const 40) (i32.const 2)
                                                (i32.const 48) (i32.const 2))
                             (f64.const 96) (f64.const 0.0001)))
      (then (return (i32.const 22))))
    ;; And back, so the pair is not one table read that happens to look right.
    (if (i32.eqz (call $near (call $convertUnit (f64.const 96) (i32.const 48) (i32.const 2)
                                                (i32.const 40) (i32.const 2))
                             (f64.const 1) (f64.const 0.0001)))
      (then (return (i32.const 23))))
    ;; 25.4 mm to the inch, which is the conversion every plugin taking a millimetre parameter
    ;; needs and none of them should be spelling out.
    (if (i32.eqz (call $near (call $convertUnit (f64.const 25.4) (i32.const 56) (i32.const 2)
                                                (i32.const 40) (i32.const 2))
                             (f64.const 1) (f64.const 0.0001)))
      (then (return (i32.const 24))))
    ;; A unit the table does not know answers NaN, which is the only answer that cannot be
    ;; mistaken for a length. Inkscape's own Unit::convert reports incompatibility by returning
    ;; -1, and -1 is both a plausible coordinate and the right answer for converting -1px to px,
    ;; so it is not passed through. NaN is not equal to itself, which is the whole test.
    (local.set $bad (call $convertUnit (f64.const 7) (i32.const 64) (i32.const 7)
                                       (i32.const 48) (i32.const 2)))
    (if (f64.eq (local.get $bad) (local.get $bad)) (then (return (i32.const 25))))
    ;; And a real conversion is emphatically not NaN, so the check above is not vacuous.
    (local.set $bad (call $convertUnit (f64.const 1) (i32.const 40) (i32.const 2)
                                       (i32.const 48) (i32.const 2)))
    (if (f64.ne (local.get $bad) (local.get $bad)) (then (return (i32.const 26))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 10))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
