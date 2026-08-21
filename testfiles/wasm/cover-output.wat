;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: a wasm module as an OUTPUT backend -- <wasm> under <output>, not <effect>.
;;
;; 17 of the 160 shipped .inx files are <output>, and none of them could be a wasm module until
;; WasmBackend implemented save(). The dispatch was already there: system.cpp reads the
;; implementation type and the functional type independently, so <output> plus <wasm> has always
;; built the right pair of objects -- the implementation simply threw save_failed.
;;
;; The guest does not touch the filesystem. It is handed the document, walks it with the same
;; API an effect uses, and emits bytes through writeOutput; the host owns the file. That is
;; the sandbox boundary in the same place it sits everywhere else in this interface, and it
;; means an output backend is an ordinary plugin that happens to produce bytes.
;;
;; The marker is written only when the document walk gives the answer the fixture should give,
;; so a host that called the module with no usable document -- or never called it -- cannot
;; produce the expected file.
;;
;; Build:  water cover-output.wat -o cover-output.wasm

(module
  (import "org.inkscape.Document" "getElementsByTagName" (func $byTag (param i32 i32 i32) (result i32)))
  (import "org.inkscape.NodeList" "length"               (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.FileBackend" "writeOutput"       (func $writeOutput (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "rect")
  (data (i32.const 8)  "wasm-output-ok\0a")
  (data (i32.const 32) "wasm-output-wrong-document\0a")

  (func (export "save") (param $document i32) (result i32)
    ;; Eleven, not twelve: the fixture declares twelve <rect> elements and Inkscape rewrites the
    ;; LPE-bearing one as <path sodipodi:type="rect">, which cover-path asserts from the other
    ;; side. Depending on that here is deliberate -- it ties the bytes written to a real walk of
    ;; a real document rather than to a constant.
    (if (i32.ne (call $nodesLength (call $byTag (local.get $document) (i32.const 0) (i32.const 4)))
                (i32.const 11))
      (then
        (drop (call $writeOutput (i32.const 32) (i32.const 27)))
        (return (i32.const 1))))

    (if (i32.eqz (call $writeOutput (i32.const 8) (i32.const 15)))
      (then (return (i32.const 2))))
    (i32.const 0))
)
