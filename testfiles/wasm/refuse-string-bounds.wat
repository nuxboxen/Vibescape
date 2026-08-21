;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A string result's out span is bounds-checked like every other span.
;;
;; The offset here is inside the memory but the capacity runs off the end of it -- the case a
;; naive "is the offset in range" test waves through. refuse-write-bounds covers the same
;; shape for a numeric out-param; this is the string side, which travels a different path
;; because it also has a length to report.
;;
;; That is exactly why it needs its own case. A short buffer is NOT an error on this ABI: the
;; host answers the length the value needs and the plugin asks again. A buffer that does not
;; lie inside the memory is a different thing entirely, and folding it into the same answer
;; would tell a plugin to retry -- at the same bad offset, forever, being told the same
;; length each time. So the span is refused as a trap, and only the fit is negotiable.
;;
;; The span is also checked against the capacity the plugin offered rather than against the
;; length of whatever came back, so this refusal does not depend on how long the attribute
;; happens to be.
;;
;; Expected: a trap, and an unchanged document.
;;
;; Build:  water refuse-string-bounds.wat -o refuse-string-bounds.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"    (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))

  (memory (export "memory") 1)          ;; one page: 65536 bytes

  (data (i32.const 0)  "svg:g")
  (data (i32.const 8)  "id")
  (data (i32.const 16) "scratch-sb")

  (func (export "effect") (param $document i32) (result i32)
    (local $scratch i32)
    ;; Tagged, because the fixture may already contain a <g>: the rollback assertion has to
    ;; be about something only this module could have added.
    (local.set $scratch (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $scratch)
      (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 10))
    (drop (call $appendChild (call $documentElement (local.get $document)) (local.get $scratch)))

    ;; 65520 + 64 > 65536: the offset is valid, the span is not. The value asked for is ten
    ;; bytes and would have fitted in 64, so a host checking the written length instead of
    ;; the offered span would wave this through.
    (drop (call $getAttribute (local.get $scratch)
      (i32.const 8) (i32.const 2)
      (i32.const 65520) (i32.const 64)))
    (i32.const 0))
)
