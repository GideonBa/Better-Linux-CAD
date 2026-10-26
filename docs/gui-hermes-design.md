# Hermes-inspired desktop redesign

Reference: [Hermes Agent](https://hermes-agent.nousresearch.com/), inspected 2026-10-26.
The reference contributes electric blue, paper-colored typography, condensed display lettering,
technical metadata and carefully spaced, lightly divided sections. BLCAD adapts these principles to
a dense desktop CAD workspace. The user's latest instruction explicitly releases the previous
black/white and white-frame requirement. English labels, square geometry and solid fills remain.

## Implementation plan and acceptance

| Phase | Deliverable | Acceptance | Status |
| --- | --- | --- | --- |
| 1. Define the system | Palette, typography, spacing and state roles below | One documented contract; no unlicensed reference assets | Complete |
| 2. Build the foundation | Shared Qt palette/QSS and bundled desktop fonts | Offline font loading, readable controls, visible hover/focus/disabled states | Complete |
| 3. Redesign the shell | Identity/navigation bar, lighter ribbon groups, browser/properties/task panels and compact diagnostics | Commands reachable at 1280×800 and compact 1024×640; same action IDs and shortcuts | Complete |
| 4. Finish all workspaces | Matching task actions, viewport overlays, menus, tables, dialogs and startup screen | Sketch, Part, Surface and Assembly follow the same visual roles | Complete |
| 5. Exercise real workflows | Render/state checks plus combined authoring/editing/history/Save/Open tests | Offscreen and native Qt/OCCT pass; capture and inspect multiple window sizes | Complete |
| 6. Publish | Final evidence, reviewed commits, normal protected-branch PR merge | Tested source matches main; clean working tree | Delivery through the normal PR merge gate |

Phases run in order. Existing modeling authority, persistence, selection IDs and undo semantics remain
with their existing owners. This is a desktop design follow-up; Block 137 remains complete and the
Block-138 STEP reader remains planned.

## Visual system

| Role | Value / rule |
| --- | --- |
| Identity and primary actions | Electric blue `#0000f2`, paper/white foreground |
| Desktop/panel paper | `#f5f4ef`, input surface `#ffffff` |
| Main text | Deep indigo `#191638` |
| Secondary text | Muted indigo with a minimum readable text contrast |
| Quiet separators | Pale indigo; one-pixel rules only at meaningful group boundaries |
| Interactive field edges and scroll thumbs | Indigo `#8b85a3`, at least 3:1 contrast against paper |
| Hover / selection | Pale blue hover; blue selected rows with white text |
| Focus | Explicit contrasting outline, independent of hover and selection |
| Disabled | Muted readable text and subdued flat surface; no italic command labels |
| Modeling canvas | Solid dark navy `#101426`, retaining distinct CAD geometry/selection colors |
| Display typography | Bundled Barlow Condensed Medium for identity, navigation and section headings |
| UI typography | Bundled Barlow Regular/Medium, readable at ordinary CAD control sizes |
| Numeric typography | Bundled IBM Plex Mono for editable scalar values and technical readouts |
| Spacing | Four-pixel base; 8/12/16-pixel control/group rhythm, compact ribbon commands |
| Geometry / effects | Square corners, solid fills, no gradients or decorative drop shadows |

The typography assets are original font files from the official Google Fonts repository with their
SIL OFL licenses and exact source/checksum manifest in `assets/gui/fonts/`. No Hermes logos, artwork
or commercial/trial fonts are copied. The existing BLCAD startup artwork remains its own asset.
The upstream license bytes include original line endings and trailing spaces; `.gitattributes`
preserves these three license files verbatim and scopes whitespace-check exemptions to them.
Authored sources and documentation retain the repository's normal formatting checks.

## Interaction and verification boundaries

- Selection, hover, keyboard focus, checked and disabled states remain distinguishable.
- Inputs, combo/spin arrows, checkboxes, menu branches and scrollbars remain visible on paper and blue.
- STEP source checksum readouts must show the complete wrapped SHA-256 text without their own
  horizontal or vertical scrolling at 1280×800. Their height must include the polished font,
  document margins and the actual styled frame/padding.
- The ribbon keeps horizontal scrolling for long command groups; labels are not renamed for styling.
  Workspace tabs have explicit previous/next buttons when space is limited. Their reserved control
  slot keeps both arrows inside the header during native resize; pixel, containment and actual
  mouse-hit tests verify that the last workspace remains reachable.
- Dark-canvas dimensions and manipulators use coordinated readable overlays, independent of paper panels.
- Diagnostics remain available without taking most of the initial modeling canvas.
- The canvas permits a 360×260 logical-pixel minimum; the combined task workflow checks a compact
  1024×640 window as well as the ordinary and larger layout, bounded to the native available desktop.
- Render checks inspect actual widget pixels and contrast, not only stylesheet strings.
- Combined workflows cover Rapid Dimension, sketch completion/re-entry, feature preview/cancel/apply,
  visibility, Undo/Redo, Save/Open and existing Part/Surface/Assembly commands at native sizes.
- Source edits, builds, GUI runs and separate startup remain serial per `development-setup.md`.

## Verification evidence

Final acceptance passed on 2026-10-26:

| Check | Cases | Assertions | Result |
| --- | ---: | ---: | --- |
| Full Core | 545 | 17,037 | Passed |
| Full Geometry | 452 | 31,557 | Passed |
| Full offscreen GUI | 566 | 73,433 | 546 passed; 20 native-only skips |
| Full canonical native GUI with all subsequent follow-up tags | 378 | 70,304 | Passed; no skips |
| Native design/workflow at 150% | 5 | 771 | Passed |
| Required shell coverage closure | 3 | 299 | Passed |
| Native STEP checksum/layout regression | 1 | 180 | Passed |
| Separate native application startup | — | — | Running until the intentional six-second timeout, exit 124 |

The new combined workflow authors a rectangle and driving Rapid Dimension, cancels and reapplies
an Extrude preview, edits the depth/name, verifies independent 4,800/7,200 mm³ solid volumes,
checks exact two-step Undo/Redo and canonical Save/Open, then exports STEP through the actual
ribbon/dialog controls. It verifies task action hits and browser/model selection at compact,
ordinary and larger window sizes. The expanded appearance cases inspect real rendered control
states, font registration, contrast, input arrows, placeholders and workspace-tab mouse navigation.

Native popup helpers await exact cursor/hover agreement over separate event turns, bounded to two
seconds and four pointer requests; strict row and actual-click assertions remain. Checksum heights
use polished font/document metrics and the style's frame-content rectangle. The existing native
STEP regression confirms both complete digests have zero internal scroll range.

All five GUI-preset targets were built. Builds, source edits, GUI runs and separate startup remained
serial. The 733 CMake/source/test/asset files in
`/tmp/blcad-hermes-source-freeze-final.json` remained unchanged through the final full GUI/native,
scaled and startup checks; Core/Geometry sources were unchanged by the final GUI-only fixes.
Final logs are `/tmp/blcad-hermes-{core-full,geometry-full,gui-full,native-full,closure}.log`,
`/tmp/blcad-hermes-native-150-final.log`, `/tmp/blcad-hermes-step-style-native.log` and
`/tmp/blcad-hermes-startup-final.log`. Commands and the canonical native filter extension are in
`development-setup.md` and `gui-shell-acceptance-mvp9r.md`.

Native captures in `/tmp/blcad-hermes-native/` and `/tmp/blcad-hermes-native-150/` were inspected
for Sketch, feature preview/editing, Surface, Assembly, input/control states and startup. The available
X11 visual emits OCCT's known Medium depth/stencil warning; final native/scaled/startup logs contain
no High-severity, BadDrawable or BadWindow errors. No required check is unavailable.

Implementation and local acceptance are complete. Publication uses the repository's normal
protected-branch PR merge into `main`. The delivery gate requires comparing the tested branch tree
with `origin/main` after integration and a clean working tree. Numbered feature sequencing remains
Block 137 complete / Block 138 planned.

The versioned captures below show the implemented design.

Native Qt/OCCT feature preview at 1280×800:

![Paper task panels, electric-blue navigation and dark modeling canvas](images/hermes-feature-preview.png)

The same live task at 1024×640, with visible tab navigation and reachable Apply/Cancel:

![Compact desktop with visible workspace arrows and task actions](images/hermes-feature-compact.png)
