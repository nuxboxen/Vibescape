;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: layers.
;;
;; Corpus rank 13 -- 12 of 129 shipped scripts want the current layer, and today they get it by
;; reading namedview/@inkscape:current-layer themselves.
;;
;; Layers are a group with inkscape:groupmode="layer" and nothing more, so all of this is
;; document state and works headless. The one part that looks like view state, the CURRENT
;; layer, is document state too: Inkscape persists it to the namedview and restores the
;; desktop from it on open. So a headless plugin asking which layer is current gets the same
;; answer the editor would, and setting it is a real setting rather than a stand-in.
;;
;; The fixture declares no layers at all, which is worth testing against before any exist:
;; "the current layer" still has an answer, and that answer is the root. The module then
;; builds two layers and walks the three ways the question resolves -- no layers, layers but
;; no recorded choice, and a recorded choice.
;;
;; Build:  water cover-layers.wat -o cover-layers.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.SVGSVGElement"      "namedView" (func $namedView (param i32) (result i32)))
  (import "org.inkscape.SVGElement"         "label"     (func $label (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "isHidden"  (func $isHidden (param i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "isLocked"  (func $isLocked (param i32) (result i32)))

  (import "org.inkscape.Layers" "layers"          (func $layers (param i32) (result i32)))
  (import "org.inkscape.Layers" "isLayer"         (func $isLayer (param i32) (result i32)))
  (import "org.inkscape.Layers" "createLayer"     (func $createLayer (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Layers" "layerForObject"  (func $layerFor (param i32) (result i32)))
  (import "org.inkscape.Layers" "renameLayer"     (func $renameLayer (param i32 i32 i32 i32)))
  (import "org.inkscape.Layers" "currentLayer"    (func $currentLayer (result i32)))
  (import "org.inkscape.Layers" "setCurrentLayer" (func $setCurrentLayer (param i32) (result i32)))
  (import "org.inkscape.Layers" "layerSolo"       (func $layerSolo (param i32 i32)))
  (import "org.inkscape.Layers" "hideAllLayers"   (func $hideAll (param i32 i32)))
  (import "org.inkscape.Layers" "lockAllLayers"   (func $lockAll (param i32 i32)))
  (import "org.inkscape.Layers" "lockOtherLayers" (func $lockOthers (param i32 i32)))

  (import "org.inkscape.Node"     "nextSibling" (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:g")
  (data (i32.const 8)  "id")
  (data (i32.const 16) "ok-layers")
  (data (i32.const 32) "group")
  (data (i32.const 40) "box")
  (data (i32.const 48) "Alpha")
  (data (i32.const 56) "Beta")
  (data (i32.const 64) "alpha")
  (data (i32.const 72) "inkscape:current-layer")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $group i32) (local $box i32)
    (local $a i32) (local $b i32) (local $c i32) (local $view i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $view (call $namedView (local.get $root)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 32) (i32.const 5)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 40) (i32.const 3)))
    (if (i32.eqz (local.get $group)) (then (return (i32.const 1))))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 2))))
    (if (i32.eqz (local.get $view)) (then (return (i32.const 3))))

    ;; ── a document with no layers ─────────────────────────────────────────────
    (if (i32.ne (call $nodesLength (call $layers (local.get $document))) (i32.const 0))
      (then (return (i32.const 4))))
    ;; A plain <g> is not a layer. Only inkscape:groupmode="layer" makes one, which is why
    ;; asking the element is not the same as asking whether it is a group.
    (if (i32.ne (call $isLayer (local.get $group)) (i32.const 0))
      (then (return (i32.const 5))))
    ;; The current layer is still a question with an answer: the root acts as the layer when
    ;; there is none, which is what the editor does too.
    (if (i32.ne (call $currentLayer) (local.get $root))
      (then (return (i32.const 6))))

    ;; ── build two ─────────────────────────────────────────────────────────────
    ;; 1 is LPOS_CHILD -- Inkscape's own numbering, not a second one invented here.
    (local.set $a (call $createLayer (local.get $root) (i32.const 48) (i32.const 5) (i32.const 1)))
    (if (i32.eqz (local.get $a)) (then (return (i32.const 7))))
    (if (i32.ne (call $isLayer (local.get $a)) (i32.const 1))
      (then (return (i32.const 8))))
    (if (i32.ne (call $nodesLength (call $layers (local.get $document))) (i32.const 1))
      (then (return (i32.const 9))))
    (if (i32.ne (call $label (local.get $a) (i32.const 512) (i32.const 64)) (i32.const 5))
      (then (return (i32.const 10))))

    ;; With a layer present but no choice recorded, the current layer is the topmost one.
    (if (i32.ne (call $currentLayer) (local.get $a))
      (then (return (i32.const 11))))

    ;; 0 is LPOS_ABOVE, so this one goes on top of the first.
    (local.set $b (call $createLayer (local.get $a) (i32.const 56) (i32.const 4) (i32.const 0)))
    (if (i32.eqz (local.get $b)) (then (return (i32.const 12))))
    (if (i32.ne (call $nodesLength (call $layers (local.get $document))) (i32.const 2))
      (then (return (i32.const 13))))
    ;; Topmost means last in document order, so adding above moves the answer.
    (if (i32.ne (call $currentLayer) (local.get $b))
      (then (return (i32.const 14))))

    ;; ── which layer something is on ───────────────────────────────────────────
    ;; box sits in a plain group, so its layer is the root -- not nothing, and not the group.
    (if (i32.ne (call $layerFor (local.get $box)) (local.get $root))
      (then (return (i32.const 15))))
    (drop (call $appendChild (local.get $a) (local.get $box)))
    (if (i32.ne (call $layerFor (local.get $box)) (local.get $a))
      (then (return (i32.const 16))))

    ;; ── the recorded choice ───────────────────────────────────────────────────
    ;; Written on the namedview, which is where Inkscape keeps it, so a plugin and the editor
    ;; are reading the same thing.
    (call $setAttribute (local.get $a) (i32.const 8) (i32.const 2) (i32.const 64) (i32.const 5))
    (call $setAttribute (local.get $view) (i32.const 72) (i32.const 22) (i32.const 64) (i32.const 5))
    (if (i32.ne (call $currentLayer) (local.get $a))
      (then (return (i32.const 17))))

    (if (i32.eqz (call $setCurrentLayer (local.get $b))) (then (return (i32.const 18))))
    (if (i32.ne (call $currentLayer) (local.get $b))
      (then (return (i32.const 19))))

    ;; ── renaming ──────────────────────────────────────────────────────────────
    ;; Without uniquifying, a duplicate name is allowed: "Alpha" is already taken by the other
    ;; layer and the rename goes through anyway, giving 5 characters back.
    (call $renameLayer (local.get $b) (i32.const 48) (i32.const 5) (i32.const 0))
    (if (i32.ne (call $label (local.get $b) (i32.const 512) (i32.const 64)) (i32.const 5))
      (then (return (i32.const 20))))
    ;; With it, the same request answers a different name -- "Alpha 1", seven characters. The
    ;; two calls differ only in that flag, so an implementation ignoring it fails here.
    (call $renameLayer (local.get $b) (i32.const 48) (i32.const 5) (i32.const 1))
    (if (i32.ne (call $label (local.get $b) (i32.const 512) (i32.const 64)) (i32.const 7))
      (then (return (i32.const 21))))

    ;; ── hiding ────────────────────────────────────────────────────────────────
    (call $hideAll (local.get $document) (i32.const 1))
    (if (i32.ne (call $isHidden (local.get $a)) (i32.const 1)) (then (return (i32.const 22))))
    (if (i32.ne (call $isHidden (local.get $b)) (i32.const 1)) (then (return (i32.const 23))))
    (call $hideAll (local.get $document) (i32.const 0))
    (if (i32.ne (call $isHidden (local.get $a)) (i32.const 0)) (then (return (i32.const 24))))
    (if (i32.ne (call $isHidden (local.get $b)) (i32.const 0)) (then (return (i32.const 25))))

    ;; Solo is the asymmetric one: the named layer stays, its siblings go.
    (call $layerSolo (local.get $a) (i32.const 0))
    (if (i32.ne (call $isHidden (local.get $a)) (i32.const 0)) (then (return (i32.const 26))))
    (if (i32.ne (call $isHidden (local.get $b)) (i32.const 1)) (then (return (i32.const 27))))

    ;; Forcing is what the flag decides, and it only shows when the siblings are ALREADY hidden.
    ;; Unforced, solo is a toggle and brings them back; forced, it leaves them hidden. Starting
    ;; from everything hidden is therefore the only state where the two answers differ.
    (call $hideAll (local.get $document) (i32.const 1))
    (call $layerSolo (local.get $a) (i32.const 0))
    (if (i32.ne (call $isHidden (local.get $b)) (i32.const 0)) (then (return (i32.const 33))))
    (call $hideAll (local.get $document) (i32.const 1))
    (call $layerSolo (local.get $a) (i32.const 1))
    (if (i32.ne (call $isHidden (local.get $b)) (i32.const 1)) (then (return (i32.const 34))))

    ;; ── locking ───────────────────────────────────────────────────────────────
    (call $lockAll (local.get $document) (i32.const 1))
    (if (i32.ne (call $isLocked (local.get $a)) (i32.const 1)) (then (return (i32.const 28))))
    (if (i32.ne (call $isLocked (local.get $b)) (i32.const 1)) (then (return (i32.const 29))))
    (call $lockAll (local.get $document) (i32.const 0))
    (if (i32.ne (call $isLocked (local.get $a)) (i32.const 0)) (then (return (i32.const 30))))

    (call $lockOthers (local.get $a) (i32.const 0))
    (if (i32.ne (call $isLocked (local.get $b)) (i32.const 1)) (then (return (i32.const 31))))
    (if (i32.ne (call $isLocked (local.get $a)) (i32.const 0)) (then (return (i32.const 32))))

    ;; And the same toggle-versus-force distinction, from everything already locked.
    (call $lockAll (local.get $document) (i32.const 1))
    (call $lockOthers (local.get $a) (i32.const 0))
    (if (i32.ne (call $isLocked (local.get $b)) (i32.const 0)) (then (return (i32.const 35))))
    (call $lockAll (local.get $document) (i32.const 1))
    (call $lockOthers (local.get $a) (i32.const 1))
    (if (i32.ne (call $isLocked (local.get $b)) (i32.const 1)) (then (return (i32.const 36))))

    ;; ── the third layer position ──────────────────────────────────────────────
    ;; 2 is LPOS_BELOW, the one the two calls above did not reach.
    (local.set $c (call $createLayer (local.get $b) (i32.const 56) (i32.const 4) (i32.const 2)))
    (if (i32.eqz (local.get $c)) (then (return (i32.const 37))))
    (if (i32.ne (call $nextSibling (local.get $c)) (local.get $b)) (then (return (i32.const 38))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 9))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
