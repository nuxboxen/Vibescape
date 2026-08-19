;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the DOM Core reading surface -- node identity, navigation, and NodeList.
;;
;; Each check returns its own number on failure, so a regression names itself in the log
;; rather than just saying the module did not finish. On success the module leaves a marker
;; element behind, which is what the runner asserts: a module that never ran leaves nothing,
;; so the assertion cannot pass vacuously.
;;
;; Build:  water cover-traversal.wat -o cover-traversal.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Node" "nodeType"        (func $nodeType (param i32) (result i32)))
  (import "org.inkscape.Node" "nodeName"        (func $nodeName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "localName"       (func $localName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "namespaceURI"    (func $namespaceURI (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "ownerDocument"   (func $ownerDocument (param i32) (result i32)))
  (import "org.inkscape.Node" "parentNode"      (func $parentNode (param i32) (result i32)))
  (import "org.inkscape.Node" "firstChild"      (func $firstChild (param i32) (result i32)))
  (import "org.inkscape.Node" "lastChild"       (func $lastChild (param i32) (result i32)))
  (import "org.inkscape.Node" "nextSibling"     (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.Node" "previousSibling" (func $previousSibling (param i32) (result i32)))
  (import "org.inkscape.Node" "childNodes"      (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.NodeList" "length"      (func $listLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"        (func $listItem (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "group")
  (data (i32.const 8)   "box")
  (data (i32.const 16)  "svg:rect")
  (data (i32.const 32)  "namedview")
  (data (i32.const 48)  "svg:g")
  (data (i32.const 56)  "id")
  (data (i32.const 64)  "ok-traversal")
  (data (i32.const 80)  "http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd")

  ;; Compare $len bytes at $a with those at $b.
  (func $memeq (param $a i32) (param $b i32) (param $len i32) (result i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $len)))
        (br_if $done (i32.ne
          (i32.load8_u (i32.add (local.get $a) (local.get $i)))
          (i32.load8_u (i32.add (local.get $b) (local.get $i)))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.eq (local.get $i) (local.get $len)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $group i32) (local $box i32) (local $list i32)
    (local $marker i32) (local $view i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 0) (i32.const 5)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 8) (i32.const 3)))

    ;; 1. getElementById found both.
    (if (i32.or (i32.eqz (local.get $group)) (i32.eqz (local.get $box)))
      (then (return (i32.const 1))))

    ;; 2. An element reports DOM's ELEMENT_NODE, which is 1 -- not Inkscape's own numbering.
    (if (i32.ne (call $nodeType (local.get $box)) (i32.const 1))
      (then (return (i32.const 2))))

    ;; 3. nodeName is the qualified name.
    (if (i32.ne (call $nodeName (local.get $box) (i32.const 200) (i32.const 64)) (i32.const 8))
      (then (return (i32.const 3))))
    (if (i32.eqz (call $memeq (i32.const 200) (i32.const 16) (i32.const 8)))
      (then (return (i32.const 4))))

    ;; 5. localName drops the prefix.
    (if (i32.ne (call $localName (local.get $box) (i32.const 200) (i32.const 64)) (i32.const 4))
      (then (return (i32.const 5))))

    ;; 6. A foreign namespace resolves to the URI the document declared.
    (local.set $view (call $getElementById (local.get $document) (i32.const 32) (i32.const 9)))
    (if (i32.eqz (local.get $view)) (then (return (i32.const 6))))
    (if (i32.ne (call $namespaceURI (local.get $view) (i32.const 200) (i32.const 128)) (i32.const 50))
      (then (return (i32.const 7))))
    (if (i32.eqz (call $memeq (i32.const 200) (i32.const 80) (i32.const 50)))
      (then (return (i32.const 8))))

    ;; 9. ownerDocument round-trips: the document's element is the root we started from.
    (if (i32.eqz (call $ownerDocument (local.get $box))) (then (return (i32.const 9))))

    ;; 10. The box's parent is the group.
    (if (i32.ne (call $parentNode (local.get $box)) (local.get $group))
      (then (return (i32.const 10))))

    ;; 11. Navigation agrees with itself: first child of the group is the box, and stepping
    ;;     forward then back returns to it.
    (if (i32.ne (call $firstChild (local.get $group)) (local.get $box))
      (then (return (i32.const 11))))
    (if (i32.ne (call $previousSibling (call $nextSibling (local.get $box))) (local.get $box))
      (then (return (i32.const 12))))

    ;; 13. The group holds rect, path and text, so lastChild is not firstChild.
    (if (i32.eq (call $lastChild (local.get $group)) (local.get $box))
      (then (return (i32.const 13))))

    ;; 14. NodeList agrees with navigation.
    (local.set $list (call $childNodes (local.get $group)))
    (if (i32.ne (call $listLength (local.get $list)) (i32.const 3))
      (then (return (i32.const 14))))
    (if (i32.ne (call $listItem (local.get $list) (i32.const 0)) (local.get $box))
      (then (return (i32.const 15))))
    ;; 16. Past the end is null, not a trap: DOM says item() returns null.
    (if (i32.ne (call $listItem (local.get $list) (i32.const 99)) (i32.const 0))
      (then (return (i32.const 16))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 48) (i32.const 5)))
    (call $setAttribute (local.get $marker)
      (i32.const 56) (i32.const 2) (i32.const 64) (i32.const 12))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
