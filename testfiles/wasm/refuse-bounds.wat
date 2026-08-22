;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Every span a module hands the host is checked against the memory's current size before it
;; is touched. The check is written against the space remaining rather than as offset+length,
;; because that sum overflows -- and an overflowing bounds check is the same as none.
;;
;; This module asks the host to read a name from an offset a long way past the end of a
;; one-page memory. Expected: a trap, and an unchanged document.
;;
;; Build:  water refuse-bounds.wat -o refuse-bounds.wasm

(module
  (import "org.inkscape.Document" "createElement" (func $createElement (param i32 i32 i32) (result i32)))

  (memory (export "memory") 1)   ;; one page: 65536 bytes

  (func (export "effect") (param $document i32) (result i32)
    ;; Well past the end, and with a length chosen so that offset+length wraps a signed
    ;; 32-bit sum back into range: a host comparing the sum would accept this.
    (drop (call $createElement (local.get $document) (i32.const 0x7ffffff0) (i32.const 0x20)))
    (i32.const 0))
)
