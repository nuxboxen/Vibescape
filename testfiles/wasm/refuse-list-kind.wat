;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; NodeList and DOMStringList are different types with identically named operations. A handle
;; to one must not be accepted by the other: both are integers, and both have a `length`, so
;; nothing but the kind check distinguishes them.
;;
;; Expected: a trap, and an unchanged document.

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Element"  "getAttributeNames" (func $getAttributeNames (param i32) (result i32)))
  (import "org.inkscape.NodeList" "length"          (func $nodesLength (param i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    (drop (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    ;; A DOMStringList handle handed to NodeList.length.
    (drop (call $nodesLength
      (call $getAttributeNames (call $documentElement (local.get $document)))))
    (i32.const 0))
)
