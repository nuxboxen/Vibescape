;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; The three-way string result. getAttribute answers THE LENGTH ITS ANSWER NEEDS, and writes
;; only when the whole answer fits, so a guest reads the result against the capacity it
;; passed:
;;
;;   -1              no such attribute. Nothing else ever answers -1.
;;   n <= outcap     the value, n bytes, written at the offset given
;;   n >  outcap     nothing written; the value needs n bytes -- grow and ask again
;;
;; Collapsing the last two -- as an embedder that returns -1 for both must -- leaves a plugin
;; unable to distinguish "absent" from "did not fit", and silently truncating either way.
;;
;; The result is one number with one meaning rather than a sign-encoded pair, which is what
;; lets a guest reuse the retry helper it already has for javelina's own variable-length
;; natives (docs/host-abi.md) instead of carrying a second convention for this ABI.
;;
;; The module records what it observed as attributes on a <result> element, so the assertions
;; are made against the saved document rather than against anything the module says about
;; itself.
;;
;; Build:  water string-protocol.wat -o string-protocol.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"    (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")        ;; [0,5)
  (data (i32.const 8)   "data-probe")   ;; [8,18)
  (data (i32.const 24)  "abcdefghij")   ;; [24,34)  ten bytes, the value we store
  (data (i32.const 40)  "data-missing") ;; [40,52)
  (data (i32.const 56)  "svg:rect")     ;; [56,64)
  (data (i32.const 64)  "fits")         ;; [64,68)
  (data (i32.const 72)  "absent")       ;; [72,78)
  (data (i32.const 80)  "needs")        ;; [80,85)
  (data (i32.const 88)  "roundtrip")    ;; [88,97)
  (data (i32.const 100) "0123456789")   ;; [100,110) scratch, so a short read is detectable
  (data (i32.const 120) "ZZZZZZZZZZ")   ;; [120,130) sentinel for the short-buffer case
  (data (i32.const 132) "untouched")    ;; [132,141)
  (data (i32.const 144) "retry")        ;; [144,149)

  ;; Render a small non-negative integer as decimal into [$at, ...), returning its length.
  ;; Values here are tiny, so two digits is enough and keeps the module readable.
  (func $render (param $value i32) (param $at i32) (result i32)
    (if (i32.lt_s (local.get $value) (i32.const 10))
      (then
        (i32.store8 (local.get $at) (i32.add (i32.const 48) (local.get $value)))
        (return (i32.const 1))))
    (i32.store8 (local.get $at)
      (i32.add (i32.const 48) (i32.div_s (local.get $value) (i32.const 10))))
    (i32.store8 (i32.add (local.get $at) (i32.const 1))
      (i32.add (i32.const 48) (i32.rem_s (local.get $value) (i32.const 10))))
    (i32.const 2))

  ;; setAttribute($node, key@$k len $klen, value = decimal of $value at scratch 200)
  (func $setNumber (param $node i32) (param $k i32) (param $klen i32) (param $value i32)
    (local $len i32)
    (local.set $len (call $render (local.get $value) (i32.const 200)))
    (call $setAttribute (local.get $node)
      (local.get $k) (local.get $klen)
      (i32.const 200) (local.get $len)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $probe i32) (local $result i32) (local $r i32)

    (local.set $root (call $documentElement (local.get $document)))

    ;; A group carrying a ten-byte attribute value to interrogate.
    (local.set $probe (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $probe)
      (i32.const 8) (i32.const 10)      ;; "data-probe"
      (i32.const 24) (i32.const 10))    ;; "abcdefghij"
    (drop (call $appendChild (local.get $root) (local.get $probe)))

    (local.set $result (call $createElement (local.get $document) (i32.const 56) (i32.const 8)))

    ;; 1. Ample buffer: expect 10, and the bytes actually written.
    (local.set $r (call $getAttribute (local.get $probe)
      (i32.const 8) (i32.const 10)
      (i32.const 100) (i32.const 32)))
    (call $setNumber (local.get $result) (i32.const 64) (i32.const 4) (local.get $r))
    ;; Echo what landed in the buffer, so a wrong-length or partial write is visible.
    (call $setAttribute (local.get $result)
      (i32.const 88) (i32.const 9)      ;; "roundtrip"
      (i32.const 100) (local.get $r))

    ;; 2. Absent attribute: expect -1 exactly, and never a length. This is the case the whole
    ;;    convention is built around -- -1 is absence and nothing else, so an attribute that
    ;;    is missing can never be confused with one that did not fit.
    (local.set $r (call $getAttribute (local.get $probe)
      (i32.const 40) (i32.const 12)     ;; "data-missing"
      (i32.const 100) (i32.const 32)))
    (call $setNumber (local.get $result) (i32.const 72) (i32.const 6)
      (i32.sub (i32.const 0) (local.get $r)))          ;; store 1 for -1

    ;; 3. Buffer of 4 for a 10-byte value. The answer is its own length -- 10, the size to
    ;;    come back with, not a code to decode -- and NOTHING is written.
    ;;
    ;;    Both halves are checked, because the length alone cannot tell them apart: case 1
    ;;    also answers 10, and a host that truncated four bytes into the buffer would answer
    ;;    10 here too. Only the buffer distinguishes "did not write" from "wrote part", so
    ;;    the call is aimed at a sentinel that must come back whole.
    (local.set $r (call $getAttribute (local.get $probe)
      (i32.const 8) (i32.const 10)
      (i32.const 120) (i32.const 4)))
    (call $setNumber (local.get $result) (i32.const 80) (i32.const 5) (local.get $r))
    (call $setAttribute (local.get $result)
      (i32.const 132) (i32.const 9)     ;; "untouched"
      (i32.const 120) (i32.const 10))

    ;; 4. Ask again with the length case 3 reported, which is what a guest's retry helper
    ;;    does with it. The value fits now and lands over the sentinel, so the reported
    ;;    length is shown to be the one that actually works rather than merely plausible.
    (local.set $r (call $getAttribute (local.get $probe)
      (i32.const 8) (i32.const 10)
      (i32.const 120) (local.get $r)))
    (call $setAttribute (local.get $result)
      (i32.const 144) (i32.const 5)     ;; "retry"
      (i32.const 120) (local.get $r))

    (drop (call $appendChild (local.get $root) (local.get $result)))
    (i32.const 0))
)
