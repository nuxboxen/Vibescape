;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; An operation that is registered and refuses, because the Inkscape call underneath it crashes.
;;
;; unSymbol is the only one of these so far. SPSymbol::unSymbol() segfaults with no desktop:
;; it takes the SP_ACTIVE_DESKTOP branch at sp-symbol.cpp SPSymbol::unSymbol() -- the one carrying Inkscape's
;; own "TODO: Better handle if no desktop" -- appends the replacement group into <defs> beside
;; the symbol, gives that group the symbol's id while the symbol still holds it, then
;; deleteObject()s the symbol and dies inside SPObject::deleteObject.
;;
;; The name is kept rather than dropped because a missing import and a blocked one are different
;; facts, and only one of them is useful. "unresolved import" reads as "not written yet" and
;; invites another go at wiring up the call that crashes; a trap says what is broken, where, and
;; that the fix is upstream -- at the moment someone tries, which is when they can act on it.
;;
;; This case exists so the claim cannot rot. If sp-symbol.cpp is fixed and the host starts
;; working, this case fails, which is the prompt to reclaim the capability rather than leave it
;; declared broken forever.
;;
;; Expected: a trap naming the operation and the upstream defect, and an unchanged document.

(module
  (import "org.inkscape.Document" "createElement" (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Selection" "unSymbol"   (func $unSymbol))
  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    ;; Modify first, so the assertion that nothing changed also proves the rollback.
    (drop (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $unSymbol)
    (i32.const 0))
)
