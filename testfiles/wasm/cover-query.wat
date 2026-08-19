;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: hit testing, computed style, and the vendor session tier.
;;
;; The session operations are asserted for their headless behaviour, which is the case the
;; command line actually runs: no desktop means no current layer and no selection, and the
;; honest answer is absence rather than an empty stand-in that a plugin cannot distinguish
;; from a real answer.
;;
;; Build:  water cover-query.wat -o cover-query.wasm

(module
  (import "org.inkscape.Document" "documentElement"  (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"    (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"   (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getComputedStyle" (func $getComputedStyle (param i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"      (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"     (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.CSSStyleDeclaration" "getPropertyValue"
    (func $getPropertyValue (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.DOMStringList" "length" (func $listLength (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "item"   (func $listItem (param i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGSVGElement" "getEnclosureList"
    (func $getEnclosureList (param i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "getIntersectionList"
    (func $getIntersectionList (param i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "checkEnclosure"
    (func $checkEnclosure (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "checkIntersection"
    (func $checkIntersection (param i32 i32) (result i32)))
  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"   (func $nodesItem (param i32 i32) (result i32)))

  (import "org.inkscape.Inkscape" "inkParamString"       (func $inkParamString (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkParamFloat"        (func $inkParamFloat (param i32 i32) (result f64)))
  (import "org.inkscape.Inkscape" "inkParamInt"          (func $inkParamInt (param i32 i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkParamBool"         (func $inkParamBool (param i32 i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkCurrentLayer"      (func $inkCurrentLayer (result i32)))
  (import "org.inkscape.Inkscape" "inkIsSelected"        (func $inkIsSelected (param i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectedIds"       (func $inkSelectedIds (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectedNodeCount" (func $inkSelectedNodeCount (param i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectedNode"      (func $inkSelectedNode (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkIsCancelled"       (func $inkIsCancelled (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectionClear"    (func $inkSelectionClear))
  (import "org.inkscape.Inkscape" "inkSelectionAdd"      (func $inkSelectionAdd (param i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "box")
  (data (i32.const 8)  "svg:g")
  (data (i32.const 16) "id")
  (data (i32.const 24) "ok-query")
  (data (i32.const 40) "fill")
  (data (i32.const 48) "nosuchparam")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $style i32) (local $list i32) (local $marker i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── computed style ────────────────────────────────────────────────────────
    ;; The cascade, resolved. inkex approximates this and cannot reach inherited values.
    (local.set $style (call $getComputedStyle (local.get $box)))
    (if (i32.le_s (call $listLength (local.get $style)) (i32.const 0))
      (then (return (i32.const 2))))
    (if (i32.le_s (call $listItem (local.get $style) (i32.const 0) (i32.const 512) (i32.const 256))
                  (i32.const 0))
      (then (return (i32.const 3))))
    ;; fill is set on the rect, so it resolves to a value.
    (if (i32.le_s (call $getPropertyValue (local.get $box) (i32.const 40) (i32.const 4)
                                          (i32.const 512) (i32.const 256)) (i32.const 0))
      (then (return (i32.const 4))))

    ;; ── hit testing ───────────────────────────────────────────────────────────
    ;; A rectangle covering the whole page encloses the fixture's shapes.
    (f64.store (i32.const 512) (f64.const -1000))
    (f64.store (i32.const 520) (f64.const -1000))
    (f64.store (i32.const 528) (f64.const 4000))
    (f64.store (i32.const 536) (f64.const 4000))
    (local.set $list (call $getEnclosureList (i32.const 512)))
    (if (i32.le_s (call $nodesLength (local.get $list)) (i32.const 0))
      (then (return (i32.const 5))))
    (if (i32.eqz (call $nodesItem (local.get $list) (i32.const 0)))
      (then (return (i32.const 6))))
    (if (i32.ne (call $checkEnclosure (local.get $box) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 7))))
    (if (i32.ne (call $checkIntersection (local.get $box) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 8))))

    ;; A rectangle far away encloses and intersects nothing.
    (f64.store (i32.const 512) (f64.const 9000))
    (f64.store (i32.const 520) (f64.const 9000))
    (f64.store (i32.const 528) (f64.const 10))
    (f64.store (i32.const 536) (f64.const 10))
    (if (i32.ne (call $checkEnclosure (local.get $box) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 9))))
    (if (i32.ne (call $checkIntersection (local.get $box) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 10))))
    (if (i32.ne (call $nodesLength (call $getIntersectionList (i32.const 512))) (i32.const 0))
      (then (return (i32.const 11))))

    ;; ── session tier ──────────────────────────────────────────────────────────
    ;; A parameter this .inx does not declare reports absence, distinctly from empty.
    (if (i32.ne (call $inkParamString (i32.const 48) (i32.const 11) (i32.const 512) (i32.const 64))
                (i32.const -1))
      (then (return (i32.const 12))))
    (if (f64.ne (call $inkParamFloat (i32.const 48) (i32.const 11)) (f64.const 0))
      (then (return (i32.const 13))))
    (if (i32.ne (call $inkParamInt (i32.const 48) (i32.const 11)) (i32.const 0))
      (then (return (i32.const 14))))
    (if (i32.ne (call $inkParamBool (i32.const 48) (i32.const 11)) (i32.const 0))
      (then (return (i32.const 15))))

    ;; Headless: no desktop, so no current layer and nothing selected. Absence, not a guess.
    (if (i32.ne (call $inkCurrentLayer) (i32.const 0)) (then (return (i32.const 16))))
    (if (i32.ne (call $inkIsSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 17))))
    (if (i32.ne (call $listLength (call $inkSelectedIds)) (i32.const 0))
      (then (return (i32.const 18))))
    (if (i32.ne (call $inkSelectedNodeCount (local.get $box)) (i32.const 0))
      (then (return (i32.const 19))))
    (if (i32.ne (call $inkSelectedNode (local.get $box) (i32.const 0) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 20))))

    ;; Nobody has cancelled anything, so this must answer false -- and must not hang, which
    ;; it could if the event pumping inside it went looking for a main loop that is not
    ;; running. The true answer needs a Cancel button and a person to press it.
    (if (i32.ne (call $inkIsCancelled) (i32.const 0)) (then (return (i32.const 21))))

    ;; The mutating selection operations are reachable and harmless with no selection.
    (call $inkSelectionClear)
    (call $inkSelectionAdd (local.get $box))

    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 8))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
