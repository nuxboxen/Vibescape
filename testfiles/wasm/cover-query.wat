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

  (import "org.inkscape.Params" "paramString"       (func $paramString (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramFloat"        (func $paramFloat (param i32 i32) (result f64)))
  (import "org.inkscape.Params" "paramInt"          (func $paramInt (param i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramBool"         (func $paramBool (param i32 i32) (result i32)))
  (import "org.inkscape.Layers" "currentLayer"      (func $currentLayer (result i32)))
  (import "org.inkscape.Selection" "isSelected"        (func $isSelected (param i32) (result i32)))
  (import "org.inkscape.Selection" "selectedIds"       (func $selectedIds (result i32)))
  (import "org.inkscape.Selection" "selectionNodes"    (func $selectionNodes (result i32)))
  (import "org.inkscape.Selection" "selectedNodeCount" (func $selectedNodeCount (param i32) (result i32)))
  (import "org.inkscape.Selection" "selectedNode"      (func $selectedNode (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Cancellation" "isCancelled"       (func $isCancelled (result i32)))
  (import "org.inkscape.Selection" "selectionClear"    (func $selectionClear))
  (import "org.inkscape.Selection" "selectionAdd"      (func $selectionAdd (param i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "box")
  (data (i32.const 8)  "svg:g")
  (data (i32.const 16) "id")
  (data (i32.const 24) "ok-query")
  (data (i32.const 40) "fill")
  (data (i32.const 48) "nosuchparam")
  (data (i32.const 64) "inherited")

  ;; Does the computed-style list carry an entry for `fill`? Entries are "name:value", so a
  ;; match is the five bytes "fill:" at the head of one of them.
  (func $hasFill (param $list i32) (result i32)
    (local $i i32) (local $n i32) (local $len i32)
    (local.set $n (call $listLength (local.get $list)))
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
        (local.set $len
          (call $listItem (local.get $list) (local.get $i) (i32.const 3072) (i32.const 512)))
        (if (i32.ge_s (local.get $len) (i32.const 5))
          (then
            (if (i32.and
                  (i32.and (i32.eq (i32.load8_u (i32.const 3072)) (i32.const 102))    ;; 'f'
                           (i32.eq (i32.load8_u (i32.const 3073)) (i32.const 105)))   ;; 'i'
                  (i32.and
                    (i32.and (i32.eq (i32.load8_u (i32.const 3074)) (i32.const 108))  ;; 'l'
                             (i32.eq (i32.load8_u (i32.const 3075)) (i32.const 108))) ;; 'l'
                    (i32.eq (i32.load8_u (i32.const 3076)) (i32.const 58))))          ;; ':'
              (then (return (i32.const 1))))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 0))

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
    ;; ...and it appears in the enumerated list, which is the easy half.
    (if (i32.eqz (call $hasFill (local.get $style))) (then (return (i32.const 22))))

    ;; The half that matters. `inherited` declares no fill at all -- it takes one from the
    ;; group above it -- and a COMPUTED style has a value for it regardless. A host that
    ;; enumerates only the properties this element happens to declare is answering the
    ;; specified style under the computed style's name, which is the exact failure that sends
    ;; a plugin back to walking ancestors by hand.
    ;;
    ;; getPropertyValue already answers this correctly (cover-spaces asserts it), so the two
    ;; operations on one object have to agree about what "the properties" are.
    (if (i32.eqz (call $hasFill
                    (call $getComputedStyle
                      (call $getElementById (local.get $document) (i32.const 64) (i32.const 9)))))
      (then (return (i32.const 23))))

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
    (if (i32.ne (call $paramString (i32.const 48) (i32.const 11) (i32.const 512) (i32.const 64))
                (i32.const -1))
      (then (return (i32.const 12))))
    (if (f64.ne (call $paramFloat (i32.const 48) (i32.const 11)) (f64.const 0))
      (then (return (i32.const 13))))
    (if (i32.ne (call $paramInt (i32.const 48) (i32.const 11)) (i32.const 0))
      (then (return (i32.const 14))))
    (if (i32.ne (call $paramBool (i32.const 48) (i32.const 11)) (i32.const 0))
      (then (return (i32.const 15))))

    ;; The current layer is NOT absent headless: Inkscape writes the choice to
    ;; namedview/@inkscape:current-layer and reads it back on open, so the document carries it
    ;; whether or not anyone is looking. This fixture declares no layers and records no choice,
    ;; and the answer for that is the root. cover-layers covers the other two branches.
    (if (i32.ne (call $currentLayer) (local.get $root)) (then (return (i32.const 16))))

    ;; Nothing is selected, though, which is a genuine absence rather than a fallback.
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 17))))
    (if (i32.ne (call $listLength (call $selectedIds)) (i32.const 0))
      (then (return (i32.const 18))))
    (if (i32.ne (call $selectedNodeCount (local.get $box)) (i32.const 0))
      (then (return (i32.const 19))))
    (if (i32.ne (call $selectedNode (local.get $box) (i32.const 0) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 20))))

    ;; Nobody has cancelled anything, so this must answer false -- and must not hang, which
    ;; it could if the event pumping inside it went looking for a main loop that is not
    ;; running. The true answer needs a Cancel button and a person to press it.
    (if (i32.ne (call $isCancelled) (i32.const 0)) (then (return (i32.const 21))))

    ;; The mutating selection operations are reachable and harmless with no selection.
    (call $selectionClear)
    (call $selectionAdd (local.get $box))

    ;; ── the selection as elements ─────────────────────────────────────────────
    ;; "For each selected object" is the opening line of nearly every extension ever written,
    ;; so the selection has to be reachable AS ELEMENTS. Ids are not a substitute: they make
    ;; every such plugin round-trip through getElementById, and an element that has not got one
    ;; cannot be reached at all.
    (if (i32.ne (call $nodesLength (call $selectionNodes)) (i32.const 1))
      (then (return (i32.const 22))))
    (if (i32.ne (call $nodesItem (call $selectionNodes) (i32.const 0)) (local.get $box))
      (then (return (i32.const 23))))
    (call $selectionClear)
    (if (i32.ne (call $nodesLength (call $selectionNodes)) (i32.const 0))
      (then (return (i32.const 24))))
    (call $selectionAdd (local.get $box))

    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 8))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
