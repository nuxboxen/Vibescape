;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Handle 0 is null. Every operation that takes a handle must refuse it rather than
;; dereference it, because 0 is what a guest gets back from any query that found nothing --
;; so passing one on is the single easiest mistake a plugin can make.
;;
;; Expected: a trap naming the operation, and an unchanged document.

(module
  (import "org.inkscape.Document" "createElement" (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "nodeType"      (func $nodeType (param i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 0) "svg:g")

  (func (export "effect") (param $document i32) (result i32)
    ;; Modify first, so the assertion that nothing changed also proves the rollback.
    (drop (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (drop (call $nodeType (i32.const 0)))
    (i32.const 0))
)
