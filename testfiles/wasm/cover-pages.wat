;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; Coverage: multi-page documents.
;;
;; Pages are document state, not view state, so all of this works from the command line. That
;; matters because the guides-and-pages corpus rank (21 of 129 scripts touch namedview, guides
;; or pages) is made up of extensions that run headless.
;;
;; The fixture declares no pages at all, which is the ordinary case: a document without them
;; has a viewport and no <inkscape:page> elements. So the module builds one, measures it,
;; selects it and removes it -- a round trip that starts and ends at zero.
;;
;; Zero at both ends, but never one in between, and that asymmetry is the thing to know about
;; this API. A single-page document is spelled "no pages", the viewport standing in for the
;; page. So adding a page to a document that has none yields TWO, and deleting one of two
;; yields NONE. Counts here assert that, because a plugin that reasons in ordinary arithmetic
;; will be wrong twice.
;;
;; Every operation here that takes a flag is called with both values, and asserted to give two
;; different answers. A page whose margin and bleed are unset has a margin box and a bleed box
;; equal to the page itself, and a document whose width matches its viewBox has page
;; coordinates equal to document coordinates -- so on a document left alone, an implementation
;; that ignored those arguments entirely would pass. The fixture is therefore given a margin, a
;; bleed and a document scale before those questions are asked.
;;
;; Build:  water cover-pages.wat -o cover-pages.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Document" "getElementById"  (func $getElementById (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.Pages" "pageCount"   (func $pageCount (param i32) (result i32)))
  (import "org.inkscape.Pages" "pages"       (func $pages (param i32) (result i32)))
  (import "org.inkscape.Pages" "pageIndex"   (func $pageIndex (param i32) (result i32)))
  (import "org.inkscape.Pages" "newPage"     (func $newPage (param i32 f64 f64 f64 f64) (result i32)))
  (import "org.inkscape.Pages" "deletePage"  (func $deletePage (param i32 i32)))
  (import "org.inkscape.Pages" "pageRect"    (func $pageRect (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Pages" "pageMargin"  (func $pageMargin (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Pages" "resizePage"  (func $resizePage (param i32 f64 f64)))
  (import "org.inkscape.Pages" "selectPage"  (func $selectPage (param i32) (result i32)))
  (import "org.inkscape.Pages" "selectedPage" (func $selectedPage (param i32) (result i32)))
  (import "org.inkscape.Pages" "pageItems"   (func $pageItems (param i32 i32) (result i32)))
  (import "org.inkscape.Pages" "fitPageToSelection" (func $fitPage (param i32 i32)))
  (import "org.inkscape.Selection" "selectionAdd" (func $selectionAdd (param i32)))
  (import "org.inkscape.Selection" "selectionClear" (func $selectionClear))

  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"   (func $nodesItem (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)   "svg:g")
  (data (i32.const 8)   "id")
  (data (i32.const 16)  "ok-pages")
  (data (i32.const 32)  "box")
  (data (i32.const 40)  "inkscape:margin")
  (data (i32.const 64)  "inkscape:bleed")
  (data (i32.const 80)  "2")
  (data (i32.const 88)  "3")
  (data (i32.const 96)  "width")
  (data (i32.const 104) "height")
  (data (i32.const 112) "800")
  (data (i32.const 120) "400")
  (data (i32.const 128) "svg:rect")
  (data (i32.const 144) "doomed")
  (data (i32.const 152) "x")
  (data (i32.const 160) "y")
  (data (i32.const 168) "1010")
  (data (i32.const 176) "50")
  (data (i32.const 184) "inside")
  (data (i32.const 192) "100")
  (data (i32.const 200) "40")
  (data (i32.const 208) "20")
  (data (i32.const 216) "hid")
  (data (i32.const 224) "style")
  (data (i32.const 232) "display:none")
  (data (i32.const 248) "215")
  (data (i32.const 256) "10")
  (data (i32.const 264) "30")
  (data (i32.const 272) "bnd")
  (data (i32.const 280) "120")

  ;; |a - b| < tol
  (func $near (param $a f64) (param $b f64) (param $tol f64) (result i32)
    (f64.lt (f64.abs (f64.sub (local.get $a) (local.get $b))) (local.get $tol)))

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $marker i32) (local $page i32) (local $box i32)
    (local $second i32) (local $doomed i32) (local $rect i32)
    (local $overlapping i32) (local $contained i32)
    (local $plain f64) (local $margined f64)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $box (call $getElementById (local.get $document) (i32.const 32) (i32.const 3)))
    (if (i32.eqz (local.get $box)) (then (return (i32.const 1))))

    ;; ── a document with no pages ──────────────────────────────────────────────
    ;; No <inkscape:page> elements, so no pages -- the viewport is not one of them.
    (if (i32.ne (call $pageCount (local.get $document)) (i32.const 0))
      (then (return (i32.const 2))))
    (if (i32.ne (call $nodesLength (call $pages (local.get $document))) (i32.const 0))
      (then (return (i32.const 3))))
    (if (i32.ne (call $selectedPage (local.get $document)) (i32.const 0))
      (then (return (i32.const 4))))

    ;; ── build one ─────────────────────────────────────────────────────────────
    ;; TWO pages result, not one. Adding a page to a document that has none turns pages support
    ;; on, and doing that makes a page out of the existing viewBox first, so the new one is the
    ;; second (page-manager.cpp PageManager::newPage()). A plugin that assumed its page was the only one would
    ;; be wrong about the document it just made.
    (local.set $page (call $newPage (local.get $document)
                                    (f64.const 10) (f64.const 20) (f64.const 100) (f64.const 50)))
    (if (i32.eqz (local.get $page)) (then (return (i32.const 5))))
    (if (i32.ne (call $pageCount (local.get $document)) (i32.const 2))
      (then (return (i32.const 6))))
    (if (i32.ne (call $nodesLength (call $pages (local.get $document))) (i32.const 2))
      (then (return (i32.const 7))))

    ;; Its rectangle is the one asked for. 0 is the page's own coordinates.
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 8))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.01)))
      (then (return (i32.const 9))))
    (if (i32.eqz (call $near (f64.load (i32.const 520)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 10))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 100) (f64.const 0.01)))
      (then (return (i32.const 11))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 50) (f64.const 0.01)))
      (then (return (i32.const 12))))

    ;; ── which page is which ───────────────────────────────────────────────────
    ;; The order pages appear in is the order they print in, so a plugin numbering or
    ;; reordering them needs the index and cannot get it from the list position alone: the
    ;; list it holds may be a filtered one.
    (local.set $second (call $nodesItem (call $pages (local.get $document)) (i32.const 0)))
    (if (i32.eqz (local.get $second)) (then (return (i32.const 13))))
    (if (i32.ne (call $pageIndex (local.get $second)) (i32.const 0))
      (then (return (i32.const 14))))
    ;; The page just added is the second one, not the first.
    (if (i32.ne (call $pageIndex (local.get $page)) (i32.const 1))
      (then (return (i32.const 15))))

    ;; ── page coordinates versus document coordinates ──────────────────────────
    ;; These differ by the document scale, and the fixture has none: 400 units across a 400
    ;; unit viewBox. So the document is given a scale of 2 for the length of this question,
    ;; because with a scale of 1 an implementation that ignored the argument would pass.
    (call $setAttribute (local.get $root) (i32.const 96) (i32.const 5) (i32.const 112) (i32.const 3))
    (call $setAttribute (local.get $root) (i32.const 104) (i32.const 6) (i32.const 112) (i32.const 3))

    ;; The page's own coordinates are unmoved by it -- that is what makes them its own.
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 16))))
    (if (i32.eqz (call $near (f64.load (i32.const 512)) (f64.const 10) (f64.const 0.01)))
      (then (return (i32.const 17))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 100) (f64.const 0.01)))
      (then (return (i32.const 18))))

    ;; Document coordinates carry the scale.
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 1) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 19))))
    (if (i32.eqz (call $near (f64.load (i32.const 600)) (f64.const 20) (f64.const 0.01)))
      (then (return (i32.const 20))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 200) (f64.const 0.01)))
      (then (return (i32.const 21))))

    ;; Put it back, so everything after this measures the document the fixture describes.
    (call $setAttribute (local.get $root) (i32.const 96) (i32.const 5) (i32.const 120) (i32.const 3))
    (call $setAttribute (local.get $root) (i32.const 104) (i32.const 6) (i32.const 120) (i32.const 3))
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 1) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 22))))
    (if (i32.eqz (call $near (f64.load (i32.const 616)) (f64.const 100) (f64.const 0.01)))
      (then (return (i32.const 23))))

    ;; ── margin and bleed ──────────────────────────────────────────────────────
    ;; Both are unset on a new page, and an unset margin box and an unset bleed box are both
    ;; the page itself, so they have to be set before the two can be told apart.
    (call $setAttribute (local.get $page) (i32.const 40) (i32.const 15) (i32.const 80) (i32.const 1))
    (call $setAttribute (local.get $page) (i32.const 64) (i32.const 14) (i32.const 88) (i32.const 1))

    ;; 0 is the margin box: inside the page.
    (if (i32.ne (call $pageMargin (local.get $page) (i32.const 0) (i32.const 700)) (i32.const 1))
      (then (return (i32.const 24))))
    (if (f64.ge (f64.load (i32.const 716)) (f64.const 100))
      (then (return (i32.const 25))))

    ;; 1 is the bleed box: outside it. Asking for one must not answer the other.
    (if (i32.ne (call $pageMargin (local.get $page) (i32.const 1) (i32.const 800)) (i32.const 1))
      (then (return (i32.const 26))))
    (if (f64.le (f64.load (i32.const 816)) (f64.const 100))
      (then (return (i32.const 27))))

    ;; ── select it ─────────────────────────────────────────────────────────────
    (if (i32.eqz (call $selectPage (local.get $page))) (then (return (i32.const 28))))
    (if (i32.ne (call $selectedPage (local.get $document)) (local.get $page))
      (then (return (i32.const 29))))

    ;; ── resize it ─────────────────────────────────────────────────────────────
    (call $resizePage (local.get $page) (f64.const 200) (f64.const 80))
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 30))))
    (if (i32.eqz (call $near (f64.load (i32.const 528)) (f64.const 200) (f64.const 0.01)))
      (then (return (i32.const 31))))
    (if (i32.eqz (call $near (f64.load (i32.const 536)) (f64.const 80) (f64.const 0.01)))
      (then (return (i32.const 32))))

    ;; ── what is on it ─────────────────────────────────────────────────────────
    ;; Which items a page holds is a geometric question about where they are drawn, not a
    ;; parent-child one: pages are not ancestors of anything. Bit 0 asks for the items lying
    ;; wholly inside rather than those merely touching, and the fixture has content crossing
    ;; the page edge, so the two answers must differ.
    ;;
    ;; A rect is planted well inside the page first, because "fewer contained than overlapping"
    ;; is also what an implementation that found nothing contained would report. Both ends of
    ;; the comparison have to be known non-empty for it to mean anything.
    (local.set $rect (call $createElement (local.get $document) (i32.const 128) (i32.const 8)))
    (call $setAttribute (local.get $rect) (i32.const 8) (i32.const 2) (i32.const 184) (i32.const 6))
    (call $setAttribute (local.get $rect) (i32.const 152) (i32.const 1) (i32.const 192) (i32.const 3))
    (call $setAttribute (local.get $rect) (i32.const 160) (i32.const 1) (i32.const 200) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 96) (i32.const 5) (i32.const 208) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 104) (i32.const 6) (i32.const 208) (i32.const 2))
    (drop (call $appendChild (local.get $root) (local.get $rect)))

    (local.set $overlapping (call $nodesLength (call $pageItems (local.get $page) (i32.const 0))))
    (local.set $contained   (call $nodesLength (call $pageItems (local.get $page) (i32.const 1))))
    (if (i32.le_s (local.get $contained) (i32.const 0))
      (then (return (i32.const 33))))
    (if (i32.ge_s (local.get $contained) (local.get $overlapping))
      (then (return (i32.const 34))))

    ;; Bit 2 counts what is not displayed. A hidden rect is put wholly inside the page, so the
    ;; two answers differ by exactly it.
    (local.set $rect (call $createElement (local.get $document) (i32.const 128) (i32.const 8)))
    (call $setAttribute (local.get $rect) (i32.const 8) (i32.const 2) (i32.const 216) (i32.const 3))
    (call $setAttribute (local.get $rect) (i32.const 152) (i32.const 1) (i32.const 280) (i32.const 3))
    (call $setAttribute (local.get $rect) (i32.const 160) (i32.const 1) (i32.const 200) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 96) (i32.const 5) (i32.const 208) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 104) (i32.const 6) (i32.const 208) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 224) (i32.const 5) (i32.const 232) (i32.const 12))
    (drop (call $appendChild (local.get $root) (local.get $rect)))
    (if (i32.ne (call $nodesLength (call $pageItems (local.get $page) (i32.const 1)))
                (local.get $contained))
      (then (return (i32.const 46))))
    (if (i32.ne (call $nodesLength (call $pageItems (local.get $page) (i32.const 5)))
                (i32.add (local.get $contained) (i32.const 1)))
      (then (return (i32.const 47))))

    ;; Bit 1 measures against the bleed box instead of the page. The bleed is widened to 30 and
    ;; a rect placed in the band outside the page but inside it, so the bit is the only thing
    ;; separating the two counts.
    (call $setAttribute (local.get $page) (i32.const 64) (i32.const 14) (i32.const 264) (i32.const 2))
    (local.set $rect (call $createElement (local.get $document) (i32.const 128) (i32.const 8)))
    (call $setAttribute (local.get $rect) (i32.const 8) (i32.const 2) (i32.const 272) (i32.const 3))
    (call $setAttribute (local.get $rect) (i32.const 152) (i32.const 1) (i32.const 248) (i32.const 3))
    (call $setAttribute (local.get $rect) (i32.const 160) (i32.const 1) (i32.const 200) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 96) (i32.const 5) (i32.const 256) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 104) (i32.const 6) (i32.const 256) (i32.const 2))
    (drop (call $appendChild (local.get $root) (local.get $rect)))
    (if (i32.ge_s (call $nodesLength (call $pageItems (local.get $page) (i32.const 0)))
                  (call $nodesLength (call $pageItems (local.get $page) (i32.const 2))))
      (then (return (i32.const 48))))

    ;; ── fit to the selection ──────────────────────────────────────────────────
    ;; The margin set above is what makes the second argument observable: fitting with margins
    ;; leaves room for them, so the page comes out larger than fitting without.
    (call $selectionAdd (local.get $box))
    (call $fitPage (local.get $document) (i32.const 0))
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 0) (i32.const 512)) (i32.const 1))
      (then (return (i32.const 35))))
    (local.set $plain (f64.load (i32.const 528)))
    ;; The page moved: fitting to a selection elsewhere cannot leave it at 200 wide.
    (if (call $near (local.get $plain) (f64.const 200) (f64.const 0.01))
      (then (return (i32.const 36))))

    (call $fitPage (local.get $document) (i32.const 1))
    (if (i32.ne (call $pageRect (local.get $page) (i32.const 0) (i32.const 600)) (i32.const 1))
      (then (return (i32.const 37))))
    (local.set $margined (f64.load (i32.const 616)))
    (if (f64.le (local.get $margined) (local.get $plain))
      (then (return (i32.const 38))))
    (call $selectionClear)

    ;; ── remove it ─────────────────────────────────────────────────────────────
    ;; Both go. Deleting a page that leaves exactly one behind deletes that one too, when it is
    ;; bare, and fits the document to it (page-manager.cpp PageManager::deletePage()) -- because one page is
    ;; spelled as none. The page a plugin did not ask to remove is removed as well.
    (call $deletePage (local.get $page) (i32.const 0))
    (if (i32.ne (call $pageCount (local.get $document)) (i32.const 0))
      (then (return (i32.const 39))))
    (if (i32.ne (call $nodesLength (call $pages (local.get $document))) (i32.const 0))
      (then (return (i32.const 40))))
    ;; And with them the selection, so the document is back to how it was found.
    (if (i32.ne (call $selectedPage (local.get $document)) (i32.const 0))
      (then (return (i32.const 41))))

    ;; ── deleting a page of work ───────────────────────────────────────────────
    ;; The second argument is the difference between removing a page and removing what is on
    ;; it, which is destructive and therefore the one worth pinning. A rect is put somewhere no
    ;; other page reaches, so it belongs to this page alone and goes with it.
    (local.set $rect (call $createElement (local.get $document) (i32.const 128) (i32.const 8)))
    (call $setAttribute (local.get $rect) (i32.const 8) (i32.const 2) (i32.const 144) (i32.const 6))
    (call $setAttribute (local.get $rect) (i32.const 152) (i32.const 1) (i32.const 168) (i32.const 4))
    (call $setAttribute (local.get $rect) (i32.const 160) (i32.const 1) (i32.const 168) (i32.const 4))
    (call $setAttribute (local.get $rect) (i32.const 96) (i32.const 5) (i32.const 176) (i32.const 2))
    (call $setAttribute (local.get $rect) (i32.const 104) (i32.const 6) (i32.const 176) (i32.const 2))
    (drop (call $appendChild (local.get $root) (local.get $rect)))
    (if (i32.eqz (call $getElementById (local.get $document) (i32.const 144) (i32.const 6)))
      (then (return (i32.const 42))))

    (local.set $doomed (call $newPage (local.get $document)
                                      (f64.const 1000) (f64.const 1000) (f64.const 100) (f64.const 100)))
    (if (i32.eqz (local.get $doomed)) (then (return (i32.const 43))))
    (call $deletePage (local.get $doomed) (i32.const 1))
    (if (i32.ne (call $getElementById (local.get $document) (i32.const 144) (i32.const 6)) (i32.const 0))
      (then (return (i32.const 44))))
    (if (i32.ne (call $pageCount (local.get $document)) (i32.const 0))
      (then (return (i32.const 45))))

    (local.set $marker (call $createElement (local.get $document) (i32.const 0) (i32.const 5)))
    (call $setAttribute (local.get $marker) (i32.const 8) (i32.const 2) (i32.const 16) (i32.const 8))
    (drop (call $appendChild (local.get $root) (local.get $marker)))
    (i32.const 0))
)
