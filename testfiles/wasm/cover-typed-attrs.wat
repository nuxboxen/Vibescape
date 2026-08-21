;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the typed attribute accessors.
;;
;; SVG 2 wraps attributes in SVGAnimatedLength and friends. The half of that a plugin actually
;; wants is the parsed value, and Inkscape has already written the parsers -- so these expose
;; the parsing without the object model. The wrappers themselves are not here because their
;; other half is animation: `animVal` has no meaning without an animation engine, and Inkscape
;; has none, so a plugin holding an SVGAnimatedLength would get one live field and one that
;; could never answer.
;;
;; The load-bearing case is the unit. "10mm" is 10 to a plugin echoing the author's value back
;; and 37.795 to one doing geometry, and neither number can be derived from the other without
;; knowing the unit, which is exactly what a plugin parsing the attribute string by hand keeps
;; getting wrong. So every length assertion checks the unit code, the author's number AND the
;; computed one.
;;
;; Setting is asserted by round trip rather than by reading the attribute back as a string: a
;; host that wrote "10" and dropped the unit would still produce a well-formed attribute, and
;; only re-parsing catches it.
;;
;; Build:  water cover-typed-attrs.wat -o cover-typed-attrs.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGElement" "getLength"    (func $getLength (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "getNumber"    (func $getNumber (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "getTransform" (func $getTransform (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "setLength"    (func $setLength (param i32 i32 i32 f64 i32)))
  (import "org.inkscape.SVGElement" "setNumber"    (func $setNumber (param i32 i32 i32 f64)))
  (import "org.inkscape.SVGElement" "setTransform" (func $setTransform (param i32 i32 i32 i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "box")
  (data (i32.const 8)   "svg:g")
  (data (i32.const 16)  "id")
  (data (i32.const 24)  "ok-typed-attrs")
  (data (i32.const 48)  "width")
  (data (i32.const 56)  "x")
  (data (i32.const 64)  "transform")
  (data (i32.const 80)  "nosuchattribute")

  ;; SVGLength::Unit: NONE=0, PX=1, PT=2, PC=3, MM=4, CM=5, INCH=6, EM=7, EX=8, PERCENT=9
  (global $UNIT_NONE i32 (i32.const 0))
  (global $UNIT_MM   i32 (i32.const 4))

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $marker i32) (local $r i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── reading ───────────────────────────────────────────────────────────────
    ;; width="30": a bare number, so the unit is NONE and the two values agree.
    (local.set $r (call $getLength (local.get $box) (i32.const 48) (i32.const 5) (i32.const 512)))
    (if (i32.ne (local.get $r) (global.get $UNIT_NONE)) (then (return (i32.const 2))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 30) (f64.const 0.001)))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 30) (f64.const 0.001)))
      (then (return (i32.const 4))))

    ;; An attribute that is not there is -1, which no unit code can collide with.
    (if (i32.ne (call $getLength (local.get $box) (i32.const 80) (i32.const 15) (i32.const 512))
                (i32.const -1))
      (then (return (i32.const 5))))

    ;; x="10"
    (if (i32.ne (call $getNumber (local.get $box) (i32.const 56) (i32.const 1) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 6))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.001)))
      (then (return (i32.const 7))))
    (if (i32.ne (call $getNumber (local.get $box) (i32.const 80) (i32.const 15) (i32.const 512))
                (i32.const 0))
      (then (return (i32.const 8))))

    ;; transform="translate(7,3)" is the matrix (1,0,0,1,7,3).
    (if (i32.ne (call $getTransform (local.get $box) (i32.const 64) (i32.const 9) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 9))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 1) (f64.const 0.001)))
      (then (return (i32.const 10))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 0) (f64.const 0.001)))
      (then (return (i32.const 11))))
    (if (i32.eqz (call $near (f64.load (i32.const 544)) (f64.const 7) (f64.const 0.001)))
      (then (return (i32.const 12))))
    (if (i32.eqz (call $near (f64.load (i32.const 552)) (f64.const 3) (f64.const 0.001)))
      (then (return (i32.const 13))))

    ;; ── writing, checked by re-parsing ────────────────────────────────────────
    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))

    ;; 10mm. The unit has to survive, and the computed value has to be the conversion rather
    ;; than a copy: 10mm is 37.795 user units, and a host that wrote "10" would return 10 for
    ;; both and unit NONE.
    (call $setLength (local.get $marker) (i32.const 48) (i32.const 5)
      (f64.const 10) (global.get $UNIT_MM))
    (local.set $r (call $getLength (local.get $marker) (i32.const 48) (i32.const 5) (i32.const 512)))
    (if (i32.ne (local.get $r) (global.get $UNIT_MM)) (then (return (i32.const 14))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.001)))
      (then (return (i32.const 15))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 37.795) (f64.const 0.01)))
      (then (return (i32.const 16))))

    (call $setNumber (local.get $marker) (i32.const 56) (i32.const 1) (f64.const 42.5))
    (if (i32.ne (call $getNumber (local.get $marker) (i32.const 56) (i32.const 1) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 17))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 42.5) (f64.const 0.001)))
      (then (return (i32.const 18))))

    ;; A matrix that is not a pure translation, so a writer that collapsed it to translate()
    ;; would lose the scale.
    (f64.store (i32.const 600) (f64.const 2))
    (f64.store (i32.const 608) (f64.const 0))
    (f64.store (i32.const 616) (f64.const 0))
    (f64.store (i32.const 624) (f64.const 3))
    (f64.store (i32.const 632) (f64.const 5))
    (f64.store (i32.const 640) (f64.const 6))
    (call $setTransform (local.get $marker) (i32.const 64) (i32.const 9) (i32.const 600))
    (if (i32.ne (call $getTransform (local.get $marker) (i32.const 64) (i32.const 9) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 19))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 2) (f64.const 0.001)))
      (then (return (i32.const 20))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 3) (f64.const 0.001)))
      (then (return (i32.const 21))))
    (if (i32.eqz (call $near (f64.load (i32.const 544)) (f64.const 5) (f64.const 0.001)))
      (then (return (i32.const 22))))
    (if (i32.eqz (call $near (f64.load (i32.const 552)) (f64.const 6) (f64.const 0.001)))
      (then (return (i32.const 23))))

    (call $setAttribute (local.get $marker) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 14))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
