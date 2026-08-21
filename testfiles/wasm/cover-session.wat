;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the parameter accessors, and the selection operations.
;;
;; The parameter half closes a gap that was invisible from the outside: `color`, `optiongroup`,
;; `path` and `notebook` have always been declarable in an .inx, and until now nothing could
;; read any of them back. An author could put a colour picker in their own dialog and have no
;; way to find out what the user chose. Two of them had no C++ getter either -- the typed
;; accessors each cast to one parameter class and throw otherwise -- which is why
;; Extension::get_param_any() had to exist before this test could.
;;
;; The selection half is asserted for its HEADLESS behaviour, which is what the command line
;; runs: no desktop means no selection, and the honest answer is that the query reports absence
;; and the mutations do nothing. They must still validate their arguments, though -- refusing a
;; bad handle is not something a host gets to skip just because it has nothing to do with it.
;;
;; Build:  water cover-session.wat -o cover-session.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "childNodes"      (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Params" "paramString"      (func $paramString (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramColor"       (func $paramColor (param i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramOptionGroup" (func $paramOptionGroup (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramPath"        (func $paramPath (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramNotebook"    (func $paramNotebook (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Params" "setParam"         (func $setParam (param i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.Selection" "isSelected"      (func $isSelected (param i32) (result i32)))
  (import "org.inkscape.Selection" "selectionAdd"    (func $selectionAdd (param i32)))
  (import "org.inkscape.Selection" "selectionClear"  (func $selectionClear))
  (import "org.inkscape.Selection" "selectionBounds" (func $selectionBounds (param i32 i32) (result i32)))
  (import "org.inkscape.Selection" "selectionRemove" (func $selectionRemove (param i32)))
  (import "org.inkscape.Selection" "selectionSet"    (func $selectionSet (param i32)))
  (import "org.inkscape.Selection" "toCurves"        (func $toCurves))

  (memory (export "memory") 1)

  (data (i32.const 0)   "box")
  (data (i32.const 8)   "svg:g")
  (data (i32.const 16)  "id")
  (data (i32.const 24)  "ok-session")
  (data (i32.const 40)  "group")
  (data (i32.const 48)  "teststring")
  (data (i32.const 64)  "testcolor")
  (data (i32.const 80)  "testpath")
  (data (i32.const 96)  "testchoice")
  (data (i32.const 112) "testbook")
  (data (i32.const 128) "alpha")
  (data (i32.const 136) "one")
  (data (i32.const 144) "/tmp/wasm-suite-path")
  (data (i32.const 168) "changed")

  ;; Compare $len bytes at 1024 against the literal at $at.
  (func $bytesAre (param $at i32) (param $len i32) (result i32)
    (local $i i32)
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $len)))
        (if (i32.ne (i32.load8_u (i32.add (i32.const 1024) (local.get $i)))
                    (i32.load8_u (i32.add (local.get $at) (local.get $i))))
          (then (return (i32.const 0))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 1))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $group i32) (local $marker i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 40) (i32.const 5)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))
    (if (i32.eqz (local.get $group)) (then (return (i32.const 2))))

    ;; ── parameters ────────────────────────────────────────────────────────────
    ;; The .inx declares #11223344, so the RGBA word comes back intact. A host dropping the
    ;; alpha channel, or handing back a premultiplied value, answers something else.
    (if (i32.ne (call $paramColor (i32.const 64) (i32.const 9)) (i32.const 0x11223344))
      (then (return (i32.const 3))))

    ;; optiongroup answers the selected option's VALUE, not its label: the .inx says
    ;; <option value="alpha">Alpha</option>, and "Alpha" would be the wrong one.
    (if (i32.ne (call $paramOptionGroup (i32.const 96) (i32.const 10) (i32.const 1024) (i32.const 256))
                (i32.const 5))
      (then (return (i32.const 4))))
    (if (i32.eqz (call $bytesAre (i32.const 128) (i32.const 5))) (then (return (i32.const 5))))

    ;; notebook answers the selected page's name.
    (if (i32.ne (call $paramNotebook (i32.const 112) (i32.const 8) (i32.const 1024) (i32.const 256))
                (i32.const 3))
      (then (return (i32.const 6))))
    (if (i32.eqz (call $bytesAre (i32.const 136) (i32.const 3))) (then (return (i32.const 7))))

    (if (i32.ne (call $paramPath (i32.const 80) (i32.const 8) (i32.const 1024) (i32.const 256))
                (i32.const 20))
      (then (return (i32.const 8))))
    (if (i32.eqz (call $bytesAre (i32.const 144) (i32.const 20))) (then (return (i32.const 9))))

    ;; A parameter that does not exist is absent, not a trap: a plugin asking for something
    ;; the .inx never declared has made a mistake in its own manifest, and starting with a
    ;; default is more use than refusing to start.
    (if (i32.ne (call $paramPath (i32.const 0) (i32.const 3) (i32.const 1024) (i32.const 256))
                (i32.const -1))
      (then (return (i32.const 10))))

    ;; ── writing a parameter back ──────────────────────────────────────────────
    ;; Parameters are the only storage a plugin gets that outlives the invocation.
    ;; Answers whether it wrote, so a plugin can tell a real parameter from a typo. Writing a
    ;; declared one succeeds.
    (if (i32.ne (call $setParam (i32.const 48) (i32.const 10) (i32.const 168) (i32.const 7))
                (i32.const 1))
      (then (return (i32.const 14))))
    (if (i32.ne (call $paramString (i32.const 48) (i32.const 10) (i32.const 1024) (i32.const 256))
                (i32.const 7))
      (then (return (i32.const 11))))
    (if (i32.eqz (call $bytesAre (i32.const 168) (i32.const 7))) (then (return (i32.const 12))))

    ;; ── selection, headless ───────────────────────────────────────────────────
    ;; A document has a selection whether or not anything is looking at it: SPDocument builds
    ;; one in its constructor (document.cpp SPDocument::SPDocument()), and every ObjectSet operation guards its
    ;; desktop use, using it only for user feedback. So a plugin treating the selection as a
    ;; working register -- which is what BlurEdge does -- works from the command line, where
    ;; the 132 shipped effect extensions run.
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 13))))
    ;; Nothing selected, so no aggregate bounds. Absent, not an empty rectangle: a plugin
    ;; cannot tell 0x0-at-the-origin from "there is nothing selected".
    (if (i32.ne (call $selectionBounds (i32.const 0) (i32.const 512)) (i32.const 0))
      (then (return (i32.const 15))))

    (call $selectionAdd (local.get $box))
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 1)) (then (return (i32.const 16))))
    ;; ...and now there is something to have bounds.
    (if (i32.ne (call $selectionBounds (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 17))))

    (call $selectionRemove (local.get $box))
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 18))))

    ;; Setting from a NodeList replaces the lot in one step.
    (call $selectionSet (call $childNodes (local.get $group)))
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 1)) (then (return (i32.const 19))))
    (call $selectionClear)
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 20))))

    ;; Object to path over the selection, which needs something selected to do anything.
    (call $selectionSet (call $childNodes (local.get $group)))
    (call $toCurves)
    (call $selectionClear)

    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 10))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
