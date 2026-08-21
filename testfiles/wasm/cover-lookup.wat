;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: lookup and identity.
;;
;; A quarter of the shipped Python extensions walk the tree by hand looking for elements of a
;; type, because a subprocess holding serialised SVG has nothing better. Inkscape has had
;; indexed lookup and real CSS selector matching the whole time.
;;
;; The identity half is title and desc, which are CHILD ELEMENTS rather than attributes, so
;; getAttribute cannot reach them and writing one means creating an element in the right place.
;; And label, which looks like an attribute but is not only that: label() falls back to
;; defaultLabel() when nothing is set, and that fallback is the name the UI shows.
;;
;; Build:  water cover-lookup.wat -o cover-lookup.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Document" "getElementsByTagName"
    (func $byTagName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementsByClassName"
    (func $byClassName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "querySelector"    (func $querySelector (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "querySelectorAll" (func $querySelectorAll (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "generateId"    (func $generateId (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "resourceList"  (func $resourceList (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "resolveHref"   (func $resolveHref (param i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGSVGElement" "defs"      (func $defs (param i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "namedView" (func $namedView (param i32) (result i32)))

  (import "org.inkscape.SVGElement" "label"    (func $label (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "defaultLabel"
    (func $defaultLabel (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "setLabel" (func $setLabel (param i32 i32 i32)))
  (import "org.inkscape.SVGElement" "title"    (func $title (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "setTitle" (func $setTitle (param i32 i32 i32)))
  (import "org.inkscape.SVGElement" "desc"     (func $desc (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGElement" "setDesc"  (func $setDesc (param i32 i32 i32)))
  (import "org.inkscape.SVGElement" "linkedObjects" (func $linked (param i32 i32) (result i32)))

  (import "org.inkscape.SVGGraphicsElement" "isHidden" (func $isHidden (param i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "isLocked" (func $isLocked (param i32) (result i32)))

  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"   (func $nodesItem (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-lookup")
  (data (i32.const 32)  "rect")
  (data (i32.const 40)  "marked")
  (data (i32.const 48)  ".marked")
  (data (i32.const 56)  "wasmtest")
  (data (i32.const 72)  "gradient")
  (data (i32.const 88)  "marker")
  (data (i32.const 96)  "#box")
  (data (i32.const 104) "box")
  (data (i32.const 112) "tagged")
  (data (i32.const 120) "described")
  (data (i32.const 136) "hidden-rect")
  (data (i32.const 152) "locked-rect")
  (data (i32.const 168) "gradient-user")
  (data (i32.const 184) "grad")
  (data (i32.const 192) "A title")
  (data (i32.const 200) "A description")
  (data (i32.const 216) "renamed")

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

  ;; Is $wanted somewhere in the NodeList $list?
  (func $listHas (param $list i32) (param $wanted i32) (result i32)
    (local $i i32) (local $n i32)
    (local.set $n (call $nodesLength (local.get $list)))
    (block $done
      (loop $next
        (br_if $done (i32.ge_s (local.get $i) (local.get $n)))
        (if (i32.eq (call $nodesItem (local.get $list) (local.get $i)) (local.get $wanted))
          (then (return (i32.const 1))))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $next)))
    (i32.const 0))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $list i32)
    (local $box i32) (local $tagged i32) (local $described i32) (local $grad i32) (local $user i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 104) (i32.const 3)))
    (local.set $tagged (call $getElementById (local.get $document) (i32.const 112) (i32.const 6)))
    (local.set $described (call $getElementById (local.get $document) (i32.const 120) (i32.const 9)))
    (local.set $grad (call $getElementById (local.get $document) (i32.const 184) (i32.const 4)))
    (local.set $user (call $getElementById (local.get $document) (i32.const 168) (i32.const 13)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))
    (if (i32.eqz (local.get $tagged)) (then (return (i32.const 2))))
    (if (i32.eqz (local.get $described)) (then (return (i32.const 3))))
    (if (i32.eqz (local.get $grad)) (then (return (i32.const 4))))
    (if (i32.eqz (local.get $user)) (then (return (i32.const 5))))

    ;; ── by tag name ───────────────────────────────────────────────────────────
    ;; The bare local name, as DOM spells it: Inkscape prefixes "svg:" internally, so passing
    ;; "svg:rect" would find nothing. An exact count is the point, since a changed fixture
    ;; should make someone look rather than pass quietly.
    ;;
    ;; Eleven, not twelve, although the fixture source contains twelve <rect> elements. The
    ;; twelfth carries a live path effect, and Inkscape rewrites such a shape on load as
    ;; <path sodipodi:type="rect">: the repr name becomes svg:path while the object stays an
    ;; SPRect. This matches on the REPR name, so it counts eleven -- and that is the correct
    ;; answer for an operation defined over element names.
    (local.set $list (call $byTagName (local.get $document) (i32.const 32) (i32.const 4)))
    (if (i32.ne (call $nodesLength (local.get $list)) (i32.const 11)) (then (return (i32.const 6))))
    (if (i32.eqz (call $listHas (local.get $list) (local.get $box))) (then (return (i32.const 7))))

    ;; ── by class, and by selector ─────────────────────────────────────────────
    (local.set $list (call $byClassName (local.get $document) (i32.const 40) (i32.const 6)))
    (if (i32.ne (call $nodesLength (local.get $list)) (i32.const 1)) (then (return (i32.const 8))))
    (if (i32.eqz (call $listHas (local.get $list) (local.get $tagged))) (then (return (i32.const 9))))

    ;; Real selector matching, not a tag shortcut.
    (local.set $list (call $querySelectorAll (local.get $document) (i32.const 48) (i32.const 7)))
    (if (i32.ne (call $nodesLength (local.get $list)) (i32.const 1)) (then (return (i32.const 10))))
    (if (i32.eqz (call $listHas (local.get $list) (local.get $tagged))) (then (return (i32.const 11))))

    ;; querySelector answers ONE element, not a list of one.
    (if (i32.ne (call $querySelector (local.get $document) (i32.const 48) (i32.const 7))
                (local.get $tagged))
      (then (return (i32.const 12))))

    ;; ── ids ───────────────────────────────────────────────────────────────────
    ;; The generated id must not already be taken, which is the entire point of asking the
    ;; document instead of inventing one.
    (if (i32.le_s (call $generateId (local.get $document) (i32.const 56) (i32.const 8)
                                    (i32.const 1024) (i32.const 256)) (i32.const 0))
      (then (return (i32.const 13))))
    (if (i32.ne (call $getElementById (local.get $document) (i32.const 1024) (i32.const 8))
                (i32.const 0))
      (then (return (i32.const 14))))

    ;; ── href resolution ───────────────────────────────────────────────────────
    (if (i32.ne (call $resolveHref (local.get $document) (i32.const 96) (i32.const 4))
                (local.get $box))
      (then (return (i32.const 15))))

    ;; ── defs, named view ──────────────────────────────────────────────────────
    (if (i32.eqz (call $defs (local.get $root))) (then (return (i32.const 16))))
    (if (i32.eqz (call $namedView (local.get $root))) (then (return (i32.const 17))))

    ;; ── resources ─────────────────────────────────────────────────────────────
    (local.set $list (call $resourceList (local.get $document) (i32.const 72) (i32.const 8)))
    (if (i32.ne (call $nodesLength (local.get $list)) (i32.const 1)) (then (return (i32.const 18))))
    (if (i32.eqz (call $listHas (local.get $list) (local.get $grad))) (then (return (i32.const 19))))
    ;; Markers do not register themselves, so the answer is empty rather than wrong. Asserted
    ;; so the docs and the behaviour cannot drift apart.
    (if (i32.ne (call $nodesLength (call $resourceList (local.get $document)
                                                       (i32.const 88) (i32.const 6)))
                (i32.const 0))
      (then (return (i32.const 20))))

    ;; ── back links ────────────────────────────────────────────────────────────
    ;; Ask the gradient who paints with it. -1 is LinkedObjectNature::DEPENDENT.
    (if (i32.eqz (call $listHas (call $linked (local.get $grad) (i32.const -1)) (local.get $user)))
      (then (return (i32.const 21))))

    ;; ── title and desc: child elements, not attributes ────────────────────────
    (if (i32.ne (call $title (local.get $described) (i32.const 1024) (i32.const 256))
                (i32.const 7))
      (then (return (i32.const 22))))
    (if (i32.eqz (call $bytesAre (i32.const 192) (i32.const 7))) (then (return (i32.const 23))))
    (if (i32.ne (call $desc (local.get $described) (i32.const 1024) (i32.const 256))
                (i32.const 13))
      (then (return (i32.const 24))))
    (if (i32.eqz (call $bytesAre (i32.const 200) (i32.const 13))) (then (return (i32.const 25))))

    ;; An element with neither answers absent, not empty.
    (if (i32.ne (call $title (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 26))))
    (if (i32.ne (call $desc (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 27))))

    ;; Writing one creates the child element.
    (call $setTitle (local.get $box) (i32.const 216) (i32.const 7))
    (if (i32.ne (call $title (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 28))))
    (if (i32.eqz (call $bytesAre (i32.const 216) (i32.const 7))) (then (return (i32.const 29))))
    (call $setDesc (local.get $box) (i32.const 216) (i32.const 7))
    (if (i32.ne (call $desc (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 30))))

    ;; ── label: two different questions ────────────────────────────────────────
    ;; The attribute is ABSENT until someone sets it. The displayed name never is -- it falls
    ;; back to "#id", which is Inkscape's presentation choice and lives in no attribute, so a
    ;; plugin listing objects for a user cannot reconstruct it from the document.
    (if (i32.ne (call $label (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 31))))
    ;; "#box"
    (if (i32.ne (call $defaultLabel (local.get $box) (i32.const 1024) (i32.const 256))
                (i32.const 4))
      (then (return (i32.const 32))))
    (if (i32.ne (i32.load8_u (i32.const 1024)) (i32.const 35))     ;; '#'
      (then (return (i32.const 33))))

    ;; Setting it makes the attribute present, and the displayed name follows it.
    (call $setLabel (local.get $box) (i32.const 216) (i32.const 7))
    (if (i32.ne (call $label (local.get $box) (i32.const 1024) (i32.const 256)) (i32.const 7))
      (then (return (i32.const 38))))
    (if (i32.eqz (call $bytesAre (i32.const 216) (i32.const 7))) (then (return (i32.const 39))))
    (if (i32.ne (call $defaultLabel (local.get $box) (i32.const 1024) (i32.const 256))
                (i32.const 7))
      (then (return (i32.const 40))))
    (if (i32.eqz (call $bytesAre (i32.const 216) (i32.const 7))) (then (return (i32.const 41))))

    ;; ── hidden and locked ─────────────────────────────────────────────────────
    ;; Resolved state, not an attribute reading: hidden comes from the style cascade, locked
    ;; from sodipodi:insensitive on this element or any ancestor.
    (if (i32.ne (call $isHidden
                  (call $getElementById (local.get $document) (i32.const 136) (i32.const 11)))
                (i32.const 1))
      (then (return (i32.const 34))))
    (if (i32.ne (call $isHidden (local.get $box)) (i32.const 0)) (then (return (i32.const 35))))
    (if (i32.ne (call $isLocked
                  (call $getElementById (local.get $document) (i32.const 152) (i32.const 11)))
                (i32.const 1))
      (then (return (i32.const 36))))
    (if (i32.ne (call $isLocked (local.get $box)) (i32.const 0)) (then (return (i32.const 37))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 9))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
