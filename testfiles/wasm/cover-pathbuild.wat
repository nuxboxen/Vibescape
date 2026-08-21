;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: building path data through Inkscape's own machinery.
;;
;; Writing a `d` is the most common thing a shipped extension does that this interface could
;; not do: 36 of the 166 Python extensions assign `node.path = ...`. Without a builder every one
;; of them formats the string itself, and every one of them formats it slightly differently
;; from Inkscape and from each other.
;;
;; The builder is a handle over Geom::PathBuilder, which is a Geom::PathSink -- 2geom's own
;; interface, with the verbs SVG has: moveTo, lineTo, curveTo, quadTo, arcTo, closePath. What
;; comes out goes through sp_svg_write_path, so a plugin's paths are written exactly the way
;; Inkscape writes every other path in the document.
;;
;; pathFeed is what makes read-modify-write possible: it seeds a builder from an element's
;; RESOLVED geometry, so a rect can be fed in, added to, and written back out as a path without
;; the guest parsing or emitting a single coordinate.
;;
;; Build:  water cover-pathbuild.wat -o cover-pathbuild.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"    (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGGeometryElement" "getTotalLength" (func $totalLength (param i32) (result f64)))
  (import "org.inkscape.SVGGeometryElement" "pathData"    (func $pathData (param i32 i32 i32) (result i32)))

  (import "org.inkscape.PathBuilder" "pathBegin"  (func $begin (result i32)))
  (import "org.inkscape.PathBuilder" "pathMoveTo" (func $moveTo (param i32 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathLineTo" (func $lineTo (param i32 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathCurveTo"
          (func $curveTo (param i32 f64 f64 f64 f64 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathQuadTo" (func $quadTo (param i32 f64 f64 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathArcTo"
          (func $arcTo (param i32 f64 f64 f64 i32 i32 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathClose"  (func $close (param i32)))
  (import "org.inkscape.PathBuilder" "pathFeed"   (func $feed (param i32 i32) (result i32)))
  (import "org.inkscape.PathBuilder" "pathApply"  (func $apply (param i32 i32) (result i32)))
  (import "org.inkscape.PathBuilder" "pathString" (func $pathString (param i32 i32 i32) (result i32)))
  (import "org.inkscape.PathBuilder" "pathParse"  (func $parse (param i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:path")
  (data (i32.const 16) "id")
  (data (i32.const 24) "built")
  (data (i32.const 32) "d")
  (data (i32.const 40) "box")
  (data (i32.const 48) "svg:g")
  (data (i32.const 56) "ok-pathbuild")
  (data (i32.const 72) "fed")
  (data (i32.const 88) "M 0,0 L 20,0 L 20,20")
  (data (i32.const 120) "not a path")

  ;; 'M' or 'm'
  (func $isMoveTo (param $c i32) (result i32)
    (i32.or (i32.eq (local.get $c) (i32.const 77)) (i32.eq (local.get $c) (i32.const 109))))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $box i32)
    (local $b i32) (local $path i32) (local $n i32) (local $fed i32)
    (local $step i32) (local $moves i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 40) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── a builder is a handle ─────────────────────────────────────────────────
    (local.set $b (call $begin))
    (if (i32.eqz (local.get $b)) (then (return (i32.const 2))))

    ;; Empty until something is fed in, and an empty path is an empty string rather than an
    ;; absent one: the builder exists, it just has nothing in it yet.
    (if (i32.ne (call $pathString (local.get $b) (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 3))))

    ;; ── every verb SVG has ────────────────────────────────────────────────────
    (call $moveTo  (local.get $b) (f64.const 10) (f64.const 10))
    (call $lineTo  (local.get $b) (f64.const 50) (f64.const 10))
    (call $curveTo (local.get $b) (f64.const 60) (f64.const 10) (f64.const 70) (f64.const 20)
                                  (f64.const 70) (f64.const 30))
    (call $quadTo  (local.get $b) (f64.const 70) (f64.const 50) (f64.const 50) (f64.const 50))
    (call $arcTo   (local.get $b) (f64.const 20) (f64.const 20) (f64.const 0)
                                  (i32.const 0) (i32.const 1) (f64.const 10) (f64.const 30))
    (call $close   (local.get $b))

    (local.set $n (call $pathString (local.get $b) (i32.const 1024) (i32.const 512)))
    (if (i32.le_s (local.get $n) (i32.const 0)) (then (return (i32.const 4))))
    ;; It starts with a moveto, because that is what a path is -- and with a LOWERCASE one,
    ;; because sp_svg_write_path writes relative coordinates by preference. Which case it uses
    ;; is Inkscape's decision and could change with a preference, so what is asserted is that it
    ;; is a moveto at all. That it is not the absolute `M` a plugin would have written by hand
    ;; is the whole reason for handing the writing to the host.
    (if (i32.eqz (call $isMoveTo (i32.load8_u (i32.const 1024)))) (then (return (i32.const 5))))

    ;; ONE moveto, and one only. Six verbs after a single moveTo are one subpath, so a second
    ;; moveto anywhere means the builder lost the subpath in progress and started again -- which
    ;; is what a sink rebuilt between calls does, silently, while still answering a `d` that
    ;; parses and draws something. Counting them is what tells the two apart.
    (local.set $step (i32.const 0))
    (local.set $moves (i32.const 0))
    (block $scan_done
      (loop $scan
        (br_if $scan_done (i32.ge_s (local.get $step) (local.get $n)))
        (if (call $isMoveTo (i32.load8_u (i32.add (i32.const 1024) (local.get $step))))
          (then (local.set $moves (i32.add (local.get $moves) (i32.const 1)))))
        (local.set $step (i32.add (local.get $step) (i32.const 1)))
        (br $scan)))
    (if (i32.ne (local.get $moves) (i32.const 1)) (then (return (i32.const 14))))

    ;; ── onto an element ───────────────────────────────────────────────────────
    (local.set $path (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))
    (call $setAttribute (local.get $path) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 5))
    (drop (call $appendChild (local.get $root) (local.get $path)))
    (if (i32.eqz (call $apply (local.get $b) (local.get $path))) (then (return (i32.const 6))))

    ;; What landed on the element is what the builder held.
    (if (i32.ne (call $getAttribute (local.get $path) (i32.const 32) (i32.const 1)
                                    (i32.const 2048) (i32.const 512))
                (local.get $n))
      (then (return (i32.const 7))))

    ;; And it is real geometry, not just a string: the document can measure it.
    (if (f64.le (call $totalLength (local.get $path)) (f64.const 0))
      (then (return (i32.const 8))))

    ;; ── read, modify, write ───────────────────────────────────────────────────
    ;; Feeding a RECT in is the case that matters. Its geometry is Inkscape's, resolved, and
    ;; the guest never sees a coordinate -- so a plugin can extend a shape without knowing how
    ;; to spell one.
    (local.set $fed (call $begin))
    (if (i32.eqz (call $feed (local.get $fed) (local.get $box))) (then (return (i32.const 9))))
    (local.set $n (call $pathString (local.get $fed) (i32.const 1024) (i32.const 512)))
    (if (i32.le_s (local.get $n) (i32.const 0)) (then (return (i32.const 10))))
    ;; The same geometry pathData reports for that rect, since both go through the resolved
    ;; curve and sp_svg_write_path.
    (if (i32.ne (call $pathData (local.get $box) (i32.const 2048) (i32.const 512)) (local.get $n))
      (then (return (i32.const 11))))

    ;; Added to, and written out as a path.
    (call $moveTo (local.get $fed) (f64.const 200) (f64.const 200))
    (call $lineTo (local.get $fed) (f64.const 260) (f64.const 200))
    (local.set $path (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))
    (call $setAttribute (local.get $path) (i32.const 16) (i32.const 2) (i32.const 72) (i32.const 3))
    (drop (call $appendChild (local.get $root) (local.get $path)))
    (if (i32.eqz (call $apply (local.get $fed) (local.get $path))) (then (return (i32.const 12))))
    ;; Longer than the rect alone, because the added subpath is in there too.
    (if (i32.le_s (call $getAttribute (local.get $path) (i32.const 32) (i32.const 1)
                                      (i32.const 2048) (i32.const 512))
                  (local.get $n))
      (then (return (i32.const 13))))

    ;; ── path data as text ─────────────────────────────────────────────────────
    ;; The other direction. A plugin holding a `d` as TEXT -- from a parameter, from a file it
    ;; is importing, from anywhere that is not already an element -- had no way into a builder
    ;; at all: pathFeed takes an element. 2geom's parse_svg_path writes into a PathSink,
    ;; which is exactly what the builder holds, so the two halves cost the same.
    (local.set $b (call $begin))
    (if (i32.eqz (call $parse (local.get $b) (i32.const 88) (i32.const 24)))
      (then (return (i32.const 15))))
    (local.set $n (call $pathString (local.get $b) (i32.const 1024) (i32.const 512)))
    (if (i32.le_s (local.get $n) (i32.const 0)) (then (return (i32.const 16))))
    ;; Parsed into real geometry, not kept as the string it arrived as: it can be added to.
    (call $lineTo (local.get $b) (f64.const 90) (f64.const 90))
    (if (i32.le_s (call $pathString (local.get $b) (i32.const 1024) (i32.const 512))
                  (local.get $n))
      (then (return (i32.const 17))))

    ;; Text that is not a path leaves the builder alone rather than half-filling it.
    (local.set $b (call $begin))
    (if (i32.ne (call $parse (local.get $b) (i32.const 120) (i32.const 9)) (i32.const 0))
      (then (return (i32.const 18))))
    (if (i32.ne (call $pathString (local.get $b) (i32.const 1024) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 19))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 48) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 16) (i32.const 2) (i32.const 56) (i32.const 12))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
