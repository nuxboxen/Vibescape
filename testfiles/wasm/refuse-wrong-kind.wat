;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A handle is an integer, so nothing in the value itself says what it refers to. The host
;; records a kind alongside each one and checks it, which is what stops a module presenting
;; the Document handle it was given where a Node is expected.
;;
;; This module does exactly that. Expected: a trap, and a document with no <g> in it --
;; the element created beforehand must not survive, because a trap rolls the invocation back.
;;
;; Build:  water refuse-wrong-kind.wat -o refuse-wrong-kind.wasm

(module
  (import "org.inkscape.Document" "createElement" (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"   (func $appendChild (param i32 i32) (result i32)))

  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    (local $g i32)
    (local.set $g (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))

    ;; $document is a Document handle, not a Node handle. The host must refuse rather than
    ;; treat the two as interchangeable.
    (drop (call $appendChild (local.get $document) (local.get $g)))
    (i32.const 0))
)
