;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: arranging objects -- grouping, z-order, duplication, clones, canvas fitting.
;;
;; These are ObjectSet operations, so they act on the selection rather than on an element
;; passed in. That is the shape the C++ side has and the shape the builtins use, and it works
;; headless because the document owns a Selection whether or not anyone is looking at it.
;;
;; The module builds its own subjects rather than rearranging the fixture's, since every other
;; case file asserts counts and positions over what the fixture declares.
;;
;; The three rects are placed deliberately: r1 and r3 overlap, r2 overlaps neither. That is
;; what separates raise from stackUp -- one steps over the next OVERLAPPING object and the
;; other steps exactly one position -- and with three mutually overlapping rects the two are
;; indistinguishable, so a host that confused them would pass.
;;
;; Build:  water cover-arrange.wat -o cover-arrange.wasm

(module
  (import "org.inkscape.Document" "documentElement"      (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"        (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementsByTagName" (func $byTag (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"       (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"          (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "parentNode"           (func $parentNode (param i32) (result i32)))
  (import "org.inkscape.Node"     "nextSibling"          (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.Node"     "previousSibling"      (func $prevSibling (param i32) (result i32)))
  (import "org.inkscape.Node"     "childNodes"           (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"         (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"         (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.Selection" "selectionClear" (func $selClear))
  (import "org.inkscape.Selection" "selectionAdd"   (func $selAdd (param i32)))
  (import "org.inkscape.Selection" "isSelected"     (func $isSelected (param i32) (result i32)))
  (import "org.inkscape.Layers" "createLayer"    (func $createLayer (param i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.Selection" "group"          (func $group (result i32)))
  (import "org.inkscape.Selection" "ungroup"        (func $ungroup))
  (import "org.inkscape.Selection" "ungroupAll"     (func $ungroupAll))
  (import "org.inkscape.Selection" "popFromGroup"   (func $popFromGroup))
  (import "org.inkscape.Selection" "stackUp"        (func $stackUp))
  (import "org.inkscape.Selection" "stackDown"      (func $stackDown))
  (import "org.inkscape.Selection" "raise"          (func $raise))
  (import "org.inkscape.Selection" "raiseToTop"     (func $raiseToTop))
  (import "org.inkscape.Selection" "lower"          (func $lower))
  (import "org.inkscape.Selection" "lowerToBottom"  (func $lowerToBottom))
  (import "org.inkscape.Selection" "toLayer"        (func $toLayer (param i32)))
  (import "org.inkscape.Selection" "duplicate"      (func $duplicate (param i32 i32)))
  (import "org.inkscape.Selection" "clone"          (func $clone))
  (import "org.inkscape.Selection" "unlink"         (func $unlink (param i32) (result i32)))
  (import "org.inkscape.Selection" "cloneOriginal"  (func $cloneOriginal))
  (import "org.inkscape.Selection" "fitCanvas"      (func $fitCanvas (result i32)))

  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"   (func $nodesItem (param i32 i32) (result i32)))
  (import "org.inkscape.Selection" "selectedIds"    (func $selectedIds (result i32)))
  (import "org.inkscape.DOMStringList" "length" (func $listLength (param i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-arrange")
  (data (i32.const 32)  "svg:rect")
  (data (i32.const 48)  "x")
  (data (i32.const 56)  "y")
  (data (i32.const 64)  "width")
  (data (i32.const 72)  "height")
  (data (i32.const 80)  "r1")
  (data (i32.const 88)  "r2")
  (data (i32.const 96)  "r3")
  (data (i32.const 104) "300")
  (data (i32.const 112) "350")
  (data (i32.const 120) "305")
  (data (i32.const 128) "10")
  (data (i32.const 136) "15")
  (data (i32.const 144) "20")
  (data (i32.const 152) "use")
  (data (i32.const 160) "Lyr")
  (data (i32.const 168) "hold")
  (data (i32.const 176) "g1")
  (data (i32.const 184) "g2")
  (data (i32.const 192) "nr")
  (data (i32.const 200) "n2")
  (data (i32.const 208) "grp")
  (data (i32.const 216) "n3")
  (data (i32.const 224) "hd")
  (data (i32.const 232) "style")
  (data (i32.const 240) "display:none")

  (func $mkgroup (param $document i32) (param $parent i32) (param $id_off i32) (param $id_len i32)
                 (result i32)
    (local $e i32)
    (local.set $e (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $e) (i32.const 8) (i32.const 2) (local.get $id_off) (local.get $id_len))
    (drop (call $appendChild (local.get $parent) (local.get $e)))
    (local.get $e))

  (func $mkrect (param $document i32) (param $parent i32) (param $id_off i32) (param $id_len i32)
                (param $x_off i32) (param $y_off i32)
                (result i32)
    (local $e i32)
    (local.set $e (call $createElement (local.get $document) (i32.const 32) (i32.const 8)))
    (call $setAttribute (local.get $e) (i32.const 8) (i32.const 2) (local.get $id_off) (local.get $id_len))
    (call $setAttribute (local.get $e) (i32.const 48) (i32.const 1) (local.get $x_off) (i32.const 3))
    (call $setAttribute (local.get $e) (i32.const 56) (i32.const 1) (local.get $y_off) (i32.const 2))
    (call $setAttribute (local.get $e) (i32.const 64) (i32.const 5) (i32.const 144) (i32.const 2))
    (call $setAttribute (local.get $e) (i32.const 72) (i32.const 6) (i32.const 144) (i32.const 2))
    (drop (call $appendChild (local.get $parent) (local.get $e)))
    (local.get $e))

  (func $selectOne (param $e i32)
    (call $selClear)
    (call $selAdd (local.get $e)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $hold i32)
    (local $r1 i32) (local $r2 i32) (local $r3 i32)
    (local $g i32) (local $g1 i32) (local $g2 i32) (local $nr i32) (local $n2 i32)
    (local $layer i32) (local $before i32) (local $u i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $hold (call $mkgroup (local.get $document) (local.get $root) (i32.const 168) (i32.const 4)))

    ;; r1 and r3 overlap; r2 sits clear of both.
    (local.set $r1 (call $mkrect (local.get $document) (local.get $hold)
                                 (i32.const 80) (i32.const 2) (i32.const 104) (i32.const 128)))
    (local.set $r2 (call $mkrect (local.get $document) (local.get $hold)
                                 (i32.const 88) (i32.const 2) (i32.const 112) (i32.const 128)))
    (local.set $r3 (call $mkrect (local.get $document) (local.get $hold)
                                 (i32.const 96) (i32.const 2) (i32.const 120) (i32.const 136)))
    (if (i32.eqz (local.get $r3)) (then (return (i32.const 1))))
    (if (i32.ne (call $nextSibling (local.get $r1)) (local.get $r2))
      (then (return (i32.const 2))))

    ;; ── z-order ───────────────────────────────────────────────────────────────
    ;; One position, regardless of what is or is not underneath.
    (call $selectOne (local.get $r1))
    (call $stackUp)
    (if (i32.ne (call $prevSibling (local.get $r1)) (local.get $r2)) (then (return (i32.const 3))))
    (if (i32.ne (call $nextSibling (local.get $r1)) (local.get $r3)) (then (return (i32.const 4))))

    (call $raiseToTop)
    (if (i32.ne (call $nextSibling (local.get $r1)) (i32.const 0)) (then (return (i32.const 5))))
    (call $lowerToBottom)
    (if (i32.ne (call $prevSibling (local.get $r1)) (i32.const 0)) (then (return (i32.const 6))))

    ;; Raise steps over the next OVERLAPPING object, so it clears r3 and lands on top -- past
    ;; r2, which stackUp would have stopped at. This is the case that tells the two apart.
    (call $raise)
    (if (i32.ne (call $nextSibling (local.get $r1)) (i32.const 0)) (then (return (i32.const 7))))
    ;; And lower comes back down past the same one.
    (call $lower)
    (if (i32.ne (call $nextSibling (local.get $r1)) (local.get $r3)) (then (return (i32.const 8))))
    (call $stackDown)
    (if (i32.ne (call $prevSibling (local.get $r1)) (i32.const 0)) (then (return (i32.const 9))))

    ;; ── grouping ──────────────────────────────────────────────────────────────
    ;; Grouping REBUILDS what it groups. Each member is duplicated into the new group and the
    ;; original unparented (selection-chemistry.cpp ObjectSet::group()), so a handle held across the call
    ;; names a node that is no longer in the document -- not a stale handle the host will refuse,
    ;; but a live handle onto a detached node, which is worse. A plugin has to re-find its
    ;; subjects afterwards, and every step below does, which is why this is asserted rather than
    ;; quietly worked around.
    (call $selClear)
    (call $selAdd (local.get $r1))
    (call $selAdd (local.get $r2))
    (local.set $g (call $group))
    (if (i32.eqz (local.get $g)) (then (return (i32.const 10))))
    (call $setAttribute (local.get $g) (i32.const 8) (i32.const 2) (i32.const 208) (i32.const 3))
    (if (i32.ne (call $parentNode (local.get $r1)) (i32.const 0)) (then (return (i32.const 11))))
    (if (i32.ne (call $nodesLength (call $childNodes (local.get $g))) (i32.const 2))
      (then (return (i32.const 12))))
    (if (i32.ne (call $parentNode (local.get $g)) (local.get $hold)) (then (return (i32.const 13))))

    ;; The copies carry the ids across, the originals having left, so the document still answers
    ;; for them.
    (local.set $r1 (call $getElementById (local.get $document) (i32.const 80) (i32.const 2)))
    (local.set $r2 (call $getElementById (local.get $document) (i32.const 88) (i32.const 2)))
    (if (i32.eqz (local.get $r1)) (then (return (i32.const 14))))
    (if (i32.ne (call $parentNode (local.get $r1)) (local.get $g)) (then (return (i32.const 15))))

    (call $selectOne (local.get $g))
    (call $ungroup)
    (local.set $r1 (call $getElementById (local.get $document) (i32.const 80) (i32.const 2)))
    (if (i32.eqz (local.get $r1)) (then (return (i32.const 16))))
    (if (i32.ne (call $parentNode (local.get $r1)) (local.get $hold)) (then (return (i32.const 17))))

    ;; Popping one item out of its group, rather than dissolving the group.
    (local.set $g1 (call $mkgroup (local.get $document) (local.get $hold) (i32.const 176) (i32.const 2)))
    (local.set $nr (call $mkrect (local.get $document) (local.get $g1)
                                 (i32.const 192) (i32.const 2) (i32.const 104) (i32.const 128)))
    (call $selectOne (local.get $nr))
    (call $popFromGroup)
    (local.set $nr (call $getElementById (local.get $document) (i32.const 192) (i32.const 2)))
    (if (i32.eqz (local.get $nr)) (then (return (i32.const 18))))
    (if (i32.ne (call $parentNode (local.get $nr)) (local.get $hold)) (then (return (i32.const 19))))

    ;; Ungrouping all the way down, not one level.
    ;;
    ;; The outer group holds a loose rect beside the inner group, and that is load-bearing.
    ;; ungroup_all repeats ungroup() until the SELECTION SIZE stops changing
    ;; (selection-chemistry.cpp ObjectSet::ungroup_all()), so a chain of groups each holding exactly one group
    ;; ungroups once and stops -- the size is 1 before and after, and the loop reads that as
    ;; finished. With a second child the count moves and the recursion carries on. Worth knowing
    ;; before relying on this to flatten arbitrary nesting.
    (local.set $g1 (call $mkgroup (local.get $document) (local.get $hold) (i32.const 176) (i32.const 2)))
    (local.set $g2 (call $mkgroup (local.get $document) (local.get $g1) (i32.const 184) (i32.const 2)))
    (drop (call $mkrect (local.get $document) (local.get $g1)
                        (i32.const 216) (i32.const 2) (i32.const 112) (i32.const 136)))
    (local.set $n2 (call $mkrect (local.get $document) (local.get $g2)
                                 (i32.const 200) (i32.const 2) (i32.const 104) (i32.const 128)))
    (call $selectOne (local.get $g1))
    (call $ungroupAll)
    (local.set $n2 (call $getElementById (local.get $document) (i32.const 200) (i32.const 2)))
    (if (i32.eqz (local.get $n2)) (then (return (i32.const 20))))
    (if (i32.ne (call $parentNode (local.get $n2)) (local.get $hold)) (then (return (i32.const 21))))

    ;; ── moving between layers ─────────────────────────────────────────────────
    (local.set $layer (call $createLayer (local.get $root) (i32.const 160) (i32.const 3) (i32.const 1)))
    (if (i32.eqz (local.get $layer)) (then (return (i32.const 22))))
    (call $selectOne (local.get $r1))
    (call $toLayer (local.get $layer))
    (local.set $r1 (call $getElementById (local.get $document) (i32.const 80) (i32.const 2)))
    (if (i32.eqz (local.get $r1)) (then (return (i32.const 23))))
    (if (i32.ne (call $parentNode (local.get $r1)) (local.get $layer)) (then (return (i32.const 24))))

    ;; ── duplicating ───────────────────────────────────────────────────────────
    ;; The copy becomes the selection, which is how the builtins chain work onto it.
    (local.set $r2 (call $getElementById (local.get $document) (i32.const 88) (i32.const 2)))
    (if (i32.eqz (local.get $r2)) (then (return (i32.const 25))))
    (local.set $before (call $nodesLength (call $childNodes (call $parentNode (local.get $r2)))))
    (call $selectOne (local.get $r2))
    (call $duplicate (i32.const 0) (i32.const 0))
    (if (i32.ne (call $isSelected (local.get $r2)) (i32.const 0)) (then (return (i32.const 26))))
    (if (i32.ne (call $nodesLength (call $childNodes (call $parentNode (local.get $r2))))
                (i32.add (local.get $before) (i32.const 1)))
      (then (return (i32.const 40))))

    ;; includeHidden does NOT decide whether a hidden object is copied -- it is copied either
    ;; way. It decides whether the COPY is left selected (selection-chemistry.cpp ObjectSet::duplicate()), which
    ;; matters because the selection afterwards is what a plugin chains its next operation onto.
    ;; A name that reads like it gates the duplication is worth pinning down for that reason.
    (local.set $u (call $mkrect (local.get $document) (local.get $hold)
                                (i32.const 224) (i32.const 2) (i32.const 104) (i32.const 128)))
    (call $setAttribute (local.get $u) (i32.const 232) (i32.const 5) (i32.const 240) (i32.const 12))
    (local.set $before (call $nodesLength (call $childNodes (local.get $hold))))
    (call $selectOne (local.get $u))
    (call $duplicate (i32.const 0) (i32.const 0))
    (if (i32.ne (call $nodesLength (call $childNodes (local.get $hold)))
                (i32.add (local.get $before) (i32.const 1)))
      (then (return (i32.const 41))))
    (if (i32.ne (call $listLength (call $selectedIds)) (i32.const 0))
      (then (return (i32.const 42))))

    (local.set $before (call $nodesLength (call $childNodes (local.get $hold))))
    (call $selectOne (local.get $u))
    (call $duplicate (i32.const 0) (i32.const 1))
    (if (i32.ne (call $nodesLength (call $childNodes (local.get $hold)))
                (i32.add (local.get $before) (i32.const 1)))
      (then (return (i32.const 43))))
    (if (i32.ne (call $listLength (call $selectedIds)) (i32.const 1))
      (then (return (i32.const 44))))

    ;; ── clones ────────────────────────────────────────────────────────────────
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 152) (i32.const 3)))
                (i32.const 0))
      (then (return (i32.const 28))))
    (call $selectOne (local.get $r2))
    (call $clone)
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 152) (i32.const 3)))
                (i32.const 1))
      (then (return (i32.const 29))))
    ;; The clone is what is selected now, so asking for its original selects r2 back.
    (call $cloneOriginal)
    (if (i32.ne (call $isSelected (local.get $r2)) (i32.const 1)) (then (return (i32.const 30))))

    ;; Unlinking replaces the <use> with a real copy, so that one stops being a clone. The
    ;; first clone is still around, hence one left rather than none.
    (call $selectOne (local.get $r2))
    (call $clone)
    (if (i32.eqz (call $unlink (i32.const 0))) (then (return (i32.const 31))))
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 152) (i32.const 3)))
                (i32.const 1))
      (then (return (i32.const 32))))

    ;; Recursive or not is the difference between unlinking what is selected and unlinking what
    ;; is inside what is selected. With the surviving clone tucked into a group and the GROUP
    ;; selected, the plain call finds nothing directly selected to unlink and says so.
    (local.set $u (call $nodesItem (call $byTag (local.get $document) (i32.const 152) (i32.const 3))
                                   (i32.const 0)))
    (if (i32.eqz (local.get $u)) (then (return (i32.const 33))))
    (local.set $g2 (call $mkgroup (local.get $document) (local.get $hold) (i32.const 184) (i32.const 2)))
    (drop (call $appendChild (local.get $g2) (local.get $u)))
    (call $selectOne (local.get $g2))
    (if (i32.ne (call $unlink (i32.const 0)) (i32.const 0)) (then (return (i32.const 34))))
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 152) (i32.const 3)))
                (i32.const 1))
      (then (return (i32.const 35))))
    ;; Recursively, the same selection reaches it.
    (call $selectOne (local.get $g2))
    (if (i32.eqz (call $unlink (i32.const 1))) (then (return (i32.const 36))))
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 152) (i32.const 3)))
                (i32.const 0))
      (then (return (i32.const 37))))

    ;; ── fitting the canvas ────────────────────────────────────────────────────
    ;; Last, because it moves everything: fitting the page to one 20-unit rect renumbers every
    ;; coordinate in the document.
    ;;
    ;; No margin argument, deliberately. ObjectSet::fitCanvas takes a with_margins flag and
    ;; hands it to SPDocument::fitToRect, whose definition does not name that parameter and
    ;; never reads it (document.cpp SPDocument::fitToRect()) -- so publishing the flag would publish a control
    ;; that does nothing. PageManager's separate fitToRect does honour margins, and that one is
    ;; reachable through fitPageToSelection.
    (call $selectOne (local.get $r2))
    (if (i32.eqz (call $fitCanvas)) (then (return (i32.const 38))))
    (if (i32.eq (call $getAttribute (local.get $root) (i32.const 64) (i32.const 5)
                                    (i32.const 512) (i32.const 64))
                (i32.const 3))
      (then (return (i32.const 39))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 10))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
