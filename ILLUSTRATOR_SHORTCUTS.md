# Illustrator keyboard shortcuts: Vibescape comparison

Working reference for matching our Windows workflow as closely as practical.
**This document is a mapping reference. Implemented changes are called out separately below.**

## Baseline and completeness

- Reviewed **2026-09-25**, against Vibescape source commit **30780d362** on
  `codex/ui-layout-improvements`.
- Illustrator baseline: the **Windows** column of Adobe's
  [Default keyboard shortcuts](https://helpx.adobe.com/illustrator/using/default-keyboard-shortcuts.html),
  page dated **2026-02-17**. Short command names and key combinations below are
  factual reference data; the Vibescape comparisons and implementation notes are our own.
- Covers **all 390 entries in that published list**, across 22 sections, including
  repeated commands, pointer gestures, and contextual shortcuts. This is **not a
  claim that Adobe's page enumerates every binding in every Illustrator release**:
  Adobe describes it as a selection. Version-specific menu entries can be added
  from **Edit > Keyboard Shortcuts > Illustrator Defaults > Export Text** in
  Illustrator. Record the exact Illustrator version and keyboard layout with that export.
- Keyboard notation assumes Windows and an English/US-style layout. Punctuation,
  dead keys, input methods, and text focus need hands-on checks. No macOS mappings
  are implied. Illustrator's source page has some surprising Windows Alt-symbol
  entries; these are retained as *Adobe-listed*, with verification notes.
- The matching decisions are a source review, **not completed behavioral testing**.
  A matching key does not establish matching selection scope, document behavior,
  or tool semantics.

## Implemented follow-up: Selector copy dragging

The `codex/alt-drag-duplicate` branch enables the existing **Duplicate selection on
drag** Tool Modifier (`select-duplicate`) with Alt as its default, and completes
its selection, cancellation and undo behavior. It was present but disabled in the
baseline above; the initial worksheet missed that modifier.

Alt-drag now copies an object or the selected set. Shift during a copy drag
constrains to horizontal, vertical or 45-degree movement. Alt is sampled at
mouse-down. This applies to Selector translation, not scaling/rotating handles,
node dragging, or panel dragging. See the [usage and scope](README.md#copy-by-dragging)
and [acceptance checks](doc/alt-drag-duplicate.md).

The catalogues below otherwise retain the baseline bindings. No personal
preferences or profile XML files were changed.

## How to use this file

Open **Edit > Keyboard Shortcuts...** in Vibescape. Search for the item name in the
Vibescape column. Named items link to the [binding catalogue](#binding-catalogue),
which includes their stable action IDs and the three shipped profile bindings.
English item names come from the action metadata in this checkout; translated UI
labels may differ. Tool Modifiers are on the separate tab in that same dialog.

| Fit | Meaning |
| --- | --- |
| **Direct** | A named action performs the corresponding operation. Binding and behavior still need review. |
| **Related** | A real named action opens a related workflow or performs only part of the operation. Not a drop-in equivalent. |
| **Gesture** | Tool/widget behavior or a toolbar setting; often not a named item in the shortcut editor. May require a tool-code change. |
| **Gap** | No direct item was identified. The note explains the missing feature or a possible workflow. |

The **Stock** profile is inherited Inkscape behavior. **Legacy** is the
`Adobe Illustrator CS (legacy)` choice from the earlier screenshot. **CC 2024** is
shown in the UI as `Adobe Illustrator CC`. Those are shipped profiles, not a readout
of your personal active settings. Personal overrides may change any of them.

## Suggested matching checklist

- [ ] Agree on a specific Illustrator version/layout, and compare its Defaults export with this list.
- [ ] Start a separate Vibescape Illustrator profile based on the CC 2024 preset; retain existing profiles.
- [ ] Match Selection, Node, Pen, Pencil, Rectangle, Ellipse, Text, Gradient, Dropper and Pages first.
- [ ] Move **Split Apart** off **Ctrl+Alt+Shift+K** deliberately, then assign that key to **Open Keyboard Shortcuts**.
- [ ] Check New / New from Template, Save a Copy, Export, and Paste On Page.
- [ ] Decide the intended **Ctrl+D** behavior: Repeat Transform versus Duplicate and Transform.
- [ ] Treat **Paste in Front / Back** as new stacking-aware commands, not aliases for Paste In Place.
- [ ] Separate Outline toggle from display-mode cycling; do not claim GPU/CPU Preview equivalence.
- [ ] Review Tool Modifiers and text-local shortcuts separately from global action bindings.
- [ ] Test each accepted mapping with canvas focus, text caret, docked panel focus, and floating dialogs.
- [ ] Check punctuation on the actual keyboard; save/export the working profile after hands-on acceptance.

## Conflicts to resolve first

These are concrete reasons to avoid assigning Adobe keys blindly:

| Illustrator key / intention | Current issue to resolve |
| --- | --- |
| Ctrl+Alt+Shift+K / Keyboard Shortcuts | All three shipped profiles bind Split Apart (`app.path-split`) here. Choose a replacement for Split Apart before assigning our new direct action. |
| Ctrl+F / Paste in Front | Stock and inherited profiles use Find and Replace; no exact front-paste action exists. |
| Ctrl+B / Paste Behind | Stock canvas binding toggles scrollbars; Text tool also handles Ctrl+B as Bold. |
| Ctrl+D / Transform Again | Stock is Duplicate. CC XML declares Duplicate and Transform, then Reapply Transforms on the same key: later Reapply wins. Decide whether a duplicate is intended. |
| Ctrl+K / Preferences | Stock is Combine. Both Illustrator profiles use Preferences here; CC adds Ctrl+8 for Combine, while legacy leaves Combine without its stock binding. |
| Ctrl+Shift+A / Deselect | Stock opens Align and Distribute; Illustrator profiles change it. |
| Ctrl+Shift+O / Create Outlines | Stock opens Object Properties; CC profile makes it Object to Path. Legacy does not provide the same override. |
| F8 / New Symbol | Legacy maps this to Create Clone. A linked clone is not a reusable symbol definition. |
| Ctrl+Alt+T / Paragraph panel | Legacy routes this to Text and Font; stock and CC bind Reapply Transforms here. CC adds Ctrl+Alt+Shift+T for Text and Font instead. |
| X / fill-versus-stroke target | Stock selects Shape Builder; CC inherits X as well as adding Shift+M. No global paint-target toggle is provided. |
| F / screen modes | Stock tool shortcut is Quick Preview. CC assigns Focus Mode to F later, taking that binding. |
| Ctrl+Shift+U / Underline text | Text tool uses this for Unicode entry. A global profile edit alone will not establish Illustrator text behavior. |
| Shift / shape constraints | CC remaps Selector transform modifiers, but drawing tools have their own gesture handling. Move one axis only is not Illustrator's full 45-degree constraint behavior. |

## Complete published-reference crosswalk

Each `A###` identifies a worksheet row, including repeated appearances of a
command in Adobe's different sections. These IDs are for discussion, not action IDs.


### Popular shortcuts

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A001 | Undo | <code>Ctrl + Z</code> | [Undo](#v108) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A002 | Redo | <code>Shift + Ctrl + Z</code> | [Redo](#v079) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A003 | Cut | <code>Ctrl + X</code> | [Cut](#v009) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A004 | Copy | <code>Ctrl + C</code> | [Copy](#v007) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A005 | Paste | <code>Ctrl + V</code> | [Paste](#v062) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A006 | Paste in front | <code>Ctrl + F</code> | [Paste In Place](#v063) | Related | Preserves coordinates; does not insert immediately above the selected object. Needs a stacking-aware action. |
| A007 | Paste behind | <code>Ctrl + B</code> | [Paste In Place](#v063) | Related | Preserves coordinates; does not insert immediately below the selected object. Needs a stacking-aware action. |
| A008 | Paste in place | <code>Shift + Ctrl + V</code> | [Paste In Place](#v063) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A009 | Paste onto every artboard | <code>Alt + Shift + Ctrl + V</code> | [Paste On Page](#v064) | Related | Handles the selected page, not every page in one operation. |
| A010 | Spelling | <code>Ctrl + I</code> | [Open Spellcheck](#v056) | Direct | Available only in builds with spelling support. |
| A011 | Color Settings | <code>Shift + Ctrl + K</code> | [Open Preferences](#v055) | Related | Color management lives inside Preferences; there is no direct Color Settings page action. |
| A012 | Keyboard Shortcuts | <code>Alt + Shift + Ctrl + K</code> | [Open Keyboard Shortcuts](#v050) | Direct | New direct action; no shipped binding yet. |
| A013 | Preferences | <code>Ctrl + K</code> | [Open Preferences](#v054) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |

### Work with documents

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A014 | New document | <code>Ctrl + N</code> | [New](#v032) | Direct | Vibescape creates a document directly; Illustrator normally presents its New Document dialog. |
| A015 | New from template | <code>Shift + Ctrl + N</code> | [New from Template](#v033) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A016 | New document, bypass dialog | <code>Alt + Ctrl + N</code> | [New](#v032) | Direct | Same Vibescape action as New; decide which alternate binding to retain. |
| A017 | Open | <code>Ctrl + O</code> | [Open File Dialog](#v047) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A018 | Place | <code>Shift + Ctrl + P</code> | [Import](#v024) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A019 | File Information | <code>Alt + Shift + Ctrl + I</code> | [Open Document Properties](#v045) | Related | Use document metadata controls; not an Illustrator File Information dialog. |
| A020 | Document Setup | <code>Alt + Ctrl + P</code> | [Open Document Properties](#v045) | Direct | Page and document setup; controls and document models differ. |
| A021 | Browse in Bridge | <code>Alt + Ctrl + O</code> | No direct shortcut item identified | Gap | Adobe Bridge integration has no corresponding built-in Vibescape action. |
| A022 | Close document | <code>Ctrl + W</code> | [Close](#v005) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A023 | Save | <code>Ctrl + S</code> | [Save](#v084) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A024 | Save As | <code>Shift + Ctrl + S</code> | [Save As](#v086) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A025 | Save a Copy | <code>Alt + Ctrl + S</code> | [Save a Copy](#v085) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A026 | Save as script files | <code>Ctrl + F12</code> | No direct shortcut item identified | Gap | Adobe lists this command and Ctrl+F12; no corresponding Vibescape script-file format action was identified. |
| A027 | Export for Screens | <code>Alt + Ctrl + E</code> | [Open Export](#v046) | Related | Export panel supports page/selection workflows; output options differ. |
| A028 | Save for Web | <code>Alt + Shift + Ctrl + S</code> | [Open Export](#v046) | Related | Use Export; no identical Save for Web dialog. |
| A029 | Package document | <code>Alt + Shift + Ctrl + P</code> | No direct shortcut item identified | Gap | No single built-in action for collecting the document, linked assets and fonts. |
| A030 | Print | <code>Ctrl + P</code> | [Print](#v070) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A031 | Quit | <code>Ctrl + Q</code> | [Quit](#v074) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |

### Select tools

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A032 | Artboard tool | <code>Shift + O</code> | [Pages Tool](#v060) | Direct | Corresponding tool; drawing/editing details still need hands-on comparison. |
| A033 | Selection tool | <code>V</code> | [Selector Tool](#v091) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A034 | Direct Selection tool | <code>A</code> | [Node Tool](#v035) | Direct | Corresponding tool; drawing/editing details still need hands-on comparison. |
| A035 | Magic Wand tool | <code>Y</code> | [Fill and Stroke](#v018) | Related | Select Same commands can match appearance; no tolerance-based Magic Wand tool. |
| A036 | Lasso tool | <code>Q</code> | [Node Tool](#v035) | Related | Node selection gestures are related; no separate Illustrator Lasso tool action. |
| A037 | Pen tool | <code>P</code> | [Pen Tool](#v065) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A038 | Curvature tool | <code>Shift + ~</code> | [Pen Tool](#v065) | Related | Pen drawing modes are related; not the same click-to-curve tool. |
| A039 | Blob Brush tool | <code>Shift + B</code> | [Calligraphy Tool](#v004) | Related | Creates filled strokes, but merging and brush behavior differ. |
| A040 | Add Anchor Point tool | <code>+ (plus)</code> | [Nodes insert](#v038) | Related | Node-editing command acting on selected segments; does not switch to an insert-on-click tool. |
| A041 | Delete Anchor Point tool | <code>- (minus)</code> | [Nodes delete](#v037) | Related | Deletes selected nodes; does not switch to a dedicated point-deletion tool. |
| A042 | Anchor Point tool | <code>Shift + C</code> | [Nodes to cusp](#v040) | Related | Also see Nodes to smooth; no one-to-one conversion-tool switch. |
| A043 | Type tool | <code>T</code> | [Text Tool](#v104) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A044 | Touch Type tool | <code>Shift + T</code> | [Text Tool](#v104) | Related | Text toolbar offers character adjustments; no separate Touch Type tool. |
| A045 | Line Segment tool | <code>\ (backslash)</code> | [Pen Tool](#v065) | Related | Draw a straight Pen segment; no standalone line tool. |
| A046 | Rectangle tool | <code>M</code> | [Rectangle Tool](#v078) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A047 | Ellipse tool | <code>L</code> | [Ellipse/Arc Tool](#v016) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A048 | Paintbrush tool | <code>B</code> | [Calligraphy Tool](#v004) | Related | Brush and stroke representation differ; also evaluate Pencil. |
| A049 | Pencil tool | <code>N</code> | [Pencil Tool](#v066) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A050 | Shaper tool | <code>Shift + N</code> | No direct shortcut item identified | Gap | No dedicated gesture-recognition Shaper tool was identified. |
| A051 | Rotate tool | <code>R</code> | [Open Transform](#v059) | Related | Rotation tab or Selector rotation handles; no independent Illustrator Rotate tool. |
| A052 | Reflect tool | <code>O</code> | [Open Transform](#v059) | Related | Use scale/flip controls; no independent pivot-and-reflect tool. |
| A053 | Scale tool | <code>S</code> | [Open Transform](#v059) | Related | Scale tab or Selector handles; no independent Scale tool. |
| A054 | Warp tool | <code>Shift + R</code> | [Tweak Tool](#v107) | Related | Tweak push/shrink/grow modes are approximate, not an envelope warp. |
| A055 | Width tool | <code>Shift+W</code> | [Open Live Path Effect](#v051) | Related | PowerStroke path effect is the related workflow; no standalone Width tool action. |
| A056 | Free Transform tool | <code>E</code> | [Selector Tool](#v091) | Related | Selector scale/rotate/skew handles; not Illustrator Free Transform modes. |
| A057 | Shape Builder tool | <code>Shift+M</code> | [Shape Builder Tool](#v093) | Direct | Corresponding tool; drawing/editing details still need hands-on comparison. |
| A058 | Perspective Grid tool | <code>Shift+P</code> | No direct shortcut item identified | Gap | 3D Box is not an Illustrator perspective grid for arbitrary artwork. |
| A059 | Perspective Selection tool | <code>Shift+V</code> | No direct shortcut item identified | Gap | No corresponding perspective-selection tool. |
| A060 | Symbol Sprayer tool | <code>Shift + S</code> | [Spray Tool](#v102) | Related | Sprays objects/copies/clones, not Illustrator symbol sets. |
| A061 | Column Graph tool | <code>J</code> | No direct shortcut item identified | Gap | No built-in data-driven graph tool action. |
| A062 | Mesh tool | <code>U</code> | [Mesh Tool](#v031) | Direct | Corresponding tool; drawing/editing details still need hands-on comparison. |
| A063 | Gradient tool | <code>G</code> | [Gradient Tool](#v021) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A064 | Eyedropper tool | <code>I</code> | [Dropper Tool](#v014) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A065 | Blend tool | <code>W</code> | [Open Live Path Effect](#v051) | Related | Interpolation effects/extensions are related; no equivalent Blend tool. |
| A066 | Live Paint Bucket tool | <code>K</code> | [Paint Bucket Tool](#v061) | Related | Fills visible bounded regions; does not create Illustrator Live Paint groups. |
| A067 | Live Paint Selection tool | <code>Shift + L</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge selection model. |
| A068 | Slice tool | <code>Shift + K</code> | [Open Export](#v046) | Related | Export selection/page; no slice-tool interaction model. |
| A069 | Eraser tool | <code>Shift + E</code> | [Eraser Tool](#v017) | Direct | Corresponding tool; drawing/editing details still need hands-on comparison. |
| A070 | Scissors tool | <code>C</code> | [Nodes break](#v036) | Related | Break selected nodes in Node tool; not cut-anywhere Scissors behavior. |
| A071 | Hand tool | <code>H</code> | [Quick Pan Canvas](#v072) | Related | Temporary hold-to-pan action, not a persistent Hand tool. |
| A072 | Zoom tool | <code>Z</code> | [Zoom Tool](#v117) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A073 | Blob Brush: temporary Smooth tool | <code>Alt (hold)</code> | No direct shortcut item identified | Gap | No corresponding Blob Brush/Smooth-tool pair. |

### View artwork

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A074 | Cycle screen modes | <code>F</code> | [Focus Mode](#v019) | Related | Focus Mode / Fullscreen are separate actions, not a three-state cycle. |
| A075 | Fit printable area | <code>Hand tool: double-click</code> | [Zoom Page](#v116) | Related | Fits the page; does not account for printer imageable margins. |
| A076 | Actual size | <code>Ctrl+1; Zoom tool double-click</code> | [Zoom 1:1](#v112) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A077 | Temporary Hand | <code>Space (hold, outside text editing)</code> | [Quick Pan Canvas](#v072) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A078 | Temporary zoom in | <code>Ctrl+Space (hold)</code> | [Zoom Tool](#v117) | Related | Persistent Zoom tool; Quick Zoom instead magnifies selection, so it is not equivalent. |
| A079 | Temporary zoom out | <code>Ctrl+Alt+Space (hold)</code> | [Zoom Tool](#v117) | Related | Zoom tool has zoom-out gestures; no corresponding hold-to-zoom-out action. |
| A080 | Move zoom marquee | <code>Space while dragging</code> | Zoom tool interaction | Gesture | No separately listed shortcut item; compare behavior before any tool change. |
| A081 | Hide bounding box | <code>Shift + Ctrl + B</code> | No direct shortcut item identified | Gap | No matching dedicated bounding-box visibility action identified. |
| A082 | Hide unselected artwork | <code>Ctrl + Alt + Shift + 3</code> | No direct shortcut item identified | Gap | Invert selection plus Hide selection is a multi-step approximation. |
| A083 | Turn guide horizontal/vertical | <code>Alt+drag guide</code> | Guide properties / canvas guide interaction | Gesture | No equivalent remappable shortcut item. |
| A084 | Release a guide | <code>Ctrl+Shift+double-click guide</code> | No direct shortcut item identified | Gap | Vibescape guides are not retained artwork objects to release. |
| A085 | Show/hide artboards | <code>Ctrl + Shift + H</code> | [Open Document Properties](#v045) | Related | Page border display setting; no direct artboard visibility action. |
| A086 | Rulers | <code>Ctrl + R</code> | [Rulers](#v083) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A087 | Transparency checkerboard | <code>Shift + Ctrl + D</code> | [Open Document Properties](#v045) | Related | Document display/background controls; no matching direct toggle identified. |
| A088 | Fit all artboards | <code>Ctrl+Alt+0</code> | [Zoom Drawing](#v113) | Related | Fits artwork, not the bounding rectangle of every page. Needs a pages-specific action. |
| A089 | Paste on active artboard | <code>Ctrl + Shift+V</code> | [Paste On Page](#v064) | Direct | Page-relative paste; distinct from absolute-coordinate Paste In Place. |
| A090 | Leave Artboard tool | <code>Esc</code> | Pages tool / Selector | Gesture | Tool-specific escape behavior; verify before assigning a global command. |
| A091 | Draw nested artboard | <code>Shift+drag</code> | Pages tool interaction | Gesture | Page geometry is supported; nested-page gesture is not established. |
| A092 | Select several artboards | <code>Ctrl+click artboards</code> | No direct shortcut item identified | Gap | No matching multi-artboard panel selection shortcut identified. |
| A093 | Next document | <code>Ctrl + F6</code> | [Next Window](#v034) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A094 | Previous document | <code>Ctrl + Shift + F6</code> | [Previous Window](#v069) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A095 | Next document group | <code>Ctrl + Alt + F6</code> | No direct shortcut item identified | Gap | Vibescape window/tab navigation has no corresponding Illustrator document-group action. |
| A096 | Previous document group | <code>Ctrl + Alt + Shift + F6</code> | No direct shortcut item identified | Gap | Vibescape window/tab navigation has no corresponding Illustrator document-group action. |
| A097 | Outline / GPU Preview | <code>Ctrl + Y</code> | [Display Mode: Toggle](#v013) | Related | Normal versus last non-normal mode; GPU Preview is not an equivalent renderer. CC preset uses Cycle, which can pass through more modes. |
| A098 | GPU / CPU Preview | <code>Ctrl + E</code> | No direct shortcut item identified | Gap | Do not alias this to outline or display mode; renderer switching is a different feature. |
| A099 | Overprint Preview | <code>Alt + Shift + Ctrl + Y</code> | No direct shortcut item identified | Gap | No corresponding Illustrator print-preview mode. |
| A100 | Pixel Preview | <code>Alt + Ctrl + Y</code> | No direct shortcut item identified | Gap | No matching dedicated pixel-preview action identified. |
| A101 | Leave fullscreen | <code>Esc</code> | [Fullscreen](#v020) | Related | Toggle action; global Escape has selection/tool meanings. |
| A102 | Zoom in | <code>Ctrl + =</code> | [Zoom In](#v114) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A103 | Zoom out | <code>Ctrl + -</code> | [Zoom Out](#v115) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A104 | Show/hide guides | <code>Ctrl + ;</code> | [Show All Guides](#v095) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A105 | Lock/unlock guides | <code>Alt + Ctrl + ;</code> | [Lock All Guides](#v027) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A106 | Make guides | <code>Ctrl + 5</code> | [Objects to Guides](#v043) | Related | Creates guides from object geometry; conversion semantics differ. |
| A107 | Release guides | <code>Alt + Ctrl + 5</code> | No direct shortcut item identified | Gap | There is no artwork object retained behind each guide to restore. |
| A108 | Smart Guides | <code>Ctrl + U</code> | [Snapping](#v101) | Related | Toggles all snapping; inspect alignment/distance/node options separately. |
| A109 | Show/hide perspective grid | <code>Ctrl + Shift + I</code> | No direct shortcut item identified | Gap | No corresponding Illustrator perspective-grid system. |
| A110 | Show/hide grid | <code>Ctrl + '</code> | [Show Grids](#v096) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A111 | Snap to grid | <code>Shift + Ctrl + '</code> | [Snap Grids](#v099) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A112 | Snap to point | <code>Alt + Ctrl + '</code> | [Snap Nodes](#v100) | Related | Node snapping category; review individual node types and global snapping state. |

### Work with selections

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A113 | Previous selection tool | <code>Ctrl + `</code> | [Toggle Selector Tool](#v106) | Related | Toggles Selector and last tool; Illustrator selection-tool history differs. |
| A114 | Direct/Group Selection toggle | <code>Alt (hold)</code> | Tool Modifiers: Select inside groups | Gesture | See Tool Modifiers: Select in groups; not a dedicated Alt-held tool. |
| A115 | Add clicked object to selection | <code>Shift+click</code> | Tool Modifiers: Add to selection | Gesture | Configurable Tool Modifier Add to selection (select-add-to); verify tool-specific handling. |
| A116 | Remove clicked selected object | <code>Shift+click</code> | Selector / Node selection gestures | Gesture | Shift-click toggling and box subtraction are separate interactions. |
| A117 | Magic Wand: subtract objects | <code>Alt+click</code> | No direct shortcut item identified | Gap | No Magic Wand tool. |
| A118 | Lasso: add nodes | <code>Shift+drag</code> | Node tool selection interaction | Gesture | No standalone Illustrator Lasso action; compare Node selection gestures. |
| A119 | Lasso: subtract nodes | <code>Alt+drag</code> | Node tool selection interaction | Gesture | Tool-specific behavior; no equivalent shortcut-editor item. |
| A120 | Lasso crosshair | <code>Caps Lock</code> | No direct shortcut item identified | Gap | No standalone Lasso tool. |
| A121 | Select active-artboard artwork | <code>Ctrl + Alt + A</code> | No direct shortcut item identified | Gap | Select All / All Layers is not restricted to the active page. |
| A122 | Crop marks for selection | <code>Alt, C, O (menu sequence)</code> | No direct shortcut item identified | Gap | An extension/workflow would be needed; Adobe lists a menu-access sequence, not a simultaneous chord. |
| A123 | Select all | <code>Ctrl + A</code> | [Select All in All Layers](#v090) | Direct | Selects visible unlocked layers; compare expected document/layer scope. |
| A124 | Deselect | <code>Shift + Ctrl + A</code> | [Deselect](#v011) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A125 | Reselect | <code>Ctrl + 6</code> | No direct shortcut item identified | Gap | No dedicated restore-previous-selection action identified. |
| A126 | Select next object above | <code>Alt + Ctrl + ]</code> | Selector tool selection traversal | Gesture | Tab / Shift+Tab traverses objects; there is deliberately no named select-next window action. Stacking scope differs. |
| A127 | Select next object below | <code>Alt + Ctrl + [</code> | Selector tool selection traversal | Gesture | Tab / Shift+Tab traverses objects; there is deliberately no named select-previous window action. Stacking scope differs. |
| A128 | Select behind artwork | <code>Ctrl+click twice</code> | Tool Modifiers: Cycle through objects | Gesture | Use selection cycling; no identical Ctrl-double-click shortcut item. |
| A129 | Select behind artwork | <code>Ctrl+click twice</code> | Tool Modifiers: Cycle through objects | Gesture | Use selection cycling; no identical Ctrl-double-click shortcut item. |
| A130 | Group | <code>Ctrl + G</code> | [Group](#v022) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A131 | Ungroup | <code>Shift + Ctrl + G</code> | [Ungroup](#v109) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A132 | Lock selection | <code>Ctrl + 2</code> | [Lock selection](#v028) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A133 | Unlock all | <code>Alt + Ctrl + 2</code> | [Unlock All](#v111) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A134 | Hide selection | <code>Ctrl + 3</code> | [Hide selection](#v023) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A135 | Show all | <code>Alt + Ctrl + 3</code> | [Unhide All](#v110) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A136 | Nudge selection | <code>Arrow keys</code> | Selector key handling | Gesture | Arrow-key movement uses Vibescape step preferences. |
| A137 | Nudge by 10 steps | <code>Shift+Arrow keys</code> | Selector key handling | Gesture | Compare configured movement step and Shift multiplier. |
| A138 | Lock unselected artwork | <code>Ctrl + Alt + Shift + 2</code> | No direct shortcut item identified | Gap | Invert selection plus Lock selection is a multi-step approximation. |
| A139 | Constrain movement angle | <code>Shift while moving</code> | Tool Modifiers: Move one axis only | Gesture | move-confine: stock Ctrl, CC preset Shift. Ordinary movement confines to X/Y. The copy-drag follow-up supports Shift with X/Y/45-degree movement. |
| A140 | Bring Forward | <code>Ctrl + ]</code> | [Raise](#v075) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A141 | Bring to Front | <code>Shift + Ctrl + ]</code> | [Raise to Top](#v076) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A142 | Send Backward | <code>Ctrl + [</code> | [Lower](#v029) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A143 | Send to Back | <code>Shift + Ctrl + [</code> | [Lower to Bottom](#v030) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A144 | Marquee: fully enclosed objects only | <code>E while dragging marquee</code> | Selector selection behavior | Gesture | Check selection preferences; no E-held equivalent item identified. |

### Draw

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A145 | Constrain drawing proportions / angle | <code>Shift+drag</code> | Shape/Pen tool key handling | Gesture | Usually tool-local Ctrl constraints; CC transform modifiers do not automatically remap every drawing tool. |
| A146 | Move shape during drawing | <code>Space+drag</code> | Shape tool key handling | Gesture | No general remappable draw-and-move action identified. |
| A147 | Draw from center | <code>Alt+drag</code> | Shape tool key handling | Gesture | Compare each shape tool; transform off-center modifier does not establish this gesture. |
| A148 | Change sides / points / turns while drawing | <code>Up/Down during drag</code> | Star/Polygon and Spiral toolbars | Gesture | Named controls exist; no universal arrow-key item in the shortcut editor. |
| A149 | Hold star inner radius | <code>Ctrl during drag</code> | Star/Polygon tool handles | Gesture | Compare handle modifiers; no matching configurable action. |
| A150 | Keep star edges straight | <code>Alt+drag</code> | Star/Polygon tool handles | Gesture | Different star construction; no exact mapped action. |
| A151 | Open/close arc while drawing | <code>C during drag</code> | Ellipse/Arc toolbar | Gesture | Arc/segment modes are toolbar controls. |
| A152 | Flip arc about reference | <code>F during drag</code> | Ellipse/Arc handles | Gesture | No direct F-held flip action identified. |
| A153 | Change spiral turns during drawing | <code>Alt during drag</code> | Spiral toolbar / handles | Gesture | Use revolution controls; compare gestures manually. |
| A154 | Change spiral decay | <code>Ctrl during drag</code> | Spiral toolbar / handles | Gesture | Divergence control is related; gestures differ. |
| A155 | Grid: change horizontal / concentric lines | <code>Up/Down during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A156 | Grid: change vertical / radial lines | <code>Left/Right during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A157 | Grid: reduce horizontal / radial skew | <code>F during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A158 | Grid: increase horizontal / radial skew | <code>V during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A159 | Grid: reduce vertical / concentric skew | <code>X during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A160 | Grid: increase vertical / concentric skew | <code>C during drag</code> | No direct shortcut item identified | Gap | No equivalent interactive rectangular/polar-grid drawing tool. |
| A161 | Increase brush size | <code>]</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |
| A162 | Decrease brush size | <code>[</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |
| A163 | Constrain Blob Brush path | <code>Shift (hold)</code> | No direct shortcut item identified | Gap | No Blob Brush tool; evaluate Calligraphy/Pencil separately. |
| A164 | Cycle Draw Normal/Behind/Inside | <code>Shift + D</code> | No direct shortcut item identified | Gap | No corresponding three-state drawing-mode action. |
| A165 | Join paths | <code>Ctrl+J (paths selected)</code> | [Nodes join](#v039) | Related | Node tool: select endpoints first; separate segments-join command draws a connecting segment. |
| A166 | Average anchor positions | <code>Ctrl+Alt+J</code> | [Open Align and Distribute](#v044) | Related | Node alignment can produce related results; no dedicated Average dialog. |
| A167 | Join as corner / smooth | <code>Ctrl+Alt+Shift+J</code> | [Nodes join](#v039) | Related | Then Nodes to cusp/smooth. Multi-step workflow. |
| A168 | Make compound path | <code>Ctrl + 8</code> | [Combine](#v006) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A169 | Release compound path | <code>Alt + Shift + Ctrl + 8</code> | [Break Apart](#v003) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A170 | Edit pattern | <code>Shift + Ctrl + F8</code> | [Open Fill and Stroke](#v048) | Related | Pattern controls/Node handles differ from Illustrator pattern-editing mode. |
| A171 | Perspective Grid tool | <code>Shift + P</code> | No direct shortcut item identified | Gap | 3D Box is not an Illustrator perspective grid for arbitrary artwork. |
| A172 | Perspective Selection tool | <code>Shift + V</code> | No direct shortcut item identified | Gap | No corresponding perspective-selection tool. |
| A173 | Show/hide perspective grid | <code>Ctrl + Shift + I</code> | No direct shortcut item identified | Gap | No corresponding Illustrator perspective-grid system. |
| A174 | Perspective: move perpendicular to plane | <code>5, then drag</code> | No direct shortcut item identified | Gap | No matching perspective-selection system. |
| A175 | Perspective: select active plane | <code>1 / 2 / 3 / 4 in Perspective Selection</code> | No direct shortcut item identified | Gap | No matching perspective-selection system. |
| A176 | Perspective: duplicate artwork | <code>Ctrl+Alt+drag</code> | No direct shortcut item identified | Gap | No matching perspective-selection system. |
| A177 | Perspective: repeat transform | <code>Ctrl + D</code> | [Reapply Transforms](#v077) | Related | Repeats ordinary transforms; does not add perspective-plane semantics. |
| A178 | Cycle Draw Normal/Behind/Inside | <code>Shift + D</code> | No direct shortcut item identified | Gap | No corresponding three-state drawing-mode action. |

### Edit shapes

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A179 | Pen: convert anchor temporarily | <code>Alt (hold)</code> | Pen tool handle interaction | Gesture | No corresponding configurable temporary tool switch. |
| A180 | Toggle insert/delete anchor tool | <code>Alt (hold)</code> | Node tool insert/delete commands | Gesture | Separate node actions, not an Alt-held switch. |
| A181 | Scissors: temporary insert anchor | <code>Alt (hold)</code> | No direct shortcut item identified | Gap | No separate Scissors tool. |
| A182 | Pencil: temporary Smooth tool | <code>Alt (hold; Pencil option enabled)</code> | [Simplify path](#v098) | Related | Simplify is a command, not brush smoothing; Adobe requires its Pencil option enabled. |
| A183 | Reposition current Pen anchor | <code>Space+drag</code> | Pen tool key handling | Gesture | No configurable shortcut-editor item; compare interactive behavior. |
| A184 | Knife: straight cut | <code>Alt+drag</code> | [Cut Path](#v010) | Related | Prepare a cutting path; not an interactive Knife gesture. |
| A185 | Knife: constrained cut | <code>Alt+Shift+drag</code> | [Cut Path](#v010) | Related | Constrain a cutting path first; not a remappable Knife modifier. |
| A186 | Pathfinder: live compound shape | <code>Alt+click shape-mode button</code> | [Open Live Path Effect](#v051) | Related | Boolean path effects are related; ordinary Union/Intersect are destructive. |
| A187 | Shape Builder: erase region | <code>Alt+click region</code> | [Shape Builder: Delete](#v094) | Related | Delete mode is a named action. Tool Modifiers &gt; Switch mode (bool-shift) temporarily changes mode; stock modifier is Shift, not Illustrator Alt. |
| A188 | Shape Builder tool | <code>Shift + M</code> | [Shape Builder Tool](#v093) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A189 | Shape Builder: rectangular merge marquee | <code>Shift+drag</code> | Shape Builder interaction | Gesture | No equivalent configurable marquee-mode action identified. |
| A190 | Raise symbol intensity | <code>Shift+}</code> | Spray toolbar | Gesture | Spray amount is related; no symbol-set intensity model. |
| A191 | Lower symbol intensity | <code>Shift+{</code> | Spray toolbar | Gesture | Spray amount is related; no symbol-set intensity model. |
| A192 | Make blend | <code>Alt + Ctrl + B</code> | [Open Live Path Effect](#v051) | Related | Interpolation effects/extensions need their own evaluation; no direct Make Blend action. |
| A193 | Finish/release blend operation | <code>Alt + Shift + Ctrl + B</code> | No direct shortcut item identified | Gap | No equivalent Illustrator blend object/action; Adobe describes this row as finishing objects in a blend. |
| A194 | Envelope from warp | <code>Alt + Ctrl + Shift + W</code> | [Open Live Path Effect](#v051) | Related | Bend/envelope effects are related workflows, not a single matched action. |
| A195 | Envelope from mesh | <code>Alt + Ctrl + M</code> | [Open Live Path Effect](#v051) | Related | Mesh gradient is not a mesh deformation. Do not bind this to Mesh Tool. |
| A196 | Envelope from top object | <code>Alt + Ctrl + C</code> | [Open Live Path Effect](#v051) | Related | No direct top-object-envelope action identified. |
| A197 | Choose fill or stroke target | <code>X</code> | Fill and Stroke tabs / selected-style controls | Gesture | No global X target-toggle action identified. |
| A198 | Default fill and stroke | <code>D</code> | No direct shortcut item identified | Gap | No direct D reset-to-black/white action identified; do not substitute a tool-default reset. |
| A199 | Swap fill and stroke | <code>Shift + X</code> | [Swap fill and stroke](#v103) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A200 | Apply gradient fill | <code>&gt;</code> | [Open Fill and Stroke](#v048) | Related | Choose a gradient paint mode; opening Gradient Tool alone does not apply one. |
| A201 | Show/hide gradient annotator | <code>Alt + Ctrl + G</code> | No direct shortcut item identified | Gap | No dedicated gradient-handle visibility action identified. |
| A202 | Apply solid fill | <code>&lt;</code> | [Open Fill and Stroke](#v048) | Related | Choose flat color paint mode; no direct shortcut item. |
| A203 | Remove active fill/stroke | <code>/</code> | [Open Fill and Stroke](#v048) | Related | Choose no paint in the relevant tab; target needs to be explicit. |
| A204 | Eyedropper: sample rendered color | <code>Shift+Eyedropper</code> | Dropper tool interaction | Gesture | Compare fill/stroke and opacity modifiers. Adobe Shift is not proof of the same behavior here. |
| A205 | Eyedropper: append sampled style | <code>Alt+Shift+click with Eyedropper</code> | Dropper tool / Paste Style | Gesture | No Illustrator appearance-stack append action. |
| A206 | Add another fill | <code>Ctrl+/</code> | No direct shortcut item identified | Gap | SVG objects do not have an Illustrator-style stack of independent fills. |
| A207 | Add another stroke | <code>Ctrl+Alt+/</code> | No direct shortcut item identified | Gap | SVG objects do not have an Illustrator-style stack of independent strokes. |
| A208 | Reset gradient to black/white | <code>Ctrl+click gradient-fill button</code> | Fill and Stroke gradient editor | Gesture | No dedicated gradient-reset shortcut item identified. |
| A209 | Make Live Paint group | <code>Alt + Ctrl + X</code> | No direct shortcut item identified | Gap | Paint Bucket generates filled paths, not a Live Paint group. |
| A210 | Decrease brush size | <code>[</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |
| A211 | Increase brush size | <code>]</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |

### Work with Live Paint groups

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A212 | Bucket: sample fill/stroke | <code>Alt+click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A213 | Bucket: sample rendered color | <code>Alt+Shift+click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A214 | Bucket: alternate fill/stroke options | <code>Shift (hold) in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A215 | Bucket: fill across unpainted edges | <code>Double-click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A216 | Bucket: fill all matching faces/edges | <code>Triple-click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A217 | Live Paint Selection: sample style | <code>Alt+click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A218 | Live Paint Selection: sample rendered color | <code>Alt+Shift+click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A219 | Live Paint Selection: toggle selection | <code>Shift+click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A220 | Live Paint Selection: connected matching regions | <code>Double-click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |
| A221 | Live Paint Selection: all matching regions | <code>Triple-click in the named Live Paint tool</code> | No direct shortcut item identified | Gap | No Illustrator-style Live Paint face/edge groups; Paint Bucket is only an approximate starting point. |

### Work with objects

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A222 | Set transform pivot and open options | <code>Alt+click with transform tool</code> | [Open Transform](#v059) | Related | Selector pivot handles plus Transform panel; no single Alt-click equivalent. |
| A223 | Duplicate while transforming | <code>Alt+drag with transform tool</code> | Tool Modifiers: Duplicate selection on drag | Gesture | `select-duplicate` covers Selector translation in our follow-up. Scaling/rotating handles remain separate work. [Duplicate and Transform](#v015) reapplies the prior transform. |
| A224 | Transform pattern independently | <code>Backquote+drag</code> | Pattern handles / Fill and Stroke | Gesture | No matching backquote-held transform shortcut item. |
| A225 | Transform Again | <code>Ctrl + D</code> | [Reapply Transforms](#v077) | Direct | CC preset assigns Ctrl+D twice; its later Reapply Transforms binding wins over Duplicate and Transform. |
| A226 | Repeat Pathfinder operation | <code>Ctrl + 4</code> | No direct shortcut item identified | Gap | Previous Extension is not the last Boolean/Pathfinder operation. |
| A227 | Move dialog | <code>Shift + Ctrl + M</code> | [Open Transform](#v059) | Related | Use Move tab; it does not directly select that tab. |
| A228 | Transform Each | <code>Alt + Shift + Ctrl + D</code> | [Open Transform](#v059) | Related | Use per-object controls where applicable; not the same transform dialog. |
| A229 | Make clipping mask | <code>Ctrl + 7</code> | [Set Object Clipping](#v092) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A230 | Release clipping mask | <code>Alt + Ctrl + 7</code> | [Release Object Clipping](#v080) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A231 | Choose fill or stroke target | <code>X</code> | Fill and Stroke tabs / selected-style controls | Gesture | No global X target-toggle action identified. |
| A232 | Default fill and stroke | <code>D</code> | No direct shortcut item identified | Gap | No direct D reset-to-black/white action identified; do not substitute a tool-default reset. |
| A233 | Swap fill and stroke | <code>Shift + X</code> | [Swap fill and stroke](#v103) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A234 | Apply gradient fill | <code>&gt;</code> | [Open Fill and Stroke](#v048) | Related | Choose a gradient paint mode; opening Gradient Tool alone does not apply one. |
| A235 | Show/hide gradient annotator | <code>Alt + Ctrl + G</code> | No direct shortcut item identified | Gap | No dedicated gradient-handle visibility action identified. |
| A236 | Apply solid fill | <code>&lt;</code> | [Open Fill and Stroke](#v048) | Related | Choose flat color paint mode; no direct shortcut item. |
| A237 | Remove active fill/stroke | <code>/</code> | [Open Fill and Stroke](#v048) | Related | Choose no paint in the relevant tab; target needs to be explicit. |
| A238 | Eyedropper: sample rendered color | <code>Shift+Eyedropper</code> | Dropper tool interaction | Gesture | Compare fill/stroke and opacity modifiers. Adobe Shift is not proof of the same behavior here. |
| A239 | Eyedropper: append sampled style | <code>Alt+Shift+click with Eyedropper</code> | Dropper tool / Paste Style | Gesture | No Illustrator appearance-stack append action. |
| A240 | Add another fill | <code>Ctrl+/</code> | No direct shortcut item identified | Gap | SVG objects do not have an Illustrator-style stack of independent fills. |
| A241 | Add another stroke | <code>Ctrl+Alt+/</code> | No direct shortcut item identified | Gap | SVG objects do not have an Illustrator-style stack of independent strokes. |
| A242 | Reset gradient to black/white | <code>Ctrl+click gradient-fill button</code> | Fill and Stroke gradient editor | Gesture | No dedicated gradient-reset shortcut item identified. |
| A243 | Make Live Paint group | <code>Alt + Ctrl + X</code> | No direct shortcut item identified | Gap | Paint Bucket generates filled paths, not a Live Paint group. |
| A244 | Decrease brush size | <code>[</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |
| A245 | Increase brush size | <code>]</code> | Calligraphy toolbar / tool keys | Gesture | Related brush width control; no Blob/Bristle Brush equivalence. |

### Create variable width points

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A246 | Select several width points | <code>Shift+click</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |
| A247 | Set asymmetric widths | <code>Alt+drag</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |
| A248 | Duplicate width point | <code>Alt+drag width point</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |
| A249 | Move several width points | <code>Shift+drag</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |
| A250 | Delete width point | <code>Delete</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |
| A251 | Deselect width point | <code>Esc</code> | PowerStroke path-effect handles | Gesture | Related variable-width workflow; no corresponding shortcut-editor item. Handle behavior needs comparison. |

### Work with type

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A252 | Bold text | <code>Ctrl+Shift+B</code> | Text tool key handling | Gesture | Vibescape handles Ctrl+B directly in Text tool; not a named shortcut-editor action. |
| A253 | Italic text | <code>Ctrl+Shift+I</code> | Text tool key handling | Gesture | Vibescape handles Ctrl+I directly in Text tool; not a named shortcut-editor action. |
| A254 | Underline text | <code>Shift + Ctrl + U</code> | Text toolbar / Text and Font | Gesture | Do not assign Ctrl+Shift+U blindly: Text tool uses Ctrl+U / Ctrl+Shift+U for Unicode entry. |
| A255 | Move text cursor by character | <code>Left/Right</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A256 | Move text cursor by line | <code>Up/Down</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A257 | Move text cursor by word | <code>Ctrl+Left/Right</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A258 | Move text cursor by paragraph | <code>Ctrl+Up/Down</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A259 | Select text by word | <code>Ctrl+Shift+Left/Right</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A260 | Select text by paragraph | <code>Ctrl+Shift+Up/Down</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A261 | Extend text selection | <code>Shift+click</code> | Text tool key handling | Gesture | Tool-local editing; verify behavior with a text caret, not object selection. |
| A262 | Paragraph left / right / center | <code>Ctrl+Shift+L / R / C</code> | Text toolbar alignment | Gesture | No corresponding named alignment-shortcut item; these are text alignment, not object alignment. |
| A263 | Justify paragraph, last line left | <code>Ctrl + Shift + J</code> | Text toolbar alignment | Gesture | Not the object Align and Distribute action. |
| A264 | Justify every line | <code>Shift + Ctrl + F</code> | Text toolbar alignment | Gesture | No matching dedicated action identified. |
| A265 | Toggle line composer | <code>Alt + Shift + Ctrl + C</code> | No direct shortcut item identified | Gap | No Illustrator single-line/every-line composer switch. |
| A266 | Soft line break | <code>Shift+Enter</code> | Text tool key handling | Gesture | Compare explicit line break versus paragraph behavior. |
| A267 | Highlight kerning | <code>Ctrl + Alt + K</code> | No direct shortcut item identified | Gap | Text spacing controls exist; no equivalent highlighting command. |
| A268 | Reset text horizontal scale | <code>Ctrl + Shift + X</code> | Text toolbar / text style controls | Gesture | No direct 100% horizontal-text-scale action identified. |
| A269 | Change font size | <code>Ctrl+Shift+, / .</code> | Text toolbar font-size field | Gesture | No named increase/decrease-font-size action identified. Alt+, / Alt+. changes letter spacing in this source, not font size. |
| A270 | Change font size by large step | <code>Ctrl+Alt+Shift+, / .</code> | Text toolbar font-size field | Gesture | A matching size-step shortcut would need a new action/tool binding; do not reuse spacing shortcuts. |
| A271 | Adjust leading | <code>Alt+Up/Down; vertical text: Alt+Left/Right</code> | Text toolbar line spacing | Gesture | Ctrl+Alt+, / Ctrl+Alt+. changes line spacing in Text tool; Alt+Up/Down changes vertical kerning. Compare units and text orientation. |
| A272 | Focus tracking field | <code>Alt + Ctrl + K</code> | Text toolbar spacing controls | Gesture | No named tracking-focus action identified. |
| A273 | Reset tracking / kerning | <code>Ctrl + Alt + Q</code> | [Remove Manual Kerns](#v081) | Related | Removes manual kerning and rotations; broader than resetting a single tracking value. |
| A274 | Adjust tracking / kerning | <code>Alt+Left/Right; vertical text: Alt+Up/Down</code> | Text tool key handling | Gesture | Alt+Left/Right adjusts kerning; coordinate units and selection scope differ. |
| A275 | Adjust tracking / kerning by 5 steps | <code>Ctrl+Alt+Left/Right; vertical text: Ctrl+Alt+Up/Down</code> | Text tool key handling | Gesture | Vibescape uses Shift for a larger kerning step; Illustrator Ctrl multiplier differs. |
| A276 | Adjust baseline shift | <code>Alt+Shift+Up/Down; vertical text: Alt+Shift+Left/Right</code> | Text tool key handling | Gesture | Vibescape Alt+Shift+Up/Down uses a larger vertical kerning step; compare text semantics. |
| A277 | Toggle horizontal / vertical text tool | <code>Shift (hold while choosing text tool)</code> | Text toolbar writing direction | Gesture | No separate family of Type tool switches. |
| A278 | Switch area/path text mode | <code>Alt (hold while choosing text tool)</code> | [Put on Path](#v071) | Related | Text on path and Flow into Frame are separate commands, not an Alt-held switch. |
| A279 | Create text outlines | <code>Shift + Ctrl + O</code> | [Object to Path](#v042) | Direct | Converts text to paths; retain an editable copy if needed. |
| A280 | Character panel | <code>Ctrl + T</code> | [Open Text](#v058) | Related | Text and Font combines related controls; not the same panel layout. |
| A281 | Paragraph panel | <code>Alt + Ctrl + T</code> | [Open Text](#v058) | Related | Paragraph/alignment controls also live on the Text toolbar. |
| A282 | Tabs panel | <code>Shift + Ctrl + T</code> | No direct shortcut item identified | Gap | No corresponding dedicated Illustrator tab-stop panel identified. |
| A283 | OpenType panel | <code>Alt + Shift + Ctrl + T</code> | [Open Text](#v058) | Related | OpenType/font-feature controls are part of Text and Font; not a dedicated page action. |
| A284 | Insert bullet | <code>Alt + 8</code> | Text tool Unicode input | Gesture | Ctrl+U, 2022, Enter inserts U+2022. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A285 | Insert copyright sign | <code>Alt + G</code> | Text tool Unicode input | Gesture | Ctrl+U, 00A9, Enter inserts U+00A9. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A286 | Insert ellipsis | <code>Alt + ;</code> | Text tool Unicode input | Gesture | Ctrl+U, 2026, Enter inserts U+2026. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A287 | Insert paragraph sign | <code>Alt + 7</code> | Text tool Unicode input | Gesture | Ctrl+U, 00B6, Enter inserts U+00B6. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A288 | Insert section sign | <code>Alt+6</code> | Text tool Unicode input | Gesture | Ctrl+U, 00A7, Enter inserts U+00A7. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A289 | Insert trademark sign | <code>Alt + 2</code> | Text tool Unicode input | Gesture | Ctrl+U, 2122, Enter inserts U+2122. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A290 | Insert registered trademark sign | <code>Alt + R</code> | Text tool Unicode input | Gesture | Ctrl+U, 00AE, Enter inserts U+00AE. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A291 | Superscript | <code>Shift + Ctrl + =</code> | Text toolbar superscript | Gesture | No corresponding shortcut-editor action identified. |
| A292 | Subscript | <code>Alt + Shift + Ctrl + =</code> | Text toolbar subscript | Gesture | No corresponding shortcut-editor action identified. |
| A293 | Insert em dash | <code>Alt + Shift + -</code> | Text tool Unicode input | Gesture | Ctrl+U, 2014, Enter inserts U+2014. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A294 | Insert en dash | <code>Alt + -</code> | Text tool Unicode input | Gesture | Ctrl+U, 2013, Enter inserts U+2013. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A295 | Insert discretionary hyphen | <code>Shift + Ctrl + -</code> | Text tool Unicode input | Gesture | Ctrl+U, 00AD, Enter inserts U+00AD. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A296 | Automatic hyphenation toggle | <code>Alt + Shift + Ctrl + H</code> | No direct shortcut item identified | Gap | No equivalent Illustrator hyphenation-switch action identified. |
| A297 | Insert left double quotation mark | <code>Alt + [</code> | Text tool Unicode input | Gesture | Ctrl+U, 201C, Enter inserts U+201C. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A298 | Insert right double quotation mark | <code>Alt + Shift + [</code> | Text tool Unicode input | Gesture | Ctrl+U, 201D, Enter inserts U+201D. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A299 | Insert left single quotation mark | <code>Alt + ]</code> | Text tool Unicode input | Gesture | Ctrl+U, 2018, Enter inserts U+2018. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A300 | Insert right single quotation mark | <code>Alt + Shift + ]</code> | Text tool Unicode input | Gesture | Ctrl+U, 2019, Enter inserts U+2019. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A301 | Insert em space | <code>Shift + Ctrl + M</code> | Text tool Unicode input | Gesture | Ctrl+U, 2003, Enter inserts U+2003. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A302 | Insert en space | <code>Shift + Ctrl + N</code> | Text tool Unicode input | Gesture | Ctrl+U, 2002, Enter inserts U+2002. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A303 | Insert thin space | <code>Alt + Shift + Ctrl + M</code> | Text tool Unicode input | Gesture | Ctrl+U, 2009, Enter inserts U+2009. Adobe-listed Windows Alt combinations need checking against the installed Illustrator version/layout; do not assume Windows Alt codes. |
| A304 | Show hidden text characters | <code>Alt + Ctrl + I</code> | No direct shortcut item identified | Gap | No matching dedicated text-mark visibility action identified. |

### Use panels

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A305 | New item with options | <code>Alt+click New</code> | Individual panel New controls | Gesture | Panel-specific behavior; no shared options modifier action. |
| A306 | Change measurement units | <code>Alt + Shift + Ctrl + U</code> | Document Properties / individual unit selectors | Gesture | No matching global cycle-units action identified. |
| A307 | Delete without confirmation | <code>Alt+click Delete</code> | Individual panel Delete controls | Gesture | No shared bypass-confirmation action. |
| A308 | Apply numeric value and retain focus | <code>Shift+Enter</code> | Panel numeric fields | Gesture | Widget behavior rather than a shortcut-editor action. |
| A309 | Select contiguous panel items | <code>Shift+click</code> | Panel list selection | Gesture | Depends on whether that panel supports multi-selection. |
| A310 | Select separate panel items | <code>Ctrl+click</code> | Panel list selection | Gesture | Depends on whether that panel supports multi-selection. |
| A311 | Show/hide every panel | <code>Tab</code> | [Focus Mode](#v019) | Related | Focus Mode is the closest overall UI toggle; verify which bars/dialogs it hides. |
| A312 | Show/hide panels except tool/control bars | <code>Shift+Tab</code> | [Toggle all dialogs](#v105) | Related | Toggles dialogs; does not reproduce every Illustrator workspace panel. |
| A313 | Numeric field: fractional step | <code>Ctrl+Up/Down</code> | Panel numeric fields | Gesture | Unit/step behavior is widget-specific; no universal configurable action. |
| A314 | Numeric field: large step | <code>Shift+Up/Down</code> | Panel numeric fields | Gesture | Check the individual field step; not a global shortcut-editor item. |

### Actions panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A315 | Expand/collapse action-set hierarchy | <code>Alt+click disclosure</code> | No direct shortcut item identified | Gap | No Illustrator-style Actions recording/playback panel; Command Palette is a different feature. |
| A316 | Action-set options | <code>Double-click folder</code> | No direct shortcut item identified | Gap | No Illustrator-style Actions recording/playback panel; Command Palette is a different feature. |
| A317 | Play one recorded command | <code>Ctrl+click Play</code> | No direct shortcut item identified | Gap | No Illustrator-style Actions recording/playback panel; Command Palette is a different feature. |
| A318 | Record action without dialog | <code>Alt+click New Action</code> | No direct shortcut item identified | Gap | No Illustrator-style Actions recording/playback panel; Command Palette is a different feature. |

### Brushes panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A319 | Brush options | <code>Double-click brush</code> | No direct shortcut item identified | Gap | Calligraphy presets are not an Illustrator Brushes panel. |
| A320 | Duplicate brush preset | <code>Drag brush to New Brush</code> | No direct shortcut item identified | Gap | No equivalent Brushes-panel drag target. |

### Character and Paragraph panels

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A321 | Character panel | <code>Ctrl + T</code> | [Open Text](#v058) | Related | Text and Font combines related controls; not the same panel layout. |
| A322 | Paragraph panel | <code>Alt + Ctrl + T</code> | [Open Text](#v058) | Related | Paragraph/alignment controls also live on the Text toolbar. |
| A323 | Numeric field: small step | <code>Up/Down</code> | Panel numeric fields | Gesture | Standard field interaction; compare the configured increment. |
| A324 | Numeric field: large step | <code>Shift+Up/Down</code> | Panel numeric fields | Gesture | Check the individual field step; not a global shortcut-editor item. |
| A325 | Numeric field: fractional step | <code>Ctrl+Up/Down</code> | Panel numeric fields | Gesture | Unit/step behavior is widget-specific; no universal configurable action. |
| A326 | Focus font family field | <code>Ctrl + Alt + Shift + F</code> | Text toolbar / Text and Font | Gesture | Focus First Widget is generic and does not guarantee the font-family field. |

### Color panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A327 | Complement of active paint | <code>Ctrl+click color bar</code> | Fill and Stroke color controls | Gesture | No corresponding Color-panel gesture action; choose the intended fill/stroke explicitly. |
| A328 | Change inactive paint | <code>Alt+click color bar</code> | Fill and Stroke color controls | Gesture | No corresponding Color-panel gesture action; choose the intended fill/stroke explicitly. |
| A329 | Complement of inactive paint | <code>Ctrl+Alt+click color bar</code> | Fill and Stroke color controls | Gesture | No corresponding Color-panel gesture action; choose the intended fill/stroke explicitly. |
| A330 | Invert active paint | <code>Ctrl+Shift+click color bar</code> | Fill and Stroke color controls | Gesture | No corresponding Color-panel gesture action; choose the intended fill/stroke explicitly. |
| A331 | Invert inactive paint | <code>Ctrl+Alt+Shift+click color bar</code> | Fill and Stroke color controls | Gesture | No corresponding Color-panel gesture action; choose the intended fill/stroke explicitly. |
| A332 | Color Guide panel | <code>Shift + F3</code> | No direct shortcut item identified | Gap | No one-to-one Illustrator color-harmony guide panel. |
| A333 | Change color model | <code>Shift+click color bar</code> | Fill and Stroke color model selector | Gesture | No matching Shift-click action item. |
| A334 | Move color components together | <code>Shift+drag slider</code> | Fill and Stroke color sliders | Gesture | Slider-specific behavior needs comparison. |
| A335 | RGB percent / 0-255 display | <code>Double-click beside number</code> | Fill and Stroke color fields | Gesture | No matching display-toggle action identified. |

### Gradient panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A336 | Duplicate gradient stop | <code>Alt+drag stop</code> | Gradient tool / Fill and Stroke gradient editor | Gesture | Stop editing exists; compare exact pointer gesture. |
| A337 | Swap gradient stops | <code>Alt+drag one stop onto another</code> | Gradient tool / Fill and Stroke gradient editor | Gesture | No corresponding remappable stop-swap gesture. |
| A338 | Swatch to selected gradient stop | <code>Alt+click swatch</code> | Gradient tool + Swatches / palette | Gesture | Choose a stop, then apply color; pointer modifier differs. |
| A339 | Reset gradient to black/white | <code>Ctrl+click gradient-fill button</code> | Fill and Stroke gradient editor | Gesture | No dedicated gradient-reset shortcut item identified. |
| A340 | Show/hide gradient annotator | <code>Ctrl + Alt + G</code> | No direct shortcut item identified | Gap | No dedicated gradient-handle visibility action identified. |
| A341 | Change gradient angle and endpoint together | <code>Alt+drag gradient endpoint</code> | Gradient tool handles | Gesture | Tool-local behavior; compare endpoint modifiers. |
| A342 | Constrain gradient direction | <code>Shift+drag</code> | Gradient tool handles | Gesture | Vibescape normally uses Ctrl angle constraints; not covered by general transform modifier remapping. |
| A343 | Show gradient handles | <code>G</code> | [Gradient Tool](#v021) | Direct | Switching tools reveals editable gradient handles; this is not the annotator-visibility toggle. |

### Layers panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A344 | New layer without dialog | <code>Ctrl + L</code> | [Add Layer Above](#v002) | Direct | Adds above the current layer. |
| A345 | New layer dialog | <code>Alt + Ctrl + L</code> | [Add Layer](#v001) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A346 | Select objects in layer | <code>Alt+click layer name</code> | [Select All](#v089) | Related | Current-layer selection depends on scope/settings; not the same Layers-panel gesture. |
| A347 | Isolate layer visibility | <code>Alt+click eye</code> | Layers and Objects panel visibility controls | Gesture | Compare panel modifiers; no direct isolate-layer shortcut item identified. |
| A348 | Outline selected layer | <code>Ctrl+click eye</code> | No direct shortcut item identified | Gap | Canvas outline modes are document-wide, not per-layer. |
| A349 | Outline all other layers | <code>Ctrl+Alt+click eye</code> | No direct shortcut item identified | Gap | No corresponding per-layer rendering-mode action. |
| A350 | Lock/unlock other layers | <code>Alt+click lock</code> | Layers and Objects panel lock controls | Gesture | No directly matching configurable all-other-layers action. |
| A351 | Expand complete sublayer tree | <code>Alt+click disclosure</code> | Layers and Objects panel tree | Gesture | Panel-local expansion; verify gesture support. |
| A352 | New layer with options | <code>Alt+click New Layer</code> | [Add Layer](#v001) | Related | Opens Add Layer dialog; pointer shortcut is separate. |
| A353 | New sublayer with options | <code>Alt+click New Sublayer</code> | [Add Layer](#v001) | Related | Choose relative position in Add Layer dialog. |
| A354 | New sublayer at bottom | <code>Ctrl+Alt+click New Sublayer</code> | Add Layer dialog + layer ordering | Gesture | No matching single pointer shortcut identified. |
| A355 | New layer at top | <code>Ctrl+click New Layer</code> | [Add Layer Above](#v002) | Related | Adds above current, not necessarily top of the document. |
| A356 | New layer below current | <code>Ctrl+Alt+click New Layer</code> | [Add Layer](#v001) | Related | Choose below current in Add Layer dialog. |
| A357 | Copy selection into layer/group | <code>Alt+drag selection</code> | Layers and Objects / Duplicate / Move Selection to Layer | Gesture | Multi-step approximation; verify drag-copy behavior. |

### Swatches panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A358 | New spot swatch | <code>Ctrl+click New Swatch</code> | No direct shortcut item identified | Gap | No one-to-one Illustrator spot-swatch action. |
| A359 | New global process swatch | <code>Ctrl+Shift+click New Swatch</code> | Swatches / Fill and Stroke | Gesture | SVG swatches differ from Illustrator global process colors. |
| A360 | Replace one swatch with another | <code>Alt+drag swatch over swatch</code> | Swatches panel | Gesture | No equivalent remappable replacement gesture identified. |

### Transform panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A361 | Apply numeric value and retain focus | <code>Shift+Enter</code> | Panel numeric fields | Gesture | Widget behavior rather than a shortcut-editor action. |
| A362 | Apply numeric transform to a copy | <code>Alt+Enter</code> | Transform panel / Duplicate | Gesture | No matching field-specific Alt+Enter action. |
| A363 | Apply proportional width/height | <code>Ctrl+Enter</code> | Transform panel scale lock / Selector aspect lock | Gesture | Use the lock control; no identical Ctrl+Enter action identified. |

### Transparency panel

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A364 | Repeat last effect | <code>Shift + Ctrl + E</code> | [Previous Extension](#v067) | Related | Repeats an extension, not Illustrator appearance/live-effect history. |
| A365 | Last effect with options | <code>Alt + Shift + Ctrl +E</code> | [Previous Extension Settings](#v068) | Related | Reopens extension settings; not Illustrator appearance/live-effect history. |
| A366 | Edit opacity mask alone | <code>Alt+click mask thumbnail</code> | Node tool mask editing / Layers and Objects | Gesture | No matching grayscale-mask-thumbnail view action. |
| A367 | Temporarily disable opacity mask | <code>Shift+click mask thumbnail</code> | Object mask controls | Gesture | Release Object Mask removes a mask; it is not an enable/disable toggle. |
| A368 | Re-enable opacity mask | <code>Shift+click disabled mask</code> | Object mask controls | Gesture | No matching nondestructive enable/disable action identified. |
| A369 | Opacity: one-percent step | <code>Focus opacity; Up/Down</code> | Fill and Stroke / Object Properties opacity field | Gesture | Field-specific editing; verify actual step. |
| A370 | Opacity: ten-percent step | <code>Focus opacity; Shift+Up/Down</code> | Fill and Stroke / Object Properties opacity field | Gesture | Field-specific editing; verify actual Shift multiplier. |

### Function keys

| Ref | Illustrator operation | Adobe Windows default | Vibescape item / route | Fit | Comparison / next step |
| --- | --- | --- | --- | --- | --- |
| A371 | Help | <code>F1</code> | [Inkscape Manual](#v025) | Related | Opens retained Inkscape manual; no dedicated Vibescape help system. |
| A372 | Cut | <code>F2</code> | [Cut](#v009) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A373 | Copy | <code>F3</code> | [Copy](#v007) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A374 | Paste | <code>F4</code> | [Paste](#v062) | Direct | Candidate for direct binding; verify focus, selection scope, and behavior. |
| A375 | Brushes panel | <code>F5</code> | No direct shortcut item identified | Gap | No equivalent Illustrator Brushes library panel. |
| A376 | Color panel | <code>F6</code> | [Open Fill and Stroke](#v048) | Related | Fill and Stroke combines paint controls; opens/focuses instead of necessarily hiding an existing panel. |
| A377 | Layers panel | <code>F7</code> | [Open Objects](#v053) | Direct | Opens/focuses Layers and Objects; not a visibility toggle. |
| A378 | New symbol | <code>F8</code> | [Open Symbols](#v057) | Related | Use Symbols panel controls. Legacy preset maps F8 to Create Clone, which is not Create Symbol. |
| A379 | Info panel | <code>Ctrl + F8</code> | [Open Object Properties](#v052) | Related | Object properties is only a partial substitute for cursor/measurement information. |
| A380 | Gradient panel | <code>Ctrl + F9</code> | [Open Fill and Stroke](#v048) | Related | Select gradient controls; action does not guarantee opening a specific tab. |
| A381 | Stroke panel | <code>Ctrl + F10</code> | [Open Fill and Stroke](#v048) | Related | Select Stroke style; action does not guarantee opening a specific tab. |
| A382 | Attributes panel | <code>Ctrl + F11</code> | [Open Object Properties](#v052) | Related | Attribute sets differ, especially print/overprint controls. |
| A383 | Revert document | <code>F12</code> | [Revert](#v082) | Direct | Potentially discards unsaved edits; retain the application confirmation behavior. |
| A384 | Graphic Styles panel | <code>Shift + F5</code> | No direct shortcut item identified | Gap | No equivalent Illustrator multi-attribute Graphic Styles library. |
| A385 | Appearance panel | <code>Shift + F6</code> | [Open Fill and Stroke](#v048) | Related | Single fill/stroke controls do not reproduce Illustrator appearance stacks. |
| A386 | Align panel | <code>Shift + F7</code> | [Open Align and Distribute](#v044) | Direct | Opens/focuses panel instead of necessarily toggling its visibility. |
| A387 | Transform panel | <code>Shift + F8</code> | [Open Transform](#v059) | Direct | Opens/focuses panel instead of necessarily toggling its visibility. |
| A388 | Pathfinder panel | <code>Shift + Ctrl + F9</code> | [Shape Builder Tool](#v093) | Related | Shape Builder and Path Boolean commands are alternatives; there is no identical Pathfinder panel. |
| A389 | Transparency panel | <code>Shift + Ctrl + F10</code> | [Open Fill and Stroke](#v048) | Related | Opacity/blend controls are related; mask workflow is separate. |
| A390 | Symbols panel | <code>Shift + Ctrl + F11</code> | [Open Symbols](#v057) | Direct | Opens/focuses panel; symbol behavior differs. |

## Binding catalogue

These are **source-derived profile bindings**, with recursive inclusion and later
key conflicts resolved in file order. They are not a running-application export.
`—` means no non-keypad binding survives in that XML profile; a tool/widget can
still handle keys directly. Keypad aliases are omitted for readability. `Meta`
entries are shown literally rather than guessed to be a particular Windows key.
Multiple surviving bindings are separated by semicolons; order does not imply
which one the menu displays. Layout-dependent aliases still need runtime checks.

A link on the item name opens its source metadata. `win.` means a window action,
`app.` an application action, `doc.` a document action, and `tool.` a tool-local
shortcut. These prefixes and quoted targets are part of the action ID, not text
to type into the shortcut-capture field.

| Item and source | Stable action ID | Stock | Legacy | CC 2024 |
| --- | --- | --- | --- | --- |
| <a id="v001"></a>[Add Layer](src/actions/actions-layer.cpp#L480) | <code>win.layer-new</code> | <code>Ctrl+Shift+N</code> | <code>—</code> | <code>Ctrl+Alt+L</code> |
| <a id="v002"></a>[Add Layer Above](src/actions/actions-layer.cpp#L481) | <code>win.layer-new-above</code> | <code>—</code> | <code>—</code> | <code>Ctrl+L</code> |
| <a id="v003"></a>[Break Apart](src/actions/actions-paths.cpp#L256) | <code>app.path-break-apart</code> | <code>Ctrl+Shift+K</code> | <code>Ctrl+Shift+K</code> | <code>Ctrl+Shift+K; Ctrl+Alt+Shift+8</code> |
| <a id="v004"></a>[Calligraphy Tool](src/actions/actions-tools.cpp#L295) | <code>win.tool-switch('Calligraphic')</code> | <code>C; Ctrl+F6</code> | <code>B; C; Ctrl+F6</code> | <code>B; C; Shift+B</code> |
| <a id="v005"></a>[Close](src/actions/actions-file-window.cpp#L157) | <code>win.document-close</code> | <code>Ctrl+W</code> | <code>Ctrl+W</code> | <code>Ctrl+W</code> |
| <a id="v006"></a>[Combine](src/actions/actions-paths.cpp#L255) | <code>app.path-combine</code> | <code>Ctrl+K</code> | <code>—</code> | <code>Ctrl+8</code> |
| <a id="v007"></a>[Copy](src/actions/actions-edit.cpp#L306) | <code>app.copy</code> | <code>Ctrl+C; Ctrl+Insert</code> | <code>Ctrl+C; Ctrl+Insert</code> | <code>F3; Ctrl+C; Ctrl+Insert</code> |
| <a id="v008"></a>[Create Clone](src/actions/actions-edit.cpp#L316) | <code>app.clone</code> | <code>Alt+D</code> | <code>F8; Alt+D</code> | <code>Alt+D</code> |
| <a id="v009"></a>[Cut](src/actions/actions-edit.cpp#L305) | <code>app.cut</code> | <code>Ctrl+X; Shift+Delete</code> | <code>Ctrl+X; Shift+Delete</code> | <code>F2; Ctrl+X; Shift+Delete</code> |
| <a id="v010"></a>[Cut Path](src/actions/actions-paths.cpp#L254) | <code>app.path-cut</code> | <code>Ctrl+Alt+slash; Ctrl+Alt+Shift+slash</code> | <code>Ctrl+Alt+slash; Ctrl+Alt+Shift+slash</code> | <code>Ctrl+Alt+slash; Ctrl+Alt+Shift+slash</code> |
| <a id="v011"></a>[Deselect](src/actions/actions-selection-window.cpp#L135) | <code>win.select-none</code> | <code>Esc</code> | <code>Esc; Ctrl+Shift+A</code> | <code>Esc; Ctrl+Shift+A</code> |
| <a id="v012"></a>[Display Mode: Cycle](src/actions/actions-canvas-mode.cpp#L266) | <code>win.canvas-display-mode-cycle</code> | <code>Ctrl+5</code> | <code>Ctrl+Y</code> | <code>Ctrl+Y</code> |
| <a id="v013"></a>[Display Mode: Toggle](src/actions/actions-canvas-mode.cpp#L267) | <code>win.canvas-display-mode-toggle</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v014"></a>[Dropper Tool](src/actions/actions-tools.cpp#L300) | <code>win.tool-switch('Dropper')</code> | <code>D; F7</code> | <code>D; I</code> | <code>D; I</code> |
| <a id="v015"></a>[Duplicate and Transform](src/actions/actions-edit.cpp#L315) | <code>app.duplicate-transform</code> | <code>Ctrl+Alt+D</code> | <code>Ctrl+Alt+D</code> | <code>Ctrl+Alt+D</code> |
| <a id="v016"></a>[Ellipse/Arc Tool](src/actions/actions-tools.cpp#L287) | <code>win.tool-switch('Arc')</code> | <code>E; F5</code> | <code>F5; L</code> | <code>E; F5; L</code> |
| <a id="v017"></a>[Eraser Tool](src/actions/actions-tools.cpp#L305) | <code>win.tool-switch('Eraser')</code> | <code>Shift+E</code> | <code>Shift+E</code> | <code>Shift+E</code> |
| <a id="v018"></a>[Fill and Stroke](src/actions/actions-selection-window.cpp#L128) | <code>win.select-same-fill-and-stroke</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v019"></a>[Focus Mode](src/actions/actions-view-mode.cpp#L296) | <code>win.view-focus-toggle</code> | <code>Shift+F11</code> | <code>Shift+F11</code> | <code>F; Shift+F11</code> |
| <a id="v020"></a>[Fullscreen](src/actions/actions-view-mode.cpp#L293) | <code>win.view-fullscreen</code> | <code>F11; Ctrl+Meta+F</code> | <code>F11; Ctrl+Meta+F</code> | <code>F11; Ctrl+Meta+F</code> |
| <a id="v021"></a>[Gradient Tool](src/actions/actions-tools.cpp#L298) | <code>win.tool-switch('Gradient')</code> | <code>G; Ctrl+F1</code> | <code>G; Ctrl+F1</code> | <code>G; Ctrl+F1</code> |
| <a id="v022"></a>[Group](src/actions/actions-selection-object.cpp#L150) | <code>app.selection-group</code> | <code>Ctrl+G; Ctrl+Shift+U</code> | <code>Ctrl+G; Ctrl+Shift+U</code> | <code>Ctrl+G; Ctrl+Shift+U</code> |
| <a id="v023"></a>[Hide selection](src/actions/actions-hide-lock.cpp#L202) | <code>app.selection-hide</code> | <code>—</code> | <code>—</code> | <code>Ctrl+3</code> |
| <a id="v024"></a>[Import](src/actions/actions-file-window.cpp#L154) | <code>win.document-import</code> | <code>Ctrl+I</code> | <code>Ctrl+I; Ctrl+Shift+P</code> | <code>Ctrl+Shift+P</code> |
| <a id="v025"></a>[Inkscape Manual](src/actions/actions-help-url.cpp#L123) | <code>win.help-url-manual</code> | <code>—</code> | <code>—</code> | <code>F1</code> |
| <a id="v026"></a>[Invert Selection](src/actions/actions-selection-window.cpp#L133) | <code>win.select-invert</code> | <code>!; Shift+!</code> | <code>!; Shift+!</code> | <code>!; Shift+!</code> |
| <a id="v027"></a>[Lock All Guides](src/actions/actions-edit-document.cpp#L93) | <code>doc.lock-all-guides</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Alt+;</code> |
| <a id="v028"></a>[Lock selection](src/actions/actions-hide-lock.cpp#L206) | <code>app.selection-lock</code> | <code>—</code> | <code>—</code> | <code>Ctrl+2</code> |
| <a id="v029"></a>[Lower](src/actions/actions-selection-object.cpp#L157) | <code>app.selection-lower</code> | <code>Page Down</code> | <code>Page Down; Ctrl+[</code> | <code>Page Down; Ctrl+[</code> |
| <a id="v030"></a>[Lower to Bottom](src/actions/actions-selection-object.cpp#L158) | <code>app.selection-bottom</code> | <code>End</code> | <code>End; Ctrl+Shift+[</code> | <code>End; Ctrl+Shift+[</code> |
| <a id="v031"></a>[Mesh Tool](src/actions/actions-tools.cpp#L299) | <code>win.tool-switch('Mesh')</code> | <code>—</code> | <code>—</code> | <code>U</code> |
| <a id="v032"></a>[New](src/actions/actions-file-window.cpp#L146) | <code>win.document-new</code> | <code>Ctrl+N</code> | <code>Ctrl+N</code> | <code>Ctrl+N</code> |
| <a id="v033"></a>[New from Template](src/actions/actions-file-window.cpp#L147) | <code>win.document-dialog-templates</code> | <code>Ctrl+Alt+N</code> | <code>Ctrl+Alt+N; Ctrl+Shift+N</code> | <code>Ctrl+Alt+N; Ctrl+Shift+N</code> |
| <a id="v034"></a>[Next Window](src/actions/actions-view-window.cpp#L66) | <code>win.window-next</code> | <code>Meta+Backquote</code> | <code>Meta+Backquote</code> | <code>Ctrl+F6; Meta+Backquote</code> |
| <a id="v035"></a>[Node Tool](src/actions/actions-tools.cpp#L283) | <code>win.tool-switch('Node')</code> | <code>F2; N</code> | <code>A; F2</code> | <code>A</code> |
| <a id="v036"></a>[Nodes break](src/actions/actions-node-tool.cpp#L194) | <code>win.node-nodes-break</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v037"></a>[Nodes delete](src/actions/actions-node-tool.cpp#L191) | <code>win.node-nodes-delete</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v038"></a>[Nodes insert](src/actions/actions-node-tool.cpp#L190) | <code>win.node-nodes-insert</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v039"></a>[Nodes join](src/actions/actions-node-tool.cpp#L193) | <code>win.node-nodes-join</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v040"></a>[Nodes to cusp](src/actions/actions-node-tool.cpp#L199) | <code>win.node-nodes-to-cusp</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v041"></a>[Nodes to smooth](src/actions/actions-node-tool.cpp#L200) | <code>win.node-nodes-to-smooth</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v042"></a>[Object to Path](src/actions/actions-object.cpp#L524) | <code>app.object-to-path</code> | <code>Ctrl+Shift+C</code> | <code>Ctrl+Shift+C</code> | <code>Ctrl+Shift+C; Ctrl+Shift+O</code> |
| <a id="v043"></a>[Objects to Guides](src/actions/actions-edit.cpp#L304) | <code>app.object-to-guides</code> | <code>Shift+G</code> | <code>Ctrl+5; Shift+G</code> | <code>Ctrl+5</code> |
| <a id="v044"></a>[Open Align and Distribute](src/actions/actions-dialogs.cpp#L38) | <code>win.dialog-open('AlignDistribute')</code> | <code>Ctrl+Shift+A</code> | <code>Shift+F7</code> | <code>Shift+F7</code> |
| <a id="v045"></a>[Open Document Properties](src/actions/actions-dialogs.cpp#L41) | <code>win.dialog-open('DocumentProperties')</code> | <code>Ctrl+Shift+D</code> | <code>Ctrl+Alt+P; Ctrl+Shift+D</code> | <code>Ctrl+Alt+P; Ctrl+Shift+D</code> |
| <a id="v046"></a>[Open Export](src/actions/actions-dialogs.cpp#L44) | <code>win.dialog-open('Export')</code> | <code>Ctrl+Shift+E</code> | <code>Ctrl+Shift+E</code> | <code>Ctrl+Alt+E</code> |
| <a id="v047"></a>[Open File Dialog](src/actions/actions-file-window.cpp#L148) | <code>win.document-open</code> | <code>Ctrl+O</code> | <code>Ctrl+O</code> | <code>Ctrl+O</code> |
| <a id="v048"></a>[Open Fill and Stroke](src/actions/actions-dialogs.cpp#L45) | <code>win.dialog-open('FillStroke')</code> | <code>Ctrl+Shift+F</code> | <code>Ctrl+F10; Ctrl+F9; Ctrl+Shift+F10</code> | <code>F6; Ctrl+F10; Ctrl+F9; Ctrl+Shift+F</code> |
| <a id="v049"></a>[Open Find](src/actions/actions-dialogs.cpp#L48) | <code>win.dialog-open('Find')</code> | <code>Ctrl+F</code> | <code>Ctrl+F</code> | <code>Ctrl+F</code> |
| <a id="v050"></a>[Open Keyboard Shortcuts](src/actions/actions-dialogs.cpp#L66) | <code>win.preferences-keyboard-shortcuts</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v051"></a>[Open Live Path Effect](src/actions/actions-dialogs.cpp#L52) | <code>win.dialog-open('LivePathEffect')</code> | <code>Ctrl+&amp;; Ctrl+Shift+7</code> | <code>Ctrl+&amp;; Ctrl+Shift+7</code> | <code>Ctrl+&amp;; Ctrl+Shift+7</code> |
| <a id="v052"></a>[Open Object Properties](src/actions/actions-dialogs.cpp#L53) | <code>win.dialog-open('ObjectProperties')</code> | <code>Ctrl+Shift+O</code> | <code>Ctrl+F11; Ctrl+Shift+O</code> | <code>Ctrl+F11</code> |
| <a id="v053"></a>[Open Objects](src/actions/actions-dialogs.cpp#L54) | <code>win.dialog-open('Objects')</code> | <code>Ctrl+Shift+L</code> | <code>F7; Ctrl+Shift+L</code> | <code>F7; Ctrl+Shift+L</code> |
| <a id="v054"></a>[Open Preferences](src/actions/actions-dialogs.cpp#L67) | <code>app.preferences</code> | <code>Meta+,; Ctrl+Shift+P</code> | <code>Ctrl+K; Meta+,</code> | <code>Ctrl+K; Meta+,</code> |
| <a id="v055"></a>[Open Preferences](src/actions/actions-dialogs.cpp#L55) | <code>win.dialog-open('Preferences')</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v056"></a>[Open Spellcheck](src/actions/actions-dialogs.cpp#L69) | <code>win.dialog-open('Spellcheck')</code> | <code>Ctrl+Alt+K</code> | <code>Ctrl+Alt+K</code> | <code>Ctrl+I; Ctrl+Alt+K</code> |
| <a id="v057"></a>[Open Symbols](src/actions/actions-dialogs.cpp#L60) | <code>win.dialog-open('Symbols')</code> | <code>Ctrl+Shift+Y</code> | <code>Ctrl+Shift+Y</code> | <code>Ctrl+Shift+F11; Ctrl+Shift+Y</code> |
| <a id="v058"></a>[Open Text](src/actions/actions-dialogs.cpp#L61) | <code>win.dialog-open('Text')</code> | <code>Ctrl+Shift+T</code> | <code>Ctrl+T; Ctrl+Alt+T; Ctrl+Shift+T</code> | <code>Ctrl+T; Ctrl+Shift+T; Ctrl+Alt+Shift+T</code> |
| <a id="v059"></a>[Open Transform](src/actions/actions-dialogs.cpp#L63) | <code>win.dialog-open('Transform')</code> | <code>Ctrl+Shift+M</code> | <code>Shift+F8; Ctrl+Shift+M</code> | <code>Shift+F8; Ctrl+Shift+M</code> |
| <a id="v060"></a>[Pages Tool](src/actions/actions-tools.cpp#L311) | <code>win.tool-switch('Pages')</code> | <code>—</code> | <code>—</code> | <code>Shift+O</code> |
| <a id="v061"></a>[Paint Bucket Tool](src/actions/actions-tools.cpp#L301) | <code>win.tool-switch('PaintBucket')</code> | <code>U; Shift+F7</code> | <code>K; U</code> | <code>K</code> |
| <a id="v062"></a>[Paste](src/actions/actions-edit-window.cpp#L65) | <code>win.paste</code> | <code>Ctrl+V; Shift+Insert</code> | <code>Ctrl+V; Shift+Insert</code> | <code>F4; Ctrl+V; Shift+Insert</code> |
| <a id="v063"></a>[Paste In Place](src/actions/actions-edit-window.cpp#L66) | <code>win.paste-in-place</code> | <code>Ctrl+Alt+V</code> | <code>Ctrl+Alt+V; Ctrl+Shift+F</code> | <code>Ctrl+Shift+V</code> |
| <a id="v064"></a>[Paste On Page](src/actions/actions-edit-window.cpp#L67) | <code>win.paste-on-page</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v065"></a>[Pen Tool](src/actions/actions-tools.cpp#L293) | <code>win.tool-switch('Pen')</code> | <code>B; Shift+F6</code> | <code>P; Shift+F6</code> | <code>P; Shift+F6</code> |
| <a id="v066"></a>[Pencil Tool](src/actions/actions-tools.cpp#L294) | <code>win.tool-switch('Pencil')</code> | <code>F6; P</code> | <code>N</code> | <code>N</code> |
| <a id="v067"></a>[Previous Extension](src/actions/actions-effect.cpp#L84) | <code>app.last-effect</code> | <code>Alt+Q</code> | <code>Alt+Q</code> | <code>Alt+Q; Ctrl+Shift+E</code> |
| <a id="v068"></a>[Previous Extension Settings](src/actions/actions-effect.cpp#L85) | <code>app.last-effect-pref</code> | <code>Alt+Shift+Q</code> | <code>Alt+Shift+Q</code> | <code>Alt+Shift+Q; Ctrl+Alt+Shift+E</code> |
| <a id="v069"></a>[Previous Window](src/actions/actions-view-window.cpp#L65) | <code>win.window-previous</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Shift+F6</code> |
| <a id="v070"></a>[Print](src/actions/actions-file-window.cpp#L155) | <code>win.document-print</code> | <code>Ctrl+P</code> | <code>Ctrl+P</code> | <code>Ctrl+P</code> |
| <a id="v071"></a>[Put on Path](src/actions/actions-text.cpp#L89) | <code>app.text-put-on-path</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v072"></a>[Quick Pan Canvas](src/ui/tools/shortcuts.cpp#L22) | <code>tool.all.quick-pan</code> | <code>Space</code> | <code>Space</code> | <code>Space</code> |
| <a id="v073"></a>[Quick Preview](src/ui/tools/shortcuts.cpp#L20) | <code>tool.all.quick-preview</code> | <code>F</code> | <code>F</code> | <code>—</code> |
| <a id="v074"></a>[Quit](src/actions/actions-base.cpp#L248) | <code>app.quit</code> | <code>Ctrl+Q</code> | <code>Ctrl+Q</code> | <code>Ctrl+Q</code> |
| <a id="v075"></a>[Raise](src/actions/actions-selection-object.cpp#L156) | <code>app.selection-raise</code> | <code>Page Up</code> | <code>Page Up; Ctrl+]</code> | <code>Page Up; Ctrl+]</code> |
| <a id="v076"></a>[Raise to Top](src/actions/actions-selection-object.cpp#L155) | <code>app.selection-top</code> | <code>Home</code> | <code>Home; Ctrl+Shift+]</code> | <code>Home; Ctrl+Shift+]</code> |
| <a id="v077"></a>[Reapply Transforms](src/actions/actions-transform.cpp#L165) | <code>app.transform-reapply</code> | <code>Ctrl+Alt+T</code> | <code>—</code> | <code>Ctrl+D; Ctrl+Alt+T</code> |
| <a id="v078"></a>[Rectangle Tool](src/actions/actions-tools.cpp#L286) | <code>win.tool-switch('Rect')</code> | <code>F4; R</code> | <code>F4; M; R</code> | <code>M; R</code> |
| <a id="v079"></a>[Redo](src/actions/actions-undo-document.cpp#L96) | <code>doc.redo</code> | <code>Ctrl+Y; Ctrl+Shift+Z</code> | <code>Ctrl+Shift+Z</code> | <code>Ctrl+Shift+Z</code> |
| <a id="v080"></a>[Release Object Clipping](src/actions/actions-object.cpp#L530) | <code>app.object-release-clip</code> | <code>Alt+M</code> | <code>Alt+M; Ctrl+Alt+7</code> | <code>Alt+M; Ctrl+Alt+7</code> |
| <a id="v081"></a>[Remove Manual Kerns](src/actions/actions-text.cpp#L97) | <code>app.text-unkern</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Alt+Q</code> |
| <a id="v082"></a>[Revert](src/actions/actions-file-window.cpp#L149) | <code>win.document-revert</code> | <code>—</code> | <code>Ctrl+Alt+Z</code> | <code>F12</code> |
| <a id="v083"></a>[Rulers](src/actions/actions-view-mode.cpp#L287) | <code>win.canvas-rulers</code> | <code>Ctrl+R</code> | <code>Ctrl+R</code> | <code>Ctrl+R</code> |
| <a id="v084"></a>[Save](src/actions/actions-file-window.cpp#L150) | <code>win.document-save</code> | <code>Ctrl+S</code> | <code>Ctrl+S</code> | <code>Ctrl+S</code> |
| <a id="v085"></a>[Save a Copy](src/actions/actions-file-window.cpp#L152) | <code>win.document-save-copy</code> | <code>Ctrl+Alt+Shift+S</code> | <code>Ctrl+Alt+Shift+S</code> | <code>Ctrl+Alt+S; Ctrl+Alt+Shift+S</code> |
| <a id="v086"></a>[Save As](src/actions/actions-file-window.cpp#L151) | <code>win.document-save-as</code> | <code>Ctrl+Shift+S</code> | <code>Ctrl+Shift+S</code> | <code>Ctrl+Shift+S</code> |
| <a id="v087"></a>[Scroll bars](src/actions/actions-view-mode.cpp#L288) | <code>win.canvas-scroll-bars</code> | <code>Ctrl+B</code> | <code>Ctrl+B</code> | <code>Ctrl+B</code> |
| <a id="v088"></a>[Segments join](src/actions/actions-node-tool.cpp#L195) | <code>win.node-segments-join</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v089"></a>[Select All](src/actions/actions-selection-window.cpp#L126) | <code>win.select-all</code> | <code>Ctrl+A</code> | <code>Ctrl+A</code> | <code>—</code> |
| <a id="v090"></a>[Select All in All Layers](src/actions/actions-selection-window.cpp#L127) | <code>win.select-all-layers</code> | <code>Ctrl+Alt+A</code> | <code>Ctrl+Alt+A</code> | <code>Ctrl+A; Ctrl+Alt+A</code> |
| <a id="v091"></a>[Selector Tool](src/actions/actions-tools.cpp#L282) | <code>win.tool-switch('Select')</code> | <code>F1; S</code> | <code>E; F1; S; V</code> | <code>S; V</code> |
| <a id="v092"></a>[Set Object Clipping](src/actions/actions-object.cpp#L528) | <code>app.object-set-clip</code> | <code>Ctrl+M</code> | <code>Ctrl+7; Ctrl+M</code> | <code>Ctrl+7; Ctrl+M</code> |
| <a id="v093"></a>[Shape Builder Tool](src/actions/actions-tools.cpp#L284) | <code>win.tool-switch('Booleans')</code> | <code>X</code> | <code>X</code> | <code>X; Shift+M</code> |
| <a id="v094"></a>[Shape Builder: Delete](src/actions/actions-paths.cpp#L272) | <code>win.shape-builder-mode(1)</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v095"></a>[Show All Guides](src/actions/actions-edit-document.cpp#L94) | <code>doc.show-all-guides</code> | <code>&#124;; Shift+&#124;</code> | <code>&#124;; Ctrl+;; Shift+&#124;</code> | <code>&#124;; Ctrl+;; Shift+&#124;</code> |
| <a id="v096"></a>[Show Grids](src/actions/actions-edit-document.cpp#L98) | <code>doc.show-grids</code> | <code>#; Shift+#</code> | <code>#; Ctrl+"; Shift+#</code> | <code>#; Ctrl+'; Shift+#</code> |
| <a id="v097"></a>[Simplify](src/actions/actions-paths.cpp#L261) | <code>app.path-simplify</code> | <code>Ctrl+L</code> | <code>Ctrl+L</code> | <code>—</code> |
| <a id="v098"></a>[Simplify path](src/actions/actions-node-tool.cpp#L197) | <code>win.node-simplify</code> | <code>—</code> | <code>—</code> | <code>—</code> |
| <a id="v099"></a>[Snap Grids](src/actions/actions-canvas-snapping.cpp#L276) | <code>win.snap-grid</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Shift+'</code> |
| <a id="v100"></a>[Snap Nodes](src/actions/actions-canvas-snapping.cpp#L260) | <code>win.snap-node-category</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Alt+'</code> |
| <a id="v101"></a>[Snapping](src/actions/actions-canvas-snapping.cpp#L247) | <code>win.snap-global-toggle</code> | <code>%</code> | <code>%; Ctrl+Alt+'; Ctrl+Shift+"</code> | <code>%; Ctrl+U</code> |
| <a id="v102"></a>[Spray Tool](src/actions/actions-tools.cpp#L304) | <code>win.tool-switch('Spray')</code> | <code>A; Shift+F3</code> | <code>Shift+F3; Shift+S</code> | <code>Shift+F3; Shift+S</code> |
| <a id="v103"></a>[Swap fill and stroke](src/actions/actions-edit.cpp#L326) | <code>app.swap-fill-and-stroke</code> | <code>Shift+X</code> | <code>Shift+X</code> | <code>Shift+X</code> |
| <a id="v104"></a>[Text Tool](src/actions/actions-tools.cpp#L296) | <code>win.tool-switch('Text')</code> | <code>F8; T</code> | <code>T</code> | <code>F8; T</code> |
| <a id="v105"></a>[Toggle all dialogs](src/actions/actions-dialogs.cpp#L75) | <code>win.dialog-toggle</code> | <code>F12</code> | <code>F12</code> | <code>—</code> |
| <a id="v106"></a>[Toggle Selector Tool](src/actions/actions-tools.cpp#L313) | <code>win.tool-toggle('Select')</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Backquote; Ctrl+Dead grave</code> |
| <a id="v107"></a>[Tweak Tool](src/actions/actions-tools.cpp#L303) | <code>win.tool-switch('Tweak')</code> | <code>W; Shift+F2</code> | <code>W; Shift+F2; Shift+R</code> | <code>W; Shift+F2; Shift+R</code> |
| <a id="v108"></a>[Undo](src/actions/actions-undo-document.cpp#L95) | <code>doc.undo</code> | <code>Ctrl+Z</code> | <code>Ctrl+Z</code> | <code>Ctrl+Z</code> |
| <a id="v109"></a>[Ungroup](src/actions/actions-selection-object.cpp#L151) | <code>app.selection-ungroup</code> | <code>Ctrl+U; Ctrl+Shift+G</code> | <code>Ctrl+U; Ctrl+Shift+G</code> | <code>Ctrl+Shift+G</code> |
| <a id="v110"></a>[Unhide All](src/actions/actions-hide-lock.cpp#L199) | <code>app.unhide-all</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Alt+3</code> |
| <a id="v111"></a>[Unlock All](src/actions/actions-hide-lock.cpp#L200) | <code>app.unlock-all</code> | <code>—</code> | <code>—</code> | <code>Ctrl+Alt+2</code> |
| <a id="v112"></a>[Zoom 1:1](src/actions/actions-canvas-transform.cpp#L306) | <code>win.canvas-zoom-1-1</code> | <code>1</code> | <code>1; Ctrl+1</code> | <code>1; Ctrl+1</code> |
| <a id="v113"></a>[Zoom Drawing](src/actions/actions-canvas-transform.cpp#L310) | <code>win.canvas-zoom-drawing</code> | <code>4</code> | <code>4</code> | <code>4; Ctrl+Alt+0</code> |
| <a id="v114"></a>[Zoom In](src/actions/actions-canvas-transform.cpp#L304) | <code>win.canvas-zoom-in</code> | <code>+; =</code> | <code>+; =; Ctrl++; Ctrl+=</code> | <code>+; =; Ctrl+=</code> |
| <a id="v115"></a>[Zoom Out](src/actions/actions-canvas-transform.cpp#L305) | <code>win.canvas-zoom-out</code> | <code>-; _</code> | <code>-; _; Ctrl+-; Ctrl+_</code> | <code>-; _; Ctrl+-</code> |
| <a id="v116"></a>[Zoom Page](src/actions/actions-canvas-transform.cpp#L311) | <code>win.canvas-zoom-page</code> | <code>5</code> | <code>5; Ctrl+0</code> | <code>5; Ctrl+0</code> |
| <a id="v117"></a>[Zoom Tool](src/actions/actions-tools.cpp#L309) | <code>win.tool-switch('Zoom')</code> | <code>F3; Z</code> | <code>F3; Z</code> | <code>Z</code> |

## Tool Modifier starting points

These live on **Keyboard Shortcuts > Tool Modifiers**. They affect their named
interaction, not every tool globally. Modifier IDs below are not Gio action IDs.

| Modifier item | Modifier ID | Stock | Legacy | CC 2024 |
| --- | --- | --- | --- | --- |
| Vertical pan | <code>canvas-pan-y</code> | <code>None</code> | <code>None</code> | <code>None</code> |
| Horizontal pan | <code>canvas-pan-x</code> | <code>Shift</code> | <code>Shift</code> | <code>Ctrl</code> |
| Canvas zoom | <code>canvas-zoom</code> | <code>Ctrl</code> | <code>Ctrl</code> | <code>Alt</code> |
| Canvas rotate | <code>canvas-rotate</code> | <code>Shift+Ctrl</code> | <code>Shift+Ctrl</code> | <code>Shift+Ctrl</code> |
| Add to selection | <code>select-add-to</code> | <code>Shift</code> | <code>Shift</code> | <code>Shift</code> |
| Select inside groups | <code>select-in-groups</code> | <code>Ctrl</code> | <code>Ctrl</code> | <code>Ctrl</code> |
| Select with touch-path | <code>select-touch-path</code> | <code>Alt</code> | <code>Alt</code> | <code>Alt</code> |
| Select with box | <code>select-always-box</code> | <code>Shift</code> | <code>Shift</code> | <code>Shift</code> |
| Remove from selection | <code>select-remove-from</code> | <code>Shift+Ctrl</code> | <code>Shift+Ctrl</code> | <code>Ctrl</code> |
| Forced Drag | <code>select-force-drag</code> | <code>Alt</code> | <code>Alt</code> | <code>Alt</code> |
| Duplicate selection on drag | <code>select-duplicate</code> | Disabled at baseline; Alt in follow-up | Disabled at baseline; Alt in follow-up | Disabled at baseline; Alt in follow-up |
| Cycle through objects | <code>select-cycle</code> | <code>Alt</code> | <code>Alt</code> | <code>Ctrl+Alt</code> |
| Move one axis only | <code>move-confine</code> | <code>Ctrl</code> | <code>Ctrl</code> | <code>Shift</code> |
| No Move Snapping | <code>move-snapping</code> | <code>Shift</code> | <code>Shift</code> | <code>Ctrl</code> |
| Keep aspect ratio | <code>trans-confine</code> | <code>Ctrl</code> | <code>Ctrl</code> | <code>Shift</code> |
| Transform in increments | <code>trans-increment</code> | <code>Alt</code> | <code>Alt</code> | <code>Disabled</code> |
| Transform around center | <code>trans-off-center</code> | <code>Shift</code> | <code>Shift</code> | <code>Alt</code> |
| No Transform Snapping | <code>trans-snapping</code> | <code>Shift</code> | <code>Shift</code> | <code>Ctrl</code> |
| Switch mode | <code>bool-shift</code> | <code>Shift</code> | <code>Shift</code> | <code>Shift</code> |

## Review record and sources

- Coverage check: 390 published entries and 117 referenced named actions; every named action links to metadata in this checkout.
- Current mapping counts: 79 Direct, 91 Related, 138 Gesture, 82 Gap (A223 corrected after finding `select-duplicate`). Repeated reference entries are included in these counts.
- The worksheet itself requires no rebuild. The copy-drag follow-up changes a source-code modifier default and tool behavior, and does require rebuilding; profile XML and personal preferences remain unchanged.
- This remains a planning worksheet. Mark checklist items complete only after reviewing the actual editor behavior.
- [Adobe: default keyboard shortcuts and Export Text instructions](https://helpx.adobe.com/illustrator/using/default-keyboard-shortcuts.html)
- [Stock bindings](share/keys/inkscape.xml), [legacy Illustrator preset](share/keys/adobe-illustrator-cs2.xml), [CC 2024 preset](share/keys/adobe-illustrator-cc2024.xml).
- [Profile loader and conflict handling](src/ui/shortcuts.cpp), [default.xml installation rule](share/keys/CMakeLists.txt).
- [Tool modifiers](src/ui/modifiers.cpp), [tool-local action names](src/ui/tools/shortcuts.cpp).
- [Text-tool hardcoded handling](src/ui/tools/text-tool.cpp), [Selector handling](src/ui/tools/select-tool.cpp), [shared tool handling](src/ui/tools/tool-base.cpp).
- [Dialog actions, including our new Keyboard Shortcuts entry](src/actions/actions-dialogs.cpp).

Illustrator and Adobe are referenced only to describe the user's comparison target.
Vibescape is independent of Adobe and of the Inkscape project. Credit for the
inherited actions, bindings, and Illustrator presets belongs to their original
Inkscape contributors.
