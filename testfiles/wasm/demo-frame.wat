;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A port of the shipped `frame` extension (share/extensions/frame.py), as a plugin.
;;
;; Why this one. frame.py draws a border around the selection, which means it needs the
;; selection's bounding box, and that is where the Python extension system runs out of road:
;;
;;     for node in containedelements:
;;         if isinstance(node, (inkex.TextElement, inkex.Tspan, inkex.FlowRoot)):
;;             try:
;;                 box += node.get_inkscape_bbox()
;;             except ValueError:
;;                 continue
;;         else:
;;             box += node.bounding_box()
;;                                              -- frame.py Frame.create_frame()
;;
;; `get_inkscape_bbox` writes the document to a temporary directory and SPAWNS A WHOLE INKSCAPE
;; PROCESS to ask it for one rectangle (inkex/elements/inkex/elements/_text.py TextBBMixin.get_inkscape_bbox()). Its own docstring says
;; it "is rather slow to use in a loop", and the code above is a loop. It is there because
;; inkex cannot measure text at all: TextElement.get_path() returns an empty Path, and
;; FlowPara.get_path() carries the comment "These empty paths mean the bbox for text elements
;; will be nothing". When the subprocess fails, the `except ValueError: continue` silently
;; leaves that text out of the frame.
;;
;; Here the same question is one call to selectionBounds, in process, with text included
;; because Inkscape measured it the same way it measures everything else. No branch on element
;; type, no temporary directory, no second Inkscape, and nothing silently dropped.
;;
;; The rest is a faithful port rather than a demo: the same parameters frame.inx declares, the
;; same relative-then-absolute resize, the same stacking and grouping choices.
;;
;; Build:  water demo-frame.wat -o demo-frame.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  ;; Typed setters, so no coordinate, width or colour is ever formatted inside the plugin. The
  ;; host writes them the way Inkscape writes them; a guest-side formatter differs in the last
  ;; digit and in how it spells alpha.
  (import "org.inkscape.SVGElement" "setNumber" (func $setNumber (param i32 i32 i32 f64)))
  (import "org.inkscape.CSSStyleDeclaration" "setPropertyNumber"
          (func $setPropertyNumber (param i32 i32 i32 f64)))
  (import "org.inkscape.CSSStyleDeclaration" "setPropertyColor"
          (func $setPropertyColor (param i32 i32 i32 i32)))

  (import "org.inkscape.Selection" "selectionBounds" (func $selBounds (param i32 i32) (result i32)))
  (import "org.inkscape.Selection" "selectionAdd"    (func $selAdd (param i32)))
  (import "org.inkscape.Selection" "selectionClear"  (func $selClear))
  (import "org.inkscape.Selection" "lowerToBottom"   (func $lowerToBottom))
  (import "org.inkscape.Selection" "raiseToTop"      (func $raiseToTop))

  (import "org.inkscape.Params" "paramFloat"       (func $paramFloat (param i32 i32) (result f64)))
  (import "org.inkscape.Params" "paramInt"         (func $paramInt (param i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramColor"       (func $paramColor (param i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramOptionGroup" (func $paramOption (param i32 i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:rect")
  (data (i32.const 16)  "x")
  (data (i32.const 24)  "y")
  (data (i32.const 32)  "width")
  (data (i32.const 40)  "height")
  (data (i32.const 48)  "rx")
  (data (i32.const 56)  "ry")
  (data (i32.const 64)  "style")
  (data (i32.const 72)  "id")
  (data (i32.const 80)  "frame")

  ;; Parameter names, as frame.inx spells them.
  (data (i32.const 96)  "offset_relative")
  (data (i32.const 120) "offset_absolute")
  (data (i32.const 144) "corner_radius")
  (data (i32.const 160) "width")
  (data (i32.const 168) "stroke_color")
  (data (i32.const 184) "fill_color")
  (data (i32.const 200) "z_position")
  (data (i32.const 216) "bottom")

  (data (i32.const 240) "fill")
  (data (i32.const 248) "stroke")
  (data (i32.const 256) "stroke-width")


  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $frame i32)
    (local $x f64) (local $y f64) (local $w f64) (local $h f64)
    (local $grow_x f64) (local $grow_y f64) (local $radius f64)

    ;; ── the whole point ───────────────────────────────────────────────────────
    ;; One call. Text measured like everything else, because Inkscape measured it.
    (if (i32.ne (call $selBounds (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 1))))
    (local.set $x (f64.load (i32.const 512)))
    (local.set $y (f64.load (i32.const 520)))
    (local.set $w (f64.load (i32.const 528)))
    (local.set $h (f64.load (i32.const 536)))

    ;; Relative first, then absolute -- frame.py Frame.create_frame() resizes in that order, and the two do
    ;; not commute.
    (local.set $grow_x (f64.mul (local.get $w)
                                (f64.div (call $paramFloat (i32.const 96) (i32.const 15))
                                         (f64.const 100))))
    (local.set $grow_y (f64.mul (local.get $h)
                                (f64.div (call $paramFloat (i32.const 96) (i32.const 15))
                                         (f64.const 100))))
    (local.set $grow_x (f64.add (local.get $grow_x) (call $paramFloat (i32.const 120) (i32.const 15))))
    (local.set $grow_y (f64.add (local.get $grow_y) (call $paramFloat (i32.const 120) (i32.const 15))))

    (local.set $x (f64.sub (local.get $x) (local.get $grow_x)))
    (local.set $y (f64.sub (local.get $y) (local.get $grow_y)))
    (local.set $w (f64.add (local.get $w) (f64.mul (local.get $grow_x) (f64.const 2))))
    (local.set $h (f64.add (local.get $h) (f64.mul (local.get $grow_y) (f64.const 2))))

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $frame (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))
    (call $setAttribute (local.get $frame) (i32.const 72) (i32.const 2) (i32.const 80) (i32.const 5))
    (call $setNumber (local.get $frame)(i32.const 16) (i32.const 1) (local.get $x))
    (call $setNumber (local.get $frame)(i32.const 24) (i32.const 1) (local.get $y))
    (call $setNumber (local.get $frame)(i32.const 32) (i32.const 5) (local.get $w))
    (call $setNumber (local.get $frame)(i32.const 40) (i32.const 6) (local.get $h))

    (local.set $radius (f64.convert_i32_s (call $paramInt (i32.const 144) (i32.const 13))))
    (if (f64.gt (local.get $radius) (f64.const 0))
      (then
        (call $setNumber (local.get $frame)(i32.const 48) (i32.const 2) (local.get $radius))
        (call $setNumber (local.get $frame)(i32.const 56) (i32.const 2) (local.get $radius))))

    ;; ── style ─────────────────────────────────────────────────────────────────
    ;; The colour parameters go in as the RGBA words they arrive as, the stroke width as the
    ;; number it is. Nothing here builds CSS text.
    (drop (call $appendChild (local.get $root) (local.get $frame)))
    (call $setPropertyColor (local.get $frame) (i32.const 240) (i32.const 4)
                            (call $paramColor (i32.const 184) (i32.const 10)))
    (call $setPropertyColor (local.get $frame) (i32.const 248) (i32.const 6)
                            (call $paramColor (i32.const 168) (i32.const 12)))
    (call $setPropertyNumber (local.get $frame) (i32.const 256) (i32.const 12)
                             (call $paramFloat (i32.const 160) (i32.const 5)))

    ;; ── stacking ──────────────────────────────────────────────────────────────
    ;; frame.py puts the frame before the first element or after the last; the same choice, said
    ;; in the vocabulary an editor uses rather than by splicing XML siblings.
    (if (i32.eq (call $paramOption (i32.const 200) (i32.const 10) (i32.const 1792) (i32.const 64))
                (i32.const 6))
      (then
        (call $selClear)
        (call $selAdd (local.get $frame))
        (call $lowerToBottom))
      (else
        (call $selClear)
        (call $selAdd (local.get $frame))
        (call $raiseToTop)))

    (i32.const 0))
)
