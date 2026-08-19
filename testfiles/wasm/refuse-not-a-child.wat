;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; insertBefore's reference node must be a child of the parent, and removeChild's target must
;; be too. DOM raises NotFoundError for both. Inkscape's addChild would otherwise be handed a
;; sibling pointer from an unrelated part of the tree, which is silent corruption rather than
;; an error.
;;
;; Expected: a trap, and an unchanged document.

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "insertBefore"    (func $insertBefore (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (memory (export "memory") 1)
  (data (i32.const 0)  "svg:g")
  (data (i32.const 8)  "box")
  (data (i32.const 16) "id")
  (data (i32.const 24) "scratch-nac")

  (func (export "effect") (param $document i32) (result i32)
    (local $host i32)
    ;; Tagged, because the fixture already contains a <g>: the rollback assertion has to be
    ;; about something only this module could have added.
    (local.set $host (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $host)
      (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 11))
    (drop (call $appendChild (call $documentElement (local.get $document)) (local.get $host)))

    ;; "box" lives in the fixture's group, not in $host.
    (drop (call $insertBefore (local.get $host)
      (call $createElement (local.get $document) (i32.const 0) (i32.const 5))
      (call $getElementById (local.get $document) (i32.const 8) (i32.const 3))))
    (i32.const 0))
)
