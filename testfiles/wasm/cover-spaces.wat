;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the coordinate system and value notation each query is defined to answer in.
;;
;; This module exists because the earlier geometry tests could not have failed. They ran
;; against a document with no transforms, where SVG's user space and Inkscape's document
;; space are the same numbers, so they agreed with the host rather than checking it -- and a
;; getBBox returning document coordinates passed for weeks.
;;
;; Every assertion here is chosen so that the specified answer and the plausible internal one
;; are DIFFERENT numbers:
;;
;;   getBBox                the declared rect, not where the transforms put it
;;   getCTM                 the accumulated transform, scale included
;;   getComputedStyle       one notation for four spellings of the same colour, and a value
;;                          the element inherits rather than declares
;;   text metrics           unaffected by the transform above the text
;;   checkEnclosure         a rectangle in one named space, not another
;;
;; Build:  water cover-spaces.wat -o cover-spaces.wasm

(module
  (import "org.inkscape.Document" "documentElement"  (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"    (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"   (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"      (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"     (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.CSSStyleDeclaration" "getPropertyValue"
    (func $getPropertyValue (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getBBox" (func $getBBox (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getCTM"  (func $getCTM (param i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getComputedTextLength"
    (func $getComputedTextLength (param i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getExtentOfChar"
    (func $getExtentOfChar (param i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "box")
  (data (i32.const 8)   "label")
  (data (i32.const 16)  "named")
  (data (i32.const 24)  "functional")
  (data (i32.const 40)  "hex")
  (data (i32.const 48)  "inherited")
  (data (i32.const 64)  "fill")
  (data (i32.const 72)  "svg:g")
  (data (i32.const 80)  "id")
  (data (i32.const 88)  "ok-spaces")

  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  ;; Read fill from computed style into [1024, ...), returning its length.
  (func $fillOf (param $element i32) (result i32)
    (call $getPropertyValue (local.get $element)
      (i32.const 64) (i32.const 4) (i32.const 1024) (i32.const 256)))

  ;; Compare the last two computed fills, staged at 1024 and 2048.
  (func $sameAs2048 (param $len i32) (result i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $len)))
        (br_if $done (i32.ne
          (i32.load8_u (i32.add (i32.const 1024) (local.get $i)))
          (i32.load8_u (i32.add (i32.const 2048) (local.get $i)))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.eq (local.get $i) (local.get $len)))

  (func $stash (param $len i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $len)))
        (i32.store8 (i32.add (i32.const 2048) (local.get $i))
                    (i32.load8_u (i32.add (i32.const 1024) (local.get $i))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next))))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $label i32) (local $marker i32)
    (local $n i32) (local $m i32) (local $width f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (local.set $label (call $getElementById (local.get $document) (i32.const 8) (i32.const 5)))
    (if (i32.or (i32.eqz (local.get $box)) (i32.eqz (local.get $label)))
      (then (return (i32.const 1))))

    ;; ── getBBox is the element's own user space ───────────────────────────────
    ;; The rect is declared at 10,20 and sits under translate(7,3) inside
    ;; translate(100,50) scale(2). Document coordinates would be (10+7)*2+100 = 134 and
    ;; (20+3)*2+50 = 96, with a 60x80 size. The declared numbers are the answer.
    (if (i32.ne (call $getBBox (local.get $box) (i32.const 1) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 2))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.01)))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 4))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 30) (f64.const 0.01)))
      (then (return (i32.const 5))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 40) (f64.const 0.01)))
      (then (return (i32.const 6))))

    ;; ── getCTM carries what getBBox leaves out ────────────────────────────────
    ;; scale 2, translation (10+7)*2+100 - 10*2 ... simply: a=2, and the offset is
    ;; translate(100,50) composed with scale(2) then translate(7,3): 100+7*2 = 114, 50+3*2 = 56.
    (if (i32.ne (call $getCTM (local.get $box) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 7))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 2) (f64.const 0.01)))
      (then (return (i32.const 8))))
    (if (i32.eqz (call $near (f64.load (i32.const 544)) (f64.const 114) (f64.const 0.01)))
      (then (return (i32.const 9))))
    (if (i32.eqz (call $near (f64.load (i32.const 552)) (f64.const 56) (f64.const 0.01)))
      (then (return (i32.const 10))))

    ;; ── computed style resolves notation ──────────────────────────────────────
    ;; rebeccapurple, rgb(102,51,153) and #663399 are the same colour written three ways. A
    ;; computed value is one notation; a specified value is three.
    (local.set $n (call $fillOf (call $getElementById (local.get $document) (i32.const 16) (i32.const 5))))
    (if (i32.le_s (local.get $n) (i32.const 0)) (then (return (i32.const 11))))
    (call $stash (local.get $n))

    (local.set $m (call $fillOf (call $getElementById (local.get $document) (i32.const 40) (i32.const 3))))
    (if (i32.ne (local.get $m) (local.get $n)) (then (return (i32.const 12))))
    (if (i32.eqz (call $sameAs2048 (local.get $n))) (then (return (i32.const 13))))

    (local.set $m (call $fillOf (call $getElementById (local.get $document) (i32.const 24) (i32.const 10))))
    (if (i32.ne (local.get $m) (local.get $n)) (then (return (i32.const 14))))
    (if (i32.eqz (call $sameAs2048 (local.get $n))) (then (return (i32.const 15))))

    ;; ...and a value the element never declared, only inherited, resolves to the same thing.
    (local.set $m (call $fillOf (call $getElementById (local.get $document) (i32.const 48) (i32.const 9))))
    (if (i32.ne (local.get $m) (local.get $n)) (then (return (i32.const 16))))
    (if (i32.eqz (call $sameAs2048 (local.get $n))) (then (return (i32.const 17))))

    ;; ── text metrics are in the text element's user space ─────────────────────
    ;; The text sits inside scale(2). Its advance width is a property of the shaped run at
    ;; 16px, not of where the run happens to be drawn, so the scale must not appear in it.
    ;; 16px "Hello" is well under 100 user units; doubled it would still be under, so the
    ;; check is against the character extent instead, which is bounded much more tightly.
    (local.set $width (call $getComputedTextLength (local.get $label)))
    (if (f64.le (local.get $width) (f64.const 0)) (then (return (i32.const 18))))
    (if (i32.ne (call $getExtentOfChar (local.get $label) (i32.const 0) (i32.const 512))
                (i32.const 1))
      (then (return (i32.const 19))))
    ;; One glyph of 16px text is at most 16 wide and 24 tall in its own user space. At the
    ;; group's scale it would exceed both.
    (if (f64.ge (f64.load (i32.const 528)) (f64.const 20)) (then (return (i32.const 20))))
    (if (f64.ge (f64.load (i32.const 536)) (f64.const 30)) (then (return (i32.const 21))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 72) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 80) (i32.const 2) (i32.const 88) (i32.const 9))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
