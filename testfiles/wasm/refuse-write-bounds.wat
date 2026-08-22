;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; The write side of the bounds check. refuse-bounds covers a read; this covers an out
;; parameter, which is the direction that corrupts the guest's own heap if it is not checked.
;;
;; getBBox writes four doubles at the offset it is given. The offset here is inside the
;; memory but too close to its end for 32 bytes to fit, which is the case a naive "is the
;; offset in range" test would wave through.
;;
;; Expected: a trap, and an unchanged document.

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.SVGGraphicsElement" "getBBox" (func $getBBox (param i32 i32 i32) (result i32)))
  (memory (export "memory") 1)          ;; one page: 65536 bytes
  (data (i32.const 0)  "svg:g")
  (data (i32.const 8)  "box")
  (data (i32.const 16) "id")
  (data (i32.const 24) "scratch-wb")

  (func (export "effect") (param $document i32) (result i32)
    (local $scratch i32)
    ;; Tagged, because the fixture already contains a <g>: the rollback assertion has to be
    ;; about something only this module could have added.
    (local.set $scratch (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $scratch)
      (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 10))
    (drop (call $appendChild (call $documentElement (local.get $document)) (local.get $scratch)))
    ;; 65520 + 32 > 65536: the offset is valid, the span is not.
    (drop (call $getBBox
      (call $getElementById (local.get $document) (i32.const 8) (i32.const 3))
      (i32.const 1) (i32.const 65520)))
    (i32.const 0))
)
