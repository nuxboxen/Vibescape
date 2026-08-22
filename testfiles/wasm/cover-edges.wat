;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: the boundaries. Every operation is reachable from the modules above; this one is
;; about what they do at the edges, which is where an interface is actually decided.
;;
;;   - the three-way string result on every operation that returns a string, not just the one
;;     it was first designed for
;;   - indices below zero and past the end
;;   - queries on things that are not the kind of thing being asked about
;;   - a memory that grows underneath the host, which invalidates any pointer it cached
;;
;; Build:  water cover-edges.wat -o cover-edges.wasm

(module
  (import "org.inkscape.Document" "documentElement"  (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"    (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createComment"    (func $createComment (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"   (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"      (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node"     "nodeName"         (func $nodeName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "localName"        (func $localName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "namespaceURI"     (func $namespaceURI (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "textContent"      (func $textContent (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "childNodes"       (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"             (func $nodesItem (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"     (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element"  "getAttribute"     (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Element"  "getAttributeNames" (func $getAttributeNames (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "item"        (func $namesItem (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.CSSStyleDeclaration" "getPropertyValue"
    (func $getPropertyValue (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGGraphicsElement" "getBBox" (func $getBBox (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "getTotalLength"   (func $getTotalLength (param i32) (result f64)))
  (import "org.inkscape.SVGGeometryElement" "getPointAtLength" (func $getPointAtLength (param i32 f64 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "isPointInStroke"  (func $isPointInStroke (param i32 f64 f64) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getExtentOfChar"
    (func $getExtentOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getRotationOfChar"
    (func $getRotationOfChar (param i32 i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getSubStringLength"
    (func $getSubStringLength (param i32 i32 i32) (result f64)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "box")
  (data (i32.const 8)   "label")
  (data (i32.const 16)  "group")
  (data (i32.const 24)  "svg:g")
  (data (i32.const 32)  "id")
  (data (i32.const 40)  "ok-edges")
  (data (i32.const 56)  "data-empty")
  (data (i32.const 72)  "no-such-property")
  (data (i32.const 96)  "data-far")
  (data (i32.const 112) "plain")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $box i32) (local $label i32) (local $group i32)
    (local $comment i32) (local $marker i32) (local $names i32) (local $grown i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 0) (i32.const 3)))
    (local.set $label (call $getElementById (local.get $document) (i32.const 8) (i32.const 5)))
    (local.set $group (call $getElementById (local.get $document) (i32.const 16) (i32.const 5)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── the string result, on operations other than getAttribute ──────────────
    ;; "svg:rect" is 8 bytes, and the answer is its own length whether or not it fitted. So
    ;; all three of these report 8, and what separates them is the BUFFER: a short call must
    ;; leave it alone rather than truncate into it.
    (i32.store8 (i32.const 1024) (i32.const 90))                          ;; 'Z' sentinel

    ;; A zero-capacity buffer sizes the answer without receiving any of it. This is the probe
    ;; a guest makes when it has no idea how much room to offer.
    (if (i32.ne (call $nodeName (local.get $box) (i32.const 1024) (i32.const 0)) (i32.const 8))
      (then (return (i32.const 2))))
    ;; One byte short is still short: 8 again, and still nothing written.
    (if (i32.ne (call $nodeName (local.get $box) (i32.const 1024) (i32.const 7)) (i32.const 8))
      (then (return (i32.const 3))))
    ;; Neither short call touched the buffer. Without this the three checks below would pass
    ;; just as well against a host that truncated, since the length alone cannot tell.
    (if (i32.ne (i32.load8_u (i32.const 1024)) (i32.const 90))
      (then (return (i32.const 27))))
    ;; Exactly enough fits, and now the bytes do arrive.
    (if (i32.ne (call $nodeName (local.get $box) (i32.const 1024) (i32.const 8)) (i32.const 8))
      (then (return (i32.const 4))))
    (if (i32.ne (i32.load8_u (i32.const 1024)) (i32.const 115))           ;; 's' of "svg:rect"
      (then (return (i32.const 28))))

    ;; "rect" is 4 bytes, so a capacity of 1 reports 4 rather than writing one byte of it.
    (if (i32.ne (call $localName (local.get $box) (i32.const 1024) (i32.const 1)) (i32.const 4))
      (then (return (i32.const 5))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 24) (i32.const 5)))

    ;; A prefixed name resolves to the URI that prefix stands for...
    (if (i32.le_s (call $namespaceURI (local.get $marker) (i32.const 1024) (i32.const 256))
                  (i32.const 0))
      (then (return (i32.const 6))))
    ;; ...and an unprefixed one has no namespace to report: absent, not the empty string.
    ;; (Numbered 26: the numbers are labels for locating a failure, not a sequence, so a
    ;; check added later takes the next free one rather than renumbering everything below.)
    (if (i32.ne (call $namespaceURI
                  (call $createElement (local.get $document) (i32.const 112) (i32.const 5))
                  (i32.const 1024) (i32.const 256))
                (i32.const -1))
      (then (return (i32.const 26))))

    ;; An element node has no text content of its own.
    (if (i32.ne (call $textContent (local.get $box) (i32.const 1024) (i32.const 256))
                (i32.const -1))
      (then (return (i32.const 7))))

    ;; A property that is not set is absent, distinctly from a property set to "".
    (if (i32.ne (call $getPropertyValue (local.get $box) (i32.const 72) (i32.const 16)
                                        (i32.const 1024) (i32.const 256)) (i32.const -1))
      (then (return (i32.const 8))))

    ;; An attribute set to the empty string is present and zero-length -- not absent.
    (call $setAttribute (local.get $marker) (i32.const 56) (i32.const 10) (i32.const 1024) (i32.const 0))
    (if (i32.ne (call $getAttribute (local.get $marker) (i32.const 56) (i32.const 10)
                                    (i32.const 1024) (i32.const 256)) (i32.const 0))
      (then (return (i32.const 9))))

    ;; DOMStringList: below zero and past the end both report absence.
    (local.set $names (call $getAttributeNames (local.get $box)))
    (if (i32.ne (call $namesItem (local.get $names) (i32.const -1) (i32.const 1024) (i32.const 256))
                (i32.const -1))
      (then (return (i32.const 10))))

    ;; ── indices out of range ──────────────────────────────────────────────────
    ;; NodeList below zero is null, matching past-the-end.
    (if (i32.ne (call $nodesItem (call $childNodes (local.get $group)) (i32.const -1)) (i32.const 0))
      (then (return (i32.const 11))))

    ;; Character indices below zero and past the end report absence rather than reading
    ;; whatever the layout happens to hold at that offset.
    (if (i32.ne (call $getExtentOfChar (local.get $label) (i32.const -1) (i32.const 1024))
                (i32.const 0))
      (then (return (i32.const 12))))
    (if (i32.ne (call $getExtentOfChar (local.get $label) (i32.const 9999) (i32.const 1024))
                (i32.const 0))
      (then (return (i32.const 13))))
    (if (f64.ne (call $getRotationOfChar (local.get $label) (i32.const 9999)) (f64.const 0))
      (then (return (i32.const 14))))
    ;; A zero-length substring has zero length.
    (if (f64.ne (call $getSubStringLength (local.get $label) (i32.const 0) (i32.const 0))
                (f64.const 0))
      (then (return (i32.const 15))))

    ;; ── asking the wrong kind of thing ────────────────────────────────────────
    ;; A comment draws nothing, so it has no box -- absent, never a zero rect, because a
    ;; plugin must be able to tell "nothing here" from "something of zero size".
    (local.set $comment (call $createComment (local.get $document) (i32.const 0) (i32.const 3)))
    (drop (call $appendChild (local.get $root) (local.get $comment)))
    (if (i32.ne (call $getBBox (local.get $comment) (i32.const 1) (i32.const 1024)) (i32.const 0))
      (then (return (i32.const 16))))

    ;; A group is not a shape: no length, and no point along it.
    (if (f64.ne (call $getTotalLength (local.get $group)) (f64.const 0))
      (then (return (i32.const 17))))
    (if (i32.ne (call $getPointAtLength (local.get $group) (f64.const 1) (i32.const 1024))
                (i32.const 0))
      (then (return (i32.const 18))))

    ;; ── getBBox option bits ───────────────────────────────────────────────────
    ;; fill|markers. Inkscape has a geometric box and a visual box, not one per combination,
    ;; and the visual box carries stroke, markers and filters together -- so asking for
    ;; markers gets a box that includes the stroke too, which is 34 rather than 30. That is
    ;; a documented widening, asserted here so it stays deliberate rather than drifting.
    (if (i32.ne (call $getBBox (local.get $box) (i32.const 5) (i32.const 1024)) (i32.const 1))
      (then (return (i32.const 19))))
    (if (f64.ne (f64.load (i32.const 1040)) (f64.const 34)) (then (return (i32.const 20))))
    (if (i32.ne (call $getBBox (local.get $box) (i32.const 9) (i32.const 1024)) (i32.const 1))
      (then (return (i32.const 21))))

    ;; A shape with no stroke contains no point in its stroke.
    (if (i32.ne (call $isPointInStroke (local.get $marker) (f64.const 0) (f64.const 0))
                (i32.const 0))
      (then (return (i32.const 22))))

    ;; ── distinct objects, distinct handles ────────────────────────────────────
    ;; Interning must not collapse two different nodes onto one handle; the traversal module
    ;; asserts the converse, that one node reached two ways gives one handle.
    (if (i32.eq (local.get $box) (local.get $label)) (then (return (i32.const 23))))

    ;; ── the memory moves ──────────────────────────────────────────────────────
    ;; Growing linear memory can relocate it. A host that cached the base pointer, or the
    ;; size, would now be reading the wrong address or refusing a valid span. Write a key
    ;; into the freshly added page and use it.
    (local.set $grown (i32.mul (memory.grow (i32.const 1)) (i32.const 65536)))
    (if (i32.lt_s (local.get $grown) (i32.const 0)) (then (return (i32.const 24))))
    (i32.store8 (local.get $grown) (i32.const 100))                       ;; 'd'
    (i32.store8 (i32.add (local.get $grown) (i32.const 1)) (i32.const 45)) ;; '-'
    (i32.store8 (i32.add (local.get $grown) (i32.const 2)) (i32.const 120));; 'x'
    (call $setAttribute (local.get $marker)
      (local.get $grown) (i32.const 3) (local.get $grown) (i32.const 3))
    (if (i32.ne (call $getAttribute (local.get $marker) (local.get $grown) (i32.const 3)
                                    (i32.add (local.get $grown) (i32.const 8)) (i32.const 32))
                (i32.const 3))
      (then (return (i32.const 25))))

    (call $setAttribute (local.get $marker)
      (i32.const 32) (i32.const 2) (i32.const 40) (i32.const 8))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
