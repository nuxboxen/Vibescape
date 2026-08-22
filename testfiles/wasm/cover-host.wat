;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: what a plugin asks of the host rather than of the document -- status messages,
;; its own preferences, the name of its undo entry, and Inkscape's actions.
;;
;; Two of these have limits this suite cannot reach past, and they are stated here rather than
;; discovered later:
;;
;;   * The MESSAGE operations have no working path without a desktop. A message is something
;;     shown to a person, and headless there is nobody to show it to, so they report "not
;;     shown" and log instead. What is asserted below is that contract; the desktop path is not
;;     exercised by this suite, which runs from the command line by construction.
;;
;;   * The UNDO label and coalesce key are asserted to round-trip through the host. That the
;;     entry ExecutionEnv::commit() writes actually carries them is C++ on the far side of this
;;     invocation, and no wasm case can see the undo stack -- it does not appear in the saved
;;     document.
;;
;; Preferences and actions have no such gap and are tested for what they do.
;;
;; Build:  water cover-host.wat -o cover-host.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Selection" "selectionClear" (func $selClear))
  (import "org.inkscape.Selection" "isSelected"     (func $isSelected (param i32) (result i32)))

  (import "org.inkscape.Host" "message"       (func $message (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Host" "flashMessage"  (func $flashMessage (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Host" "cancelMessage" (func $cancelMessage (param i32)))

  (import "org.inkscape.Host" "getPref" (func $getPref (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Host" "setPref" (func $setPref (param i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.Host" "undoLabel"          (func $undoLabel (param i32 i32) (result i32)))
  (import "org.inkscape.Host" "setUndoLabel"       (func $setUndoLabel (param i32 i32)))
  (import "org.inkscape.Host" "undoCoalesceKey"    (func $coalesceKey (param i32 i32) (result i32)))
  (import "org.inkscape.Host" "setUndoCoalesceKey" (func $setCoalesceKey (param i32 i32)))

  (import "org.inkscape.Host" "invokeAction" (func $invokeAction (param i32 i32) (result i32)))
  (import "org.inkscape.Host" "listActions"  (func $listActions (result i32)))

  (import "org.inkscape.DOMStringList" "length" (func $listLength (param i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-host")
  (data (i32.const 24)  "box")
  (data (i32.const 32)  "select-by-id:box")
  (data (i32.const 56)  "no-such-action-xyz")
  (data (i32.const 80)  "mykey")
  (data (i32.const 88)  "myvalue")
  (data (i32.const 96)  "Rounded corners")
  (data (i32.const 120) "wasm-preview")
  (data (i32.const 136) "../../options")
  (data (i32.const 152) "hello")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $box i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 24) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── messages ──────────────────────────────────────────────────────────────
    ;; 2 is WARNING_MESSAGE, Inkscape's own MessageType numbering (message.h MessageType).
    ;; Headless there is no message stack, so no id is issued and the text goes to the log --
    ;; which is the answer, not a failure. Zero is "not shown to anyone".
    (if (i32.ne (call $message (i32.const 2) (i32.const 152) (i32.const 5)) (i32.const 0))
      (then (return (i32.const 2))))
    (if (i32.ne (call $flashMessage (i32.const 2) (i32.const 152) (i32.const 5)) (i32.const 0))
      (then (return (i32.const 3))))
    ;; Cancelling an id that was never issued must be harmless, since headless that is the only
    ;; kind of id there is.
    (call $cancelMessage (i32.const 0))

    ;; ── preferences ───────────────────────────────────────────────────────────
    ;; Absent before anything is written, and absent is distinct from empty.
    (if (i32.ne (call $getPref (i32.const 80) (i32.const 5) (i32.const 512) (i32.const 64))
                (i32.const -1))
      (then (return (i32.const 4))))
    (if (i32.eqz (call $setPref (i32.const 80) (i32.const 5) (i32.const 88) (i32.const 7)))
      (then (return (i32.const 5))))
    (if (i32.ne (call $getPref (i32.const 80) (i32.const 5) (i32.const 512) (i32.const 64))
                (i32.const 7))
      (then (return (i32.const 6))))
    (if (i32.ne (i32.load8_u (i32.const 512)) (i32.const 109)) (then (return (i32.const 7))))

    ;; The scoping is the sandbox boundary, so a key that tries to climb out of it is refused
    ;; rather than sanitised into something that might still resolve. Refusing is reported, not
    ;; trapped: a plugin asking for a key it may not have is wrong, but not so wrong that the
    ;; host should take the invocation down over it.
    (if (i32.ne (call $setPref (i32.const 136) (i32.const 13) (i32.const 88) (i32.const 7))
                (i32.const 0))
      (then (return (i32.const 8))))
    (if (i32.ne (call $getPref (i32.const 136) (i32.const 13) (i32.const 512) (i32.const 64))
                (i32.const -1))
      (then (return (i32.const 9))))

    ;; ── naming the undo entry ─────────────────────────────────────────────────
    ;; Unset to begin with: the entry is named after the extension unless the plugin says
    ;; otherwise, which is what every extension that has ever existed relies on.
    (if (i32.ne (call $undoLabel (i32.const 512) (i32.const 64)) (i32.const -1))
      (then (return (i32.const 10))))
    (call $setUndoLabel (i32.const 96) (i32.const 15))
    (if (i32.ne (call $undoLabel (i32.const 512) (i32.const 64)) (i32.const 15))
      (then (return (i32.const 11))))

    (if (i32.ne (call $coalesceKey (i32.const 512) (i32.const 64)) (i32.const -1))
      (then (return (i32.const 12))))
    (call $setCoalesceKey (i32.const 120) (i32.const 12))
    (if (i32.ne (call $coalesceKey (i32.const 512) (i32.const 64)) (i32.const 12))
      (then (return (i32.const 13))))

    ;; ── actions ───────────────────────────────────────────────────────────────
    ;; The spelling is Inkscape's own --actions syntax, name:value, rather than a second one
    ;; invented here: a plugin author already knows it, and the host reuses the same parser, so
    ;; the parameter types cannot drift apart from the command line's.
    (call $selClear)
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 0)) (then (return (i32.const 14))))
    (if (i32.eqz (call $invokeAction (i32.const 32) (i32.const 16))) (then (return (i32.const 15))))
    ;; Selected by an action that had to receive "box" to do it -- which is the parameter half
    ;; of the syntax proven, not just the name half.
    (if (i32.ne (call $isSelected (local.get $box)) (i32.const 1)) (then (return (i32.const 16))))

    ;; An action nobody defines answers false rather than pretending.
    (if (i32.ne (call $invokeAction (i32.const 56) (i32.const 18)) (i32.const 0))
      (then (return (i32.const 17))))

    ;; And the list is how a plugin tests for a capability instead of assuming one: which
    ;; actions exist is not part of this interface's version, and this is what makes that
    ;; survivable.
    (if (i32.le_s (call $listLength (call $listActions)) (i32.const 0))
      (then (return (i32.const 18))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 7))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
