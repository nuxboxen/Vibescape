;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Systematic refusal coverage: every operation that takes a handle, against every way a
;; handle can be wrong.
;;
;; A trap ends the invocation, so each bad call needs a run of its own. Writing one module
;; per call would mean a hundred near-identical files, which is how suites end up testing
;; one representative per class and calling it covered. Instead this is one module that
;; reads a case number from its .inx parameter and makes exactly that one bad call; the
;; runner invokes it once per case and checks the diagnostic each time.
;;
;; The three ways a handle is wrong, all of which must be refused rather than dereferenced:
;;
;;   null      0, which is what every query that found nothing returns -- so passing one
;;             straight back in is the easiest mistake a plugin can make
;;   stale     an index this invocation never issued; handles must not be guessable, and one
;;             kept from a previous run must not address whatever now occupies the slot
;;   wrong     a real handle of the wrong kind: a Document where a Node belongs, or a
;;             DOMStringList where a NodeList belongs. Both are integers and nothing but the
;;             recorded kind distinguishes them.
;;
;; Build:  water trap-matrix.wat -o trap-matrix.wasm

(module
  (import "org.inkscape.Inkscape" "inkParamInt" (func $case (param i32 i32) (result i32)))

  (import "org.inkscape.Document" "documentElement"  (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"    (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createElementNS"  (func $createElementNS (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createTextNode"   (func $createTextNode (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "createComment"    (func $createComment (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"   (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getComputedStyle" (func $getComputedStyle (param i32) (result i32)))

  (import "org.inkscape.Node" "nodeType"        (func $nodeType (param i32) (result i32)))
  (import "org.inkscape.Node" "nodeName"        (func $nodeName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "localName"       (func $localName (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "namespaceURI"    (func $namespaceURI (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "ownerDocument"   (func $ownerDocument (param i32) (result i32)))
  (import "org.inkscape.Node" "parentNode"      (func $parentNode (param i32) (result i32)))
  (import "org.inkscape.Node" "firstChild"      (func $firstChild (param i32) (result i32)))
  (import "org.inkscape.Node" "lastChild"       (func $lastChild (param i32) (result i32)))
  (import "org.inkscape.Node" "nextSibling"     (func $nextSibling (param i32) (result i32)))
  (import "org.inkscape.Node" "previousSibling" (func $previousSibling (param i32) (result i32)))
  (import "org.inkscape.Node" "childNodes"      (func $childNodes (param i32) (result i32)))
  (import "org.inkscape.Node" "textContent"     (func $textContent (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "setTextContent"  (func $setTextContent (param i32 i32 i32)))
  (import "org.inkscape.Node" "insertBefore"    (func $insertBefore (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node" "removeChild"     (func $removeChild (param i32 i32) (result i32)))
  (import "org.inkscape.Node" "replaceChild"    (func $replaceChild (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node" "cloneNode"       (func $cloneNode (param i32 i32) (result i32)))

  (import "org.inkscape.Element" "getAttribute"      (func $getAttribute (param i32 i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Element" "setAttribute"      (func $setAttribute (param i32 i32 i32 i32 i32)))
  (import "org.inkscape.Element" "removeAttribute"   (func $removeAttribute (param i32 i32 i32)))
  (import "org.inkscape.Element" "hasAttribute"      (func $hasAttribute (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Element" "getAttributeNames" (func $getAttributeNames (param i32) (result i32)))

  (import "org.inkscape.NodeList"      "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList"      "item"   (func $nodesItem (param i32 i32) (result i32)))
  (import "org.inkscape.DOMStringList" "length" (func $namesLength (param i32) (result i32)))
  (import "org.inkscape.DOMStringList" "item"   (func $namesItem (param i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.SVGGraphicsElement" "getBBox"          (func $getBBox (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getCTM"           (func $getCTM (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGraphicsElement" "getScreenCTM"     (func $getScreenCTM (param i32 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "getTotalLength"   (func $getTotalLength (param i32) (result f64)))
  (import "org.inkscape.SVGGeometryElement" "getPointAtLength" (func $getPointAtLength (param i32 f64 i32) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "isPointInFill"    (func $isPointInFill (param i32 f64 f64) (result i32)))
  (import "org.inkscape.SVGGeometryElement" "isPointInStroke"  (func $isPointInStroke (param i32 f64 f64) (result i32)))

  (import "org.inkscape.SVGTextContentElement" "getNumberOfChars"       (func $getNumberOfChars (param i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getComputedTextLength"  (func $getComputedTextLength (param i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getSubStringLength"     (func $getSubStringLength (param i32 i32 i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getStartPositionOfChar" (func $getStartPositionOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getEndPositionOfChar"   (func $getEndPositionOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getExtentOfChar"        (func $getExtentOfChar (param i32 i32 i32) (result i32)))
  (import "org.inkscape.SVGTextContentElement" "getRotationOfChar"      (func $getRotationOfChar (param i32 i32) (result f64)))
  (import "org.inkscape.SVGTextContentElement" "getCharNumAtPosition"   (func $getCharNumAtPosition (param i32 f64 f64) (result i32)))

  (import "org.inkscape.SVGSVGElement" "checkEnclosure"    (func $checkEnclosure (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "checkIntersection" (func $checkIntersection (param i32 i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "getEnclosureList"  (func $getEnclosureList (param i32) (result i32)))
  (import "org.inkscape.SVGSVGElement" "getIntersectionList" (func $getIntersectionList (param i32) (result i32)))

  (import "org.inkscape.CSSStyleDeclaration" "getPropertyValue"
    (func $getPropertyValue (param i32 i32 i32 i32 i32) (result i32)))

  (import "org.inkscape.Inkscape" "inkIsSelected"        (func $inkIsSelected (param i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectedNodeCount" (func $inkSelectedNodeCount (param i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectedNode"      (func $inkSelectedNode (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Inkscape" "inkSelectionAdd"      (func $inkSelectionAdd (param i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "case")
  (data (i32.const 8)  "svg:g")
  (data (i32.const 16) "id")
  (data (i32.const 24) "box")

  ;; The handle under test, chosen by the hundreds digit of the case number:
  ;;   0xx  null            1xx  never issued      2xx  a real handle of the wrong kind
  ;;
  ;; "Wrong kind" is relative to what the operation expects, which is the whole point: a
  ;; Document handle is wrong for a Node operation and right for a Document one. Passing the
  ;; document to createElement would be testing that it works, not that it refuses. Operations
  ;; 23-28 take a Document, so those get the root element instead; everything else takes a
  ;; Node, list or element, so those get the document.
  (func $bad (param $case i32) (param $document i32) (result i32)
    (local $bucket i32) (local $op i32)
    (local.set $bucket (i32.div_u (local.get $case) (i32.const 100)))
    (local.set $op (i32.rem_u (local.get $case) (i32.const 100)))
    (if (i32.eqz (local.get $bucket)) (then (return (i32.const 0))))
    (if (i32.eq (local.get $bucket) (i32.const 1)) (then (return (i32.const 999999))))
    (if (i32.and (i32.ge_u (local.get $op) (i32.const 23)) (i32.le_u (local.get $op) (i32.const 28)))
      (then (return (call $documentElement (local.get $document)))))
    (local.get $document))

  (func (export "effect") (param $document i32) (result i32)
    (local $c i32) (local $h i32) (local $op i32)

    (local.set $c (call $case (i32.const 0) (i32.const 4)))
    (local.set $h (call $bad (local.get $c) (local.get $document)))
    (local.set $op (i32.rem_u (local.get $c) (i32.const 100)))

    ;; Node -- identity and navigation
    (if (i32.eq (local.get $op) (i32.const 0)) (then (drop (call $nodeType (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 1)) (then (drop (call $nodeName (local.get $h) (i32.const 512) (i32.const 64)))))
    (if (i32.eq (local.get $op) (i32.const 2)) (then (drop (call $localName (local.get $h) (i32.const 512) (i32.const 64)))))
    (if (i32.eq (local.get $op) (i32.const 3)) (then (drop (call $namespaceURI (local.get $h) (i32.const 512) (i32.const 64)))))
    (if (i32.eq (local.get $op) (i32.const 4)) (then (drop (call $ownerDocument (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 5)) (then (drop (call $parentNode (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 6)) (then (drop (call $firstChild (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 7)) (then (drop (call $lastChild (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 8)) (then (drop (call $nextSibling (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 9)) (then (drop (call $previousSibling (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 10)) (then (drop (call $childNodes (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 11)) (then (drop (call $textContent (local.get $h) (i32.const 512) (i32.const 64)))))
    (if (i32.eq (local.get $op) (i32.const 12)) (then (call $setTextContent (local.get $h) (i32.const 8) (i32.const 5))))

    ;; Node -- structure. Argument 0 is the parent in each, which is what is being tested.
    (if (i32.eq (local.get $op) (i32.const 13))
      (then (drop (call $insertBefore (local.get $h) (local.get $h) (i32.const 0)))))
    (if (i32.eq (local.get $op) (i32.const 14))
      (then (drop (call $appendChild (local.get $h) (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 15))
      (then (drop (call $removeChild (local.get $h) (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 16))
      (then (drop (call $replaceChild (local.get $h) (local.get $h) (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 17)) (then (drop (call $cloneNode (local.get $h) (i32.const 0)))))

    ;; Element -- attributes
    (if (i32.eq (local.get $op) (i32.const 18))
      (then (drop (call $getAttribute (local.get $h) (i32.const 16) (i32.const 2) (i32.const 512) (i32.const 64)))))
    (if (i32.eq (local.get $op) (i32.const 19))
      (then (call $setAttribute (local.get $h) (i32.const 16) (i32.const 2) (i32.const 24) (i32.const 3))))
    (if (i32.eq (local.get $op) (i32.const 20))
      (then (call $removeAttribute (local.get $h) (i32.const 16) (i32.const 2))))
    (if (i32.eq (local.get $op) (i32.const 21))
      (then (drop (call $hasAttribute (local.get $h) (i32.const 16) (i32.const 2)))))
    (if (i32.eq (local.get $op) (i32.const 22)) (then (drop (call $getAttributeNames (local.get $h)))))

    ;; Document
    (if (i32.eq (local.get $op) (i32.const 23)) (then (drop (call $documentElement (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 24)) (then (drop (call $createElement (local.get $h) (i32.const 8) (i32.const 5)))))
    (if (i32.eq (local.get $op) (i32.const 25))
      (then (drop (call $createElementNS (local.get $h) (i32.const 8) (i32.const 5) (i32.const 8) (i32.const 5)))))
    (if (i32.eq (local.get $op) (i32.const 26)) (then (drop (call $createTextNode (local.get $h) (i32.const 8) (i32.const 5)))))
    (if (i32.eq (local.get $op) (i32.const 27)) (then (drop (call $createComment (local.get $h) (i32.const 8) (i32.const 5)))))
    (if (i32.eq (local.get $op) (i32.const 28)) (then (drop (call $getElementById (local.get $h) (i32.const 24) (i32.const 3)))))
    (if (i32.eq (local.get $op) (i32.const 29)) (then (drop (call $getComputedStyle (local.get $h)))))

    ;; Lists
    (if (i32.eq (local.get $op) (i32.const 30)) (then (drop (call $nodesLength (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 31)) (then (drop (call $nodesItem (local.get $h) (i32.const 0)))))
    (if (i32.eq (local.get $op) (i32.const 32)) (then (drop (call $namesLength (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 33))
      (then (drop (call $namesItem (local.get $h) (i32.const 0) (i32.const 512) (i32.const 64)))))

    ;; Derived queries. These must trap on a bad handle rather than quietly answering
    ;; "absent", which is reserved for a real node that draws nothing.
    (if (i32.eq (local.get $op) (i32.const 34)) (then (drop (call $getBBox (local.get $h) (i32.const 1) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 35)) (then (drop (call $getCTM (local.get $h) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 36)) (then (drop (call $getScreenCTM (local.get $h) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 37)) (then (drop (call $getTotalLength (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 38))
      (then (drop (call $getPointAtLength (local.get $h) (f64.const 1) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 39))
      (then (drop (call $isPointInFill (local.get $h) (f64.const 0) (f64.const 0)))))
    (if (i32.eq (local.get $op) (i32.const 40))
      (then (drop (call $isPointInStroke (local.get $h) (f64.const 0) (f64.const 0)))))

    ;; Text metrics
    (if (i32.eq (local.get $op) (i32.const 41)) (then (drop (call $getNumberOfChars (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 42)) (then (drop (call $getComputedTextLength (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 43))
      (then (drop (call $getSubStringLength (local.get $h) (i32.const 0) (i32.const 1)))))
    (if (i32.eq (local.get $op) (i32.const 44))
      (then (drop (call $getStartPositionOfChar (local.get $h) (i32.const 0) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 45))
      (then (drop (call $getEndPositionOfChar (local.get $h) (i32.const 0) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 46))
      (then (drop (call $getExtentOfChar (local.get $h) (i32.const 0) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 47)) (then (drop (call $getRotationOfChar (local.get $h) (i32.const 0)))))
    (if (i32.eq (local.get $op) (i32.const 48))
      (then (drop (call $getCharNumAtPosition (local.get $h) (f64.const 0) (f64.const 0)))))

    ;; Hit testing and style
    (if (i32.eq (local.get $op) (i32.const 49)) (then (drop (call $checkEnclosure (local.get $h) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 50)) (then (drop (call $checkIntersection (local.get $h) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 51))
      (then (drop (call $getPropertyValue (local.get $h) (i32.const 16) (i32.const 2) (i32.const 512) (i32.const 64)))))

    ;; Session tier
    (if (i32.eq (local.get $op) (i32.const 52)) (then (drop (call $inkIsSelected (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 53)) (then (drop (call $inkSelectedNodeCount (local.get $h)))))
    (if (i32.eq (local.get $op) (i32.const 54))
      (then (drop (call $inkSelectedNode (local.get $h) (i32.const 0) (i32.const 512)))))
    (if (i32.eq (local.get $op) (i32.const 55)) (then (call $inkSelectionAdd (local.get $h))))

    ;; Reaching here means the call was accepted. That is the failure being looked for, so
    ;; report it rather than succeeding quietly.
    (i32.const 7))
)
