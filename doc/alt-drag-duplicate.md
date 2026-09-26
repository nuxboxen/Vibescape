# Selector copy dragging

Vibescape's first copy-drag experiment builds on Inkscape's existing
`select-duplicate` modifier and object duplication/selection-transform code.
It is enabled by default with Alt. Usage is in the [README](../README.md#copy-by-dragging).
This is independently maintained fork behavior, not a claim about upstream Inkscape.

## Behavior and scope

- Hold the configured duplicate modifier before left mouse-down on an object.
  Crossing the drag threshold makes one copy and starts moving it from the actual
  press point. A click or zero-distance motion creates no copy.
- A hit on a member of the current selection copies that selection. A hit on an
  unselected object copies that object. Selected children inside groups are supported.
- Starting on empty canvas keeps rubberband/touch-path selection; it does not
  copy or force-drag an unrelated selection. Existing click/cycling interactions remain.
- Shift while moving a copy projects its displacement onto the nearest horizontal,
  vertical or 45-degree line. Existing snapping applies along that line. Shift takes
  priority over stock Shift-to-disable-snapping and grid-increment movement during
  a constrained copy; other configured no-snapping modifiers still work.
- Release completes a single **Duplicate and Move** undo event, even if the copy
  has returned to its original position. Undo removes the copy; Redo restores it.
- Escape, right-click cancellation, or leaving the tool during a pending copy
  cancels duplication and movement and restores the earlier selection. After Escape,
  continued mouse movement cannot restart the operation before releasing the button.
- Selection cycling, stamping and other non-modifier canvas keys are suppressed
  during the pending copy. Escape remains available.

Alt is latched at press time. Pressing it during an ordinary drag does not change
that drag into a copy; releasing it during an existing copy drag does not remove
the copy. Shift can change the movement constraint while dragging.

Scaling/rotation handles, Node tool gestures, panel dragging and numeric-field
Alt+Enter are outside this feature. Existing user modifier overrides are respected;
we do not rewrite personal preferences or the shipped keyboard-profile XML files.

## Implementation

`SelectTool` resolves the initial hit and movement threshold, then owns a
`DuplicateDrag` transaction. The transaction reuses `ObjectSet::duplicate(true)`;
it does not introduce a second implementation of SVG copying, ID handling, or
object-reference handling. It retains the old selection until commit/cancellation.

`SelTrans::ungrab(false)` finishes the transform without a separate undo entry.
The copy transaction commits or cancels the resulting document edits together.
Ordinary transforms retain the default undo behavior. Constrained translation
snapping also accepts diagonal directions; existing axis callers remain supported.

The C++ test helper follows `BUILD_SHARED_LIBS`, matching `inkscape_base`. Previously
it was unconditionally shared, which caused duplicate-symbol linker errors with
these tests in our static MinGW build. The tests are linked normally, without
suppressing duplicate-symbol errors.

## Focused automated checks

`testfiles/src/duplicate-drag-test.cpp` checks independent copies, multiple-object
copy/movement with one Undo and Redo, group children, cancellation with an earlier
edit, empty/original selection restoration, returning to the origin, transaction
abandonment, and constraints in all four quadrants.

From the configured MSYS2 UCRT64 environment, with the private dependency prefix
in PATH and PKG_CONFIG_PATH:

```sh
cmake --build build/windows --parallel 12 --target inkscape inkview test_duplicate-drag test_object-set test_svg-affine
ctest --test-dir build/windows -R '^test_(duplicate-drag|object-set|svg-affine)$' --output-on-failure --no-tests=error
```

CTest assigns private profiles under the build directory. Local evidence is kept
under `build/windows/logs/alt-drag-duplicate/` and is not committed.

These tests exercise document transactions and geometry, not real mouse events,
modifier timing, snapping interaction, or canvas rendering. The following checks
remain necessary for hands-on acceptance before merging the feature.

## Validation record — 2026-09-25

The Windows UCRT64 RelWithDebInfo build compiled `inkscape`, `inkview` and the
three focused test executables successfully. CTest passed **3/3 targets, 40/40
individual checks**: 8 copy-drag, 18 existing ObjectSet and 14 existing SVG-affine
checks. Exit status was zero. Existing Windows locale warnings were non-fatal;
ObjectSet's null-input checks and the SVG parser's invalid-input checks also
emitted their expected diagnostics.

This is focused validation, not the full Inkscape suite. GUI acceptance is pending.

## Hands-on acceptance

Use a disposable drawing and the Selector tool. Check both the stock and active
Illustrator profile, including any personal Tool Modifier overrides.

- [ ] Alt-drag a selected rectangle; only the new rectangle moves and remains selected.
- [ ] Alt-drag an unselected object, first with nothing selected, then with a different selection.
- [ ] Copy two selected objects together, a selected group, and selected children inside a group.
- [ ] Alt-click without movement; no copy appears. Alt-drag on empty canvas; no unrelated object moves.
- [ ] Start with Alt+Shift, and also add/release Shift mid-drag; check all eight constrained directions.
- [ ] Repeat with snapping on/off, near object edges and grids, and in outline-drag display mode.
- [ ] Release Alt before the mouse; the copy remains. Press Alt after an ordinary drag starts; it stays an ordinary move.
- [ ] Drag back to the starting position; one Undo removes the overlapping copy, and Redo restores it.
- [ ] Escape while dragging, then keep moving before releasing; the original selection and earlier edits remain unchanged.
- [ ] Cancel with a right-click or tool switch; no copy remains.
- [ ] Copy text, a clipped object, a clone, and an object with live path effects; inspect their references and undo/redo.
- [ ] Check ordinary dragging, normal Shift selection, Alt-click cycling, and transform handles for regressions.
- [ ] Disable/reassign Duplicate selection on drag; verify the configured gesture and previous Alt behavior.