;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: clips, masks, and turning objects into reusable defs.
;;
;; These all reach for a desktop, but only to flash a warning when nothing is selected; the work
;; itself is document-level, so they run headless. Worth pinning rather than assuming either way.
;;
;; Clip and mask are one C++ call with a flag, and the flag is the whole difference between the
;; two, so every case here asks BOTH questions: a host that ignored the flag would set a clip
;; where a mask was asked for and still answer "something is there".
;;
;; Build:  water cover-adorn.wat -o cover-adorn.wasm

(module
  (import "org.inkscape.Document" "documentElement"      (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"        (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"       (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementsByTagName" (func $byTag (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"          (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "childNodes"           (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Node"     "parentNode"           (func $parentNode (param i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"         (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGGraphicsElement" "clipPath"   (func $clipPath (param i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "maskObject" (func $maskObject (param i32) (result i32)))

  (import "org.inkscape.Selection" "selectionClear" (func $selClear))
  (import "org.inkscape.Selection" "selectionAdd"   (func $selAdd (param i32)))
  (import "org.inkscape.Selection" "isSelected"     (func $isSelected (param i32) (result i32)))

  (import "org.inkscape.Selection" "setMask"      (func $setMask (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Selection" "unsetMask"    (func $unsetMask (param i32 i32 i32)))
  (import "org.inkscape.Selection" "setClipGroup" (func $setClipGroup))
  (import "org.inkscape.Selection" "toMarker"     (func $toMarker (param i32)))
  (import "org.inkscape.Selection" "toPattern"    (func $toPattern (param i32)))
  (import "org.inkscape.Selection" "untile"       (func $untile))
  (import "org.inkscape.Selection" "toSymbol"     (func $toSymbol))
  (import "org.inkscape.Selection" "bitmapCopy"   (func $bitmapCopy))

  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-adorn")
  (data (i32.const 32)  "svg:rect")
  (data (i32.const 48)  "x")
  (data (i32.const 56)  "y")
  (data (i32.const 64)  "width")
  (data (i32.const 72)  "height")
  (data (i32.const 80)  "c1")
  (data (i32.const 88)  "c2")
  (data (i32.const 96)  "m1")
  (data (i32.const 104) "m2")
  (data (i32.const 112) "k1")
  (data (i32.const 120) "k2")
  (data (i32.const 128) "p1")
  (data (i32.const 136) "mk")
  (data (i32.const 144) "sy")
  (data (i32.const 152) "300")
  (data (i32.const 160) "310")
  (data (i32.const 168) "100")
  (data (i32.const 176) "110")
  (data (i32.const 184) "200")
  (data (i32.const 192) "210")
  (data (i32.const 200) "060")
  (data (i32.const 208) "020")
  (data (i32.const 216) "pattern")
  (data (i32.const 224) "marker")
  (data (i32.const 232) "symbol")
  (data (i32.const 240) "hold3")
  (data (i32.const 248) "use")
  (data (i32.const 256) "g")
  (data (i32.const 264) "mq")
  (data (i32.const 272) "pq")
  (data (i32.const 280) "d1")
  (data (i32.const 288) "d2")
  (data (i32.const 296) "image")
  (data (i32.const 304) "bm")

  (func $mkrect (param $document i32) (param $parent i32) (param $id_off i32)
                (param $x_off i32) (param $y_off i32) (param $w_off i32)
                (result i32)
    (local $e i32)
    (local.set $e (call $createElement (local.get $document) (i32.const 32) (i32.const 8)))
    (call $setAttribute (local.get $e) (i32.const 8) (i32.const 2) (local.get $id_off) (i32.const 2))
    (call $setAttribute (local.get $e) (i32.const 48) (i32.const 1) (local.get $x_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 56) (i32.const 1) (local.get $y_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 64) (i32.const 5) (local.get $w_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 72) (i32.const 6) (local.get $w_off) (i32.const 3))
    (drop (call $appendChild (local.get $parent) (local.get $e)))
    (local.get $e))

  (func $selectTwo (param $a i32) (param $b i32)
    (call $selClear)
    (call $selAdd (local.get $a))
    (call $selAdd (local.get $b)))

  (func $countTag (param $document i32) (param $off i32) (param $len i32) (result i32)
    (call $nodesLength (call $byTag (local.get $document) (local.get $off) (local.get $len))))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $hold i32)
    (local $c1 i32) (local $c2 i32) (local $m1 i32) (local $m2 i32)
    (local $k1 i32) (local $k2 i32) (local $p1 i32) (local $mk i32) (local $sy i32)
    (local $markers0 i32) (local $groups0 i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $hold (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $hold) (i32.const 8) (i32.const 2) (i32.const 240) (i32.const 5))
    (drop (call $appendChild (local.get $root) (local.get $hold)))

    ;; ── clipping ──────────────────────────────────────────────────────────────
    ;; The topmost object becomes the clip and the one below it is what gets clipped.
    (local.set $c1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 80)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (local.set $c2 (call $mkrect (local.get $document) (local.get $hold) (i32.const 88)
                                 (i32.const 160) (i32.const 176) (i32.const 208)))
    (call $selectTwo (local.get $c1) (local.get $c2))
    (if (i32.eqz (call $setMask (i32.const 1) (i32.const 0) (i32.const 0)))
      (then (return (i32.const 27))))
    (local.set $c1 (call $getElementById (local.get $document) (i32.const 80) (i32.const 2)))
    (if (i32.eqz (local.get $c1)) (then (return (i32.const 1))))
    (if (i32.eqz (call $clipPath (local.get $c1))) (then (return (i32.const 2))))
    ;; And it is a clip, not a mask -- which is the only thing the flag decides.
    (if (i32.ne (call $maskObject (local.get $c1)) (i32.const 0)) (then (return (i32.const 3))))

    (call $selClear)
    (call $selAdd (local.get $c1))
    (call $unsetMask (i32.const 1) (i32.const 0) (i32.const 0))
    (local.set $c1 (call $getElementById (local.get $document) (i32.const 80) (i32.const 2)))
    (if (i32.eqz (local.get $c1)) (then (return (i32.const 4))))
    (if (i32.ne (call $clipPath (local.get $c1)) (i32.const 0)) (then (return (i32.const 5))))

    ;; ── masking ───────────────────────────────────────────────────────────────
    ;; Same call, other flag, other answer.
    (local.set $m1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 96)
                                 (i32.const 152) (i32.const 184) (i32.const 200)))
    (local.set $m2 (call $mkrect (local.get $document) (local.get $hold) (i32.const 104)
                                 (i32.const 160) (i32.const 192) (i32.const 208)))
    (call $selectTwo (local.get $m1) (local.get $m2))
    (if (i32.eqz (call $setMask (i32.const 0) (i32.const 0) (i32.const 0)))
      (then (return (i32.const 28))))
    (local.set $m1 (call $getElementById (local.get $document) (i32.const 96) (i32.const 2)))
    (if (i32.eqz (local.get $m1)) (then (return (i32.const 6))))
    (if (i32.eqz (call $maskObject (local.get $m1))) (then (return (i32.const 7))))
    (if (i32.ne (call $clipPath (local.get $m1)) (i32.const 0)) (then (return (i32.const 8))))

    (call $selClear)
    (call $selAdd (local.get $m1))
    (call $unsetMask (i32.const 0) (i32.const 0) (i32.const 0))
    (local.set $m1 (call $getElementById (local.get $document) (i32.const 96) (i32.const 2)))
    (if (i32.eqz (local.get $m1)) (then (return (i32.const 9))))
    (if (i32.ne (call $maskObject (local.get $m1)) (i32.const 0)) (then (return (i32.const 10))))

    ;; applyToLayer needs a desktop -- ObjectSet::setMask returns immediately without one
    ;; (selection-chemistry.cpp ObjectSet::setMask()) -- so headless it answers false instead of doing nothing
    ;; quietly. That is the difference between a refusal and a no-op, and it is the whole reason
    ;; this operation reports a result at all.
    ;; Fresh ids: c1 and c2 are still in the document from the section above, and reusing an id
    ;; would leave getElementById answering about the wrong element.
    (local.set $c1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 280)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (local.set $c2 (call $mkrect (local.get $document) (local.get $hold) (i32.const 288)
                                 (i32.const 160) (i32.const 176) (i32.const 208)))
    (call $selectTwo (local.get $c1) (local.get $c2))
    (if (i32.ne (call $setMask (i32.const 1) (i32.const 1) (i32.const 0)) (i32.const 0))
      (then (return (i32.const 29))))
    ;; And having refused, it left the document alone.
    (if (i32.ne (call $clipPath (local.get $c1)) (i32.const 0)) (then (return (i32.const 30))))

    ;; removeOriginal decides the fate of the object that BECOMES the clip -- the topmost of the
    ;; selection, per /options/maskobject/topmost. Without it that object stays in the document
    ;; alongside the copy inside the clipPath; with it, it goes.
    (call $selectTwo (local.get $c1) (local.get $c2))
    (if (i32.eqz (call $setMask (i32.const 1) (i32.const 0) (i32.const 1)))
      (then (return (i32.const 31))))
    (if (i32.ne (call $getElementById (local.get $document) (i32.const 288) (i32.const 2))
                (i32.const 0))
      (then (return (i32.const 32))))
    ;; The one underneath is still there, and is now clipped.
    (local.set $c1 (call $getElementById (local.get $document) (i32.const 280) (i32.const 2)))
    (if (i32.eqz (local.get $c1)) (then (return (i32.const 33))))
    (if (i32.eqz (call $clipPath (local.get $c1))) (then (return (i32.const 34))))

    ;; ── clip group ────────────────────────────────────────────────────────────
    ;; A different result from setMask, not a convenience over it: the selection is wrapped
    ;; in a pair of groups -- an inner one holding the contents and an outer one carrying the
    ;; clip-path -- and the clip is a <use> pointing back at the inner group
    ;; (selection-chemistry.cpp ObjectSet::setClipGroup()). So the shape is what gets asserted,
    ;; not a count of <g> elements: the structure is the promise, and counting only says
    ;; something happened.
    ;;
    ;; As with grouping, the members are duplicated in, so k1 has to be re-found afterwards.
    (local.set $k1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 112)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (local.set $k2 (call $mkrect (local.get $document) (local.get $hold) (i32.const 120)
                                 (i32.const 160) (i32.const 176) (i32.const 208)))
    (call $selectTwo (local.get $k1) (local.get $k2))
    (call $setClipGroup)
    (local.set $k1 (call $getElementById (local.get $document) (i32.const 112) (i32.const 2)))
    (if (i32.eqz (local.get $k1)) (then (return (i32.const 11))))
    (local.set $k2 (call $parentNode (local.get $k1)))
    (if (i32.eqz (local.get $k2)) (then (return (i32.const 19))))
    (local.set $k2 (call $parentNode (local.get $k2)))
    (if (i32.eqz (local.get $k2)) (then (return (i32.const 20))))
    (if (i32.eqz (call $clipPath (local.get $k2))) (then (return (i32.const 21))))

    ;; ── object to pattern ─────────────────────────────────────────────────────
    (if (i32.ne (call $countTag (local.get $document) (i32.const 216) (i32.const 7)) (i32.const 0))
      (then (return (i32.const 12))))
    (local.set $p1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 128)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (call $selClear)
    (call $selAdd (local.get $p1))
    (call $toPattern (i32.const 1))
    (if (i32.ne (call $countTag (local.get $document) (i32.const 216) (i32.const 7)) (i32.const 1))
      (then (return (i32.const 13))))
    ;; Undoing it puts the pattern's contents back as ordinary objects.
    (call $untile)
    (if (i32.eqz (call $nodesLength (call $childNodes (local.get $hold))))
      (then (return (i32.const 14))))

    ;; As with markers, apply decides whether the definition is put to use or merely created,
    ;; and without it the source object stays where it is.
    (local.set $p1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 272)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (call $selClear)
    (call $selAdd (local.get $p1))
    (call $toPattern (i32.const 0))
    (if (i32.lt_s (call $countTag (local.get $document) (i32.const 216) (i32.const 7))
                  (i32.const 2))
      (then (return (i32.const 35))))
    (if (i32.eqz (call $getElementById (local.get $document) (i32.const 272) (i32.const 2)))
      (then (return (i32.const 36))))

    ;; ── object to marker ──────────────────────────────────────────────────────
    ;; The fixture already defines one, so this counts the change rather than the total.
    (local.set $markers0 (call $countTag (local.get $document) (i32.const 224) (i32.const 6)))
    (local.set $mk (call $mkrect (local.get $document) (local.get $hold) (i32.const 136)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (call $selClear)
    (call $selAdd (local.get $mk))
    (call $toMarker (i32.const 1))
    (if (i32.ne (call $countTag (local.get $document) (i32.const 224) (i32.const 6))
                (i32.add (local.get $markers0) (i32.const 1)))
      (then (return (i32.const 15))))
    ;; apply decides whether the definition is put to use or merely created. Without it the
    ;; source object is left where it is, which is what a plugin building a library of parts
    ;; wants; with it the source is consumed.
    (local.set $markers0 (call $countTag (local.get $document) (i32.const 224) (i32.const 6)))
    (local.set $mk (call $mkrect (local.get $document) (local.get $hold) (i32.const 264)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (call $selClear)
    (call $selAdd (local.get $mk))
    (call $toMarker (i32.const 0))
    (if (i32.ne (call $countTag (local.get $document) (i32.const 224) (i32.const 6))
                (i32.add (local.get $markers0) (i32.const 1)))
      (then (return (i32.const 25))))
    (if (i32.eqz (call $getElementById (local.get $document) (i32.const 264) (i32.const 2)))
      (then (return (i32.const 26))))

    ;; ── object to symbol ──────────────────────────────────────────────────────
    (if (i32.ne (call $countTag (local.get $document) (i32.const 232) (i32.const 6)) (i32.const 0))
      (then (return (i32.const 16))))
    (local.set $sy (call $mkrect (local.get $document) (local.get $hold) (i32.const 144)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (if (i32.eqz (local.get $sy)) (then (return (i32.const 22))))
    (call $selClear)
    (call $selAdd (local.get $sy))
    (if (i32.eqz (call $isSelected (local.get $sy))) (then (return (i32.const 23))))
    (call $toSymbol)
    ;; ONE symbol, though the document now holds a <use> of it as well. Inkscape's lookups walk
    ;; the object tree, where a <use> holds a copy of its target as a child object; those copies
    ;; are not document elements and are not results. The same applies to getElementsByClassName
    ;; and the selector lookups, which walk the same tree.
    (if (i32.ne (call $countTag (local.get $document) (i32.const 232) (i32.const 6)) (i32.const 1))
      (then (return (i32.const 17))))
    ;; The <use> is real and is still counted, which is what makes the case above a fix rather
    ;; than a filter that hides the clone entirely.
    (if (i32.eqz (call $countTag (local.get $document) (i32.const 248) (i32.const 3)))
      (then (return (i32.const 24))))

    ;; No round trip back out of a symbol: unSymbol is `blocked` and traps, because
    ;; SPSymbol::unSymbol() crashes with no desktop. refuse-blocked.wat covers it.

    ;; ── object to bitmap ──────────────────────────────────────────────────────
    ;; Renders the selection and puts the result back as an <image>. It reaches for a desktop
    ;; only to flash "Rendering bitmap..." and set a cursor, so it runs from the command line --
    ;; which is where an extension that rasterises part of a drawing would want it.
    (if (i32.ne (call $countTag (local.get $document) (i32.const 296) (i32.const 5)) (i32.const 0))
      (then (return (i32.const 37))))
    (local.set $p1 (call $mkrect (local.get $document) (local.get $hold) (i32.const 304)
                                 (i32.const 152) (i32.const 168) (i32.const 200)))
    (call $selClear)
    (call $selAdd (local.get $p1))
    (call $bitmapCopy)
    (if (i32.ne (call $countTag (local.get $document) (i32.const 296) (i32.const 5)) (i32.const 1))
      (then (return (i32.const 38))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 8))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
