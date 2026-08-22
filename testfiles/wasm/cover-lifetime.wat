;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: node lifetime and the DOM insertion rules that go with it.
;;
;; Inkscape's addChild() asserts that the node being inserted has no parent, so handing it one
;; that is still attached elsewhere aborts the process -- not a trap, not an error return, a
;; dead editor. DOM says inserting an attached node MOVES it, and moving a node between groups
;; is about the most ordinary thing a plugin does, so this is the difference between an API
;; and a landmine.
;;
;; The refcounting matters as much as the tree surgery. A node in the document is kept alive
;; by its parent alone, so between being detached and reattached it is held by nothing the
;; collector can see; and a node the guest removes must stay usable, because DOM hands it back
;; and the guest still has a handle to it.
;;
;; Build:  water cover-lifetime.wat -o cover-lifetime.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "insertBefore"    (func $insertBefore (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "removeChild"     (func $removeChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "parentNode"      (func $parentNode (param i32) (result i32)))
  (import "org.inkscape.Node"     "nextSibling"     (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.Node"     "childNodes"      (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Node"     "nodeName"        (func $nodeName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.NodeList" "length"          (func $listLength (param i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"    (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:g")
  (data (i32.const 8)  "id")
  (data (i32.const 16) "ok-lifetime")
  (data (i32.const 32) "box")
  (data (i32.const 40) "group")
  (data (i32.const 48) "svg:rect")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $group i32) (local $box i32) (local $host i32)
    (local $a i32) (local $b i32) (local $removed i32) (local $marker i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 40) (i32.const 5)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 32) (i32.const 3)))
    (if (i32.or (i32.eqz (local.get $group)) (i32.eqz (local.get $box)))
      (then (return (i32.const 1))))

    (local.set $host (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (drop (call $appendChild (local.get $root) (local.get $host)))

    ;; ── moving an attached node ───────────────────────────────────────────────
    ;; The rect is a child of the fixture's group. Appending it somewhere else must move it,
    ;; not abort, and must leave it in exactly one place.
    (drop (call $appendChild (local.get $host) (local.get $box)))
    (if (i32.ne (call $parentNode (local.get $box)) (local.get $host))
      (then (return (i32.const 2))))
    ;; The group had three children and has lost one.
    (if (i32.ne (call $listLength (call $childNodes (local.get $group))) (i32.const 2))
      (then (return (i32.const 3))))
    ;; The moved node is still whole: reading an attribute proves it was not collected in the
    ;; window between being detached and reattached.
    (if (i32.le_s (call $getAttribute (local.get $box) (i32.const 8) (i32.const 2)
                                      (i32.const 512) (i32.const 64)) (i32.const 0))
      (then (return (i32.const 4))))

    ;; ── moving with insertBefore, within one parent ───────────────────────────
    (local.set $a (call $createElement (local.get $document) (i32.const 48) (i32.const 8)))
    (local.set $b (call $createElement (local.get $document) (i32.const 48) (i32.const 8)))
    (drop (call $appendChild (local.get $host) (local.get $a)))
    (drop (call $appendChild (local.get $host) (local.get $b)))

    ;; Order is [box, a, b]. Moving b before a gives [box, b, a] -- a reorder within the same
    ;; parent, where the node has to be detached and reinserted without the insertion point
    ;; going stale in between.
    (drop (call $insertBefore (local.get $host) (local.get $b) (local.get $a)))
    (if (i32.ne (call $nextSibling (local.get $box)) (local.get $b))
      (then (return (i32.const 5))))
    (if (i32.ne (call $nextSibling (local.get $b)) (local.get $a))
      (then (return (i32.const 6))))
    (if (i32.ne (call $listLength (call $childNodes (local.get $host))) (i32.const 3))
      (then (return (i32.const 7))))

    ;; Moving a node to where it already is must leave the tree unchanged rather than losing
    ;; it: the insertion point is its own previous sibling, which detaching invalidates.
    (drop (call $insertBefore (local.get $host) (local.get $b) (local.get $a)))
    (if (i32.ne (call $listLength (call $childNodes (local.get $host))) (i32.const 3))
      (then (return (i32.const 8))))
    (if (i32.ne (call $nextSibling (local.get $b)) (local.get $a))
      (then (return (i32.const 9))))

    ;; ── a removed node stays usable ───────────────────────────────────────────
    ;; DOM returns the node it removed, and the guest still holds a handle. It is out of the
    ;; tree and nothing in the document points at it, so it survives only if the host anchored
    ;; it -- reading its name is what would fault if it had not.
    (local.set $removed (call $removeChild (local.get $host) (local.get $a)))
    (if (i32.ne (local.get $removed) (local.get $a)) (then (return (i32.const 10))))
    (if (i32.ne (call $parentNode (local.get $a)) (i32.const 0)) (then (return (i32.const 11))))
    (if (i32.ne (call $nodeName (local.get $a) (i32.const 512) (i32.const 64)) (i32.const 8))
      (then (return (i32.const 12))))

    ;; ...and can be put back.
    (drop (call $appendChild (local.get $host) (local.get $a)))
    (if (i32.ne (call $parentNode (local.get $a)) (local.get $host))
      (then (return (i32.const 13))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 11))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
