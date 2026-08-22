;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; The staging memory is found by NAME, not by kind.
;;
;; This module exports a perfectly good linear memory -- under the name "mem". A host that
;; selects "the export that happens to be a memory" accepts it and runs; a host that looks up
;; "memory", as the ABI requires, refuses it.
;;
;; The distinction is not pedantry. WebAssembly 3.0 allows an instance more than one memory,
;; so kind stopped identifying anything: faced with two, a by-kind walk picks whichever it
;; reaches last, and a plugin's strings are marshalled through a buffer it never nominated.
;; Naming the memory is what makes the choice deterministic, and this case is what stops the
;; by-kind version from coming back.
;;
;; Expected: refused before the entry point is called, and an unchanged document.
;;
;; Build:  water refuse-wrong-memory-name.wat -o refuse-wrong-memory-name.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))

  ;; A memory, but not the one the ABI names.
  (memory (export "mem") 1)

  (data (i32.const 0) "svg:g")

  ;; Never reached: the host has nothing to marshal strings through, so it does not call.
  (func (export "effect") (param $document i32) (result i32)
    (drop (call $appendChild
      (call $documentElement (local.get $document))
      (call $createElement (local.get $document) (i32.const 0) (i32.const 5))))
    (i32.const 0))
)
