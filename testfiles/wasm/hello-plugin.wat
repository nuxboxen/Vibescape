;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A minimal WebAssembly extension, hand-written to exercise the host without
;; involving any guest-language toolchain.
;;
;; It covers the four things the host must get right before any real plugin can run:
;;   - an import resolved by the (module, name) pair, not by name alone
;;   - a memory exported as "memory", which the host looks up by that name
;;   - a string handed to the host as (offset, length) into that memory
;;   - an entry point taking the document handle and returning a status
;;
;; Build:  water hello-plugin.wat -o hello-plugin.wasm

(module
  ;; The document is a handle, so createElement takes one; the element name crosses as
  ;; (offset, length) UTF-8 in linear memory, and the result is another handle.
  (import "org.inkscape.Document" "createElement"
    (func $createElement (param i32 i32 i32) (result i32)))

  ;; setAttribute takes the element handle plus two (offset, length) string spans.
  (import "org.inkscape.Element" "setAttribute"
    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Node" "appendChild"
    (func $appendChild (param i32 i32) (result i32)))

  ;; A Document handle is not a Node handle: the host checks the kind, so the root element
  ;; has to be asked for rather than assumed to be the document itself.
  (import "org.inkscape.Document" "documentElement"
    (func $documentElement (param i32) (result i32)))

  ;; The staging buffer. The name is not decoration: the host looks it up by it, because
  ;; WebAssembly 3.0 lets an instance export more than one memory and "the one that is a
  ;; memory" stops identifying anything as soon as a plugin has two.
  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:rect")     ;; [0,8)
  (data (i32.const 8)  "width")        ;; [8,13)
  (data (i32.const 13) "100")          ;; [13,16)

  ;; effect(document) -> status. 0 is success; anything else tells the host to roll back.
  (func (export "effect") (param $document i32) (result i32)
    (local $rect i32)

    (local.set $rect
      (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))

    ;; A null handle back means the host refused; report failure rather than pressing on.
    (if (i32.eqz (local.get $rect))
      (then (return (i32.const 1))))

    (call $setAttribute
      (local.get $rect)
      (i32.const 8) (i32.const 5)      ;; "width"
      (i32.const 13) (i32.const 3))    ;; "100"

    (drop (call $appendChild
      (call $documentElement (local.get $document))
      (local.get $rect)))
    (i32.const 0))
)
