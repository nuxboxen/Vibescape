;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the DOM Core writing surface -- insertBefore, removeChild, replaceChild,
;; cloneNode, the node factories, text content, and the attribute operations.
;;
;; insertBefore gets the most attention because it is the one whose reference argument is
;; inverted against Inkscape's addChild(): DOM names the node to insert BEFORE, Inkscape the
;; one to insert AFTER. Getting that backwards reorders a drawing without erroring, so the
;; ordering is asserted rather than assumed.
;;
;; Build:  water cover-mutation.wat -o cover-mutation.wasm

(module
  (import "org.inkscape.Document" "documentElement"  (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"    (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createElementNS"  (func $createElementNS (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createTextNode"   (func $createTextNode (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createComment"    (func $createComment (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"   (func $getElementById (param i32 i32 i32) (result i32)))

  (import "org.inkscape.Node" "appendChild"    (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node" "insertBefore"   (func $insertBefore (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "removeChild"    (func $removeChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node" "replaceChild"   (func $replaceChild (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "cloneNode"      (func $cloneNode (param i32 i32) (result i32)))
  (import "org.inkscape.Node" "firstChild"     (func $firstChild (param i32) (result i32)))
  (import "org.inkscape.Node" "lastChild"      (func $lastChild (param i32) (result i32)))
  (import "org.inkscape.Node" "nextSibling"    (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.Node" "childNodes"     (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Node" "textContent"    (func $textContent (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "setTextContent" (func $setTextContent (param i32 i32 i32)))
  (import "org.inkscape.Node" "nodeType"       (func $nodeType (param i32) (result i32)))
  (import "org.inkscape.NodeList" "length"     (func $listLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"       (func $listItem (param i32 i32) (result i32)))

  (import "org.inkscape.Element" "setAttribute"      (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element" "getAttribute"      (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Element" "hasAttribute"      (func $hasAttribute (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Element" "removeAttribute"   (func $removeAttribute (param i32 i32 i32)))
  (import "org.inkscape.Element" "getAttributeNames" (func $getAttributeNames (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "length"      (func $namesLength (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "item"        (func $namesItem (param i32 i32 i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "svg:rect")
  (data (i32.const 24)  "id")
  (data (i32.const 32)  "ok-mutation")
  (data (i32.const 48)  "hello")
  (data (i32.const 56)  "a comment")
  (data (i32.const 72)  "data-x")
  (data (i32.const 80)  "1")
  (data (i32.const 88)  "http://example.com/ns")
  (data (i32.const 120) "ex:thing")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $host i32) (local $a i32) (local $b i32) (local $c i32)
    (local $clone i32) (local $text i32) (local $names i32) (local $marker i32)

    (local.set $root (call $documentElement (local.get $document)))

    ;; A container of our own, so the fixture's contents are untouched.
    (local.set $host (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (drop (call $appendChild (local.get $root) (local.get $host)))

    (local.set $a (call $createElement (local.get $document) (i32.const 8) (i32.const 8)))
    (local.set $b (call $createElement (local.get $document) (i32.const 8) (i32.const 8)))
    (local.set $c (call $createElement (local.get $document) (i32.const 8) (i32.const 8)))

    ;; Append $a, then insert $b BEFORE it. DOM order must be [b, a] -- if the reference
    ;; argument were passed through unchanged, Inkscape's addChild would give [a, b].
    (drop (call $appendChild (local.get $host) (local.get $a)))
    (drop (call $insertBefore (local.get $host) (local.get $b) (local.get $a)))
    (if (i32.ne (call $firstChild (local.get $host)) (local.get $b))
      (then (return (i32.const 1))))
    (if (i32.ne (call $nextSibling (local.get $b)) (local.get $a))
      (then (return (i32.const 2))))

    ;; A null reference means append, as DOM specifies.
    (drop (call $insertBefore (local.get $host) (local.get $c) (i32.const 0)))
    (if (i32.ne (call $lastChild (local.get $host)) (local.get $c))
      (then (return (i32.const 3))))

    ;; replaceChild swaps a fresh node in for $a and leaves the count alone.
    (local.set $clone (call $cloneNode (local.get $c) (i32.const 0)))
    (drop (call $replaceChild (local.get $host) (local.get $clone) (local.get $a)))
    (if (i32.ne (call $listLength (call $childNodes (local.get $host))) (i32.const 3))
      (then (return (i32.const 4))))
    (if (i32.ne (call $listItem (call $childNodes (local.get $host)) (i32.const 1)) (local.get $clone))
      (then (return (i32.const 5))))

    ;; removeChild takes the count back down.
    (drop (call $removeChild (local.get $host) (local.get $c)))
    (if (i32.ne (call $listLength (call $childNodes (local.get $host))) (i32.const 2))
      (then (return (i32.const 6))))

    ;; A shallow clone of a populated node has no children; a deep one has them all.
    (if (i32.ne (call $listLength (call $childNodes
        (call $cloneNode (local.get $host) (i32.const 0)))) (i32.const 0))
      (then (return (i32.const 7))))
    (if (i32.ne (call $listLength (call $childNodes
        (call $cloneNode (local.get $host) (i32.const 1)))) (i32.const 2))
      (then (return (i32.const 8))))

    ;; Text and comment nodes report DOM's numbering: TEXT_NODE is 3, COMMENT_NODE is 8.
    (local.set $text (call $createTextNode (local.get $document) (i32.const 48) (i32.const 5)))
    (if (i32.ne (call $nodeType (local.get $text)) (i32.const 3))
      (then (return (i32.const 9))))
    (if (i32.ne (call $nodeType (call $createComment (local.get $document) (i32.const 56) (i32.const 9)))
                (i32.const 8))
      (then (return (i32.const 10))))

    ;; textContent round-trips.
    (call $setTextContent (local.get $text) (i32.const 56) (i32.const 9))
    (if (i32.ne (call $textContent (local.get $text) (i32.const 200) (i32.const 64)) (i32.const 9))
      (then (return (i32.const 11))))

    ;; An element in a namespace the document never declared is still creatable -- authoring
    ;; is not limited to what Inkscape itself renders.
    (if (i32.eqz (call $createElementNS (local.get $document)
        (i32.const 88) (i32.const 21) (i32.const 120) (i32.const 8)))
      (then (return (i32.const 12))))

    ;; Attributes: absent, then set, then removed.
    (if (i32.ne (call $hasAttribute (local.get $b) (i32.const 72) (i32.const 6)) (i32.const 0))
      (then (return (i32.const 13))))
    (call $setAttribute (local.get $b) (i32.const 72) (i32.const 6) (i32.const 80) (i32.const 1))
    (if (i32.ne (call $hasAttribute (local.get $b) (i32.const 72) (i32.const 6)) (i32.const 1))
      (then (return (i32.const 14))))

    ;; getAttributeNames sees it. The order is not asserted and neither is the count: a node
    ;; that has been inserted also carries an id Inkscape assigned, so pinning either would
    ;; be testing an implementation detail rather than the operation.
    (local.set $names (call $getAttributeNames (local.get $b)))
    (if (i32.lt_s (call $namesLength (local.get $names)) (i32.const 1))
      (then (return (i32.const 15))))
    (if (i32.le_s (call $namesItem (local.get $names) (i32.const 0) (i32.const 200) (i32.const 64))
                  (i32.const 0))
      (then (return (i32.const 16))))
    ;; Past the end reports absence rather than an empty string.
    (if (i32.ne (call $namesItem (local.get $names) (i32.const 999) (i32.const 200) (i32.const 64))
                (i32.const -1))
      (then (return (i32.const 17))))

    (call $removeAttribute (local.get $b) (i32.const 72) (i32.const 6))
    (if (i32.ne (call $hasAttribute (local.get $b) (i32.const 72) (i32.const 6)) (i32.const 0))
      (then (return (i32.const 18))))
    ;; ...and getAttribute now reports absence, distinctly from "did not fit".
    (if (i32.ne (call $getAttribute (local.get $b) (i32.const 72) (i32.const 6)
                                    (i32.const 200) (i32.const 64)) (i32.const -1))
      (then (return (i32.const 19))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 24) (i32.const 2) (i32.const 32) (i32.const 11))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
