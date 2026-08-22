;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: enumerating a style declaration, and writing to one.
;;
;; The assertion that matters is the one separating SPECIFIED from COMPUTED. `inherited` in the
;; fixture declares no fill and takes one from the group above it, so:
;;
;;   getPropertyValue   answers a colour  -- the cascade resolved it
;;   specifiedValue  answers absent    -- this element declared nothing
;;
;; Both are right, and a plugin deciding whether to overwrite something the author set needs
;; the second question. Without it the only way to ask is to walk ancestors comparing values,
;; which is the guesswork an in-process API exists to remove.
;;
;; BlurEdge reads and writes individual style properties through sp_repr_css_*; before these
;; operations a plugin had to read the whole `style` attribute, parse CSS itself, and write it
;; back -- losing whatever it failed to parse.
;;
;; Build:  water cover-style-write.wat -o cover-style-write.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.CSSStyleDeclaration" "getPropertyValue"
    (func $getPropertyValue (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.CSSStyleDeclaration" "length" (func $styleLength (param i32) (result i32)))
  (import "org.inkscape.CSSStyleDeclaration" "item"
    (func $styleItem (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.CSSStyleDeclaration" "setProperty"
    (func $setProperty (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.CSSStyleDeclaration" "removeProperty"
    (func $removeProperty (param i32 i32 i32)))
  (import "org.inkscape.CSSStyleDeclaration" "specifiedValue"
    (func $specifiedValue (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.CSSStyleDeclaration" "setPropertyNumber"
          (func $setPropertyNumber (param i32 i32 i32 f64)))
  (import "org.inkscape.CSSStyleDeclaration" "setPropertyColor"
          (func $setPropertyColor (param i32 i32 i32 i32)))
  (import "org.inkscape.CSSStyleDeclaration" "changeRecursive"
    (func $changeRecursive (param i32 i32 i32 i32 i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "box")
  (data (i32.const 8)   "svg:g")
  (data (i32.const 16)  "id")
  (data (i32.const 24)  "ok-style-write")
  (data (i32.const 48)  "fill")
  (data (i32.const 56)  "#00ff00")
  (data (i32.const 72)  "inherited")
  (data (i32.const 88)  "opacity")
  (data (i32.const 96)  "0.5")
  (data (i32.const 104) "painted")
  (data (i32.const 200) "0.25")
  (data (i32.const 116) "named")

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
    (local $root i32) (local $box i32) (local $marker i32)
    (local $inherited i32) (local $painted i32) (local $named i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (local.set $inherited (call $getElementById (local.get $document) (i32.const 72) (i32.const 9)))
    (local.set $painted (call $getElementById (local.get $document) (i32.const 104) (i32.const 7)))
    (local.set $named (call $getElementById (local.get $document) (i32.const 116) (i32.const 5)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))
    (if (i32.eqz (local.get $inherited)) (then (return (i32.const 2))))
    (if (i32.eqz (local.get $painted)) (then (return (i32.const 3))))
    (if (i32.eqz (local.get $named)) (then (return (i32.const 4))))

    ;; ── enumeration ───────────────────────────────────────────────────────────
    ;; Every property has a computed value, so a declaration is never empty, and item(0)
    ;; names one of them.
    (if (i32.le_s (call $styleLength (local.get $box)) (i32.const 0))
      (then (return (i32.const 5))))
    (if (i32.le_s (call $styleItem (local.get $box) (i32.const 0) (i32.const 1024) (i32.const 256))
                  (i32.const 0))
      (then (return (i32.const 6))))
    ;; Past the end is absent, not a trap and not an empty string.
    (if (i32.ne (call $styleItem (local.get $box) (i32.const 100000) (i32.const 1024) (i32.const 256))
                (i32.const -1))
      (then (return (i32.const 7))))

    ;; ── specified is not computed ─────────────────────────────────────────────
    ;; The whole reason specifiedValue exists. `inherited` resolves a fill through the
    ;; cascade but declares none of its own.
    (if (i32.le_s (call $getPropertyValue (local.get $inherited) (i32.const 48) (i32.const 4)
                                          (i32.const 1024) (i32.const 256)) (i32.const 0))
      (then (return (i32.const 8))))
    (if (i32.ne (call $specifiedValue (local.get $inherited) (i32.const 48) (i32.const 4)
                                      (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 9))))

    ;; ── writing one property ──────────────────────────────────────────────────
    (local.set $marker (call $createElement (local.get $document) (i32.const 8) (i32.const 5)))
    (call $setProperty (local.get $marker) (i32.const 48) (i32.const 4) (i32.const 56) (i32.const 7))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 48) (i32.const 4)
                                      (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 10))))
    (if (i32.eqz (call $bytesAre (i32.const 56) (i32.const 7))) (then (return (i32.const 11))))

    ;; Setting a second property must not discard the first: sp_repr_css_change merges, where
    ;; a host writing the style attribute wholesale would drop everything else.
    (call $setProperty (local.get $marker) (i32.const 88) (i32.const 7) (i32.const 96) (i32.const 3))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 48) (i32.const 4)
                                      (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 12))))
    (if (i32.eqz (call $bytesAre (i32.const 56) (i32.const 7))) (then (return (i32.const 13))))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 88) (i32.const 7)
                                      (i32.const 1024) (i32.const 256)) (i32.const 3))
      (then (return (i32.const 14))))

    ;; ── removing one property ─────────────────────────────────────────────────
    (call $removeProperty (local.get $marker) (i32.const 48) (i32.const 4))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 48) (i32.const 4)
                                      (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 15))))
    ;; ...and leaves the other one alone.
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 88) (i32.const 7)
                                      (i32.const 1024) (i32.const 256)) (i32.const 3))
      (then (return (i32.const 16))))

    ;; ── down the subtree ──────────────────────────────────────────────────────
    ;; `named` declares its own fill, so setting fill on the group above it changes nothing
    ;; visible; recursion is what "make this whole selection red" actually needs.
    (call $changeRecursive (local.get $painted) (i32.const 88) (i32.const 7)
                           (i32.const 96) (i32.const 3))
    (if (i32.ne (call $specifiedValue (local.get $named) (i32.const 88) (i32.const 7)
                                      (i32.const 1024) (i32.const 256)) (i32.const 3))
      (then (return (i32.const 17))))
    (if (i32.eqz (call $bytesAre (i32.const 96) (i32.const 3))) (then (return (i32.const 18))))

    ;; ── typed property setters ────────────────────────────────────────────────
    ;; A number, written the way Inkscape writes numbers into CSS rather than the way a plugin
    ;; happens to format floats.
    (call $setPropertyNumber (local.get $marker) (i32.const 88) (i32.const 7) (f64.const 0.25))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 88) (i32.const 7)
                                      (i32.const 1024) (i32.const 256)) (i32.const 4))
      (then (return (i32.const 19))))
    (if (i32.eqz (call $bytesAre (i32.const 200) (i32.const 4))) (then (return (i32.const 20))))

    ;; A colour, from the RGBA word every colour-valued operation here answers with -- the form
    ;; paramColor hands a colour parameter over in. Fully opaque, so the answer is #rrggbb.
    (call $setPropertyColor (local.get $marker) (i32.const 48) (i32.const 4) (i32.const 0x00ff00ff))
    (if (i32.ne (call $specifiedValue (local.get $marker) (i32.const 48) (i32.const 4)
                                      (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 21))))
    (if (i32.eqz (call $bytesAre (i32.const 56) (i32.const 7))) (then (return (i32.const 22))))

    (call $setAttribute (local.get $marker) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 14))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
