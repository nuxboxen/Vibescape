;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; An import whose name matches a host function but whose type does not.
;;
;; The host must leave it unresolved rather than bind it: binding would hand the module a
;; function it did not describe, and the mismatch would surface later as arguments read from
;; the wrong stack slots. Failing at instantiation makes it a load error with a name in it.
;;
;; Expected: instantiation fails, nothing runs, document unchanged.
;;
;; Build:  water refuse-bad-signature.wat -o refuse-bad-signature.wasm

(module
  ;; appendChild really takes (i32 i32) -> i32. This asks for a one-argument version.
  (import "org.inkscape.Node" "appendChild" (func $appendChild (param i32) (result i32)))

  (memory (export "memory") 1)

  (func (export "effect") (param $document i32) (result i32)
    (drop (call $appendChild (local.get $document)))
    (i32.const 0))
)
