;; SPDX-License-Identifier: GPL-2.0-or-later
;;
;; A port of the shipped `addnodes` extension (share/extensions/addnodes.py), as a plugin.
;;
;; Why this one. addnodes subdivides a path into more segments, and its first line of work is:
;;
;;     for node in self.svg.selection.filter(PathElement):
;;                                              -- addnodes.py AddNodes.effect()
;;
;; A rectangle, a star, a spiral or anything carrying a live path effect is filtered out and
;; silently unchanged. Not slow, not approximate: excluded. The reason is the last line,
;; `node.path = new` -- inkex can only write a `d` back to something that has one, and for a
;; <rect> its `node.path` is not Inkscape's geometry but a re-derivation in Python string
;; formatting, arcs and all (inkex/elements/_polygons.py, RectangleBase.get_path).
;;
;; Here the selection is walked as elements and there is no filter, because there is no need
;; for one. Three things are the host's rather than this plugin's:
;;
;;   * where the points are      -- getTotalLength and getPointAtLength, SVG's own path
;;                                  measurement, already right for arcs and curves. addnodes.py
;;                                  carries inkex's path algebra to do this.
;;   * what the shape IS         -- the length and the points come from Inkscape's resolved
;;                                  geometry, so a rect is the rect Inkscape draws.
;;   * how a path is written     -- the builder, so the `d` is spelled the way Inkscape spells
;;                                  every other `d` in the document.
;;
;; What is left is the extension: sample, and join the samples up.
;;
;; Build:  water demo-addnodes.wat -o demo-addnodes.wasm

(module
  (import "org.inkscape.Document" "documentElement" (func $documentElement (param i32) (result i32)))
  (import "org.inkscape.Document" "createElement"   (func $createElement (param i32 i32 i32) (result i32)))
  (import "org.inkscape.Node"     "appendChild"     (func $appendChild (param i32 i32) (result i32)))
  (import "org.inkscape.Element"  "setAttribute"    (func $setAttribute (param i32 i32 i32 i32 i32)))

  (import "org.inkscape.NodeList" "length" (func $nodesLength (param i32) (result i32)))
  (import "org.inkscape.NodeList" "item"   (func $nodesItem (param i32 i32) (result i32)))

  (import "org.inkscape.Selection" "selectionNodes" (func $selectionNodes (result i32)))
  (import "org.inkscape.Params" "paramInt"       (func $paramInt (param i32 i32) (result i32)))
  (import "org.inkscape.Params" "paramFloat"     (func $paramFloat (param i32 i32) (result f64)))
  (import "org.inkscape.Params" "paramOptionGroup"
          (func $paramOption (param i32 i32 i32 i32) (result i32)))
  (import "org.inkscape.Selection" "convertUnit"
          (func $convertUnit (param f64 i32 i32 i32 i32) (result f64)))

  (import "org.inkscape.SVGGeometryElement" "getTotalLength"
          (func $totalLength (param i32) (result f64)))
  (import "org.inkscape.SVGGeometryElement" "getPointAtLength"
          (func $pointAtLength (param i32 f64 i32) (result i32)))

  (import "org.inkscape.PathBuilder" "pathBegin"  (func $begin (result i32)))
  (import "org.inkscape.PathBuilder" "pathMoveTo" (func $moveTo (param i32 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathLineTo" (func $lineTo (param i32 f64 f64)))
  (import "org.inkscape.PathBuilder" "pathClose"  (func $close (param i32)))
  (import "org.inkscape.PathBuilder" "pathApply"  (func $apply (param i32 i32) (result i32)))

  (memory (export "memory") 1)

  (data (i32.const 0)  "svg:path")
  (data (i32.const 16) "id")
  (data (i32.const 32) "subdivided")
  (data (i32.const 48) "segments")
  (data (i32.const 64) "max")
  (data (i32.const 72) "unit")
  (data (i32.const 80) "method")
  (data (i32.const 88) "bynum")
  (data (i32.const 96) "px")

  (func (export "effect") (param $document i32) (result i32)
    (local $root i32) (local $selected i32) (local $count i32) (local $i i32)
    (local $item i32) (local $path i32) (local $builder i32)
    (local $total f64) (local $maxlen f64) (local $splits i32) (local $step i32)

    (local.set $root (call $documentElement (local.get $document)))
    (local.set $selected (call $selectionNodes))
    (local.set $count (call $nodesLength (local.get $selected)))
    (if (i32.eqz (local.get $count)) (then (return (i32.const 1))))

    ;; addnodes.py opens with viewport_to_unit(f"{max}{unit}"), so the maximum segment length
    ;; arrives as a number and a unit name and has to be converted before it means anything.
    (local.set $maxlen (call $convertUnit (call $paramFloat (i32.const 64) (i32.const 3))
                                          (i32.const 96) (i32.const 2)
                                          (i32.const 96) (i32.const 2)))

    (block $items_done
      (loop $item_next
        (br_if $items_done (i32.ge_s (local.get $i) (local.get $count)))
        (local.set $item (call $nodesItem (local.get $selected) (local.get $i)))

        ;; No filter on element type: rect, star, path, or a shape under a path effect.
        (local.set $total (call $totalLength (local.get $item)))
        (if (f64.gt (local.get $total) (f64.const 0))
          (then
            ;; bynum takes the count as given; otherwise it follows from the longest segment
            ;; allowed, which is what the unit conversion above was for.
            (if (i32.eq (call $paramOption (i32.const 80) (i32.const 6) (i32.const 512) (i32.const 64))
                        (i32.const 5))
              (then (local.set $splits (call $paramInt (i32.const 48) (i32.const 8))))
              (else (local.set $splits
                      (i32.trunc_f64_s (f64.ceil (f64.div (local.get $total) (local.get $maxlen)))))))
            (if (i32.lt_s (local.get $splits) (i32.const 2)) (then (local.set $splits (i32.const 2))))

            (local.set $builder (call $begin))
            (local.set $step (i32.const 0))
            (block $pts_done
              (loop $pt_next
                (br_if $pts_done (i32.gt_s (local.get $step) (local.get $splits)))
                (if (i32.eqz (call $pointAtLength
                                    (local.get $item)
                                    (f64.div (f64.mul (local.get $total)
                                                      (f64.convert_i32_s (local.get $step)))
                                             (f64.convert_i32_s (local.get $splits)))
                                    (i32.const 1024)))
                  (then (return (i32.const 2))))
                (if (i32.eqz (local.get $step))
                  (then (call $moveTo (local.get $builder)
                                      (f64.load (i32.const 1024)) (f64.load (i32.const 1032))))
                  (else (call $lineTo (local.get $builder)
                                      (f64.load (i32.const 1024)) (f64.load (i32.const 1032)))))
                (local.set $step (i32.add (local.get $step) (i32.const 1)))
                (br $pt_next)))
            (call $close (local.get $builder))

            (local.set $path (call $createElement (local.get $document) (i32.const 0) (i32.const 8)))
            (call $setAttribute (local.get $path) (i32.const 16) (i32.const 2)
                                (i32.const 32) (i32.const 10))
            (drop (call $appendChild (local.get $root) (local.get $path)))
            (if (i32.eqz (call $apply (local.get $builder) (local.get $path)))
              (then (return (i32.const 3))))))

        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $item_next)))

    (i32.const 0))
)
