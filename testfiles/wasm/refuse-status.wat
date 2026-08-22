;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A module that fails without trapping. Returning a non-zero status is how a plugin reports
;; a problem it handled itself -- bad parameters, nothing to do, a refusal of its own -- and
;; it must roll back exactly as a trap does, because by then the document has been modified.
;;
;; The alternative, committing whatever a failed run happened to leave behind, is how an
;; extension corrupts a drawing.
;;
;; Expected: no <g> in the saved document.
;;
;; Build:  water refuse-status.wat -o refuse-status.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))

  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    (local $g i32)
    (local.set $g (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))

    ;; Really append it: the point is that a *committed-looking* change is still discarded.
    (drop (call $appendChild
      (call $documentElement (local.get $document))
      (local.get $g)))

    (i32.const 1))   ;; non-zero: "I failed"
)
