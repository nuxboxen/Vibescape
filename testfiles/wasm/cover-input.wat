;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: a wasm module as an INPUT backend -- <wasm> under <input>.
;;
;; 8 of the 160 shipped .inx files are <input>. The module is handed the bytes of the file being
;; opened and a fresh, empty document, and builds into it with the same DOM API an effect uses.
;; There is no SVG text round trip: an importer that had to serialise its result for the host to
;; re-parse would be doing the work twice and losing everything the object tree knows.
;;
;; The marker is written only if the bytes arrived intact, byte for byte, so a host that called
;; the module with no input -- or with somebody else's -- cannot produce the expected document.
;;
;; Build:  water cover-input.wat -o cover-input.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.FileBackend" "inputBytes"   (func $inputBytes (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:rect")
  (data (i32.const 16) "id")
  (data (i32.const 24) "ok-input")
  (data (i32.const 40) "wasm-input-source")
  (data (i32.const 64) "x")
  (data (i32.const 72) "y")
  (data (i32.const 80) "width")
  (data (i32.const 88) "height")
  (data (i32.const 96)  "10")
  (data (i32.const 104) "100")
  (data (i32.const 112) "viewBox")
  (data (i32.const 120) "0 0 100 100")

  ;; memcmp over [a, a+n) and [b, b+n)
  (func $same (param $a i32) (param $b i32) (param $n i32) (result i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
        (if (i32.ne (i32.load8_u (i32.add (local.get $a) (local.get $i)))
                    (i32.load8_u (i32.add (local.get $b) (local.get $i))))
          (then (return (i32.const 0))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 1))

  (func (export "open") (param $document i32) (result i32)
    (local $root i32) (local $rect i32)

    ;; The file's bytes, through the same length-first protocol every string result uses.
    (if (i32.ne (call $inputBytes (i32.const 512) (i32.const 256)) (i32.const 17))
      (then (return (i32.const 1))))
    (if (i32.eqz (call $same (i32.const 512) (i32.const 40) (i32.const 17)))
      (then (return (i32.const 2))))

    ;; A fresh document, not the one being edited: an <input> module is opening a file, so what
    ;; it is given is an empty <svg> and its job is to fill it -- including the size, which a
    ;; blank document does not have and only the importer knows.
    (local.set $root (call $documentElement (local.get $document)))
    (if (i32.eqz (local.get $root)) (then (return (i32.const 3))))
    (call $setAttribute (local.get $root) (i32.const 80) (i32.const 5) (i32.const 104) (i32.const 3))
    (call $setAttribute (local.get $root) (i32.const 88) (i32.const 6) (i32.const 104) (i32.const 3))
    (call $setAttribute (local.get $root) (i32.const 112) (i32.const 7) (i32.const 120) (i32.const 11))

    (local.set $rect (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))
    (call $setAttribute (local.get $rect) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 8))
    (call $setAttribute (local.get $rect) (i32.const 64) (i32.const 1) (i32.const 96) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 72) (i32.const 1) (i32.const 96) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 80) (i32.const 5) (i32.const 96) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 88) (i32.const 6) (i32.const 96) (i32.const 2))
    (drop (call $appendChild (local.get $root) (local.get $rect)))
    (i32.const 0))
)
