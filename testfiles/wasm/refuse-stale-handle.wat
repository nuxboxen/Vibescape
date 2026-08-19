;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A handle is an index into a table that lives for exactly one invocation. An index that was
;; never issued must be refused -- otherwise a guest could reach objects by guessing integers,
;; and a handle kept from a previous run would silently address whatever now sits in that slot.
;;
;; Expected: a trap, and an unchanged document.

(module
  (import "org.inkscape.Document" "createElement" (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "nodeType"      (func $nodeType (param i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    (drop (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    ;; Far beyond anything this invocation could have handed out.
    (drop (call $nodeType (i32.const 100000)))
    (i32.const 0))
)
