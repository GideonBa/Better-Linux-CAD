# Development Setup

This document covers the current local BLCAD build, test, formatting, and executable workflows.
Feature status and numbered sequencing live in `docs/mvp-plan.md`; exact contracts live in the
feature-specific documents under `docs/`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT/Open CASCADE for optional Geometry targets
- Qt 6 for the optional desktop, Interactive Sketcher, and Interactive Modeling
- nlohmann-json
- Catch2 3
- OpenSSL Crypto (SHA-256 for external STEP source freshness)

The Assembly numeric engine and Block-109 planar Sketch solver use project-owned deterministic dense
elimination routines. Eigen is installed by the reference dependency set but is not currently a BLCAD
target dependency.

## Install dependencies

See `docs/dependencies-ubuntu-24.04.md` for package notes.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 libssl-dev \
  doxygen graphviz
```

## Core build and tests

Complete Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

## Geometry build and tests

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry --output-on-failure
```

## GUI build and tests

The GUI requires Geometry. Complete GUI workflow:

```bash
cmake --workflow --preset dev-gui-build-test
```

Equivalent commands:

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui
ctest --preset dev-gui --output-on-failure
```

The executable output is `blcad`; the CMake target is `blcad_app`. Run it with:

```bash
./build/dev-gui/blcad
```

The `dev-gui` build directory registers the Core, Geometry, and GUI test executables, so
`ctest --preset dev-gui` is the broadest run. Only the discovered GUI cases:

```bash
ctest --preset dev-gui -R '^gui\.' --output-on-failure
```

`CMakeLists.txt` sets `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"` on every discovered GUI test, so
ctest needs no platform prefix. Invoking the test binary directly does:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
```

On Linux the OCCT viewport uses Qt xcb when `DISPLAY` is available and `QT_QPA_PLATFORM` is unset.
Offscreen GUI tests use a logical viewport without X11/GLX. A change to viewport rendering or to
interactive behaviour therefore needs a targeted run under `QT_QPA_PLATFORM=xcb` against the test
binary directly, plus a smoke run of `./build/dev-gui/blcad`; offscreen alone does not prove it.

## Interactive Sketcher focused proof

Blocks 106–121 are implemented and accepted.

Block 106 workspace and command lifecycle:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-workspace]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-command-lifecycle]"
```

Block 107 plane interaction:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-hit-test]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-snap]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-box-selection]"
```

Block 108 shared topology and migration:

```bash
./build/dev/blcad_core_tests "[core][sketch-topology]"
./build/dev/blcad_core_tests "[core][sketch-edit-command]"
./build/dev/blcad_core_tests "[core][sketch-json-migration]"
```

Block 109 deterministic planar solver:

```bash
./build/dev/blcad_core_tests "[core][sketch-solver]"
./build/dev/blcad_core_tests "[core][sketch-dof]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
```

Block 110 solver-backed drag:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-drag]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

Block 111 basic creation — the Block-133 shell reset replaced the MVP-8 creation binder and its
tests; line, circle, and rectangle creation is now proved through the shell tool surface:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create]"
./build/dev-geometry/blcad_geometry_tests "[gui][sketch-profile-selection]"
```

Block 112 conic and slot creation uses the shell tool surface and exact spline spans:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-create-conics]"
```

These focused cases cover persistent creation, degenerate-input rejection and tangent references.
They do not close the remaining Block-136 feature coverage work recorded in
`docs/gui-shell-reset-sequence-mvp9r.md`.

Block 113 spline editing and Sketch text:

```bash
./build/dev/blcad_core_tests "[core][sketch-spline-edit]"
./build/dev/blcad_core_tests "[core][sketch-text]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-spline-edit]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-text]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-spline]"
```

Block 114 manual/automatic constraints, conflict preview, and glyph interaction:

```bash
./build/dev/blcad_core_tests "[core][sketch-constraints]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-auto-constraint]"
```

Block 115 driving/reference dimensions, typed parameters, expressions, and annotation editing:

```bash
./build/dev/blcad_core_tests "[core][sketch-dimensions]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-dimensions]"
./build/dev/blcad_core_tests "[integration][sketch-expression-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The Block-115 proof covers all nine family signatures, deterministic sidecar roundtrip, typed
Length/Angle parameter compatibility, direct and expression-backed value changes, driving/reference
semantics, calibrated arc length, stable semantic dimension annotations, exact endpoint hit roles,
atomic add/edit/rebind/mode history, Save/Open, Qt command registration, and later drag enforcement.

Block 116 trim/extend/split, two-line fillet/chamfer, and remap-or-reject rewrite:

```bash
./build/dev/blcad_core_tests "[core][sketch-modify]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-modify]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-trim-extend]"
```

The Block-116 proof covers trim shorten/split-middle/remove, extend to the next intersection,
line/arc/spline split, fillet tangent-arc and chamfer connector insertion, preserved in-place
constraints, explicit rejection of modifications that invalidate a referencing dimension or profile,
a chamfer result resolving through the Geometry region pipeline, atomic GUI preview/commit with exact
undo/redo, and fail-closed commit against a Block-114 catalog reference.

Block 117 offset, associative projection/include, and break-link conversion:

```bash
./build/dev/blcad_core_tests "[core][sketch-offset-project]"
```

Block 118 transforms, mirror, and Sketch patterns:

```bash
./build/dev/blcad_core_tests "[core][sketch-transform-pattern]"
```

Block 119 region recognition, profile selection, diagnostics, and Finish Sketch:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-regions]"
./build/dev-geometry/blcad_geometry_tests "[integration][sketch-finish]"
./build/dev-gui/blcad_geometry_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
```

Block 120 Interactive Sketch3D (headless Core proof):

```bash
./build/dev/blcad_core_tests "[gui][sketch-3d-edit]"
./build/dev/blcad_core_tests "[integration][sketch-3d-direct-manipulation]"
```

## Interactive Modeling focused proof

Blocks 122 and 123 have no focused proof in the tree. Their cases asserted against the deleted
MainWindow shell — stable Qt object names, the mini toolbar, the ViewCube/bookmark widgets, and the
manipulator overlay binder — and were removed with the Block-133 shell reset
(`docs/gui-shell-reset-sequence-mvp9r.md`). The headless authorities they covered,
`GuiModelingWorkspace` and `GuiViewportManipulatorLayer`, are unchanged and still exercised by the
Block 124–130 controller proofs below. Block 135 re-establishes manipulator overlay coverage against
`ShellWindow`; Block 136 restores camera navigation through `[block136-navigation]` and
`[shell-combined-navigation]`. Context/filter/repeat presentation remains open.

The Block 124–130 proofs below are headless controller tests and are unaffected by the shell reset.

Block 124 interactive Extrude, path Extrude, and Revolve authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][extrude-revolve]"
```

These cases verify extent-parameter driving with one atomic transaction and exact undo, operation/
taper/thin/through-all modes with their offered handles, fail-closed rejection of invalid inputs,
full/angle/symmetric revolve authoring, and manipulator/typed document equivalence. The dedicated
`[gui][interactive-extrude]`, `[gui][interactive-revolve]`, and
`[integration][extrude-revolve-manipulator]` cases asserted against the deleted MainWindow shell and
were removed with the Block-133 shell reset; Block 135 restores their replacement-shell path through
the focused `[gui][shell-features]` proof.

Block 125 interactive Fillet, Chamfer, Shell, and Draft authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-shell-draft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][edge-chain-picking]"
```

These cases verify ordered edge collection with duplicate rejection and exact undo, the three chamfer
modes with their offered handles, shell direction toggle and thickness, draft pull/neutral/angle
authoring, fail-closed rejection of invalid inputs, and manipulator/typed document equivalence for
fillet radius and coordinator-routed shell thickness.

Block 126 interactive Pattern, Mirror, Body Boolean, and Body Transform authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-pattern-mirror]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-body-operation]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][pattern-ghost-preview]"
```

These cases verify linear/circular pattern parameter driving with exact undo, mirror authoring,
boolean role validation and keep toggle, the translate/rotate/scale transform stack, fail-closed
rejection of invalid inputs, and manipulator/typed document equivalence for pattern spacing and
coordinator-routed transform translation.

Block 127 interactive PathCurve, Sweep, and Loft authoring:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-path-sweep]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][ordered-section-picking]"
```

These cases verify ordered connected path collection with duplicate rejection, sweep authoring with
exact undo, the sweep-twist handle and combined guide-plus-twist rejection, loft two-section authoring
with G2 rejection, fail-closed rejection of invalid inputs, and drag-reorder/direct-order document
equivalence.

Block 128 interactive Surface authoring and surface-to-solid conversion:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-surface]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][stitch-diagnostic-overlay]"
```

These cases verify fill authoring with duplicate rejection, extend distance driving and apply,
closed-shell-to-solid fail-closed reporting, fail-closed rejection of invalid inputs, the
tolerance-driven stitch free-edge/gap diagnostic surfaced before commit, a clean coplanar stitch, and
coordinator-routed extend-distance/typed document equivalence.

Block 129 feature edit lifecycle and Core feature-update commands:

```bash
./build/dev/blcad_core_tests "[core][feature-update-command]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][feature-edit]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][feature-edit-recompute]"
```

These cases verify in-place feature update with stable id/position/JSON and preserved downstream and
co-producer edges, dependency-protected remove, double-click family identification, one update
transaction with exact undo and fail-closed rejection, and downstream recompute after an upstream
edit.

Block 130 interactive Assembly placement, relationships, joints, and motion:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-assembly-placement]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-relationships]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][interactive-joint-motion]"
```

These cases verify component and rigid-subassembly placement, translate/rotate triads, one-click state
editing and undo, compatibility-filtered relationship/joint picking, solved relationship candidates,
root-space joint frames, typed slot limits, one-coordinate vector drives, motion HUD solve/DOF state,
atomic pose/intent application, and Spherical drive rejection.

Block 131 integrated acceptance:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][interactive-modeling]"
```

The original `[gui][measure]` and `[integration][modeling-coverage-manifest]` shell proofs were deleted
by Block 133. Their replacement-shell coverage is restored through `[block136-measure]` and
`[integration][gui-feature-coverage]`; commands and bounded acceptance are recorded below.

## Representative existing validation tags

```bash
./build/dev/blcad_core_tests "[core][part-body]"
./build/dev/blcad_core_tests "[core][parameter-expression]"
./build/dev/blcad_core_tests "[core][sketch-update]"
./build/dev-geometry/blcad_geometry_tests "[geometry][revolve-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][surface-stitch]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][document-session]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][model-browser]"
```

The exact test registration in `CMakeLists.txt` is authoritative, and the tags there are the only
authority for what exists — this document lists representative entry points, not a complete set. All
cases live in the single `blcad_gui_tests` executable; no second Qt application instance is
introduced.

## Headless tools

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_project \
  examples/component_instances.blcad.project.json assembly.parameter 1 build/export
./build/dev-geometry/blcad_export_posed_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_export_structured_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_move_joint input.blcad.project.json joint.id 30 output.blcad.project.json
./build/dev-geometry/blcad_analyze_assembly input.blcad.project.json
```

## Public Blocks 122–131 boundaries

Block 133 deleted the MainWindow shell, all seven binders, and the shell-bound test files; the headless
boundaries below are what survived and are consumed unchanged by `ShellWindow`
(`docs/gui-shell-reset-sequence-mvp9r.md`). `gui_interactive_extrude_revolve_binder.hpp` was renamed to
`gui_interactive_feature_coordinator.hpp`: it never was a binder, and it coordinates all nine Block
124–128 controllers, not just Extrude/Revolve.

```text
include/blcad/gui/gui_modeling_workspace.hpp
include/blcad/gui/gui_viewport_manipulator.hpp
include/blcad/gui/gui_interactive_extrude_revolve.hpp
include/blcad/gui/gui_interactive_finishing.hpp
include/blcad/gui/gui_interactive_pattern_body.hpp
include/blcad/gui/gui_interactive_path_sweep_loft.hpp
include/blcad/gui/gui_interactive_surface.hpp
include/blcad/gui/gui_feature_edit.hpp
include/blcad/gui/gui_interactive_assembly.hpp
include/blcad/gui/gui_interactive_feature_coordinator.hpp
include/blcad/gui/gui_measure.hpp
include/blcad/geometry/geometry_measure.hpp
include/blcad/core/part_document.hpp
tests/gui/gui_interactive_finishing_tests.cpp
tests/gui/gui_interactive_pattern_body_tests.cpp
tests/gui/gui_interactive_path_sweep_loft_tests.cpp
tests/gui/gui_interactive_surface_tests.cpp
tests/gui/gui_feature_edit_tests.cpp
tests/gui/gui_interactive_assembly_tests.cpp
tests/gui/gui_interactive_modeling_mvp9_acceptance_tests.cpp
tests/core/feature_update_command_tests.cpp
docs/gui-modeling-workspace-mvp9.md
docs/gui-viewport-manipulators-mvp9.md
docs/gui-interactive-extrude-revolve-mvp9.md
docs/gui-interactive-finishing-mvp9.md
docs/gui-interactive-pattern-body-mvp9.md
docs/gui-interactive-path-sweep-loft-mvp9.md
docs/gui-interactive-surface-mvp9.md
docs/gui-feature-edit-mvp9.md
docs/gui-feature-update-inventory-mvp9.json
docs/gui-interactive-assembly-mvp9.md
docs/gui-measure-mvp9.md
docs/gui-feature-coverage-manifest-mvp9.json
docs/interactive-modeling-mvp9-acceptance.md
```

All Block-122 through Block-128 transient state — command catalogs, verified preselection
capabilities, mini-toolbar state, repeat, filters, Home/bookmarks, camera mappings, handle
descriptors, pointer measurements, hit products, HUD text, edge/face chains, pattern sources, tool
bodies, path segments, loft section order, boundary/surface selections, candidate feature parameters,
and previews — does not change Core/Geometry or any save format. A controller's Apply is the only
mutation, and it flows through one existing `commit_part_transaction`. Block 129 is the single
exception: it adds Core `update_<family>`/`remove_<family>` commands (the `[core][feature-update-command]`
boundary) that preserve ids, ordered position, and JSON shape, consumed by the headless
`GuiFeatureEditController` through the same transaction path.
Block 130 adds no new persistence authority: its transient occurrence candidates, resolved target
capabilities, oriented frames, solved poses, coordinate-control/HUD state, and motion results apply
through existing `commit_project_transaction`, solver-result, and motion-result boundaries.
Block 131 likewise adds no persistent authority: Measure selections/readouts/overlays are transient,
and coverage plus integrated acceptance are checked-in validation artifacts.

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

```bash
find include src tests -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i
```

Formatting a focused change set instead:

```bash
git diff --name-only --diff-filter=ACM -- '*.hpp' '*.cpp' | xargs -r clang-format -i
```

## Clean generated files

```bash
rm -rf build/
```

## Documentation entry points

- `README.md`: repository entry point
- `docs/mvp-plan.md`: numbered status/current block
- `docs/architecture-summary.md`: condensed authority model
- `docs/file-format.md`: historical save-format authority
- `docs/interactive-sketcher-sequence-mvp8.md`: Blocks 106–121 sequence
- `docs/interactive-modeling-sequence-mvp9.md`: Blocks 122–131 sequence
- `docs/gui-modeling-workspace-mvp9.md`: Block-122 selection-first shell contract
- `docs/gui-viewport-manipulators-mvp9.md`: Block-123 manipulator and HUD contract
- `docs/gui-interactive-assembly-mvp9.md`: Block-130 Assembly interaction contract
- `docs/sketch-planar-constraint-solver-mvp8.md`: solver mathematics and dimension mappings
- `docs/gui-sketch-solver-drag-mvp8.md`: constraint/dimension-aware direct manipulation
- `docs/gui-sketch-constraint-authoring-mvp8.md`: Block-114 constraint/glyph contract
- `docs/gui-sketch-dimension-authoring-mvp8.md`: Block-115 dimension/expression contract

## Current development boundary

`docs/mvp-plan.md` is the status authority; this section only names the current build/test boundary.

Interactive Sketcher MVP-8 (Blocks 106–121) and Interactive Part & Assembly Modeling MVP-9 (Blocks
122–131) are implemented and accepted. GUI Shell Reset MVP-9R replaces the Qt shell those phases grew:
Blocks 132 (QOpenGLWidget viewport), 133 (`ShellWindow` + `ShellRibbon`, origin seeding), 134
(`ShellSketchTools`), and 135 (`ShellFeatureTools`, Extrude/Revolve, manipulator overlay) are
implemented; Block 136 restores full bounded feature coverage and is accepted with full offscreen,
native Qt/OCCT and closure checks. Block 137 adds verified STEP source intent, freshness and shell
management. Block 138 adds the verified STEP/XDE reader and read-only geometry inspection.
All Blocks through 140 are complete; Block 141 (EditableBody downstream modeling) is in progress
after PR #132 is merged and main is verified.
Source contracts and acceptance evidence live in `docs/step-import-sequence-mvp10.md`; the completed shell milestone
lives in `docs/gui-shell-acceptance-mvp9r.md`. Dated proof sections below retain their original
incremental status.

## GUI Shell Reset focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][viewport]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][ribbon]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][origin]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-entry]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-sketch]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-features]"
```

Block 132 proves the QOpenGLWidget render surface and the single-repaint funnel; Block 133 proves the
ribbon, model browser with its origin folder, document lifecycle, selection and revision-gated refresh,
and origin seeding; Block 134 proves the shell-owned draw/constraint/dimension/modify tool surface and
solver-backed dragging. Block 135 proves persistent profile handoff/reselection, strict numeric
units, pointer-drag numeric override and rollback, candidate-only auto-provisioning, mutation-free
Cancel, recomputed Extrude/Revolve results, and atomic Undo.

### Block 136 finishing slice

```bash
./build/dev-gui/blcad_core_tests "[rectangular-finishing]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The shell proof covers Fillet, three Chamfer modes, Shell and Draft through actual actions and task
controls, current-body multi-selection, recomputed solid results, pending parameter isolation,
Cancel, rejection/rollback, atomic Undo/Redo and Save/Open. The native-only case also checks actual
subshape mouse detection and drag/numeric coupling; it skips explicitly with the offscreen backend.
Optional `BLCAD_UI_CAPTURE_DIR` captures the test windows.

### Block 136 Body pattern and Mirror slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-patterns]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

This proof covers Body multiselection, linear spacing/total extent and direction, circular angles,
Mirror planes, box and closed-spline solid sources, count and scalar rollback, candidate-only
parameters/result Body, Cancel, atomic Undo/Redo and Save/Open. The native-only case checks actual Body picking through a
live preview and spacing/angle/count drag refinement; it skips explicitly offscreen. Optional
`BLCAD_UI_CAPTURE_DIR` captures the test windows.

### Block 136 Body Boolean and Transform slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-body]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

This proof covers Add/Subtract/Intersect, target/tool picking, both result modes, tool retention,
candidate-only NewBody, actual solid results, multi-Body XYZ/rotation/scale, ordered stack retention,
explicit ownership flags, failed-preview isolation, Cancel, atomic Undo/Redo and Save/Open. Native
checks exercise actual Boolean target/tool picks, multi-Body detection, drag/numeric refinement and
Escape rollback; offscreen skips those two cases explicitly. Optional `BLCAD_UI_CAPTURE_DIR` captures
the preview windows.

### Block 136 PathCurve, Sweep and Loft slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
./build/dev-gui/blcad_geometry_tests "[block136-spatial]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The proof covers connected/reversed PathCurve creation, canonical path presentation, planar rational
spline trajectories, solid/cut/surface Sweep and Loft, ordered list-model moves, twist, guides,
center paths, G1/G2 handling, fitted spline profiles, candidate-only inputs, failed-preview isolation,
Cancel, atomic Undo/Redo and Save/Open. Native checks exercise profile/path/section mouse picking
and twist drag/numeric refinement; offscreen skips the native case explicitly. Optional
`BLCAD_UI_CAPTURE_DIR` captures the preview windows.

### Block 136 Surface slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-surface],[interactive-surface]"
./build/dev-gui/blcad_geometry_tests "[surface-boundary-fill],[surface-trim-extend],[surface-stitch],[surface-to-solid]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The Surface slice verifies all six commands, candidate-only Bodies/Length parameters, invalid
geometry/numeric rollback, ordered selections, role-specific picking, actual result shapes, exact
Undo/Redo and Save/Open. Geometry proof covers exact planar Fill and recomputed outward extension
frames. Native proof uses real target/boundary clicks, extension dragging, typed refinement and the
visible Apply button. Optional `BLCAD_UI_CAPTURE_DIR` saves review captures. Contract and bounded
limits: `docs/gui-interactive-surface-mvp9.md`. These checks do not close Block-136 acceptance.

### Block 136 Measure slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-measure]"
./build/dev-gui/blcad_geometry_tests "[block136-measure]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-measure]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The slice verifies all four readouts, clipboard and clear, explicit pick roles/filters, cancellation
through all close paths, scope restoration, stale and replaced geometry, byte-identical saved Parts,
unchanged dirty/history state and a known posed Project-component distance. Geometry proof verifies
World-space targets and intersecting-plane witnesses. Native proof opens the ribbon command, clicks
actual model vertices, follows a camera change, selects circular edges/cylindrical faces and closes
the reachable task dock. Optional `BLCAD_UI_CAPTURE_DIR` records review captures. Scope and existing
analytic distance semantics: `docs/gui-measure-mvp9.md`. Block-136 acceptance remains open.

### Sketch operation follow-up

The shell wires pointer dragging, live previews, numeric refinement and Apply/Cancel for transforms,
independent patterns and offsets. Mirror and Symmetric have separate axis and element pick modes.
Projection supports associative references and explicitly unlinked snapshots of other Sketch curves.
Focused regressions:

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-transform-pattern],[core][sketch-offset-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-transform-pattern],[gui][sketch-offset-project],[gui][sketch-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[spline-create-workflow],[sketch-delivery-review]"
./build/dev-gui/blcad_geometry_tests "[sketch-reference]"
```

`gui_shell_sketch_operations_tests.cpp` covers the real ribbon editor as well as the dialog-free
surface, history/persistence, source-point preservation, copied rational profiles, and atomic
constraint/dimension conflict rejection. The spline workflow covers unlimited fit points, right-click
and Enter completion, exact smooth closure, Finish Sketch handoff, helper-line exclusion, region
selection and actual extrusion. The remaining limits are recorded in the operation contracts and
the interactive Sketch coverage manifest.

Sketch UI regressions for visible windows and native OpenGL (requires an authorized local X display):

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review],[gui][viewport]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=2 ./build/dev-gui/blcad_gui_tests "[gui][sketch-ui-review]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[native-viewer-lifecycle]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-mouse-operations],[spline-create-workflow],[sketch-delivery-review]"
```

Set `BLCAD_UI_CAPTURE_DIR` to an optional writable directory to capture the test shell, previews and
operation dialogs for visual inspection. Captures use the test application's own widgets. The suite
checks 1280×800 layout, horizontal command reachability, supported/mixed selection gating, actual
button and Escape events, preview cleanup, and main-window Undo/Redo.
`[native-viewer-lifecycle]` additionally creates, paints and destroys three native shell windows.
It rejects high-severity OCCT teardown messages, including attempts to make a GLX context current
after its top-level drawable has already been destroyed. The shell releases its central viewer while
that drawable and the document session still exist. This case is skipped on the offscreen backend.

### Block 136 Assembly slice

```bash
./build/dev-gui/blcad_geometry_tests "[block136-assembly]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle]"
```

The focused slice covers disposable semantic targets, definition imports, component/rigid-subassembly
insertion, placement and state previews, compatibility-filtered relationships, all joint families,
group solve, ordered slot limits, slider/numeric motion, history and persistence. Native cases also
verify real occurrence picking and translate/rotate/motion handles, numeric refinement, drag Escape,
and reachable task controls. Offscreen explicitly skips the native-only cases; run xcb to cover them.
`BLCAD_UI_CAPTURE_DIR` enables native captures. These checks do not complete Block 136 acceptance.

### Block 136 browser Feature Edit slice

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-feature-edit],[gui][feature-edit],[integration][feature-edit-recompute]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-feature-edit]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The focused shell cases cover all thirteen update-capable families, complete downstream previews and
failure rejection, parameter values/rebinding/formulas, ordered sections and edge membership, optional
Sweep twist, stale tasks and exact history/persistence. The native-only case checks actual tree mouse
double-clicks, scalar drag/numeric refinement, drag Escape and reachable Apply at 1280×800; offscreen
explicitly skips it. The headless proof also rejects attempts to redirect an edit to another feature id.
`BLCAD_UI_CAPTURE_DIR` enables captures. Whole-block acceptance and manifest validation remain open.


### Block 136 analysis and STEP consumers

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-inspect-export]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-inspect-export]"
```

The shell tests cover Part creation/STEP output, flat and structured nested Assembly export, fresh/idle
context guards, preserved targets and source files, read-only collision/clearance/contact/DOF results,
input/model invalidation and history/persistence preservation. The native case checks reachable task
controls at 1280×800 and Qt file-dialog acceptance/cancellation; only that case skips offscreen.
`BLCAD_UI_CAPTURE_DIR` enables captures. These bounded consumers do not complete whole-shell acceptance.

### Block 136 shell coverage inventory

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][shell-coverage],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The inventory checks exact family parity, real registered proof tags/source files, shell entry points,
explicit gap accounting and plan/manifest completion consistency. Its negative controls reject missing
families, deleted tags, headless substitutions, OR masking and false readiness. Inventory validity
is separate from whole-shell acceptance: `docs/gui-shell-acceptance-mvp9r.md` describes the remaining
work and the opt-in `BLCAD_REQUIRE_SHELL_ACCEPTANCE=1` closure gate, which fails while gaps remain.
Run full Core, Geometry and offscreen GUI suites plus native interaction coverage before closing the
block; historical controller tests alone do not establish equivalent shell coverage.

### Block 136 construction and workplane creation

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_geometry_tests blcad_core_tests -j4
./build/dev-gui/blcad_geometry_tests "[geometry][construction-presentation]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-captures QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

The shell cases cover nine creation modes, normalized directions, stable source selectors, disposable
Geometry previews, degenerate-input rejection, parameter-plus-plane atomicity and existing bindings,
exact Undo/Redo, canonical Save/Open, committed-only saving during preview, context/revision guards,
Escape/dock close and construction/derived planes as Sketch/Extrude inputs. Geometry tests inspect
actual OCCT bounds for resolved points, lines, derived axes and offset/generated workplanes.
The native case checks real ribbon mouse input, pending numeric keyboard input consumed by Apply,
reachable Apply and OCCT preview at 1280×800; only that case skips offscreen.
`BLCAD_UI_CAPTURE_DIR` enables captures.
Remaining relation/pick workflows keep the datum family and Block 136 open; see
`docs/gui-sketch-workbench-mvp7.md` and `docs/gui-shell-acceptance-mvp9r.md`.

### Combined replacement-shell UI workflows

```bash
cmake --build --preset dev-gui --target blcad_gui_tests -j4
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-combined-workflow]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-combined-workflows QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-combined-workflow]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[block136-construction-picking],[shell-mixed-path-loft],[block136-construction-relations],[block136-construction-edit],[shell-combined-workflow],[gui][shell-coverage],[block136-construction],[block136-inspect-export],[block136-feature-edit],[block136-assembly],[block136-measure],[block136-surface],[block136-spatial],[block136-body],[block136-patterns],[block136-finishing],[sketch-ui-review],[gui][viewport],[native-viewer-lifecycle],[shell-features]"
```

`tests/gui/gui_shell_combined_workflow_tests.cpp` combines long interaction sequences in thirteen
scenarios, twelve with fully UI-authored inputs and one with an imported seated Part definition:
machining/edit/pattern/transform/Measure/STEP, Boolean/scale/Mirror with history branching,
multi-workplane Loft section ordering/editing, authored-Part import/Assembly placement/Mate/solve/
analysis/STEP, invalid/cancelled tasks plus dirty-file and document-switch recovery, and workplane
editing with cyclic-reference recovery and downstream solid recompute, plus generated-reference
construction chains feeding a second Sketch/Extrude and later upstream edits, and construction grips
rotating a workplane and moving its downstream solid, plus a typed Sketch3D chain feeding a
PathCurve/Sweep with endpoint editing and four-step history. Three further scenarios sew authored
solid faces into a shell/solid/Boolean chain, edit a revolved source through circular pattern and
rotation/scale consumers, and import the retained seated-Part example to author/drive all three
coordinates of a bounded Planar joint with branching motion history, analysis and Assembly exports.
The latter does not claim missing dedicated Hole-authoring coverage. A thirteenth workflow copies
UI-authored planar endpoints into Sketch3D and drives a PathCurve/Sweep through rejected/corrected
edits, history, canonical reopening and STEP export.

The shared harness operates Qt ribbon buttons and popup menus, task widgets, browser clicks and
double-clicks, viewport Sketch picks/hover, Escape and absolute/relative numeric coordinates, and
actual Qt input/file/confirmation dialogs. Native combo selection establishes and confirms native
window focus, synchronizes the desktop pointer and window-system events before opening/hovering,
awaits a visible popup with a two-second deadline, then checks the hit-tested row and its actual
highlight before the click. This preserves mouse coverage without direct index
assignment or timing sleeps. It does not seed Core intent or invoke authoring
controllers/workbenches. Assertions observe canonical
intent, resolved volumes/solid counts/bounds, source/result identities, task/preview/filter cleanup,
multi-step Undo/Redo and redo-branch truncation. Save/Open/Save compares complete bytes, including GUI
sidecars; exports preserve source bytes. Dialog automation has bounded watchdogs, and native input
checks widget reachability at 1280×800. Both backends execute all thirteen scenarios; these tests introduce
no native-only skips. The native run is required to prove actual OCCT presentation.

These bounded combined workflows extend the shell evidence. Remaining families, manipulator/input
variants and whole-block acceptance stay tracked in `docs/gui-shell-acceptance-mvp9r.md`.

The three extended cross-family workflows can also be run on their own:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-combined-surface-solid],[shell-combined-revolve-pattern],[shell-combined-planar-motion]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-complex-workflows QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-combined-surface-solid],[shell-combined-revolve-pattern],[shell-combined-planar-motion]"
```

The input harness edits numeric widgets and joint-limit table cells through keyboard events and
selects actual popup/list rows. These tests reject open-shell conversion, recover invalid pattern
counts and joint limits, compare every committed history checkpoint, and verify canonical file bytes
and read-only consumer/export behavior. Run native GUI tests serially with other GUI suites and
application startup.

The Surface/Boolean chain also guards the shared Body task's field geometry after a transform.
Wrapping guidance stays outside its conditional form rows: the reference Qt 6.4.2
[height-for-width pass](https://github.com/qt/qtbase/blob/v6.4.2/src/widgets/kernel/qformlayout.cpp#L479)
can otherwise process stale indices of hidden rows. The test must reach the actual result-mode
popup at 1280×800. Keep this input proof when changing the task layout.

### Block 136 construction and workplane editing

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_core_tests "[core][construction-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-edit],[block136-construction],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-edit QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-edit],[block136-construction]"
QT_QPA_PLATFORM=xcb timeout --signal=TERM 6s ./build/dev-gui/blcad
```

Core tests check identity/order preservation, reference rebinds, downstream invalidation, canonical
JSON and atomic rejection of missing references and cycles. GUI regressions reopen all nine modes
through browser double-click, preserve precision/dependencies, reject degenerate downstream geometry,
recompute dependent solids, exercise exact history/persistence and cancel stale tasks. The combined
UI scenario also covers pending numeric text, cyclic-reference correction, three-step history and
STEP via actual widget/dialog input. Run the combined native filter above and full suites for broad
regression evidence. Startup timeout exit 124 is expected; it is an intentional smoke-test stop.

### Block 136 generated construction references and parallel relations

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_core_tests "[core][construction-generated]"
./build/dev-gui/blcad_geometry_tests "[geometry][construction-generated]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-relations],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-relations QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-relations]"
```

Core/Geometry regressions check generated source validation, dependency-ready loading without point
reordering, evaluated three-point and parallel plane frames, upstream parameter changes and evaluated
degeneracy. Shell regressions create/rebind all five added modes, consume generated points in existing
line/plane/axis and Sketch/Extrude workflows, reject missing/degenerate/cyclic inputs, recover/cancel,
and preserve exact history and canonical persistence. The seventh combined UI workflow uses actual
widget input through construction, downstream modeling, reference edits, three-step history, file
roundtrip and STEP, including horizontal task fit with long type labels. Native selectors and
captures use the existing 1280×800 harness; no new native-only
skips are introduced. Native reference collection/manipulators still keep Block 136 open.

### Mixed-path Loft and Sweep verification

```bash
cmake --build --preset dev-gui --target blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_geometry_tests "[mixed-path-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-mixed-loft QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-mixed-path-loft]"
```

Geometry checks three/five unequal sections on differently oriented/rolled planes along line,
32-span cubic spline and true circular-arc controls. Independent station/boundary assertions cover
closed/open Loft, LoftSurface, guide control and Sweep, plus recompute/persistence and failed edits.
The shell case imports only preauthored Sketch/plane inputs through Open, then uses actual Qt input
for path collection/reversal, five-section ordering, invalid/recovered previews, Loft Apply, middle
width property editing, three-step history, canonical reopen and STEP. This imported-input boundary
is separate from the nine fully UI-authored `[shell-combined-workflow]` scenarios. Add the distinct
`[shell-mixed-path-loft]` tag to the combined native regression above; both backends run it without
new skips. Run native GUI suites and the application startup smoke sequentially so a second window
cannot disturb popup focus. Contracts and current limits: `docs/part-guided-loft-geometry-mvp6.md` and
`docs/gui-interactive-path-sweep-loft-mvp9.md`. Full-suite evidence lives in
`docs/gui-shell-acceptance-mvp9r.md`; Block 136 remains open.

### Native construction reference collection

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j4
./build/dev-gui/blcad_geometry_tests "[geometry][construction-picking]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-picking],[block136-construction],[block136-construction-edit],[block136-construction-relations],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-picking QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-picking]"
```

Geometry checks the typed 26-source catalogue, bounded face area and upstream dimension changes.
Shell tests collect/revisit reference roles, reject/recover repeated defining points, edit generated
sources, preserve history/reopen and restore custom masks/scopes after Apply, Escape, close, Save and
revision changes. Hidden-Body and explicit numeric definitions cannot receive unintended topology
picks. The native case clicks actual model locations for three defining points, a parallel edge,
vertex point and face plane, then checks four-step history and canonical reopen at 1280×800; only
that case skips offscreen. Inspect captures and run the expanded combined native filter above, then
the startup smoke sequentially. The source-feature geometry contract and remaining construction
manipulators are documented in `docs/gui-sketch-workbench-mvp7.md`; Block 136 remains open.

### Construction position, direction/frame and new-offset grips

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j4
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[shell-combined-manipulators],[construction-manipulator-camera],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-construction-manipulators QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction-manipulators],[shell-combined-manipulators],[construction-manipulator-camera]"
```

The same Qt mouse/key cases execute on both backends: XYZ position, composed direction/frame
rotations with full-precision persistence, new-offset normal/bindings, numeric override, final release,
Escape/focus/camera rollback, dependent-degeneracy recovery and capture cleanup on task expiry. The
eighth combined UI workflow adds rotated-plane Sketch/Extrude, downstream preview on same-ID plane
movement, exact history, canonical reopen and STEP. Native tests preserve the fitted OCCT zoom factor;
headless tests restore a useful model-span camera for their mapping. Include the new tag in the
combined native filter above and run startup separately. Contracts and remaining acceptance:
`docs/gui-sketch-workbench-mvp7.md`, `docs/gui-shell-acceptance-mvp9r.md`.


## Explicit reference-repair checks

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-reference-repair]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-reference-repair QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-reference-repair]"
```

Run the native application smoke separately from GUI tests to preserve popup focus. The input case
opens authored fixtures, repairs point/line sources using actual ribbon/popup/button input, cancels
another preview, and checks history plus save/reopen. Initial-load failures, repair batches and
constraint-diagnostic suggestions remain open; see `docs/gui-sketch-workbench-mvp7.md` and the
Block-136 coverage manifest. The manifest continues to report the family as partial.


## Reference-repair list checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repair-batches QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-reference-repair-batches],[block136-reference-repair]"
```

The batch input workflow repairs two lost planes and their projected inputs through actual ribbon,
popup, list and button events. It exercises a failed partial preview, five queued changes,
remove/restage, whole-list preview/Apply, one-step history and canonical save/reopen. Retained
single-reference proofs run alongside it. Run native tests and the application startup smoke
serially. Repair-time loading and constraint-diagnostic controls remain the next documented work.


## Repair-time opening checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repair-open QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-repair-open]"
```

The new session cases verify admission, validation, cache/freshness guards, repair transactions,
failed-state history and canonical persistence. The native input case opens a failed-recompute Part
through the real file dialog, repairs two plane-driven solids, checks Preview/Fit/Apply and Undo/Redo,
then saves and reopens a fresh Part through file controls. Run native tests and startup serially.
The bounded missing-source loading extension is verified separately below.


## Missing-source recovery checks

```bash
./build/dev-gui/blcad_core_tests "[missing-reference-recovery]"
./build/dev-gui/blcad_geometry_tests "[missing-reference-recovery]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[missing-reference-session],[block136-missing-reference],[repair-open-session],[block136-repair-open],[block136-reference-repair-batches],[block136-reference-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-missing-reference QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[missing-reference-session],[block136-missing-reference]"
```

The fixture removes one producer and both construction source records, leaving six typed references,
constraints, a generated line and two plane-driven solids. Checks cover strict ordinary loading,
bounded admission, unchanged canonical failed intent, wrong-kind/cyclic/invalid rejection, partial
staging/Preview failures, complete atomic repair, cache protection, failed-state history and saving.
The shell input case uses actual file dialogs, popup/list/buttons, Fit, Undo/Redo and canonical
Save/Open. Run native tests and application startup serially. Constraint-diagnostic suggestion
controls remain the next work; Block 136 remains in progress.


## Explicit Sketch diagnostic repair checks

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch-diagnostics],[gui][sketch-repair],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch-diagnostics QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch-diagnostics]"
```

The fixture combines a solid, raw lines/spline, all seven Core diagnostic kinds, supported and
manual suggestions, three Sketches and accepted constraint/dimension sidecars. Tests exercise
read-only inspection, all three supported repairs, catalog preservation, unsupported actions and
identity collisions, preview invalidation, cancellation/Save/history/context guards and restoration
of an active Sketch. The native input case uses actual model/Sketch ribbon buttons, diagnostic-list
clicks, Preview/Cancel/Apply, Fit, Undo/Redo and Save, with canonical reopening. Run native tests and
application startup serially. The subsequent typed Sketch3D slice is documented below; see
`docs/mvp-plan.md` for Block 136 status.


## Typed Sketch3D point/line checks

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-3d-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d],[shell-combined-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d],[shell-combined-sketch3d]"
```

Core checks stable order/dependencies, canonical roundtrip and atomic rejection of missing sources,
removed segments and disconnected junctions. Shell checks typed staging, shared endpoints, retained
curved/guide intent and precision, explicit binding detachment, invalid geometry, history and task
expiry. The ninth fully UI-authored combined workflow builds a four-point chain, PathCurve and Sweep,
rejects collapse, extends the swept solid, exercises four-step history, cancels, reopens and exports
STEP. Both backends run all new scenarios without skips. Include both new tags in the combined native
regression above; run application startup separately. Contract and remaining native Sketch3D gaps:
`docs/gui-sketch3d-interaction-mvp8.md`. Block 136 remains in progress.

## Sketch3D viewport collection and direct point grips

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-grips],[shell-sketch3d-grips-workflow],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-grips QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-grips],[shell-sketch3d-grips-workflow]"
```

The same Qt input cases execute on both backends: candidate point/line roles, all seven grip-direction
modes, precise parameter detachment, raw final release, numeric override, Escape/camera/size/focus
rollback, invalid/recovered downstream candidates and stale task cleanup. The combined Sweep workflow
now picks the endpoint and drives a locked Z grip through rejected collapse and corrected solid
recompute, followed by exact history, canonical reopening and STEP. Include both new tags in the
combined native filter above, inspect the 1280×800 capture and run application startup serially.
Offscreen camera mapping uses a model span; native retains the fitted OCCT zoom factor. Contract,
bounded projection-copy semantics and block status:
`docs/gui-sketch3d-interaction-mvp8.md`, `docs/mvp-plan.md`.

## Anchored Sketch3D pointer placement

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-placement],[shell-sketch3d-placement-workflow],[block136-sketch3d-grips],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-placement QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-placement],[shell-sketch3d-placement-workflow]"
```

The same input cases cover all seven locks in orthographic and perspective views, copied bound and
local anchors, hover versus final-release staging, preserved earlier previews, pending spin text,
unobservable directions, Escape/camera/resize/focus/control changes and save/revision/document expiry.
The combined workflow authors a profile and spatial chain through actual ribbon, popup, viewport,
numeric and file input, then checks PathCurve/Sweep recompute, rejection/recovery, history and STEP.
Native pixel quantization is bounded using projected model axes; typed refinement keeps downstream
dimensions exact. Native Sketch3D fixtures await actual window activation through a conditional
event loop with a two-second deadline, retaining the active-window assertion before pointer input.
Inspect `sketch3d-point-placement-hover.png` at 1280×800. Include the two new tags in
the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md` and run startup separately.
Pointer depth/anchor/copy semantics and the planar-copy contract:
`docs/gui-sketch3d-interaction-mvp8.md`, `docs/mvp-plan.md`.

## Sketch3D distance/direction checks

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[gui][sketch-3d-edit],[integration][sketch-3d-direct-manipulation],[core][sketch-3d-update]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-direction],[shell-sketch3d-direction-workflow],[block136-sketch3d-placement],[block136-sketch3d-grips],[block136-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-direction QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-direction],[shell-sketch3d-direction-workflow]"
```

The new cases cover precise copied bound/local anchors, tuple conventions, numeric/text/coordinate
bounds, invalid/corrected candidates, competing authoring, live-gesture rollback, pending XYZ,
history, persistence and task expiry. The combined input workflow mixes XYZ, pointer and direction
placement, rejects an incomplete tuple, then authors PathCurve/Sweep and retains downstream grip
recovery, four-step history, canonical reopening and STEP export. Include the two new tags in the
canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`. Inspect
`sketch3d-distance-direction-input.png` at 1280×800 and run startup separately, with builds and GUI
executions serial. The planar-point copy extension is documented below; Block 136 stays open.

## Explicit planar-point copying into Sketch3D

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[gui][sketch-3d-edit],[integration][sketch-3d-direct-manipulation],[core][sketch-3d-update]"
./build/dev-gui/blcad_geometry_tests "[geometry][sketch-3d],[geometry][sketch-reference],[geometry][workplane]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-projection],[shell-combined-projected-sweep],[block136-sketch3d-direction],[block136-sketch3d-placement],[block136-sketch3d-grips],[block136-sketch3d],[shell-combined-sketch3d],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-sketch3d-projection QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-sketch3d-projection],[shell-combined-projected-sweep]"
```

The same focused cases check four plane orientations, projected construction points, cached
generated-face/semantic-vertex sources, copied precision, stable identities, per-copy consent,
pending-input guards, gesture rollback, bounds, independence after source editing and canonical
history/persistence/expiry. The thirteenth combined workflow authors both planar sources through
actual Sketch input, copies endpoints and adds a direction point, then checks PathCurve/Sweep,
failed/corrected edits, four-step history, canonical reopening and STEP. Its source rectangle hover
stays beyond the origin-axis snap tolerance and source rows use observed stable entity identities.
Inspect `sketch3d-planar-copy-controls.png` and `sketch3d-planar-copy-provenance.png` at 1280×800.
Include both new tags in the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md`.
Builds, GUI runs and the separate startup smoke remain serial. Numeric/target/copy contracts live
in `docs/gui-sketch3d-interaction-mvp8.md`; Block 136 and remaining work live in `docs/mvp-plan.md`.

## Standalone Part parameter management

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
./build/dev-gui/blcad_core_tests "[core][parameter],[core][parameter-expression]"
./build/dev-gui/blcad_geometry_tests "[geometry][parameter-expression]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-parameters],[shell-combined-parameters],[gui][shell][property-editor],[sketch-dimension-workflow],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-parameters QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-parameters],[shell-combined-parameters]"
```

Core checks later-input formula loading, re-derived values/edges, authored order and strict rejection
of missing names/cycles/invalid units in both loading modes. Shell cases cover all literal types,
formula chains, full precision, global graph-ID collisions, pending input, duplicates/type/unit/cycle
rejection, complete downstream geometry, correction, one-step batch history and session expiry.
The fourteenth combined workflow authors a Sketch/Extrude and parameter batch through actual ribbon,
popup, field, button, browser and file input, rebinds the extrusion to a formula, checks 2400→3600→9600
mm³ volumes, recovers from invalid formulas/count text, exercises three-step Undo/Redo, cancels local
changes, reopens canonically and exports STEP. Thirteen combined workflows have fully UI-authored
inputs; Planar motion retains its imported seated Part boundary. Inspect
`parameter-manager-preview.png` and `parameter-manager-usage.png` at 1280×800. Include both new tags
in the canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`, then run startup separately.
Contracts and remaining work: `docs/parameter-model.md`, `docs/mvp-plan.md`.

## Open-line Thin Extrude authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-thin-extrude],[shell-combined-thin-extrude],[block136-extrude-extents],[block136-path-extrude],[gui][extrude-revolve],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-thin-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[native-projection-precision],[block136-thin-extrude],[shell-combined-thin-extrude]"
```

The focused cases cover all three wall modes and signed bounds on XY/XZ/YZ, every Body operation,
all seven extents, diagonal/reversed lines on a tilted offset plane, invalid text/recovery,
independent thickness/depth rollback, semantic selection,
pointer Escape in an oblique view with distinct handle projections, bounded input rejection,
global identity collisions, Cancel, document replacement
and canonical reopening. The nineteenth combined workflow authors a plate and two single-line
Sketches through actual UI input, cuts a mid-plane groove and a one-sided through slot, creates a
separate two-sided symmetric wall and edits an earlier thickness through the parameter manager.
It edits the earlier feature's name while retaining thin intent and checks downstream volumes,
exact five-step history, cancellation, canonical reopening and STEP.
Eighteen combined workflows have fully UI-authored inputs; Planar motion retains its imported Part.

Inspect `thin-midplane-cut-preview.png`, `thin-two-sided-new-body-preview.png` and
`thin-through-all-cut-preview.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`, then run startup separately. Keep builds, GUI runs and startup
serial. The one-line boundary and remaining native limit collection are recorded in
`docs/gui-interactive-extrude-revolve-mvp9.md` and `docs/mvp-plan.md`.

## Path-directed Extrude authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "[core][path-extrude]"
./build/dev-gui/blcad_geometry_tests "[geometry][path-extrude]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-path-extrude],[shell-combined-path-extrude],[block136-extrude-extents],[gui][extrude-revolve],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-path-extrude-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-path-extrude],[shell-combined-path-extrude]"
```

The focused shell cases cover all four Body operations on XY/XZ/YZ, a line/spline/reversed-arc
trajectory, path/straight switching, unused input suppression, stale handles, missing selection,
excluded closed paths, incompatible orientation with recovery, Cancel, document replacement and
canonical reopening. The eighteenth combined workflow authors a plate, separate pocket/tool
profiles and a three-line shared-point Sketch3D path through actual UI
input. It applies a pocket and path Cut, edits the earlier pocket through the downstream route,
rejects a coincident endpoint and corrects it, checks exact four-step history, cancels a switched
draft, reopens and exports STEP. Seventeen combined workflows have fully UI-authored inputs;
Planar motion retains its imported Part boundary.

Inspect `path-extrude-cut-preview.png`, `path-extrude-downstream-edit-preview.png` and
`path-extrude-return-to-normal-preview.png` at 1280×800. Include both tags in the canonical native
regression in `docs/gui-shell-acceptance-mvp9r.md`, then run native startup separately. Keep builds,
GUI runs and startup serial. Contract and remaining work:
`docs/gui-interactive-extrude-revolve-mvp9.md`, `docs/mvp-plan.md`.

## Dedicated through-all hole authoring

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app -j6
./build/dev-gui/blcad_core_tests "*hole*,[core][extrude-extent]"
./build/dev-gui/blcad_geometry_tests "*circular*,[geometry][extrude-extent],[geometry][workplane]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-holes],[shell-combined-holes],[gui][part-foundation],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-holes QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-holes],[shell-combined-holes]"
```

The same new shell cases execute on both backends: single/circular creation; XY/XZ/YZ and tilted
construction planes; generated-face bounds; global ID/name collisions; literal/formula bindings,
typed/count limits and preserved local text; no-op/erased-solid rejection; whole-model previews,
atomic history, sidecars, canonical reopening and session expiry. The fifteenth combined workflow
uses actual ribbon, popup, field, browser, button and file input to author an 80×60×12 plate, Fillet,
four-hole pattern, parameter-driven six-hole recompute and an additional single hole sharing the
existing diameter. It exercises rejected/corrected inputs, browser workplane selection during the
task, four-step Undo/Redo, cancellation, canonical reopening and STEP. Fourteen combined workflows
have fully UI-authored inputs; Planar motion retains its imported seated Part boundary.

Inspect `hole-circle-preview.png` and `hole-single-bound-preview.png` at 1280×800. Include both new
tags in the canonical native regression in `docs/gui-shell-acceptance-mvp9r.md`, then run startup
separately, with builds and GUI executions serial. Contract and remaining extent-editor work:
`docs/gui-part-foundation-workbench-mvp7.md`, `docs/mvp-plan.md`. The semantic Hole Wizard is not
claimed by these plain cylindrical cut controls.

## Native Extrude limit-face collection regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-extrude-limit-picking],[shell-combined-extrude-limit-picking],[block136-extrude-extents],[block136-thin-extrude],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-limit-face-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-extrude-limit-picking],[shell-combined-extrude-limit-picking]"
```

The focused cases cover typed source identities, all three origin-plane normals, both limit roles,
transverse/coincident failure recovery, selection scope/highlights, Apply/Cancel/Escape, mode changes,
Undo and document replacement. The native case clicks actual Right/Top/Bottom source planes; it skips
offscreen. The combined Qt-input workflow authors a plate, stop Body, pocket, ToFace cut and independent
Between Thin wall; native runs click actual source-plane locations and offscreen uses the visible face
combos. Source-depth changes, an earlier-feature edit, exact history, canonical reopening and STEP
exercise the full chain. Inspect `extrude-native-both-limits.png`, `extrude-picked-to-face-cut.png` and
`extrude-picked-between-thin-wall.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`. Keep builds, GUI runs and separate native startup serial.

## ViewCube, Home and named-camera regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-navigation],[shell-combined-navigation],[gui][viewport],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-navigation-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-navigation],[shell-combined-navigation]"
```

Both backends execute all new cases. The focused tests compare independent standard camera axes,
full Home/bookmark restoration and projection state; guard Sketch/grips and preserve invalid previews,
reference scopes, history and document replacement. The combined Qt-input workflow covers named views,
Sketch re-entry, ViewCube popup input, real drag/Escape after camera restoration, cut/source-depth
edits, exact history, canonical reopening and STEP. Inspect `navigation-machining-preview.png` and
`navigation-bookmarks-after-history.png` at 1280×800. Include both tags in the canonical native command
in `docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Visible model selection-filter regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-selection-filters],[shell-combined-selection-filters],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-selection-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-selection-filters],[shell-combined-selection-filters]"
```

Both backends exercise the eight-filter semantic matrix including All/Vertex, browser fallback
rejection, task/Sketch/capture guards, task-owned Edge/Datum collection, history and document changes.
The combined workflow authors a plate, corrects invalid Fillet and Hole input under a Body filter,
changes the source depth, verifies exact history, re-enters Sketch, reopens canonically and exports
STEP. Native additionally clicks the actual Body through the filtered OCCT context. Inspect
`selection-filter-finishing-preview.png`, `selection-filter-hole-preview.png` and
`selection-filter-after-history.png` at 1280×800. Include both tags in the canonical native command in
`docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Last accepted modeling-command repeat regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-repeat],[shell-combined-repeat],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-repeat-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-repeat],[shell-combined-repeat]"
```

The focused cases distinguish accepted commands from failed/cancelled candidates, use a different
current profile and fresh Hole/Assembly defaults, guard tasks/Sketch/capture and check Save, exact
history and all document-replacement boundaries. The combined workflow authors two different holes,
rejects invalid placement, applies Fillet, uses Ctrl+R/Escape, edits upstream depth, verifies four-step
history, reopens canonically and exports STEP. Inspect `repeat-hole-preview.png` and
`repeat-after-history.png` at 1280×800. Include both tags in the canonical native regression in
`docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and separate native startup serial.

## Bounded profile-context toolbar regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-profile-context],[shell-combined-profile-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-profile-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-profile-context],[shell-combined-profile-context]"
```

The focused cases validate materialized Core profiles and single-line Thin eligibility, reject hidden
or unmaterialized sources, consume/restore preselection across all cancellation routes, guard competing
interactions and check Save/history/replacement boundaries. The combined workflow uses actual toolbar
input to author a plate, cancel and correct a Cut, add a Thin wall on another plane, edit upstream
depth and an earlier feature, verify five-step history, reopen canonically and export STEP. Native
additionally clicks the actual profile and checks toolbar bounds. Inspect `context-cut-preview.png`,
`context-thin-preview.png` and `context-after-history.png` at 1280×800. Include both tags in the canonical
native command in `docs/gui-shell-acceptance-mvp9r.md`; keep builds, GUI runs and startup serial.

## Sweep/Loft profile-context regression

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-spatial-context],[shell-combined-spatial-context],[block136-profile-context],[block136-spatial],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-spatial-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-spatial-context],[shell-combined-spatial-context],[block136-profile-context]"
```

The five new focused cases exercise remaining-input eligibility, exact nondefault Sweep/Loft source
handoff, all three cancellation paths with actual viewport/session selection and restored filter/scope,
raw-region exclusion, duplicate rejection, three-section atomic Apply, Save and replacement. The combined workflow authors
all three Sketch sections, two offset planes and its trajectory through Qt, cancels an invalid Sweep,
corrects Loft order and G2 input, creates a separate Sweep, edits its twist parameter and the earlier
Loft, verifies four-step history, reopens canonically and exports STEP. Native additionally picks the
actual profile and verifies toolbar bounds. Inspect `spatial-context-loft-preview.png`,
`spatial-context-sweep-preview.png` and `spatial-context-after-history.png` at 1280×800. Include both
new tags in the canonical native command in `docs/gui-shell-acceptance-mvp9r.md`. Keep builds, GUI
runs and separate startup serial.

## Stable Edge/Face finishing-context regression

After building the four targets above, run builds, GUI suites and native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-finishing-context],[shell-combined-finishing-context],[block136-profile-context],[block136-spatial-context],[block136-finishing],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-finishing-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-finishing-context],[shell-combined-finishing-context],[block136-profile-context],[block136-spatial-context],[block136-finishing]"
```

The five focused cases exercise actual shared context buttons, all four finishing commands,
nondefault Body seeding, complete selection/filter/scope restoration, every cancellation route,
empty-target rejection, atomic history, stale/hidden sources, Save and replacement. Explicit Edges or
Faces filters publish idle stable subshape targets; All retains its existing Body/profile picking.
The twenty-sixth combined workflow authors both Bodies and all modeling intent through Qt. It uses
actual native idle OCCT Edge/Face hits, cancels Draft and two-distance Chamfer, applies Shell/Fillet/
angular Chamfer, batches two upstream parameter edits, edits an earlier feature, checks five-step exact
history, reopens and exports STEP. It explicitly skips offscreen; the existing native finishing case
also skips there. Inspect `finishing-context-shell-preview.png`,
`finishing-context-chamfer-preview.png` and `finishing-context-after-history.png` at 1280×800 in the
capture directory. Include both new tags in the canonical native filter in
`docs/gui-shell-acceptance-mvp9r.md`. Block 136 remains open.

## Body pattern and Mirror context regression

Run builds, GUI suites and the native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-pattern-context],[shell-combined-pattern-context],[block136-patterns],[block136-finishing-context],[block136-profile-context],[block136-selection-filters],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-pattern-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-pattern-context],[shell-combined-pattern-context],[block136-patterns]"
```

Five focused cases cover exactly one current Body, a nondefault cylinder, all three commands,
idempotent source seeding, task-owned Body/reference scopes, all cancellation routes, atomic Apply,
history, Save and replacement. The twenty-seventh combined Qt-input workflow cancels/applies Mirror,
creates Circular and Linear patterns, chains the reflected source, stages three upstream parameter
changes together, edits the earlier Mirror, checks five-step exact history, reopens and exports STEP.
Small rectangles first use a larger pointer preview before exact numeric refinement, beyond the
axis-inference tolerance. Offscreen source selection uses the browser; native input hits actual OCCT
Bodies. The existing
native pattern-handle case skips explicitly offscreen. Inspect four 1280×800 captures:
`pattern-context-mirror-preview.png`, `pattern-context-circular-preview.png`,
`pattern-context-linear-preview.png` and `pattern-context-after-history.png` in the capture directory.
Include both new tags in the canonical native filter in `docs/gui-shell-acceptance-mvp9r.md`.
Block 136 remains open.

## Replacement-shell Body contexts — 2026-10-25

After building the four targets above, run GUI suites serially with builds and native startup:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-body-context],[shell-combined-body-context],[block136-body],[block136-pattern-context],[block136-finishing-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-body-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-body-context],[shell-combined-body-context],[block136-body]"
```

Core and Geometry `[feature-order]` regressions cover interleaved producer graph/JSON restoration,
corrupt metadata, legacy compatibility and incremental geometry. Five focused cases verify typed
Boolean target/Transform source handoff, solid versus surface eligibility, idempotence, task-owned
role scopes, cancellation restoration, atomic Apply/history, Save and New/Open replacement. The twenty-eighth combined Qt-input workflow authors two solids,
cancels multi-Body translation, translates the nondefault target into overlap, switches Boolean
targets through the browser, cuts with retained tools, rotates using a picked axis and scales about
an explicit center. It checks unchanged previews, invalid-input recovery, bounds/volumes, exact
four-step history, canonical reopening and STEP. Native runs click actual OCCT Bodies; offscreen
uses browser selection. Inspect `body-context-translate-preview`, `body-context-boolean-preview`,
`body-context-rotate-preview` and `body-context-scale-preview` at 1280×800. Existing native Body
manipulator cases skip explicitly offscreen. Full-suite and native evidence live in
`docs/gui-shell-acceptance-mvp9r.md`. Surface/Assembly contexts remain next; Block 136 stays open.

## Replacement-shell Surface contexts — 2026-10-25

Build the four targets above before running GUI suites and native startup serially:

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-surface-context],[shell-combined-surface-context],[block136-surface],[block136-body-context],[block136-finishing-context],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-surface-context-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface-context],[shell-combined-surface-context],[block136-surface]"
```

Six focused cases cover exact typed path/edge/surface handoff, all six commands, stable solid roles,
key/kind/scope guards, idempotence, cancellation selection/filter restoration, task-only subshape
cleanup, atomic Apply/history, Save and New/Open. The twenty-ninth combined Qt workflow authors its
box, closed PathCurves and trim profile through input events, cancels Boundary, applies Fill/Trim/
Extend, rejects an open shell, sews six surfaces and converts to solid. It checks duplicate/removal/
reordering controls, upstream depth edits, bounds/volumes, six-step exact history, canonical reopening
and STEP. Browser selections use the actual widgets on both platforms. Existing native Surface
regressions also hit actual surfaces/edges and drag/refine the extension handle; their offscreen
skip is explicit. Inspect `surface-context-fill-preview`, `surface-context-trim-preview`,
`surface-context-extend-preview`, `surface-context-stitch-preview` and `surface-context-solid-preview`
at 1280×800. Full evidence and remaining acceptance: `docs/gui-shell-acceptance-mvp9r.md`. Assembly
contexts remain next; Block 136 stays open.


## Final Assembly shell and acceptance proof — Block 136

Build all four GUI-preset targets before running these suites. Keep source edits, builds, GUI runs
and separate native startup serial. The Core/Geometry binaries may run alongside one GUI suite.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[shell-combined-assembly-context],[shell-combined-assembly-hierarchy],[block136-assembly],[block136-inspect-export],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-assembly-acceptance-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[shell-combined-assembly-context],[shell-combined-assembly-hierarchy],[block136-assembly],[block136-inspect-export]"
```

The context cases exercise exact occurrence/target handoffs, all cancellation paths, guards, history
and Save during disposable active drafts. Advanced cases cover hierarchy validation, occurrence-qualified create/edit
and solve, all five joint families, four driven families and repeated definition authority. Analysis
cases cover local/cross Revolute samples, occurrence links, repeated clicks, read-only lifecycle,
preselection restoration and actual native framebuffer highlight/clear/reselect transitions.
Two new integrated Qt workflows combine these surfaces with real file
import, invalid candidates, branching history, canonical reopening and flat/structured STEP.

The canonical full/native command and final closure gate live in
`docs/gui-shell-acceptance-mvp9r.md`. Use its latest evidence section for actual results and omissions.

## STEP source identity and GUI proof — Block 137

Configure again after adding OpenSSL Crypto, then build the four targets serially before tests:

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests -j6
./build/dev-gui/blcad_core_tests "[step-import-source],[step-import-json],[step-import-freshness]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[step-import-workbench],[block137-step-import],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad137-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block137-step-import]"
```

The complex shell cases combine existing Sketch/Extrude modeling, explicit modes, preview invalidation,
external file changes, refresh, exact history, Save/Open and complete-folder relocation. They also
exercise missing/unreadable recovery, nested Project intent, source-owner isolation, native file
chooser acceptance/cancellation and 1280×800 task layout. Headless GUI checks cover stale/forged
evidence, no-op refresh, definition loading, Save As and document/sidecar/STEP overwrite protection.
Core checks cover canonical identity/JSON, SHA-256 known vectors, filesystem boundaries and stale
refresh evidence. These source-management fixtures do not claim STEP geometry reader coverage.

Run the full Core, Geometry and offscreen GUI suites and append `[block137-step-import]` to the
canonical native shell filter from `docs/gui-shell-acceptance-mvp9r.md`. Keep GUI runs and separate
native application startup serial; keep production/test sources frozen throughout completed runs.
Actual acceptance results are recorded in `docs/step-import-sequence-mvp10.md`.

## Automatic Loft section refinement follow-up

The 2026-10-26 Loft follow-up accepts unequal section edge counts through exact derived Geometry
subdivision. It does not rewrite Sketch entities or add JSON fields. Numbered STEP import progress
remains through Block 137; Block 138 is still planned.

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_geometry_tests "[loft-section-compatibility],[loft-feature],[multi-section-loft],[guided-loft],[mixed-path-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-loft-auto-segments],[shell-combined-loft],[shell-mixed-path-loft],[block136-spatial]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-loft-auto-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-loft-auto-segments],[shell-combined-loft],[shell-mixed-path-loft],[block136-spatial]"
```

The focused Geometry oracle checks the original spline boundaries independently of the refinement
algorithm, rational and circular curves, center/guide anchors and bounded failure. The combined UI
workflow checks preview/cancellation, G2 rejection/recovery, exact Sketch persistence, downstream
Body translation, history, canonical Save/Open and STEP export. Run the full Core/Geometry and
offscreen GUI binaries after the focused proof, plus separate native startup. Keep source edits,
builds and GUI runs serial. Actual follow-up results live in
`docs/gui-interactive-path-sweep-loft-mvp9.md`.

## Dimension label input follow-up — issues #107 and #108

Build all five GUI-preset targets before testing. Keep source edits, builds, GUI suites and separate
startup serial. Prepare the capture directories before invoking existing native capture helpers.

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j6
mkdir -p /tmp/blcad-dimension-input-native /tmp/blcad-dimension-input-native-150
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-dimension-input-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[dimension-label-input],[sketch-hit-test],[sketch-dimension-workflow],[rapid-dimension],[sketch-constraints],[sketch-drag],[hermes-design-workflow],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-dimension-input-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[dimension-label-input]"
```

The new cases prepare geometry through authoring APIs, then select, double-click, type and delete
through real Qt mouse/key events. Native fixtures require an OCCT viewer and bounded actual window/
keyboard focus readiness. They cover length/radius/diameter/angle labels at both edges and center,
geometry overlap, references, invalid-input correction, Escape, stacked and multiple selection,
highlight pixels, retained formula parameters, a downstream circle extrusion, history and canonical
document/catalog reopening with recomputed body volumes. The controller case varies zoom and uses
an offset, rotated workplane. Inspect `dimension-selected-overlap.png`, then smoke-start `blcad`
separately. The behavior contract and dated validation evidence live in
`docs/gui-sketch-dimension-authoring-mvp8.md`; numbered implementation status remains in
`docs/mvp-plan.md`.

## Cut outcome and curved-profile regression — issue #113

Build all five targets, then keep source edits, builds, GUI suites and application startup serial.
Core/Geometry tests may run alongside one GUI suite. The native commands require a working X11/GLX
display; offscreen verifies the logical backend only.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_geometry_tests "[cut-material-diagnostics],[cut-material-removal],[body-boolean],[extrude-extent],[recompute],[revolve-feature]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[cut-material-workflow],[shell-combined-bodies]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-cut-native BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[cut-material-workflow],[block136-extrude-extents],[shell-combined-extrude-extents],[shell-combined-revolve-controls],[shell-combined-path-extrude],[shell-combined-thin-extrude],[shell-combined-extrude-limit-picking],[shell-combined-bodies],[shell-combined-body-context],[shell-combined-finishing-context],[curved-union-workflow],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-cut-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[cut-material-workflow],[shell-combined-bodies]"
```

The new native workflows fit their window on the first available screen before strict popup input.
Inspect the `cut-*-diagnostic` captures and both `cut-*-native-hole-proof` views. The latter magnify
and pick the empty hole center and retained material independently. Rejected numeric values restore
the last valid preview and report in the status bar; invalid target/direction/extent controls instead
clear preview and disable Apply. Existing summary and hint widgets have stable object names for
regression assertions. Run full Core, Geometry and offscreen GUI regression, required shell coverage
closure and separate native application startup after the focused checks. Exact behavior and dated
verification are recorded in `docs/part-body-boolean-geometry-mvp6.md` and
`docs/gui-interactive-extrude-revolve-mvp9.md`; numbered delivery remains in `docs/mvp-plan.md`.

## English desktop and modeling usability follow-up

Build all five GUI-preset targets, then keep source changes, builds, GUI runs and separate native
startup serial. Core/Geometry tests may run alongside one GUI suite.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_core_tests "[edge-treatment],[part-feature-input-reference],[spline-junction-regression],[closed-spline-seam-regression],[sketch-topology]"
./build/dev-gui/blcad_geometry_tests "[nested-spline-region],[curved-finishing],[union-boundary],[boolean-cache-isolation],[finishing-selection],[body-boolean]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[appearance],[rapid-dimension],[shell-reference-visibility],[presentation-visibility],[nested-spline-region],[curved-union-workflow],[spline-sidecar-migration],[shell-construction-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-ui-modeling-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[appearance],[rapid-dimension],[shell-reference-visibility],[presentation-visibility],[nested-spline-region],[curved-union-workflow],[spline-sidecar-migration],[shell-construction-lifecycle]"
```

The combined workflows exercise curve hover, projected dimensions, full-circle size editing and arc
radius, invalid-input recovery, preview cancellation, origin/axis constraints, fitted-spline junction
drag (including the closed seam before Finish Sketch), browser visibility, nested spline holes,
circular-edge Fillet/Chamfer, Union, exact history and Save/Open. Older fitted-spline sidecars remap
only explicitly migrated point aliases; unknown or degenerate references reject opening atomically.
Visibility checks also keep active Hole/construction tasks and their pick scopes intact while hiding
and restoring source geometry; real model edits still invalidate stale tasks. Inspect monochrome controls and the model/annotation state in native captures. Run full
Core, Geometry and offscreen GUI regression plus the shell coverage closure gate after focused proof.
Actual results belong to the dated follow-up entry in `docs/mvp-plan.md`.

## Hermes-inspired desktop redesign

The implementation plan and current design contract live in `docs/gui-hermes-design.md`. Build all
five targets using the command above before testing. Keep source edits, builds, GUI runs and the
separate native startup serial. Fonts are bundled Qt resources; no desktop font installation or
runtime network access is required.

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-hermes-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon],[shell-combined-part],[shell-combined-spatial-context],[shell-combined-finishing-context],[shell-combined-parameters],[shell-combined-assembly-context],[block137-step-import],[native-viewer-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-hermes-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[appearance],[startup-splash],[hermes-design-workflow]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][gui-feature-coverage]"
```

Inspect the `hermes-*` captures for the navigation, typography, inputs, disabled states, dimension
badges, task action reachability and model visibility at both window sizes. Also run a focused
1.5× scale proof, the full offscreen GUI suite, the shell coverage closure gate and separate native
application startup; record actual results and any environment constraints in the design document.
For final native acceptance, extend the canonical shell filter from
`docs/gui-shell-acceptance-mvp9r.md` with the subsequent Block-137, Loft and modeling-usability tags
above, plus `[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon]`.
Native widget exposure, focus and compact navigation checks await bounded Qt event-loop readiness
and then require real focus, complete button bounds and actual mouse hits.
Combined native workflows also await exact cursor/hover agreement across two separate event turns
before pressing a popup row. This settling is bounded to two seconds and four pointer requests;
the final index and real click assertions remain strict. STEP source checks retain their complete
checksum/no-internal-scroll assertions with the new styled field height.

## Loft profile correspondence follow-up

Build all five targets, then keep source edits, builds, GUI runs and separate startup serial.
The Core/Geometry binaries may run alongside one GUI suite.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_geometry_tests "[loft-section-alignment],[loft-section-compatibility],[multi-section-loft],[guided-loft],[mixed-path-loft]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[loft-section-alignment],[shell-loft-section-alignment],[shell-loft-auto-segments],[shell-combined-loft],[shell-mixed-path-loft],[block136-spatial],[shell-combined-spatial-context]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-loft-alignment-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[loft-section-alignment],[shell-loft-section-alignment],[shell-loft-auto-segments],[shell-combined-loft],[shell-mixed-path-loft],[block136-spatial],[shell-combined-spatial-context]"
```

The analytic prism and circular/rational ellipse oracles check the shape between sections, not only
validity or interpolation at authored planes. Inspect native `loft-section-alignment-*` captures for
the matching option, task action reachability and the corrected three-section solid. After focused
proof, run full Core, Geometry and offscreen GUI regression, required shell closure and separate
native startup. Record actual counts, native-only omissions and source-freeze evidence in
`docs/gui-interactive-path-sweep-loft-mvp9.md`.

## Dark-violet color-only follow-up

The current palette and restored pre-redesign splash contract live in `docs/gui-hermes-design.md`.
Build the five GUI-preset targets above. Keep source changes, builds, GUI runs and separate startup
serial; retain the existing fonts, spacing, widget sizes and interactions.

```bash
mkdir -p /tmp/blcad-violet-native /tmp/blcad-violet-native-150
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-violet-native BLCAD_SPLASH_SCREENSHOT=/tmp/blcad-violet-native/splash.png QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon],[shell-combined-part],[shell-combined-spatial-context],[shell-combined-finishing-context],[shell-combined-parameters],[shell-combined-assembly-context],[block137-step-import],[block136-measure],[gui][viewport],[native-viewer-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-violet-native-150 BLCAD_SPLASH_SCREENSHOT=/tmp/blcad-violet-native-150/splash.png QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[appearance],[startup-splash],[hermes-design-workflow]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

The standalone startup intentionally stops with timeout exit 124. Inspect captures for panel/input
contrast, checked/disabled/focus states, dark dimension/Measure/manipulator badges, and the original
black-and-white splash. The existing combined workflow proves compact and ordinary window sizes,
model edits, history, Save/Open and STEP export. Record actual skips and native results in the design
document; offscreen tests cannot prove OCCT rendering.

## Curated Pixelarticons integration

The icon catalog, exact pinned upstream/license and optional mask-regeneration instructions live in
`assets/gui/pixelarticons/README.md`. Builds use committed Qt resources; no SVG plugin, npm install,
asset generation or network access is required. Build all five GUI-preset targets as above, then keep
source edits, builds, GUI runs and separate startup serial.

```bash
mkdir -p /tmp/blcad-pixelart-offscreen /tmp/blcad-pixelart-native /tmp/blcad-pixelart-native-150
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-pixelart-offscreen QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[icons],[appearance],[startup-splash],[hermes-design-workflow],[ribbon],[model-browser]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-pixelart-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[icons],[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon],[shell-combined-part],[shell-combined-spatial-context],[shell-combined-finishing-context],[shell-combined-parameters],[shell-combined-assembly-context],[block137-step-import],[block136-measure],[gui][viewport],[native-viewer-lifecycle],[block136-profile-context],[block136-body-context],[block136-pattern-context],[block136-surface-context],[block136-feature-edit],[block136-assembly-context],[block136-construction]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-pixelart-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[icons],[appearance],[startup-splash],[hermes-design-workflow]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

The focused proof checks resource/license hashes, nonempty masks, exact normal/active/checked/disabled
colors, binary alpha and physical pixel sizes at 100%, 150% and 200%. It verifies labeled commands,
unchanged callbacks/shortcuts, typed browser metadata and text-only fallbacks for unsupported CAD
functions. Native workflows exercise compact tab navigation, context buttons, task/dialog actions,
history, Save/Open and STEP export. Inspect ordinary/compact captures and scaled icons, then record
actual results and skips in `docs/gui-hermes-design.md`. Startup timeout 124 is intentional.


## Complex-Part cache and rotor profile — issue #114

Build all five targets. Keep source edits, builds, GUI suites and startup serial. Core/Geometry
regressions may run with one correctness GUI suite. Timing runs must be isolated from other builds
and test processes; native X11/GLX is required for publication/redraw measurements.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_core_tests "[recompute_plan]"
./build/dev-gui/blcad_geometry_tests "[finishing-selection],[result-edge],[curved-finishing],[body-boolean],[recompute]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[preview-cache],[complex-part-workflow],[shell-combined-bodies]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-rotor-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[preview-cache],[complex-part-workflow],[shell-combined-bodies]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-rotor-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[complex-part-workflow],[interactive-pattern-mirror],[interactive-body-operation],[interactive-path-sweep],[interactive-loft]"
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-rotor-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[preview-cache],[complex-part-workflow],[cut-material-workflow],[shell-combined-part],[shell-combined-loft],[shell-combined-spatial-context],[shell-combined-finishing-context],[shell-combined-pattern-context],[shell-combined-body-context],[shell-combined-bodies],[shell-combined-surface-context],[shell-combined-extrude-extents],[shell-combined-revolve-controls],[shell-combined-path-extrude],[shell-combined-thin-extrude],[shell-combined-extrude-limit-picking],[curved-union-workflow],[interactive-finishing],[interactive-surface],[integration][gui-feature-coverage],[native-viewer-lifecycle]"
BLCAD_PROFILE_SAMPLES=11 BLCAD_PROFILE_OUTPUT=/tmp/blcad-rotor-profile.json QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[complex-part-profile]"
BLCAD_PROFILE_SEQUENCE=1 BLCAD_PROFILE_SAMPLES=5 BLCAD_PROFILE_OUTPUT=/tmp/blcad-rotor-sequence.json QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[complex-part-profile]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[preview-cache],[interactive-surface],[stitch-diagnostic-overlay]"
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-rotor-sequence-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[complex-part-workflow],[interactive-surface],[stitch-diagnostic-overlay],[shell-combined-surface-context],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
```

The profile is explicitly selected by tag and excluded from CTest/default GUI execution. It uses
5/20/40 spline blades around one revolved hub; set `BLCAD_PROFILE_BLADES=20` to isolate a size.
One first call and ten repeated samples yield separate first/p50/p95 values and raw measurements.
An empty-cache full candidate is also computed as a volume oracle, with native publication and
redraw/readback measured for both full and cached Fillet results. These reference operations do not
replace the actual controller preview being profiled. JSON timing covers serialization plus parsing,
not complete application history. The queued zero-timer measures event-loop blocking during that
call, not physical mouse latency. No portable timing assertion is introduced.

The sequence variant uses 4/12/24 serial Body translations after the 5/20/40-blade pattern
(7/15/27 committed producers). It additionally measures the real curved-Sketch baseline solver,
driving dimension authoring, a 32-pointer burst coalesced to one drag solve, native mouse selection,
preview through publication/event processing/capture, and actual Fillet Apply/Undo/Redo including
their history/recompute costs. Five samples give one first observation and four repeats; warm p95
is consequently the maximum repeat, not a high-confidence population estimate. The original
11-sample comparison remains a separate fixture and series. Run only one timing process at once.

`[complex-part-sequence]` is an ordinary correctness gate for all three sizes. It checks upstream
recompute through the full transform chain against an empty-cache oracle, retained independent
sources, driving dimension edits, coalescing, drag commit, stale commit rejection, history and
catalog persistence. The shell `[complex-part-workflow]` runs both 20- and 40-blade deep fixtures,
including actual native edge picking, recovery, cancellation, parameter editing and STEP.
Its source-volume and remote-Fillet oracles use adaptive integration in the test-only
`blcad_geometry_test_support` library; OCCT headers stay private to that Geometry support target.
The ordinary production `ShapeSummary` integration and all modeling tolerances are unchanged.

Inspect `rotor-fillet-recovered-preview` and `rotor-edited-reopened` captures for the native workflow.
Parameter Manager literal fields take values without units; the primary manipulator HUD accepts
units and restores the last accepted value on a failed preview. The exact candidate-cache contract
and evidence boundaries live in `docs/gui-modeling-workspace-mvp9.md`; result-witness comparison
remains owned by `docs/gui-interactive-finishing-mvp9.md`. Full Core/Geometry/offscreen GUI,
affected native feature families, required shell coverage and separate startup still follow the
focused proof. Numbered implementation sequencing remains in `docs/mvp-plan.md`.

## Combined remaining-PR integration — 2026-10-29

Resolve #116/#118 on top of current main and include the #121 follow-up from the already-merged
performance branch. Preserve both appended verification sections when merging this document.
The viewport combines measured badge bounds/selection layers with accent/panel/text theme colors.
Keep source edits, builds, GUI suites and separate startup serial; Core/Geometry may run alongside
one correctness GUI suite. This integration does not rerun isolated timing profiles.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
mkdir -p /tmp/blcad-merge-native /tmp/blcad-merge-native-150
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-merge-native BLCAD_SPLASH_SCREENSHOT=/tmp/blcad-merge-native/splash.png QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[icons],[appearance],[startup-splash],[hermes-design-workflow],[rapid-dimension],[ribbon],[model-browser],[dimension-label-input],[sketch-hit-test],[sketch-dimension-workflow],[sketch-constraints],[sketch-drag],[coincident-closure],[complex-part-workflow],[cut-material-workflow],[shell-combined-part],[shell-combined-spatial-context],[shell-combined-finishing-context],[shell-combined-parameters],[shell-combined-assembly-context],[block137-step-import],[block136-measure],[gui][viewport],[native-viewer-lifecycle],[block136-profile-context],[block136-body-context],[block136-pattern-context],[block136-surface-context],[block136-feature-edit],[block136-assembly-context],[block136-construction],[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[dimension-label-input]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-merge-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[dimension-label-input]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-merge-native-150 BLCAD_SPLASH_SCREENSHOT=/tmp/blcad-merge-native-150/splash.png QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[icons],[appearance],[startup-splash],[hermes-design-workflow],[dimension-label-input]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

All five targets build. Full Core **549 / 17,217**, Geometry **459 / 35,243**, offscreen GUI
**569 passed / 20 native-only skips / 80,332 assertions**, and native **136 / 29,127** pass.
These broad runs precede a native test-fixture-only adjustment: fit dimension windows onto the
first available screen and require stable actual focus across two event turns before keyboard
delivery, retaining the two-second deadline. The helper also checks the complete typed text.
Initial scaled and ordinary runs exposed focus/input interference; no production input behavior
was changed. After rebuilding, final focused dimension checks pass offscreen **7 / 1,618** and
natively **7 / 1,739**; final 150% UI/dimension checks pass **16 / 3,209**, without skips.
Separate startup reaches expected timeout 124. Final native logs have no High-severity,
BadDrawable or BadWindow errors; the known Medium X11 visual warning remains.

Logs: `/tmp/blcad-merge-{build,core,geometry,gui,native}.log`,
`/tmp/blcad-merge-build-final.log`, `/tmp/blcad-merge-dim-{offscreen,native}-final.log`,
`/tmp/blcad-merge-native-150-final.log` and `/tmp/blcad-merge-startup.log`.
Both freezes cover **886** tracked source/test/asset/build-definition inputs. The broad freeze is
`/tmp/blcad-merge-source-freeze.json`; the final focused/scaled/startup freeze is
`/tmp/blcad-merge-final-source-freeze.json` (SHA-256
`00fa1d248cc8b166f7c03a8bab605e10a850c0c94c5e907fb656582bdc4a2eff`).
Only `tests/gui/gui_dimension_label_interaction_tests.cpp` differs between them. Documentation
records the actual results afterward. Before delivery, require the final integration tree to match
the tested branch plus these documentation updates, then verify the merged main tree and ancestry.

## Complex-Part transaction/history completion — issue #114

Build all five targets. Run correctness suites with one GUI process at a time. Source edits/builds,
native GUI suites and separate startup stay serial. Run timing profiles alone, without concurrent
builds or tests; they are explicit opt-in measurements, excluded from default GUI/CTest selection.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[part-history-cache],[preview-cache]"
BLCAD_PROFILE_SEQUENCE=1 BLCAD_PROFILE_SAMPLES=5 BLCAD_PROFILE_OUTPUT=/tmp/blcad-issue114-history-after.json QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[complex-part-profile]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
mkdir -p /tmp/blcad-issue114-history-native /tmp/blcad-issue114-history-native-150
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue114-history-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[part-history-cache],[preview-cache],[complex-part-workflow],[shell-combined-part],[shell-combined-finishing-context],[shell-combined-bodies],[shell-combined-body-context],[shell-combined-extrude-extents],[shell-combined-revolve-controls],[shell-combined-path-extrude],[shell-combined-thin-extrude],[shell-combined-extrude-limit-picking],[shell-combined-pattern-context],[shell-combined-surface-context],[interactive-finishing],[interactive-surface],[stitch-diagnostic-overlay],[cut-material-workflow],[coincident-closure],[dimension-label-input],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue114-history-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[complex-part-workflow],[dimension-label-input]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

The canonical intent comparison and four-state history LRU contract, raw before/after benchmarks,
measurement scope and unchanged/local review targets are in `docs/gui-modeling-workspace-mvp9.md`.
In particular, assert actual upstream backing reuse during Apply, full-recompute volume equality,
cached and evicted history, stale provenance, atomic failures, Redo branching and document reset.
Parts with STEP registrations, repair, unsupported structural edits or legacy Extrude records
(even alongside Body declarations) retain full recomputation.
Do not add timing assertions that turn machine load into correctness failures.

Final focused history/preview proof: **7 / 969**. Isolated native 5/20/40-blade profile: **1 / 1,379**.
The fresh before-run has 40 blades only and uses main `8e72a8e`; reproduce with
`BLCAD_PROFILE_BLADES=40` and the same five-sample command before applying the change. Keep before
and after build/source identities separate. The final source freeze is
`/tmp/blcad-issue114-history-source-freeze.json` (**886** inputs, SHA-256
`0c9363d37a0e497b43215fa77814cfd25d119c3f6b20d8b3af9eae306190d4ee`).
The intermediate profile showing fast history but slow Apply is preserved separately as
`/tmp/blcad-issue114-history-intermediate.{json,log}`; it is not the final performance result.
The subsequent Apply-fixed series before the last legacy guard is
`/tmp/blcad-issue114-history-apply-fixed-prelegacy.{json,log}`. A targeted review regression
reproduced the legacy mixed-model final-result defect before that guard. The initial scaled
workflow also caught a native pointer mismatch after popup-loop unwinding. The fixture now
revalidates pointer/hover/row agreement after unwinding using the same original two-second deadline
and four pointer requests; failure captures include post-loop mismatches. Final scaled rotor/input
proof passes **8 / 2,435**. No focus, row, actual-click or geometry requirement was relaxed.
All five targets build. Full Core **549 / 17,217**, Geometry **459 / 35,243** and final offscreen GUI
**594 cases — 574 passed, 20 expected native-only skips; 80,765 assertions** pass. Core/Geometry
sources/binaries were unchanged by the final GUI-only guard and fixture refinements. Final native
affected-family/rotor and required shell gate passes **39 / 12,547**, without skips; final 150%
rotor/dimension input passes **8 / 2,435**, without skips. Separate native startup reaches intentional
timeout **124**. Inspected normal/scaled rotor and selected-dimension captures. Final profile,
native, scaled and startup logs contain no High-severity, BadDrawable or BadWindow errors; the
available X11 visual retains the known Medium depth/stencil warning. All **886** frozen source
inputs remain unchanged throughout the final focused/scaled/profile/full GUI/native/startup checks.

Exact logs: `/tmp/blcad-issue114-history-build-final.log`,
`/tmp/blcad-issue114-history-focused-final.log`, `/tmp/blcad-issue114-history-after.log`,
`/tmp/blcad-issue114-history-{core,geometry,gui,native,startup}.log` and
`/tmp/blcad-issue114-history-native-150-final.log`. Earlier full/native runs before the final legacy
guard are retained separately as `/tmp/blcad-issue114-history-prelegacy-{gui,native}.log`.
The initial scaled mismatch is `/tmp/blcad-issue114-history-native-150.log`; it is superseded by
the final scaled proof. No later numbered block is completed by this issue.


## Issue #104 — persistent line endpoint snaps (2026-10-29)

The existing creation transaction now publishes line geometry and automatic Coincident recipes
atomically. Press-phase snap evidence carries canonical endpoint identity through deferred input;
coordinate overlap never selects a relationship target. See `gui-sketch-basic-creation-mvp8.md`.
Keep the following GUI suites and startup serial; captures are optional local artifacts.

```bash
cmake --build --preset dev-gui --target blcad_gui_tests blcad_app blcad_core_tests blcad_geometry_tests blcad_export_step -j6
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[endpoint-coincidence]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue104-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[endpoint-coincidence]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue104-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[endpoint-coincidence]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue104-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-sketch],[sketch-create],[sketch-create-basic],[sketch-create-conics],[spline-create-workflow],[sketch-mouse-operations],[coincident-closure],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

All five build targets succeed. Final focused checks pass **6 / 313** offscreen and **6 / 331**
natively at both scales, without native skips. Four added cases plus two strengthened existing loop
cases guard exact and near closure, restart/overlap, twelve segments, persistence/drag/history,
Polyline atomic recipes, stale/foreign/missing evidence and zero-length rejection, queued movement,
cancellation and three presses across automatic Line rearming. The new tests first reproduced
missing/nonautomatic closure constraints, a separate history entry and deferred-hover retargeting.
Native fixtures publish manually seeded scenes before snapping, fit their windows to the screen and
bring all inputs into view using bounded ordinary wheel zoom. They do not select endpoint targets
by coordinate equality or send off-canvas events. Both saved-after-drag captures were inspected at
100% and 150%.

The final full offscreen GUI suite passes **598 cases — 578 passed, 20 expected native-only skips;
81,018 assertions**. Native affected-sketch workflows and the required shell gate pass
**82 / 3,609**, without skips.
Separate native startup reaches expected timeout **124**. Final native/scaled/startup logs have no
High-severity, BadDrawable or BadWindow errors. The pre-existing OCCT Medium visual-depth/stencil
warning remains. All 886 fingerprints stay unchanged through the final checks.

The final runtime input manifest has **886** files, SHA-256
`14d5cc545522777065132b3497bf53a438ab5b7a55b32d4e7a34fa0b0979c520`, stored at
`/tmp/blcad-issue104-source-freeze.json`. Core and Geometry inputs/binaries are unchanged from
verified main `736f100` (its accepted full-suite results are **549 / 17,217** and **459 / 35,243**);
this issue's full regression targets the changed GUI layer. No performance profile or later numbered
block is claimed by this fix. Logs use `/tmp/blcad-issue104-{build-final,focused-final,native-final,
native-150-final,gui-final,native-broad-final,startup-final}.log`.


## Issue #109 — reversible construction conversion (2026-10-29)

`Sketch::set_construction` changes only editable curve/profile roles after validating the entire
selection. The shell publishes one transaction, preserves the selected identities after its scene
refresh and rejects downstream-invalid conversions atomically. Optional curved-role persistence,
eligible-profile counting, shared point identity and native dashed wire presentation are documented
in `file-format.md`, `sketch-shared-topology-mvp8.md` and `gui-sketch-transform-pattern-mvp8.md`.
Keep builds, source edits, correctness GUI suites and separate startup serial; no timing profile is
part of this issue. Core/Geometry correctness tests can accompany one GUI suite.

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j6
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[construction-conversion]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue109-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[construction-conversion]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue109-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[construction-conversion]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue109-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[construction-conversion],[shell-sketch],[sketch-create],[sketch-create-conics],[sketch-transform-pattern],[sketch-offset-project],[spline-create-workflow],[sketch-drag],[sketch-dimension-workflow],[coincident-closure],[cut-material-workflow],[block136-profile-context],[shell-combined-profile-context],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

All five targets build. Final full Core passes **555 / 17,463**; full Geometry passes **461 /
35,315**. Core/Geometry production inputs and Geometry binaries are unchanged by final Core composite-fixture,
native pick-fixture and GUI projection-role refinements; no redundant Geometry rerun is claimed. The full GUI suite includes conversion/projection regressions with the native-only workflow explicitly
skipped offscreen. Core regressions first reproduced rejected construction arcs/splines and
silently flattened circle flags. A later GUI review regression guards unlinked projection role
selection across equal text in curve/profile id namespaces; role inheritance uses the existing
geometry-source precedence. Added coverage includes exact rational roles through JSON,
malformed flag rejection, overlapping/nested generated groups, stable shared point ids, transforms,
copies, splits, spline control edits and whole-selection reference rejection. Geometry proof checks
raw/materialized eligibility, visible auxiliary curves, hole volume exclusion and restored eligibility.
GUI proof adds retained dimension/constraint catalogs, one Undo step, no-op history and downstream
hole recompute versus rejected outer-profile rollback.

The native complex workflow creates a rectangle, a circle, a rational ellipse and an open arc. It
selects six curves with real viewport clicks, converts through the visible ribbon action, authors a
diameter, drags the auxiliary circle, saves/reopens, restores only the closed inner curves, then
Extrudes and Cuts with exact local Undo/Redo and another Save/Open. Several eligible regions use the
existing explicit profile-selection client before the Extrude ribbon task. Final cached Body volume
matches empty-cache recompute; STEP export does not alter intent. Picks stay inside the viewport.
The circle is selected opposite the opaque diameter label at high DPI, retaining strict Curve kind,
full Sketch-qualified semantic identity, selection and button-center checks. Hit rules are unchanged.
The historical composite profile bridge remains line-contour-only; the new nested fixture respects
that boundary. Native-only coverage is explicit, not substituted by the offscreen pass.

The final native affected-family run and required shell gate pass **101 / 7,086**, without skips.
Final scaled construction/projection proof passes **5 / 614** at 150%, without skips; its source
inputs also cover the four earlier native construction cases.
The initial full GUI run reached **580 passed / 21 expected native-only skips** with one inventory
proof-source mismatch: a shared family filter matched two files. Dedicated per-file proof filters
correct that metadata; targeted projection/inventory checks pass **4 / 342**. The final full GUI
run passes **603 cases — 582 passed, 21 expected native-only skips; 81,234 assertions** and
supersedes that pre-review result. Captures `construction-dimensioned-dragged` and
`construction-machined-solid` were inspected at both 100% and 150%: auxiliary curves remain visible
and dimensionable, restored inner loops remain distinct and the machined Body survives reopening.

Separate native startup reaches expected timeout **124**. Final broad/scaled/startup logs contain
no High-severity, BadDrawable or BadWindow errors; the pre-existing OCCT Medium depth/stencil visual
warning remains. All 889 final fingerprints remain unchanged through broad/scaled/full GUI/startup
verification. No performance profile or later numbered block is claimed by this issue.

The final manifest records **889** code/asset/build and runtime inventory inputs (including the new
profile-role helper and both coverage JSONs), SHA-256
`6b59f69ddc9bc43af74f844dbe0a9875ad7d308cab4d4a2bd7dc644de0356352` at
`/tmp/blcad-issue109-source-freeze-final.json`. Earlier reproduction/diagnostic logs are separate
from final proof. Final logs use `/tmp/blcad-issue109-{core-full-final,geometry-full,
gui-full-final,native-final-2,native-150-review-final,native-broad-final,startup-final}.log`;
final build logs are `/tmp/blcad-issue109-build-review-final.log` (all requested targets after review)
and `/tmp/blcad-issue109-build-final-core.log` (Core fixture/all requested targets).

## Issue #111 — Body Rotate reference parity (2026-10-29)

Body Rotate offers resolvable explicit/line-derived datum axes, construction lines and supported
straight semantic edges, with typed identities and source labels. `BodyRotationAxisResolver` owns
referenced model-space resolution for the chooser, manipulator frame and Geometry execution;
explicit numeric axes retain the consuming transform's existing coordinate-frame handling.
Task-only edge presentations use the same cache-aware semantic geometry. The contracts live in
`gui-interactive-pattern-body-mvp9.md` and `part-body-transform-geometry-mvp6.md`.

Keep source edits, builds, GUI suites and startup serial. Geometry correctness tests may accompany
one GUI suite; no timing profile is part of this issue.

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j6
./build/dev-gui/blcad_geometry_tests "[body-axis-parity]"
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-body][body-axis-parity],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue111-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[body-axis-native]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue111-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[body-axis-native]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue111-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-body],[block136-body-context],[shell-combined-bodies],[shell-combined-body-context],[shell-combined-revolve-pattern],[shell-combined-surface-solid],[block136-pattern-context],[shell-combined-pattern-context],[shell-combined-revolve-controls],[shell-combined-construction],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

All five targets build. The initial red GUI chooser test fails on both missing derived-datum and
semantic-edge entries (**1 case / 24 assertions, 2 failures**). Final focused Geometry passes
**2 / 127** and focused GUI plus inventory passes **5 / 508**. Full Geometry passes **463 / 35,442**.
Core production/tests and binary remain unchanged from verified main `d32edff`, retaining its full
Core result **555 / 17,463**. The final native-cancel fixture refinement changes only the combined
GUI test file, leaving the accepted full Geometry inputs/binary unchanged.

The native workflow passes **1 / 320** at both 100% and 150%, with no skips. It creates two solids
through the ribbon, seeds authored construction/datum reference records, uses actual popup input
for the derived datum and actual viewport picking for a supported semantic edge, preserves the
selected Body list and restores masks/temporary presentations. It checks cancellation without
persistence changes, two off-origin rotations, upstream depth 8 → 12 mm, exact local Undo/Redo,
empty-cache full-recompute bounds, canonical Save/Open and STEP. Native captures were inspected at
both scales. The strict existing native pointer, focus, visible-input and popup checks remain.

The final runtime input manifest has **890** files, SHA-256
`4bb421f9630f0e9f2d2d27f032e2984b723cac349020ec208115dc998d04ad1c`, stored at
`/tmp/blcad-issue111-source-freeze.json`. The check script is
`/tmp/blcad-issue111-freeze.py check`. Logs use `/tmp/blcad-issue111-{build-3,build-4,
build-native-2,geometry-focused-2,gui-focused-2,geometry-full,native-2,native-150,gui-full}.log`.

The full offscreen GUI suite passes **606 cases: 584 passed, 22 expected native-only skips;
81,444 assertions**. The affected native Body/pattern/Revolve/construction workflows plus required
shell gate pass **34 / 7,497**, without skips. Separate startup reaches intentional timeout **124**.
Final native/scaled/broad/startup logs contain no High-severity, BadDrawable or BadWindow errors.
The pre-existing OCCT Medium visual-depth/stencil warning remains (90 entries in the broad batch).
All 890 runtime fingerprints remain unchanged through the final focused-native, scaled, full GUI,
broad and startup checks. Broad/startup logs are `/tmp/blcad-issue111-native-broad.log` and
`/tmp/blcad-issue111-startup.log`. No new numbered implementation block is claimed.

## Issue #110 — persistent Sketch-line rotation axes (2026-10-29)

Revolve and Body Rotate store an exact `(SketchId, SketchEntityId)` source for editable normal,
construction and centerline segments. `SketchLineAxisResolver` resolves their current endpoints
through the current cache-aware workplane exactly once. Both consumers depend on the source
Sketch. The existing axis choosers provide the Sketch-line collection route; existing model-pick
scopes remain bounded to their prior sources. Contracts are in `file-format.md`,
`part-feature-input-reference-mvp6.md`, `part-revolve-intent-mvp6.md`,
`part-body-transform-ownership-mvp6.md` and `gui-interactive-extrude-revolve-mvp9.md`.

Keep source edits, builds, GUI suites and separate startup serial. Core/Geometry correctness tests
may accompany one GUI suite; no timing profile is part of this issue.

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j6
./build/dev-gui/blcad_core_tests "[sketch-line-axis]"
./build/dev-gui/blcad_geometry_tests "[sketch-line-axis],[sketch-line-axis-revolve]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][shell-body][sketch-line-axis],[sketch-line-axis-revolve]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue110-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-line-axis-native]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue110-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[sketch-line-axis-native]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue110-native QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[shell-body],[block136-revolve],[shell-combined-revolve-controls],[shell-combined-body-context],[shell-combined-revolve-pattern],[shell-combined-bodies],[body-axis-native],[block136-body-context],[shell-combined-construction],[block136-feature-edit],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

All five targets build. The initial red Body Rotate selector test reproduces all three absent line
roles (**1 / 24, 3 failures**). Focused Geometry passes **3 / 213** and focused GUI **2 / 120**.
Full final Core passes **557 / 17,711**, including strict new JSON fields/types, retained scoped
identity across equal entity ids, renamed same-coordinate source rejection, duplicate-id rejection
and endpoint invalidation of both consumers. Full Geometry passes **466 / 35,655**, including posed
owned-Sketch axis equivalence against an explicit World-axis rotation, oblique workplane/endpoint
edits, incremental/full Revolve equality and retained profile-plane/crossing guards.

The native workflow passes **1 case / 851 assertions** at both 100% and 150%, without skips. Its
three generator paths author normal, construction and centerline axes through real ribbon/numeric
input. Each chooses the exact oblique off-origin Sketch-line identity for a 180° Revolve and a 90°
Body Rotate, drags the original line endpoint in the native viewport, verifies the changed volume
independently using the profile's swept-centroid volume, and authors a downstream two-instance
linear pattern. Exact local Undo/Redo, full recompute, canonical Save/Open and STEP pass. Captures
of the selected axis, edited endpoint and resulting model were inspected at both scales. Existing
strict popup, pointer, focus and visible-input assertions remain.

The final runtime input manifest has **892** files, SHA-256
`fcc386c0d1050239c58052fb0776e8394086df69c1e8d3b0cde9da01785f189f`, stored at
`/tmp/blcad-issue110-source-freeze.json`; check with `/tmp/blcad-issue110-freeze.py check`.
Logs use `/tmp/blcad-issue110-{red,build,build-2,core-full,geometry-full,native-first,native-150,
gui-full}.log`. This issue adds a typed source family without advancing a numbered implementation
block; Pattern/Draft axes and associative projected-line records do not gain new source variants.

The final full offscreen GUI suite passes **609 cases: 586 passed, 23 expected native-only skips;
81,564 assertions**. The final affected native Body/Revolve/construction/Feature Edit workflows and
required shell gate pass **45 / 7,735**, without skips or unmatched filters. Separate startup reaches
intentional timeout **124**. Final native/scaled/broad/startup logs contain no High-severity,
BadDrawable or BadWindow errors. The pre-existing OCCT Medium visual-depth/stencil warning remains
(112 entries in the broad batch). All 892 runtime fingerprints remain unchanged through the final
full Core/Geometry, native, scaled, full GUI, broad and startup checks. Final broad/startup logs are
`/tmp/blcad-issue110-native-broad-final.log` and `/tmp/blcad-issue110-startup-final.log`.

### Issue #112 — associative current-result-face Sketch verification

The regression for the initial selection gate failed **2 of 15 assertions** before
implementation (`/tmp/blcad-issue112-red.log`). Intermediate checks passed Geometry
**8 / 302**, UI cancellation/history/repair **1 / 110**, and native workflows **2 / 1,021**.
These intermediate counts precede the final source freeze and do not substitute for the
final checks below. Native scenarios include actual face clicks and visible controls;
the producer-family test explicitly opens serialized Core source fixtures.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_geometry_tests "[result-face-support]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[result-face-sketch],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue112-native-100 QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[result-face-native],[result-face-producers-native]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue112-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[result-face-native],[result-face-producers-native]"
```

The final gate also runs complete Core, Geometry and offscreen GUI suites, affected native
construction/repair/feature-edit/finishing workflows with the required shell inventory,
and a separate native startup. Runtime fingerprints include tracked/untracked source,
tests, assets, build definitions and both GUI coverage manifests. Plain Markdown evidence
can be completed after the runtime freeze; runtime changes require renewed verification.

Final runtime freeze: **896 files**, SHA-256
`2f5d9b0bcc891440258b6387a503cda01b27fba5ad5d49a9564073130e67ff4a`,
`/tmp/blcad-issue112-source-freeze.json`; verify with
`python3 /tmp/blcad-issue112-freeze.py check`. All five targets build
(`/tmp/blcad-issue112-build-11.log`; full formatted rebuild and copy-origin integration
are logged in `build-final`/`build-9`). Final targeted Geometry passes **12 / 420**,
including copy removal, curved walls, lost empty-Sketch freshness and owned pose prefixes.
UI cancellation/repair plus the inventory pass **4 / 412**. Complete Core passes
**557 / 17,711** and Geometry **478 / 36,075**. Exact logs:
`/tmp/blcad-issue112-{geometry-focused-3,gui-focused-final-2,core-full-final,geometry-full-final}.log`.

On the final freeze, native pointer/visible-control workflows pass **2 / 1,021** at both
100% and 150%; captures for pocket preview/edited thickness and oblique polygon/Loft/
Boolean/Fillet were inspected across the two scales. The affected native construction,
repair, feature-edit and selection gate passes **39 / 6,975**, without skips or unmatched
filters. A separate native startup reaches intentional timeout **124**. Logs:
`/tmp/blcad-issue112-{native-final-100,native-final-150,native-broad,startup}.log`.
No High-severity, BadDrawable or BadWindow message appears; the existing OCCT Medium
visual-depth/stencil warning remains. The registered gate is:

```bash
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue112-native-broad QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-construction],[shell-combined-workplane-edit],[shell-combined-construction],[block136-reference-repair-batches],[block136-repair-open],[block136-feature-edit],[shell-combined-finishing-context],[block136-selection-filters],[result-face-sketch],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
```

The creation checkpoint advances both model and presentation revisions when cancelling;
its final regression explicitly checks these cache invalidation contracts. Complete GUI
validation is repeated after that final correction (`gui-full-final`), rather than using
the preceding **612 cases / 81,683 assertions** run as final-source evidence.

Final complete GUI passes **612 cases — 587 passed, 25 expected native-only skips /
81,685 assertions** (`/tmp/blcad-issue112-gui-full-final.log`). Complete Core and Geometry
also pass on the same final freeze (`core-full-final`, `geometry-full-final`), and the
896-file manifest remains unchanged after every native/full/startup gate. No required
check is skipped. The 25 GUI skips are expected platform guards; the affected native
cases were exercised separately as listed above.
No numbered implementation block is advanced.

Delivery: [PR #128](https://github.com/GideonBa/Better-Linux-CAD/pull/128), implementation
commit `76b68ed`. The final documentation-link commit changes only Markdown; runtime
fingerprints and the verified implementation remain unchanged.

### Issue #106 — explicit dimension presentation

The offset-point regression first failed **2 of 9 assertions**: horizontal/vertical values
were correct but the compatibility span remained diagonal (`/tmp/blcad-issue106-red.log`).
The implementation separates measured references, placed model geometry and projected strokes,
text bounds and terminators. Paint, inline-editor placement, click and box picking share the
projected result. The presentation rules and official standards verification boundary are in
`docs/gui-sketch-dimension-authoring-mvp8.md`; no full DIN/ISO conformity is asserted.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_geometry_tests "[dimension-projection],[sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[dimension-layout],[dimension-label-input],[rapid-dimension],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue106-native-100 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 ./build/dev-gui/blcad_gui_tests "[dimension-layout-native],[dimension-label-input]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue106-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[dimension-layout-native],[dimension-label-input]"
```

Create capture directories first. Native tests prepare source geometry through authoring APIs,
then exercise displayed captions and strokes with actual viewport mouse/key events. The new
normal/oblique fixture has eight neighboring reference dimensions, actual partial-arc radius
and arc length, label-drag preview/release, zoom, exact Undo/Redo and canonical reopen.
Native pointer setup synchronizes the physical cursor before button events; the existing
focus-readiness, precise semantic selection and unmodified persistence assertions stay strict.
An initial oblique fixture omitted the shell refresh after direct source setup; that setup
was corrected before final verification. Intermediate checks are not final-source evidence.

Final runtime freeze: **900 files**, SHA-256
`b4f7acb415f38811c77a731cbeb439c9c9aec042d50b6c2af282051de8dec78d`,
`/tmp/blcad-issue106-source-freeze.json`; verify with
`python3 /tmp/blcad-issue106-freeze.py check`. All five targets build
(`/tmp/blcad-issue106-build-final-4.log`). Complete Core passes **557 / 17,711** and
Geometry **479 / 36,084** (`core-full-final-2`, `geometry-full-final-2`). Native layout and
existing dimension input pass **8 / 2,074** at both 100% and 150%
(`/tmp/blcad-issue106-native-final-{100,150}-2.log`). Neighboring dimensions and oblique
label-drag/zoom captures were inspected across both scales. Labels deliberately moved onto
another caption retain the existing foreground-selection and overlap-cycling behavior;
this change does not introduce automatic collision avoidance.

Visual review of the 150% neighboring-dimension capture exposed tight manual fixture
placements. The fixture now asserts that every pair of initial displayed text rectangles is
disjoint, in addition to exact mouse selection. Its length and circular captions are spaced
explicitly; the interrupted full run (`gui-full-before-label-spacing`) is not final evidence.

The Shell coverage registry requires proof files named `gui_shell_*`. The existing
dimension input/native presentation test is therefore renamed to
`tests/gui/gui_shell_dimension_label_interaction_tests.cpp`; its native proof filters
are registered there. The deterministic layout unit tests remain a separate non-shell
proof, without weakening the inventory validator or its source/tag matching.

The native 100%/150% runs above precede only that mechanical file rename and registry
correction; their test content and all production sources are unchanged. The rebuilt final
layout/inventory check passes **10 / 572** (`gui-coverage-final`); the affected native gate
passes **52 / 1,840**, without skips or unmatched filters (`native-broad-final`):

```bash
BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-issue106-native-broad QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[dimension-layout],[sketch-hit-test],[sketch-box-selection],[sketch-dimension-workflow],[rapid-dimension],[sketch-constraints],[sketch-drag],[hermes-design-workflow],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
```

Separate native startup reaches intentional timeout **124** (`/tmp/blcad-issue106-startup.log`).
No High-severity, BadDrawable, BadWindow or unmatched-filter message appears in the completed
native/startup gates. The existing OCCT Medium incomplete visual depth/stencil warning remains.

Final complete offscreen GUI passes **620 cases — 594 passed, 26 expected native-only
skips / 81,956 assertions** (`/tmp/blcad-issue106-gui-full-final.log`). The final 900-file
runtime manifest remains unchanged after all completed gates. The platform skips are
reported explicitly; affected native dimension/Sketch/desktop/viewer cases were exercised
as listed above. No numbered implementation block is advanced by this issue.

Delivery: [PR #129](https://github.com/GideonBa/Better-Linux-CAD/pull/129), implementation
commit `8dd28dc`. The delivery-link commit changes only Markdown and preserves the verified
runtime fingerprints.

### Block 138 — STEP/XDE geometry inspection

The reader consumes the secure immutable source snapshot and keeps product-local body
geometry separate from direct occurrence placements. The existing source task adds a
separate read-only geometry inspection; metadata registration/refresh/removal retain their
Block-137 contracts. Exact reader, unit, ordering, repair-policy and inspection lifecycle
boundaries live in `docs/step-import-sequence-mvp10.md`. Block 138 is accepted on 2026-10-29
with the complete gates below; imported topology identity remains the next planned boundary.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_core_tests "[step-import-source],[step-import-json],[step-import-freshness],[step-import-snapshot]"
./build/dev-gui/blcad_geometry_tests "[step-import-reader]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[step-import-inspection],[step-import-workbench],[block137-step-import],[block138-step-inspection],[integration][gui-feature-coverage]"
mkdir -p /tmp/blcad138-native-100 /tmp/blcad138-native-150
BLCAD_UI_CAPTURE_DIR=/tmp/blcad138-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[step-import-inspection],[step-import-workbench],[block137-step-import],[block138-step-inspection],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad138-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-inspection]"
BLCAD_STEP_IMPORT_PROFILE_OUTPUT=/tmp/blcad138-inspection-profile.json ./build/dev-gui/blcad_geometry_tests "[step-import-profile]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

Create the capture directories before native runs. Keep builds, source edits, GUI suites
and timing runs serial; the opt-in rotor profile excludes fixture creation/export from
its timings and is excluded from ordinary CTest discovery. It records the first measured
inspection and five repeated inspections of an already-written source, with a warm OS
cache; these are observations, not portable correctness/performance budgets. This host
uses Debug GCC, OCCT **7.6.3**, Qt **6.4.2**, an Intel **i7-1370P** with 20 logical CPUs,
and Linux **7.0.0-38-generic**. No background Geometry worker is introduced.

The first native capture exposed a real far-origin preview problem: fitting all origin
references and imported geometry left the translated body about two pixels wide. The
inspection now uses an explicit preview-only fit and the native test requires at least
80 logical DIPs of projected model width, while preserving camera restoration. History
assertions use the actual Shell Undo/Redo availability during a task and verify the real
history after cancellation; `GuiDocumentSession::can_undo`/labels intentionally suppress
history while any task is active. Existing session guards are preserved.

The initial generated unit fixtures created the writer before changing its unit setting;
OCCT had already captured its output scale. Corrected fixtures set the unit before writer
creation and independently parse raw STEP point spans × declared scale, retaining the
original physical bounds/volume expectations. Reversed-definition fixtures use a nonidentity
placement so the OCCT writer preserves the authored occurrence name. Located-root fixtures
assert the writer's actual auxiliary assembly plus direct placement and exact posed bounds;
they do not assume a flattened source product. The duplicate-unit fixture accepts the
writer's whitespace. Intermediate failing logs are development evidence, not final checks.

Final review added regressions before fixing two concrete defects. Two finite `1e308`
translations overflowed only after hierarchy composition or combination with a body-local
placement; both new checks failed in `/tmp/blcad138-transform-red.log`. Reading and preview
now share rooted placement validation before publishing geometry. The native atomic-replacement
case failed four checks in `/tmp/blcad138-watcher-red.log`: after an identical-byte replacement,
the old file watch disappeared and a later in-place change left stale geometry visible. Valid
notifications now rearm the replacement watch. The unchanged regression first drains replacement
events and uses independently prepared changed bytes, so no later directory/export event masks
the lost watch. Both fixes are included in every closure gate below.

Final runtime freeze: **904 files**, SHA-256
`c465168447c31608584d4f7109c74b3ba6c1274d3f447c8d97808e961859118b`,
`/tmp/blcad138-source-freeze.json`; verify with `python3 /tmp/blcad138-freeze.py check`.
All five targets build (`/tmp/blcad138-build-closure.log`). Focused Core passes
**12 / 681**, Geometry **12 / 634**; focused
GUI/required inventory passes **20 cases — 17 passed, 3 expected native-only skips /
2,402 assertions** (`core-focused-closure`, `geometry-focused-closure`, `gui-focused-closure`).
Complete Core passes **560 / 17,987** and Geometry **491 / 36,718**
(`/tmp/blcad138-{core-closure,geometry-closure}.log`).
Complete offscreen GUI passes **625 cases — 597 passed, 28 expected native-only skips /
82,197 assertions** (`/tmp/blcad138-gui-closure.log`).
After the final numbered-status update, the required Shell inventory passes **3 / 306**
(`/tmp/blcad138-final-inventory.log`); historical Block-136 acceptance ownership is preserved.

Final native 100% source/inspection/workbench/viewer/required inventory passes **21 / 3,267**
(`/tmp/blcad138-native-closure-100.log`), including both new native inspection cases. Those cases
also pass at 150% **2 / 673** (`native-closure-150`). Empty Part/Project, changed-source
rejection and explicit-refresh captures were inspected across both scales. Separate startup
reaches intentional timeout **124** (`startup-closure`). No High-severity, BadDrawable, BadWindow
or unmatched-filter message appears; the existing OCCT Medium incomplete visual warning remains.

The isolated final rotor profile passes **1 / 64** (`/tmp/blcad138-profile-closure.log`).
Raw samples, host, workload and timing scope are retained in
`docs/benchmarks/block138-step-inspection.json`: **615,000 bytes**, one product/body/root
occurrence, **122 faces**, source-volume agreement at **75,778.64478 mm³**. First measured
reader time is **167.5 ms**; the five repeated reader samples have median **169.8 ms** and
observed maximum/nearest-rank p95 **173.2 ms**. Separate snapshot median is **0.560 ms**;
posed-preview composition median is **0.021 ms**. Full reader time includes its own secure
snapshot; these columns are not additive. Fixture creation/export, rendering, GUI events
and native publication are excluded. Five warm samples do not establish a reliable tail
budget, and the already-written file was in the OS cache. The generated STEP header timestamp
changes its byte digest across exports; the code recreates the same geometric workload.

### Block 139 — Imported topology identity and recovery (accepted 2026-10-29)

The catalog/JSON, normalized Geometry recovery, explicit source context, Part/Assembly consumers
and source-task semantic selection are implemented on `feat/step-import-topology` from merged
main `ca2db6e`. Exact ownership, immutable-baseline, three-state recovery and capability contracts
live in `docs/step-import-sequence-mvp10.md` and `docs/file-format.md`. All runtime acceptance
gates pass on 2026-10-29; numbered completion advances through Block 139 only.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_core_tests "[step-import-topology]"
./build/dev-gui/blcad_geometry_tests "[step-import-topology-recovery],[step-import-assembly-target]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[step-import-topology-workbench],[block139-step-topology],[step-import-inspection],[step-import-workbench],[block137-step-import],[integration][gui-feature-coverage]"
mkdir -p /tmp/blcad139-native-100 /tmp/blcad139-native-150
BLCAD_UI_CAPTURE_DIR=/tmp/blcad139-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[native-step-topology],[native-step-inspection],[step-import-workbench],[block137-step-import],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad139-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-topology]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

Native selection must exercise actual Face/Edge/Vertex clicks and exact analytic capability filters,
Part/Project catalog Apply/history/reopen, lost-topology display and source-change invalidation.
The new Part proof executes an authored Mirror through an imported Plane, resolves imported axes,
uses imported planar faces for ToFace/Between extents, and applies a real Fillet to an exactly matching
authored edge. Nested Assembly target proof uses
shared Part intent and three noncommuting transforms with independent numeric position/direction
oracles. A stale source must fail without changing the prior cache or persisted intent. These focused
and native checks pass together with the full regression and separate startup.

Review regressions first reproduced the remaining defects against unchanged production paths.
`/tmp/blcad139-duplicate-red.log` records **2 failing cases / 5 failed checks**: Apply accepted a
reconstructed public preview for an already captured product, while an unrelated ambiguous product
blocked a distinct capture (including a distinct product sharing a trimmed face but excluded by the
global exact-product rule). `/tmp/blcad139-legacy-red.log` records **1 failing case / 12 failed checks**:
full and incremental legacy recompute removed prior feature/final shapes after missing-context or
stale-source failure. Their assertions pass unchanged in the final focused suites.
The first native pass also exposed origin-inclusive fitting after standard-view changes; preview
focus now survives those changes and deferred GL initialization. The native case requires a useful
on-screen model span before real analytic picks. Its later filter-restoration oracle records the
document-open reset separately from the initial strict Face-filter restoration after Apply.

The first broad Geometry run aborted in the existing Revolute sweep analyzer. A focused GDB run
reproduced the stack-canary failure at its `Project` copy boundary. Ninja inspection found exactly
two old object files with **zero recorded header dependencies** (the sweep analyzer and Loft pipeline
tests); the sweep object predated the changed `Project` layout and had been omitted by incremental
builds. The compiler rules themselves correctly use `-MD`/`-MF`. Invalidating exactly those two
local artifacts and rebuilding all five targets restores **485 nonempty dependency records**;
the unchanged focused sweep suite passes **5 / 373** (`/tmp/blcad139-sweep-rebuilt.log`). This is a
local build-artifact repair, with no repository rule or test change. The complete suites are rerun
against the repaired build and pass.

Final verified runtime freeze: **914 files**, SHA-256
`47ae3f7a77d1ce76f6379ef9e51537ea2a86aa7b5371b967250c37088b564cf4`,
`/tmp/blcad139-source-freeze.json`; check with `python3 /tmp/blcad139-freeze.py check`.
All five targets build (`/tmp/blcad139-build-8.log`). Focused Core passes **6 / 247**,
Geometry **15 / 1,878**, and GUI **24 cases — 22 passed, 2 expected native-only skips /
3,348 assertions** (`/tmp/blcad139-{core,geometry,gui}-focused-final.log`). Unchanged review
regressions are included. Full Core passes **566 / 18,234** and Geometry **506 / 38,596**
(`/tmp/blcad139-{core,geometry}-final.log`). Native source/topology/inspection, viewer lifecycle and
required coverage pass **26 / 5,137** at 100% (`/tmp/blcad139-native-final-100.log`); the new native
topology case passes **1 / 934** at 150% (`/tmp/blcad139-native-final-150.log`). Both Part and
Project runs use real analytic picks, Apply/history/reopen and explicit source refresh with Lost
rows remaining unselectable. Captures are in `/tmp/blcad139-closure-native-{100,150}`.

Full GUI passes **631 cases — 602 passed, 29 expected native-only skips / 83,143 assertions**
(`/tmp/blcad139-gui-final.log`). Separate native startup reaches intentional **124**
(`/tmp/blcad139-startup.log`). All eight new Part/Project picking/recovery captures at 100% and
150% were visually reviewed: useful model framing, readable capability/recovery controls and no
horizontal table overflow. Native logs contain the existing Medium Visual warning; no High severity
OpenGL or drawable/context failure is present. All 914 final runtime fingerprints remain unchanged.
Final documentation/coverage inventory passes **3 / 309**
(`/tmp/blcad139-final-inventory.log`) after advancing the plan through Block 139; historical
Block-136 coverage ownership is preserved.
Delivery: [PR #131](https://github.com/GideonBa/Better-Linux-CAD/pull/131), verified implementation
commit `95f15a6`; its documentation-only follow-up preserves all 914 runtime fingerprints.

### Block 140 — Reference Parts in assemblies (accepted 2026-10-29)

Branch `feat/step-reference-parts` starts from verified main `d403460` after PR #131 is merged.
Main exactly matches tested head `945cccd`, all 914 Block-139 runtime fingerprints match, and no
open issues or PRs remain at this boundary. Core immutability/JSON, separate Reference shape
authority, shared verified Assembly context and matching headless/GUI commands are accepted.
Numbered completion advances through Block 140 only. Each source owner freezes before the common
build; no tests or builds run while code changes are in flight. Focused tags are
`[core][step-reference-part]` and `[geometry][assembly-step-reference-part]`, with native
Part/Project workflow proof and the full regressions required before publication.

Block-140 gate commands:

```sh
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_core_tests "[step-reference-part]"
./build/dev-gui/blcad_geometry_tests "[assembly-step-reference-part],[step-reference-commands]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[gui][assembly-authoring],[gui][assembly-motion],[block137-step-import],[step-reference-part],[step-reference-parts],[block140-reference-parts],[integration][gui-feature-coverage]"
mkdir -p /tmp/blcad140-native-100 /tmp/blcad140-native-150
BLCAD_UI_CAPTURE_DIR=/tmp/blcad140-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[native-step-reference-parts],[native-step-topology],[native-step-inspection],[block136-assembly],[block136-assembly-context],[block136-assembly-advanced],[block136-assembly-analysis],[integration][gui-feature-coverage],[native-viewer-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad140-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-reference-parts]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
./build/dev-gui/blcad_export_step examples/reference_plate.blcad.json /tmp/blcad140-cli-legacy.step
```

The frozen runtime inventory includes `examples/` as well as source, headers, tests, assets, CMake,
scripts and coverage manifests. The common build is followed by an audit of Ninja dependency records;
no source edits occur while a build or test is active. Native checks run sequentially with all other
GUI/startup runs. Failed export cleanup removes a reserved temporary pathname only while its device
and inode still match the retained descriptor; an independent replacement survives rollback.

Validation accepted (2026-10-29): all five targets build
(`/tmp/blcad140-build-11.log`). Focused Core **11 / 1,070**, Geometry **27 / 3,037** and GUI
**18 cases — 15 passed, 3 expected native-only skips / 2,138 assertions** pass. The expanded GUI
filter also covers existing STEP source workflow/lifecycle/catalog clients and joint authoring/motion.
Full Core
**577 / 19,304** and full Geometry **533 / 41,638** pass; their implementation remains unchanged
through the final GUI-only corrections. The historical bodyless Part CLI export passes and writes
**19,170 bytes**. Logs: `/tmp/blcad140-core-focused-2.log`,
`/tmp/blcad140-geometry-focused-2.log`, `/tmp/blcad140-gui-focused-final.log`,
`/tmp/blcad140-core-full.log`, `/tmp/blcad140-geometry-full.log` and
`/tmp/blcad140-cli-legacy.log`.

Final native 100% passes **37 / 5,771** and native 150% passes **2 / 1,110**
(`/tmp/blcad140-native-100-final.log`, `/tmp/blcad140-native-150-final.log`). The native proof
clicks an actual recovered imported vertex and checks the exact semantic endpoint. Keyboard input
establishes real editor focus; the spinbox must hold 25 before Apply and the same sole Revolute
joint must retain 25 degrees afterward, including exact Undo/Redo. No application motion-path
change was required for that input proof.

Product-local/child-definition previews isolate persistent scene presentations. Actual native
`IsDisplayed` assertions verify hiding and restoration, root-Assembly placement retains context,
and Cancel/Apply preserve the documented camera and Core authority. Reviewed 100% and 150%
captures show the complete conversion geometry without coplanar striping or unrelated clipped
occurrences, with readable fields and reachable Apply/Close. Captures are in
`/tmp/blcad140-native-100/` and `/tmp/blcad140-native-150/`. An earlier full GUI run was interrupted
before this correction; it is not an accepted gate. A subsequent full GUI run found one old
joint-authoring fixture naming nonexistent `feature.hole.axis` even though its Part contains only
the base extrusion and XY datum. Its corrected test uses a valid explicit Frame and verifies atomic
preview/Apply rejection of missing and Plane-only targets, with JSON and complete history state
unchanged. Production compatibility remains strict. All other full-suite cases passed in that run
(`/tmp/blcad140-gui-full-pre-fixture-fix.log`). The final full GUI rerun passes **638 cases —
607 passed, 31 expected native-only skips / 83,543 assertions**
(`/tmp/blcad140-gui-full-final.log`).
Separate native startup reaches its intentional **124** timeout (`/tmp/blcad140-startup-final.log`).

All **961** runtime files remain frozen under final manifest SHA-256
`b62553c647c81ea3d1df8038987d7aefc9c5dc7448c70a83bce602734e3de6a1`. The preserved native manifest
is `1d6943711ef586ba557df7ebc960d33d3c0c0ba193953c1e801f5971b9708d3d`; comparison confirms only
`tests/gui/gui_assembly_workbench_tests.cpp` changed afterward. All production and selected native
test files are identical, so the native gates remain valid. All **496** Ninja dependency records
are nonempty (`/tmp/blcad140-ninja-deps-final.log`). Only Markdown may change during gates.

After updating numbered completion through Block 140, the serialized documentation/GUI coverage
inventory passes **3 / 313** with Shell acceptance required
(`/tmp/blcad140-final-inventory.log`). The runtime freeze remains unchanged.
Delivery: [PR #132](https://github.com/GideonBa/Better-Linux-CAD/pull/132), verified implementation
commit `16fd35e`.
PR #132 merged as `f426480`; main exactly matches tested head `ba79e27` and all **961** runtime
fingerprints, with no open issues or PRs before Block 141 starts.

### Block 141 — EditableBody downstream modeling (accepted 2026-10-29)

Branch `feat/step-editable-body-modeling` starts from verified main `f426480` after PR #132 is
merged. The new Core factory, source-to-base dependency graph, immutable per-output base cache,
incremental downstream replay, shared direct-import/refresh commands and matching GUI authoring
are implemented. Sources were frozen before each common build/test window. Full acceptance is
complete as recorded below; numbered completion advances through Block 141 only.


Block-141 acceptance commands (results are recorded below only after execution):

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
./build/dev-gui/blcad_core_tests "[imported-body-feature]"
./build/dev-gui/blcad_geometry_tests "[imported-body-recompute],[imported-body-downstream-feature],[step-editable-commands]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[imported-body-feature],[step-editable-parts],[block141-editable-parts],[step-reference-parts],[block140-reference-parts],[integration][gui-feature-coverage]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
BLCAD_UI_CAPTURE_DIR=/tmp/blcad141-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[native-step-editable-parts],[native-step-reference-parts],[native-step-topology],[native-step-inspection],[block136-feature-edit],[block136-surface-context],[shell-combined-revolve-controls],[native-shell-surface],[native-construction-picking],[shell-combined-workplane-edit],[shell-repair-open-input],[integration][gui-feature-coverage],[native-viewer-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad141-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-editable-parts]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
./build/dev-gui/blcad_export_step examples/reference_plate.blcad.json /tmp/blcad141-cli-legacy.step
```

The first common build was stopped before tests after independent review found that reusing an old
Geometry cache after imported-base deletion could retain removed geometry. A narrow cache ownership correction and full/empty-plan regressions were added before the next
frozen build. Pristine removed bases reconcile on a working copy; deleted modifier chains and
foreign/reused ownership fail atomically, including direct feature entrypoints. This interrupted
build is not recorded as a passing gate. The corrected build freezes **970** runtime inputs;
manifest SHA-256 `b4e445ff1a40202e7b623460eed89263def50f04457b5d17b31d29961e92d288`.
The next build caught a `string_view`/`string` helper mismatch, which was corrected before tests.
Final review also added a reused-base-id/output-remapping regression: old immutable definition or
catalog evidence cannot be reused for a different base; digest-only refresh remains supported.

The first focused Core run found two test assumptions (10/12 cases passed): dependency-ready JSON
replay preserves the complete semantic edge set rather than vector insertion order, and the
state-driven planner executes `Dirty` descendants rather than the already accepted `Changed`
source. The corrected assertions retain exact sorted edge equality, ordering, every downstream
feature and both output Bodies, and also verify the explicit changed-root planner. Production code
was unchanged by this test correction. Final gates below must use the corrected freeze.

Corrected frozen Core gates pass: focused **12 cases / 1,544 assertions**
(`/tmp/blcad141-core-focused-final.log`), full **589 / 20,850**
(`/tmp/blcad141-core-full.log`). Runtime fingerprints remain unchanged. Geometry/GUI/native gates
are still pending; this evidence alone does not advance numbered completion.

The new native test required explicit `QPoint` overload selection when compiling against Qt 6.
The first focused Geometry run passed 15/17 cases; its two failures used `RectangleProfile` through
an older SubtractiveExtrude path that accepts closed profiles instead. The fixture now uses the
actual four-line `ClosedProfile` at (13,13)–(17,17), with four directed driving dimensions bound to
the original width/height parameters. The real imported-base SubtractiveExtrude, 160 mm³ cut,
incremental dependencies and all later volume/JSON/re-export assertions are retained. No production
geometry behavior changed for this fixture correction.

The corrected frozen Geometry focused gate passes **17 / 921**
(`/tmp/blcad141-geometry-focused-final.log`), including full/empty-plan deletion and reused-id
regressions, immutable multi-output base retention, actual imported face-supported Sketches,
placed imported-edge Fillet, downstream Add/Cut/Hole, JSON and physical re-export. Full Core repeats
successfully under the current freeze: **589 / 20,850** (`/tmp/blcad141-core-full-final.log`).

All five targets build successfully (`/tmp/blcad141-build-6.log`); the dependency audit contains
**502 nonempty records** (`/tmp/blcad141-ninja-deps.log`). Full Geometry passes **550 / 42,559**
(`/tmp/blcad141-geometry-full.log`). Focused offscreen GUI passes **14 cases: 11 passed, 3
native-only skips / 849 assertions** (`/tmp/blcad141-gui-focused.log`). The 970-file freeze remains
unchanged. Native workflows and the complete GUI suite still gate acceptance.

The first native 100% run finished **29 cases: 27 passed, 2 failed / 17,364 assertions**
(`/tmp/blcad141-native-100.log`). The Editable workflow reached the real face-supported boss, Cut,
Hole and imported-edge Fillet, then exposed a missing saved STEP context when the Parameters tool
built candidate/original preview scenes. That product path is being corrected before native rerun.
The other failure is an existing Workplane test's actual cursor-position guard (requested 3208,304,
observed 2017,356); it requires an isolated native recheck with the same pointer assertions retained.
Neither failure is counted as acceptance.

The Parameters product correction changes only `shell_parameter_tools.cpp`; its candidate and
original scenes now share the saved-root context and validate observed sources before publishing
preview. Native diagnostics also include the task summary before the Apply assertion. The five-target
incremental build passes (`/tmp/blcad141-build-7.log`, two GUI translation units plus GUI/app links);
Core/Geometry sources and test binaries remain unchanged from their successful complete gates.
The updated 970-input freeze is held for subsequent GUI/native acceptance.

Native targeted rerun (`/tmp/blcad141-native-fix.log`) confirms the unchanged Workplane pointer
workflow passes and the Parameters correction reaches successful Apply, physical +16 mm³ change,
history, Save/Open and export. The remaining failure is the test's combo helper assuming each Down
key advances one numeric row: standalone Repeat is correctly disabled, so selecting Refresh skips
that row. The helper is corrected to stop at the requested actual enabled index using bounded real
key events; production command availability remains unchanged. The focused rerun itself is not a
passing gate (1/2 cases passed).

The bounded real-key combo helper fix builds successfully (`/tmp/blcad141-build-8.log`, native test
translation unit and GUI test link only). The current source/test/coverage freeze remains **970**
files, SHA-256 `b4e445ff1a40202e7b623460eed89263def50f04457b5d17b31d29961e92d288`.
No further source edits occur during subsequent native/full GUI gates.

Final native 100% matrix passes **29 / 17,979**, no skips
(`/tmp/blcad141-native-100-final.log`). It includes new direct Editable import, actual imported top-face
mouse selection, face-supported boss, Cut/Hole/imported-edge Fillet, physical parameter edit with
unchanged 1,000 mm³ base, Undo/Redo/Open/export, full modeled refresh and incompatible-face rollback.
Existing source/topology/Reference, feature edit, Revolve, Surface drag, construction picking,
Workplane edit and no-cache repair paths also pass; the prior pointer failure does not recur.
Captures in `/tmp/blcad141-native-100-final` were inspected for preview/refresh state and control
accessibility. This proves existing Revolve context regression, not a new native imported-axis pick.

Native 150% passes **1 / 554**, no skips (`/tmp/blcad141-native-150-final.log`). Its captures in
`/tmp/blcad141-native-150-final` show the complete modeled refresh result and accessible fixed
Prepare/Apply/Close controls within the existing scrolled source task. The complete GUI suite is
running on the unchanged freeze; numbered acceptance still waits for it and the final startup/
legacy-export/inventory checks.

Full offscreen GUI passes **643 cases: 611 passed, 32 expected native-only skips / 83,923 assertions**
(`/tmp/blcad141-gui-full.log`). Separate native startup reaches intentional **124**
(`/tmp/blcad141-startup-final.log`). Historical bodyless Part CLI export passes and writes **19,170
bytes** (`/tmp/blcad141-cli-legacy-final.log`); the initially omitted input argument was a command
invocation error and was corrected in the repeatable command above. Final Ninja audit retains
**502 nonempty** records (`/tmp/blcad141-ninja-deps-final.log`). All **970** runtime fingerprints
remain unchanged under `b4e445ff1a40202e7b623460eed89263def50f04457b5d17b31d29961e92d288`.
Numbered completion advances through Block 141; Blocks 142–143 remain planned and cannot start
until this block's PR is merged and main verified. Final serialized inventory/focused evidence and
PR delivery are recorded next.

The final documentation audit initially rejected `current_block: 141` after completion advanced to
141: the existing contract requires the next numbered block. The plan now records **current 142,
planned**, explicitly blocked from starting until Block 141 merges; no Block-142 implementation or
acceptance is claimed. The inventory rule is unchanged.

Final focused GUI plus serialized Shell/documentation inventory passes **14 cases: 11 passed,
3 expected native-only skips / 849 assertions** (`/tmp/blcad141-gui-focused-final.log`) after the
numbered boundary is updated. Runtime freeze is unchanged; only Markdown changed after the
successful full/native gates. Block 141 is ready for one reviewed commit/PR and protected merge.

Delivery: [PR #133](https://github.com/GideonBa/Better-Linux-CAD/pull/133), verified implementation
commit `c4fc431`. Runtime fingerprints remain unchanged; this PR must merge before Block 142 starts.
Final serialized documentation/GUI inventory passes **3 / 313** after PR references are added
(`/tmp/blcad141-final-inventory.log`); all 970 runtime fingerprints remain unchanged.

PR #133 merged as `525c98f`; main exactly matches tested head `2db9891` and all **970** runtime
fingerprints. Fresh GitHub searches find no open issues or PRs before Block 142 begins.

### Block 142 — Structured STEP assembly import (accepted 2026-10-29)

Branch `feat/structured-step-assembly-import` starts after PR #133 is merged and main `525c98f`
exactly matches tested head `2db9891`, tree `03aaa5c110de8710e6d2fdb5e1058c5953e9444c`, and all
970 runtime fingerprints. Fresh GitHub searches find no open issues or PRs. Core, Geometry and GUI
source owners work in one branch; all owners must freeze before any shared build/test starts.
The always-synthetic root policy, initial-only candidate validation, exact placements, Part-owned
catalogs and distinct protected Project-copy workflow are recorded in the STEP import sequence.
Numbered completion remains through Block 141 until the acceptance gates below are complete.

The first five-target build passes (`/tmp/blcad142-build-1.log`). A signed/unsigned comparison in a
new Geometry assertion was corrected; the second build passes without that warning
(`/tmp/blcad142-build-2.log`). The initial runtime inventory contains **978** files. Focused Core
passes **7 / 1,651** (`/tmp/blcad142-core-focused.log`). No full/native gates have run yet.

The time-limited malformed-source run terminates normally, with cycle and complete-depth checks
working; its missing-definition fixture failed while writing an invalid null handle, before reaching
the reader. It is being changed to write valid STEP and then replace one usage child field with `$`.
The first GUI focused run exposes an overstrict Assembly-ambiguity policy: the existing structured
writer intentionally emits one Assembly product per rooted occurrence. Its valid five Assembly
products must remain distinct even when geometry is equal; the importer now needs deterministic
metadata/context ordering, while leaf Part/Body recovery remains strict. Test oracles compare the
actual STEP product definitions, not the original author's three Assembly definitions. No exporter
semantics are changed for this correction. These initial failing runs are not accepted gates.

The third five-target build passes (`/tmp/blcad142-build-3.log`) with canonical Assembly ordering
and corrected actual-source GUI oracles. Full Core passes **596 / 22,501**
(`/tmp/blcad142-core-full.log`). The malformed-source test now reaches the real reader; its missing
mandatory child is rejected by strict entity validation before usage preflight. Its assertion now
accepts that exact diagnostic or the explicit missing-product-definition diagnostic, while cycles
still require the cycle diagnostic and source bytes remain unchanged. Production validation order
is unchanged. This test-only correction is built under the 978-file freeze
`3e1e6b619a3986d755e1483c666d3cf25d8e8139efcdccb33dad0ed7053f859d`.

The malformed-source regression passes **2 / 74**. The next focused GUI run exposes only a test
metadata assumption: the public exporter writes its actual product name
`blcad:part-definition:supplier.plate`, rather than the authoring display name. Both GUI oracles now
compare the exact actual Reader product name; production naming is unchanged. The fifth five-target
build passes (`/tmp/blcad142-build-5.log`). Under the final **978-file** freeze
`a39896f9dac3f7bb6ecbcdfcb8bf61a7d8007bea99ac042e4ed5b36c70c980c0`, focused Geometry passes
**10 / 1,313**, and focused GUI passes **7 cases — 6 passed, 1 native-only skip / 1,590 assertions**
(`/tmp/blcad142-geometry-focused-final.log`, `/tmp/blcad142-gui-focused-final.log`). Full Geometry
and native Sources workflows then run against that freeze.

Full Geometry passes **560 / 43,872** (`/tmp/blcad142-geometry-full.log`). Native 100% passes
**11 / 5,238** and native 150% passes **1 / 1,414**, without skips
(`/tmp/blcad142-native-100.log`, `/tmp/blcad142-native-150.log`). The native set includes both new
structured import modes, existing Reference/Editable workflows, topology/source inspection, viewer
lifecycle and current Shell coverage. Structured workflows prepare all five world-space Body
occurrences, save and explicitly open the distinct Project, and reject source changes during the
save dialog and while consuming original derived geometry. Original JSON/history/camera/selection
and source bytes are checked across cancellation and publication failures. Captures in
`/tmp/blcad142-native-{100,150}` were visually inspected for posed geometry, hierarchy, controls and
stale-state clearing. All **507** Ninja dependency records are nonempty and valid
(`/tmp/blcad142-ninja-deps.log`). Legacy CLI export passes with **19,170 bytes**
(`/tmp/blcad142-cli-legacy.log`). These entries describe the successive gates; final acceptance
follows below.

```sh
BLCAD_UI_CAPTURE_DIR=/tmp/blcad142-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[native-step-structured-import],[native-step-editable-parts],[native-step-reference-parts],[native-step-topology],[native-step-inspection],[integration][gui-feature-coverage],[native-viewer-lifecycle]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad142-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-structured-import]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
```

Final acceptance: full GUI passes **647 cases — 614 passed, 33 expected native-only skips /
85,200 assertions** (`/tmp/blcad142-gui-full.log`). The separate native startup reaches intentional
timeout **124** without a crash (`/tmp/blcad142-startup.log`). After numbered documentation advances
through Block 142 with **current Block 143 planned**, the final Shell inventory passes **3 / 313**
(`/tmp/blcad142-final-inventory.log`). Runtime inputs remain exactly the frozen **978** files; all
five builds, focused/full suites, native workflows, visual review, startup and CLI gates pass.
Core and Geometry sources/binaries are unchanged by the final GUI-only name-oracle correction.
The staged diff must be reviewed and delivered through one protected PR; Block 143 cannot start
before that PR merges and main matches the tested tree and runtime fingerprints.

Delivery: [PR #134](https://github.com/GideonBa/Better-Linux-CAD/pull/134), verified implementation
commit `dbe6560`. Runtime fingerprints remain unchanged; this PR must merge before Block 143 starts.

PR #134 merged as `c61f5fe`. Main exactly matches tested head `d930a9a`, tree
`5a2aad73ad0a76398fd131c0ee12253329acde4f`, and all **978** frozen runtime inputs. Fresh GitHub
searches find no open issues or PRs before Block 143 begins.

### Block 143 — Integrated STEP import acceptance (accepted 2026-10-29)

Branch `test/integrated-step-import-acceptance` starts from verified main `c61f5fe` after PR #134.
The shared commands/tasks from Blocks 137–142 remain the application authorities. Extend continuous
Reference, direct/converted Editable and structured workflows with read-back export geometry,
ordinary post-import authoring, exact GUI/headless equivalence and atomic failure evidence. The
execution matrix is in the STEP sequence. Source owners edit disjoint Geometry/GUI test files;
all owners must freeze before the shared five-target build and focused/full/native gates. No
Block-143 acceptance is claimed yet; numbered completion remains through Block 142.

All source owners freeze before the first five-target build. The initial **978-file** fingerprint is
`b450ea1d0106297b3f9465f798285294bcaef45be1dfaa3e1afc0a8adbcb1c6f`.
Changes extend existing Geometry/GUI integration tests and current documentation/coverage scope;
application and library implementation remain unchanged. Continuous Reference acceptance includes
actual command insert/repeat, imported relationships, 25-degree joint motion, analysis, JSON,
grouped explicit refresh, conversion and physical read-back of both export forms. Editable acceptance
uses direct and converted command entries through the same ordered additive/subtractive/finishing
history. Structured Editable acceptance adds normal post-import relationships, a Body edit, actual supplier
height refresh, exact hierarchy/placement retention and posed export read-back. GUI acceptance uses
actual analysis/export controls, canonical headless parity and missing/stale/Lost/Ambiguous matrices.
The first build and independent read-only peer reviews are running; no gate outcome is assumed.

The first five-target build passes (`/tmp/blcad143-build-1.log`). Peer review identifies two new
Reference cache-count assumptions: Reference Bodies are stored in `reference_result`, separately
from authored `body_shapes`. The assertions now require **zero authored entries, one Reference
entry and the exact resolved Body shape**. This test-only correction is built by the second
five-target build (`/tmp/blcad143-build-2.log`). Other independent Geometry/GUI peer reviews find
no blocker. The current **978-file** freeze is
`e0942e2c06a4aa15f79ce60597a12a764a9fa36f24e8b3a2fc7f0789615d8722`.

Full Core passes **596 / 22,501** (`/tmp/blcad143-core-full.log`). Focused integrated Geometry
passes **5 / 5,978** (`/tmp/blcad143-geometry-focused.log`); focused GUI passes **8 cases — 5 passed,
3 native-only skips / 1,765 assertions** (`/tmp/blcad143-gui-focused.log`). Full Geometry and native
100% workflows run next; no full/native completion is assumed.

Full Geometry passes **564 / 49,787** (`/tmp/blcad143-geometry-full.log`). Native 100% passes
**13 / 7,391**, without skips (`/tmp/blcad143-native-100.log`), including the continuous
Reference/Editable/structured workflows, source/topology inspection, viewer lifecycle, headless
failure matrix and current Shell inventory. The native Reference workflow opens actual interference,
clearance and contact tasks after save/reopen/refresh and compares their result counts to headless
analysis; flattened and structured exports are re-read for all three posed leaves. Direct Editable
and structured exports likewise compare real solid count, volume and footprint/world placements.
Reviewed 100% captures show analysis controls, modeled refresh and the five posed structured leaves.
Native 150% is still running. All **507** dependency records are nonempty and valid
(`/tmp/blcad143-ninja-deps.log`); legacy CLI again writes **19,170 bytes**
(`/tmp/blcad143-cli-legacy.log`). Final GUI/startup/inventory acceptance remains pending.

Native 150% passes **4 / 3,875**, without skips (`/tmp/blcad143-native-150.log`). Captures in
`/tmp/blcad143-native-{100,150}` were reviewed for reachable analysis/source controls, current
modeled results and posed structured previews. Both native scales prove real UI analysis, physical
export read-back and unchanged source assets. Full GUI now runs against the same frozen inputs;
startup and final numbered-documentation inventory follow its result.

```sh
./build/dev-gui/blcad_geometry_tests "[integration][step-import-mvp]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[integration][gui-step-import],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad143-native-100 QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[integration][gui-step-import],[native-step-reference-parts],[native-step-topology],[native-step-inspection],[native-viewer-lifecycle],[integration][gui-feature-coverage]"
BLCAD_UI_CAPTURE_DIR=/tmp/blcad143-native-150 QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[native-step-reference-parts],[native-step-editable-parts],[native-step-structured-import]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
```

Final acceptance: full GUI passes **648 cases — 615 passed, 33 expected native-only skips /
85,528 assertions** (`/tmp/blcad143-gui-full.log`). Separate native startup reaches intentional
timeout **124** without a crash (`/tmp/blcad143-startup.log`). After the plan records completed
Block 143 and the unassigned numeric successor 144, final Shell inventory passes **3 / 313**
(`/tmp/blcad143-final-inventory.log`). No Block-144 implementation scope or work is introduced.
Independent requirement review maps every canonical import action, both import modes, direct and
converted modeling, physical export equivalence, source-byte/schema proofs and the complete
negative matrix to actual tests. Reused Block-142 malformed-structure fixtures remain in full
regression; their pure candidate construction is distinguished from session/cache/output failure
transactions. All runtime inputs remain the final **978-file** freeze recorded above.

STEP Import MVP-10 is complete through Block 143. Final delivery stops at this requested boundary.

Delivery: [PR #135](https://github.com/GideonBa/Better-Linux-CAD/pull/135), verified implementation
commit `220d030`. All runtime fingerprints remain unchanged. The branch contains only this
integrated acceptance slice and its documentation; later implementation remains unassigned.

### Final issue/status completion audit — 2026-10-29

On verified main `471faff`, the actual GitHub issue requirements, implementation, regression tests,
existing native/full/profile logs and merged PRs were independently audited for the seven-item
execution queue (#114, #104, #109, #111, #110, #112, #106). No implementation or acceptance gap was
found. GitHub creation/merge timestamps, exact predecessor bases and local branch/commit records
confirm serial delivery through PR #135. Fresh searches find no open issues or PRs.
The earlier completed #105/#107/#108/#113 were also checked against their original requirements,
current implementation/tests, existing result logs and merged predecessor PRs; their acceptance
criteria remain covered.

The central triage table still described #106 and #112 as missing despite their accepted delivery
sections. This documentation-only follow-up corrects those cells and preserves completed Block 143.
The tested tree's **978** runtime fingerprints remain unchanged; the five-target Ninja dry run has
no work. Existing full Core/Geometry/GUI and native 100%/150% evidence above therefore remains the
runtime evidence. Final Shell inventory passes **3 / 313** after this plan correction
(`/tmp/blcad-completion-audit-inventory.log`); no full suite or native test rerun is claimed for
these Markdown-only edits.


### Block 144 — Inventor-inspired workflow layout (2026-11-04–05)

Starts from main `c9e4560` after merged PR #136, with no open PR. The active main ruleset requires
PR delivery (zero approvals, no required status check); no bypass or direct push to main is used.
Implementation retains the existing fonts, icon assets, palette and CAD/session owners. Changes
cover full-width command placement, File/Quick Access, seven workflow tabs, command discovery and
supporting dock recovery. `user-interface.md` owns current interaction and layout mapping.

Initial five-target build passes. Initial offscreen focus passes **20 cases / 1,240 assertions**;
initial native layout passes **4 / 89** with captures inspected at 1280×800 and 1024×640. The first
sandboxed xcb process could not open display `:0`; the native repeat uses authorized display access.
The known OCCT Medium depth/stencil visual warning appears. These are development results before
final refinements and do not substitute for the final gates below.

Reproduction (builds, source changes, GUI suites and startup remain serial):

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j 8
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[workflow-layout],[appearance],[icons],[shell-combined-workflow],[block136-inspect-export],[block136-reference-repair],[block136-sketch-diagnostics],[shell-repair-open-input],[integration][gui-step-import],[native-step-inspection],[native-step-topology],[integration][gui-feature-coverage],[gui][viewport],[sketch-ui-review],[native-viewer-lifecycle]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 ./build/dev-gui/blcad_gui_tests "[workflow-layout],[appearance],[icons]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

The initial sandboxed full-GUI run was deliberately interrupted (exit 130) after it established
obsolete ribbon indexes/menu assumptions in STEP, repair and diagnostics input helpers and the
sandbox's protected file-publication failures (`Could not publish STEP file`, structured copy hard
links). The actual input helpers now use the new visible tabs/File menu; their modeling expectations
remain unchanged. Final gates use regular filesystem access and are listed below. Existing
Core/Geometry behavior is not expanded by this GUI change.


The first complete run with normal filesystem access reaches **653 cases: 619 passed, 1 failed,
33 native-only skips / 85,338 assertions** (`/tmp/blcad144-gui-delivery.log`). Its only failure is
the combined 40-blade rotor's five-second menu watchdog around Open. The test now prefers the
visible Quick Access control when a command also has a File menu entry, so the menu-only watchdog
no longer includes synchronous Part loading. The same full run passes file publication, repair,
all five new layout/discovery cases and the remaining modeling expectations. Final acceptance repeats on the corrected input route. A subsequent native screenshot review
also replaces the new command chooser's colored Qt standard buttons with the existing Pixelarticons
Check/Close assets; no modeling code changes. Focused native/scaled checks repeat after that
presentation-only correction.


Broad native Qt/OCCT passes **96 cases / 33,276 assertions**, without skips
(`/tmp/blcad144-native-100.log`). It covers the command layout, appearance/icons, combined Sketch,
Part, Surface and Assembly workflows, file exchange and repair, STEP inspection/topology/import,
viewport lifecycle and coverage inventory. Initial 150% native verification passes **13 / 1,565**
without skips (`/tmp/blcad144-native-150.log`), including actual dimension/feature/history/file input.
The subsequent chooser-icon correction receives focused final native/scaled proof below; all other
application behavior is identical. Feature captures at ordinary/compact sizes are reviewed and
saved under `docs/images/inventor-workflow-{feature,compact}.png`.


Final chooser/layout/icon verification passes **12 cases / 1,288 assertions** at both native 100%
and 150%, without skips (`/tmp/blcad144-native-final.log`, `/tmp/blcad144-native-150-final.log`).
Both capture sets are reviewed, including actual Ctrl+K, arrow navigation and Escape, and the final
monochrome dialog is recorded as `docs/images/inventor-workflow-search.png`. Separate native startup
reaches the intentional six-second timeout **124** (`/tmp/blcad144-startup.log`). Native logs contain
only the existing Medium depth/stencil visual warning; no High-severity or drawable/window failures.
All five requested targets build (`/tmp/blcad144-build-icons.log`); all **508** Ninja dependency
records are nonempty and valid. Fonts, icon assets, palette definitions and splash sources are
unchanged. Core/Geometry standalone suites were not rerun for this shell-only revision; the full GUI
and native workflows exercise their existing modeling, persistence and exchange paths.

The final **948-file** runtime manifest `/tmp/blcad144-runtime-freeze.json` covers `src/`, `include/`,
`tests/`, `assets/`, `cmake/`, `scripts/`, `CMakeLists.txt` and `CMakePresets.json`; its SHA-256 is
`f18e3f6a4f7e99d484aeb2b7dfd06e28a163b2e9ec4a330a64a2ac8a6b8bad7f`. Documentation and its native
screenshots are outside that runtime freeze. Final full GUI passes **653 cases — 620 passed, 33 expected native-only skips / 85,483 assertions**
(`/tmp/blcad144-gui-final.log`). All runtime hashes remain unchanged through final native, scaled,
startup and full-GUI verification. The earlier rotor menu watchdog does not recur. Source and staged
diffs are reviewed; no Core/Geometry, persistence, font, palette, icon-asset or splash changes are
included. The separate planning commit is `76705a1`; implementation delivery is recorded in the plan.


Delivery is [PR #137](https://github.com/GideonBa/Better-Linux-CAD/pull/137), implementation `e538be8`
after planning commit `76705a1`. The plan records completed implementation through Block 144;
Block 145 is an unassigned planning boundary. The PR must merge normally before any successor
branch starts, with main/tree/runtime verification after integration. This final acceptance/status
update changes documentation only; the runtime freeze remains unchanged.

## Advanced workflow planning validation — 2026-11-06

Documentation-only delivery from main `e08280c` on `docs/inventor-feature-parity-plan` after
verified merged PR #137 and refreshed main. `inventor-parity-sequence.md` allocates 60 planned
Blocks 145–204; `inventor-parity-test-plan.md` defines 37 scenario IDs, mandatory combinations and
future native/performance gates. Existing catalog/feature owners link to that single future
sequence; `user-interface.md` owns the clearly labelled planned Inventor-like interaction contract.
No new feature, schema, runtime code or test implementation is delivered here.

Validation:

```bash
git diff --check
python3 /tmp/blcad-validate-planning.py
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[gui][shell-coverage]"
```

The temporary standard-library audit checks changed Markdown local links/anchors, referenced
contract paths, paired code fences, unique consecutive blocks, backward-only prerequisites,
defined scenario IDs and unchanged `implemented_through: Block 144` / `current_block: 145`.
It also verifies that the changed paths contain only documentation. All checks pass. Existing
shell coverage passes **3 test cases / 313 assertions**, with no skips, using the current built
GUI binary. Source and staged documentation diffs are reviewed before delivery.

Builds, full CAD suites and native Qt/OCCT are not rerun for these documentation-only changes.
The new feature/native tests and performance budgets are planned obligations, not measured or
executed results. Actual native interaction evidence is required by their implementation blocks.
Delivery is PR #138 with reviewed plan commit `86fc04f`; this final delivery-record update is
documentation only and retains the same runtime tree. `mvp-plan.md` records the protected-merge
handoff; no successor implementation begins here.

## Block 145A Capability evidence and baseline — 2026-11-06

Starts on `feat/advanced-workflow-evidence` from refreshed main `dbff649`, after PR #138 merged.
This is the first serial slice of 145. It adds an explicit 24-entry planned/implemented capability
section to the existing coverage manifest and three actual tests; accepted historical family
records remain semantically identical. No production CAD API, UI widget, command or file schema
changes. The new source has per-file CMake build/OCCT metadata definitions, keeping kernel headers
private to Geometry. Initial development compiler errors (private OCCT header and Catch2 INFO
expression grouping) and the fixture's missing datum are corrected before final verification.

```bash
cmake --build build/dev-gui --target blcad_gui_tests -j 8
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_WORKFLOW_BASELINE=/tmp/blcad145a-baseline.json ./build/dev-gui/blcad_gui_tests "[parity-b145],[gui][shell-coverage]"
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145a-native-100 ./build/dev-gui/blcad_gui_tests "[hermes-design-workflow],[workflow-layout]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145a-native-150 ./build/dev-gui/blcad_gui_tests "[hermes-design-workflow],[workflow-layout]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

Final build passes (`/tmp/blcad145a-build.log`). Focused coverage/baseline passes **6 cases /
1,523 assertions**, no skips (`/tmp/blcad145a-focused.log`). The first sandboxed artifact run
passed its geometry/history assertions but could not commit Qt's atomic output file; the final
identical run with regular filesystem access passes, including publication. Native original
modeling/layout passes **6 / 390** at each scale, without skips, in the corresponding
`/tmp/blcad145a-native-{100,150}.log`. Ordinary/compact feature and command captures in the listed
directories are reviewed; completion controls remain reachable. Separate native startup reaches
the intentional timeout **124**. Native logs retain the existing Medium depth/stencil warning,
with no High-severity or drawable failures. Full Core/Geometry/GUI suites are not rerun for this
test/inventory-only slice; the relevant coverage and real-input native baseline are executed.

The checked raw report is [block145a-workflow-baseline.json](benchmarks/block145a-workflow-baseline.json).
On recorded Debug/Qt/OCCT hardware, fresh-session preview samples have p50/p95 **0.795/1.150 ms**
(3 trials); reused-controller previews have **0.769/0.890 ms** (10 trials). These measure only the
existing simple Extrude `preview_shape`, excluding setup, validation readouts and UI publication;
they are not a cold-process measurement or a portable latency guarantee. Every result has one
solid and **1600 mm³**; preview preserves source JSON/revision/history. Apply/Undo/Redo and canonical
reopening are also checked. Environment, raw samples, process-wide memory high-water mark and
943 runtime fingerprints are in the report. All hashes match after final native verification;
inventory digest is `2d3b6cf35471d6800ae5999230f4bdfd8ab443516b53f3036a0a1f4d6611e1ae`.

Source/staged diff review and normal protected PR delivery are required. Block 145 remains open
for shared controls (145B), breadcrumb/Create Another lifecycle (145C) and whole-block acceptance.
Delivery is PR #139, implementation `40bcd79`; the final delivery-record edit changes documentation
only and retains the same runtime inventory. Main must be refreshed after regular merge.


## Block 145B shared property controls — 2026-11-06

Slice B starts from verified main `ff39198` after merged PR #139. Reusable property groups and
semantic selector presentation are adopted in the existing Extrude owner. No Core/Geometry/schema
change is made; breadcrumb transactions and Create Another remain assigned to 145C. Tests cover
keyboard collapse, plain-text diagnostics/accessibility, Pick/Clear owner lifetime, blocked-signal
selection synchronization, disabled inputs, invalid/corrected fields, compact completion controls,
Ctrl+Enter's actual typed scalar, and physical native first/second face picking.

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad_app -j8
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[parity-b145],[gui][shell-coverage],[block136-extrude-extents],[block136-extrude-limit-picking],[block136-thin-extrude]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145b-native-100 ./build/dev-gui/blcad_gui_tests "[property-controls],[shell-combined-workflow],[workflow-layout],[appearance],[icons]"
```

Final GUI test/application build passes (`/tmp/blcad145b-build.log`). Focused offscreen passes
**28 cases — 27 passed, 1 expected native-only skip / 3,682 assertions**
(`/tmp/blcad145b-focused.log`). The native property-controls focus separately passes **4 / 183**,
without skips (`/tmp/blcad145b-native-focus.log`). Full offscreen GUI passes **659 cases —
626 passed, 33 expected native-only skips / 85,853 assertions** (`/tmp/blcad145b-gui-full.log`).
Broad native 100% passes **57 / 23,719**, without skips (`/tmp/blcad145b-native-100.log`).
Native 150% passes **12 / 1,862**, without skips (`/tmp/blcad145b-native-150-repeat.log`). The first
150% run passed all affected feature assertions but missed one Down-arrow navigation in the existing
command-search test (row remained 0); its identical fresh-process repeat passes the whole filter.
This native focus observation is retained rather than counted as an initial clean run. No search
implementation change is included. Full Core/Geometry suites are not rerun for this presentation
slice; full GUI and native combinations exercise their existing consumers.

During development the physical scalar oracle caught Ctrl+Enter applying the old 10 mm depth
instead of the typed 8 mm when native focus changed. The final route validates displayed text
regardless of focus; it produces exactly one 1600 mm³ solid and one undoable feature. A second
regression caught stale selection counts when the existing viewport owner blocks combo signals;
explicit presentation synchronization now refreshes count and Clear enablement without issuing
another pick callback. Source profile summaries use the existing selection helpers, since the idle
command-discovery helper deliberately returns no context during a task.

The scaled command is:

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145b-native-150 ./build/dev-gui/blcad_gui_tests "[property-controls],[hermes-design-workflow],[workflow-layout],[shell-combined-extrude-extents],[shell-combined-extrude-limit-picking]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

Captures under `/tmp/blcad145b-native-{100,150}` are inspected: ordinary and compact panels retain
reachable Apply/Cancel, source names and ordered groups; native both-limit capture shows the
selected semantic faces and valid result. At 150% the scroll area carries the longer form without
pushing completion controls offscreen. Startup reaches intentional timeout **124**. The known
Medium depth/stencil warning remains; no High-severity/drawable failure is observed.

Final source formatting and `git diff --check` pass. The 797 code/test/CMake fingerprints frozen in
`/tmp/blcad145b-runtime-freeze.json` are unchanged through focused/full/native checks (digest
`f45229c2857acd480651763b952f23fb369b2f9eec1e0d2476fbcd8ce31ec7fd`). Documentation is outside
that freeze. Source and staged review precede protected PR delivery. Block 145 remains open for C.

Delivery: [PR #140](https://github.com/GideonBa/Better-Linux-CAD/pull/140), implementation `84fd6e1`.
The final delivery record changes documentation only; runtime fingerprints remain unchanged.

## Block 145C1 complete Part workflow transaction — 2026-11-06

C1 starts on `feat/compound-part-workflow` from verified main `6c15fe1` after normal merge of
PR #140 and exact tree comparison. The existing `GuiDocumentSession` owns the transient checkpoint;
its normal validated transactions, full sidecar history and atomic save publication are reused.
No Core/Geometry intent or JSON schema changes. Qt breadcrumb/Create Another adoption remains C2.

The focused tests combine actual source parameter changes, constraint/dimension catalogs and a
headless Extrude producing 2400 mm³. They require one final history entry, full Undo/Redo, complete
Cancel (including selection/filter/visibility/workspace/task state), preserved old redo branches,
committed-only Save/Save As/reopen for all three files, failed-save path/byte invariance, failed
geometry publication followed by correction, scope/replacement guards, immutable checkpoints in
copied sessions, no-op publication and catalog-only edits. Missing committed sidecars must be
removed rather than populated from draft catalogs. Length values remain strictly positive; the
geometry-failure injection uses a positive sub-tolerance height, rather than an invalid Quantity.

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad_app -j8
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[part-workflow]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[part-workflow],[document-session],[feature-edit],[parity-b145],[gui][shell-coverage]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
```

Final GUI/application build passes (`/tmp/blcad145c1-build.log`). The transaction focus passes
**11 cases / 655 assertions**, without skips (`/tmp/blcad145c1-transaction-focus.log`). Broader
session/feature-edit/evidence/property focus passes **36 cases — 35 passed, 1 expected native-only
skip / 2,223 assertions** (`/tmp/blcad145c1-focused.log`). Full offscreen GUI passes **670 cases —
637 passed, 33 expected native-only skips / 86,508 assertions** (`/tmp/blcad145c1-gui-full.log`).
No native breadcrumb evidence or whole-Block-145 completion is claimed by C1.


Native compatibility on the same frozen implementation passes **5 cases / 462 assertions** at
both 100% and 150%, without skips. These exercise the existing dimension/Extrude/feature-edit/history/
reopen/STEP workflow and property controls; they do not exercise a Qt breadcrumb, since C1 has no UI
adopter. Logs: `/tmp/blcad145c1-native-{100,150}.log`. Fresh compact feature capture at 150% is
reviewed; completion controls remain reachable. Commands run serially:

```bash
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c1-native-100 ./build/dev-gui/blcad_gui_tests "[hermes-design-workflow],[property-controls]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c1-native-150 ./build/dev-gui/blcad_gui_tests "[hermes-design-workflow],[property-controls]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

The 953 source/test/asset/build-script fingerprints (including six `.gitkeep` placeholders) in
`/tmp/blcad145c1-runtime-freeze.json` remain unchanged through these checks; SHA-256 of the compact
inventory is `2c5030e8b8dfc330c75583e48996d6ef7042609d8996b9e13c0a997bfd11f5fe`.
Formatting and diff checks pass. Full standalone Core/Geometry suites are not rerun for this
GUI-session-only change; full GUI and native compatibility exercise their existing consumers.

Separate native startup reaches the intentional timeout **124**. Logs retain the existing Medium
depth/stencil warning, with no High-severity or drawable failure. No native retry is required in C1.

Delivery: [PR #141](https://github.com/GideonBa/Better-Linux-CAD/pull/141), implementation `861a5c6`.
The delivery-record commit is documentation only and preserves the runtime freeze.


## Block 145C2 source Sketch breadcrumb — 2026-11-06

C2 starts on `feat/feature-source-workflow` from verified main `dc0a235`, after normal merge of
PR #141 and exact tree comparison. The current Extrude owner adopts C1's complete Part workflow.
A fixed Part/Draft context and compact Extrude/source breadcrumb row sit above scrolling inputs;
Sketch editing uses the existing workspace/tools. Local Undo/Redo, complete Cancel, committed-only
Save, pending-source revalidation, current frames/limit choices and root-only publication are
covered. STEP and full-model Measure reject a workflow draft. No Core/schema change occurs.
C3 still owns command-first profile collection/Create Another and complete Block-145 acceptance.

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad_app -j8
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[source-sketch-workflow],[property-controls-widget],[part-workflow]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[parity-b145],[gui][shell-coverage],[block136-extrude-extents],[block136-extrude-limit-picking],[block136-thin-extrude],[block136-path-extrude]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[block136-path-extrude],[gui][shell-coverage]"
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests
```

Build passes (`/tmp/blcad145c2-build.log`). Initial source/property/transaction focus passes
**19 cases — 18 passed, 1 expected native-only skip / 1,080 assertions**
(`/tmp/blcad145c2-focused.log`). Final broader focus passes **50 cases — 48 passed, 2 expected
native-only skips / 5,537 assertions** (`/tmp/blcad145c2-focused-final.log`). Path/Coverage passes
**11 / 1,260**, without skips (`/tmp/blcad145c2-path-coverage.log`). The shell inventory keeps only
actual Qt widget/native proof sources in its `shell.proofs`; controller/consumer proofs remain in
their owning contracts/tests until the whole shared workflow entry is adopted in C3.

The first new UI fixture attempted Extrude immediately after Save As, which invalidates its prior
profile handoff in the existing shell. It now explicitly selects the source Sketch in the browser
before command entry. No change to that existing selection policy is hidden in C2. Native initial
source focus passes **5 / 551**, without skips (`/tmp/blcad145c2-native-focus.log`), including a
physically selected and typed driving dimension, 6400 mm³ final geometry, failed input recovery,
complete cancellation, one-step Undo/Redo and reopening. The subsequent breadcrumb caption/layout
refinement is covered by final frozen full/native checks below; this initial run alone is not
claimed as final native acceptance.

Runtime source/test/asset/build-script freeze contains 955 files in
`/tmp/blcad145c2-runtime-freeze.json`; compact-inventory SHA-256 is
`33fd9b98982e002c5e6bf71f3fc6f139a25915bbab8a2d539da1c503d1be4227`.
The full GUI and final native checks run serially against this unchanged implementation.


The first full run stops at case 159 because an existing Finish-Sketch test retained a borrowed
Sketch pointer across complete workflow cancellation. GDB locates the invalid dereference in the
test's line-semantic construction. The test now reacquires the restored Sketch by stable ID before
continuing; no durable raw-pointer guarantee is introduced. The subsequent broad feature/finish
focus also catches selection being cleared by the late scene rebuild after cancellation. The owner
now rebuilds the restored scene before reapplying its prior semantic selection, preserving browser-
only session selection where no viewport target existed. The new source-widget case checks exact
entry selection after source edits and Cancel. These corrections invalidate the initial full run;
full/native final acceptance repeats against the updated freeze above.

Final Shell-feature/Finish-Sketch/source focus passes **62 cases — 60 passed, 2 expected native-only
skips / 2,392 assertions** (`/tmp/blcad145c2-finish-regression.log`). Scoped formatting of changed
tracked lines produces an empty patch; newly added source/test files also pass clang-format checks.
Unrelated existing formatting is preserved.

Final full offscreen GUI passes **675 cases — 641 passed, 34 expected native-only skips /
86,827 assertions** (`/tmp/blcad145c2-gui-full.log`). All runtime fingerprints match the updated
freeze. Final native/scaled acceptance follows on that same source.


Final broad native 100% executes **59 cases — 57 passed, 2 failed / 22,785 passed assertions**
(`/tmp/blcad145c2-native-100.log`). Both failures are the existing popup harness's native cursor-
settling precondition in Surface contexts and the complex rotor parameter chooser: requested row
positions remain correct, but actual desktop cursor positions differ, once outside the first screen.
No product or harness logic is changed to hide this condition. A targeted fresh-process repeat of
both complete workflows passes **2 cases / 1,486 assertions**, without skips
(`/tmp/blcad145c2-native-popup-repeat.log`), including 20/40-blade rotor variants. The new source,
selection, property and viewer cases pass in the original broad run.

Native 150% passes **22 cases / 3,920 assertions**, without skips
(`/tmp/blcad145c2-native-150.log`). It includes source editing, physical dimension label input,
compact completion controls, ordinary modeling/history/reopen, all affected Extrude modes and
selection/Finish-Sketch regressions. These final commands run serially against the freeze:

```bash
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c2-native-100 ./build/dev-gui/blcad_gui_tests "[source-sketch-workflow],[shell-combined-workflow],[property-controls],[workflow-layout],[extrude-regression],[native-viewer-lifecycle]"
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c2-native-100 ./build/dev-gui/blcad_gui_tests "[shell-combined-surface-context],[complex-part-workflow]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c2-native-150 ./build/dev-gui/blcad_gui_tests "[source-sketch-workflow],[property-controls],[hermes-design-workflow],[workflow-layout],[shell-combined-extrude-extents],[shell-combined-extrude-limit-picking],[shell-combined-thin-extrude],[shell-combined-path-extrude],[extrude-regression]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

Final ordinary/compact source and return captures at both scales are inspected. The source view
shows the existing Sketch ribbon/tools and disabled Extrude inputs; return restores the authored
8 mm depth, current preview and reachable Apply/Cancel. Representative real captures are checked
in at `docs/images/extrude-source-sketch.png` and `docs/images/extrude-source-compact.png`. Long
breadcrumb captions elide visually while retaining their full accessible name. Full standalone
Core/Geometry suites are not rerun: no source in those libraries changes; full GUI and native
combinations exercise their current geometry and persistence consumers.

Separate native startup reaches intentional timeout **124** (`/tmp/blcad145c2-startup.log`).
The known Medium depth/stencil warning remains; no High-severity or drawable failure is observed.
All 955 runtime fingerprints remain unchanged through final native/scaled/startup verification.
Final source/staged diff review and protected PR delivery follow; Block 145 remains open for C3.

Delivery: [PR #142](https://github.com/GideonBa/Better-Linux-CAD/pull/142), implementation `11fd8a8`.
The delivery-record commit changes documentation only and preserves the final runtime freeze.

## Block 145C3 profile collection and Create Another — 2026-11-06

This slice extends the existing Extrude owner, Part workflow and property selector. It adds explicit
command-first profile/open-line collection, exclusive primary/limit-face Pick roles, correction after
source changes and Create Another with literal/mode retention and fresh semantic references. Each
accepted feature includes its source Sketch edits in one history step. Selection-stage Cancel,
committed-only Save, independent Undo/Redo, canonical reopening and ordinary Repeat retain their
existing owners. Construction/Centerline single-line sources are excluded by the shared thin-source
helper. No Core intent/schema or Geometry executor changes.

New proofs are `[create-another]`, `[create-another-workflow]` and
`[workflow-interaction-baseline]`. The first uses real Qt keyboard input for profile choices/scalars
and mouse events for completion/pick/clear controls. Analytic volumes are 960, 2200 and 1320 mm³ for
independent New Bodies and 17520 mm³ for the bounded Cut fixture. Required Cut target/limit choices
remain empty after Create Another. The combined workflow authors two real Sketches, edits a Point
inside the second source, saves the first committed feature during that draft and checks one complete
Undo/Redo plus reopening. The baseline checks 1600 mm³ and exact entry JSON after every cycle.

Development findings and corrections (not accepted gates):

- Initial build succeeds. First three-case focus finds disabled Cancel during empty primary collection;
  `active()` now recognizes the owned Extrude task before its controller exists. Selection-stage
  conflict, Cancel and Escape assertions cover the correction. Two original cases already pass.
- Real-input fixture corrections use a wider unsnapped rectangle hover and explicit control-center
  scrolling before hit checks. Required hit/enabled checks remain in place.
- The first broad focus reports 93 passed, one failed and two native-only skips / 7460 passed,
  two failed assertions. The old thin selection test still expected global Extrude to reject entry
  despite another valid source. It now verifies empty explicit collection, exclusions and Cancel.
- Initial native focus passes **11 cases / 1135 assertions**, without skips. The first full offscreen
  attempt is deliberately stopped (143) after seven stale assertions across four filter/Repeat cases;
  it is not counted as a full result. Those tests now require the global command/Repeat to open
  selection while preserving incompatible-selection rejection and restoring original filters/selection.
- A new test initially equated the session mask with a task-owned viewport role mask. Existing
  `synchronize_selection_filter` intentionally admits task references in the session while the viewport
  guards their roles. The assertion now checks that established bridge explicitly. Focused final
  filter/Repeat/interaction proof passes **13 cases / 2078 assertions**, without skips.

Before the final empty-input hint correction, the complete GUI run passes **648 cases, 34
native-only skips / 87776 assertions**, and native 100% passes **80 / 11600**, no skips. Review
then finds a stale profile-summary label after Clear; it now explicitly asks for a source, with
regression assertions and an empty-stage capture. The first corrected-hint native runs pass
**80 / 11605** at 100% and **36 / 6927** at 150%.

Visual review of the latter then finds inherited scroll position hiding part of the new primary
Pick/count row. Newly shown Extrude tasks reset their owned scroll position; a regression requires
full visible regions for both controls without a reveal helper. Final gates below use this
corrected source.

The final source/test/asset/build-script freeze is `/tmp/blcad145c3-runtime-freeze.json`: **956 files**,
including six `.gitkeep` placeholders; compact-inventory SHA-256 is
`a841100e841bfe0a9f9ab5b9acb38a769d368a492734ee6b6aadcdfd6d2d7e86`.
The benchmark's existing Qt iterator omits hidden placeholders; its separate inventory contains
950 fingerprints. Each inventory is compared using its own documented enumeration.

Final checks use the existing presets, serial GUI/native execution and these filters:

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad_app -j8
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
QT_QPA_PLATFORM=xcb BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c3-native-100 ./build/dev-gui/blcad_gui_tests "[parity-b145],[shell-combined-profile-context],[block136-profile-context],[block136-selection-filters],[shell-combined-selection-filters],[block136-repeat],[shell-combined-repeat],[block136-feature-edit],[hermes-design-workflow],[workflow-layout],[appearance],[icons],[shell-combined-extrude-extents],[shell-combined-extrude-limit-picking],[shell-combined-thin-extrude],[shell-combined-path-extrude],[extrude-regression],[native-viewer-lifecycle]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad145c3-native-150 ./build/dev-gui/blcad_gui_tests "[create-another],[create-another-workflow],[source-sketch-workflow],[property-controls],[hermes-design-workflow],[workflow-layout],[shell-combined-profile-context],[shell-combined-selection-filters],[shell-combined-repeat],[block136-feature-edit],[shell-combined-extrude-extents],[shell-combined-extrude-limit-picking],[shell-combined-thin-extrude],[shell-combined-path-extrude]"
QT_QPA_PLATFORM=xcb timeout 6s ./build/dev-gui/blcad
```

P01 adds UI-transition evidence beside the retained historical 145A shape-only baseline. Execute
`[workflow-interaction-baseline]` alone in three separate native processes, setting
`BLCAD_WORKFLOW_INTERACTION_BASELINE` to a distinct JSON path for each. Each process records its
first measured workflow and ten subsequent trials on the same window after Apply/Create Another,
Cancel and Undo. Measurements include synchronous Qt input dispatch, kernel work and UI event
publication; fixture setup, application startup and independent oracle checks are excluded. Record
first-workflow and reused-window distributions separately. A command uses its actual QAction;
profile/scalar selection and completion/cancellation use Qt input events. This is a bounded existing
Extrude baseline, not a latency guarantee for future features or the complete P01 stress matrix.
Full standalone Core/Geometry suites are not rerun for these GUI changes; full GUI and native
integration exercise their existing consumers. Final acceptance and measurements are recorded below.

The final 150% scroll/selector focus passes **5 cases / 438 assertions**, without skips. Both the
empty selection-stage and compact candidate captures are reviewed: Profile label/count/Pick/Clear
are visible, disabled retained fields remain understandable, and Apply/Create Another/Cancel stay
outside scrolling content. The fixed source summary asks for input after Clear.

P01 raw results are checked in [block145c3-workflow-baseline.json](benchmarks/block145c3-workflow-baseline.json),
including environment, source JSON, 950 runtime fingerprints, all shape samples and all three native
interaction runs. The shape-only test passes **1153 assertions / one case**; its exact console count
is retained in `/tmp/blcad145c3-shape-baseline.log`. Each standalone native interaction process passes
**353 assertions / one case**, without skips; there are three first-workflow and thirty reused-window
samples, with zero failures. Fresh-session shape p50/p95 is **0.925/1.136 ms** (3); reused-controller
shape p50/p95 is **0.746/1.207 ms** (10). These have the same limited measurement boundary as 145A
and do not establish a portable speed comparison.

| Native whole transition | First workflow p50 / p95 / max ms (n=3) | Reused window p50 / p95 / max ms (n=30) |
| --- | --- | --- |
| Begin command | 11.916 / 12.114 / 12.114 | 5.512 / 8.646 / 9.854 |
| Profile keyboard choice | 88.747 / 96.393 / 96.393 | 41.830 / 57.437 / 69.611 |
| Numeric keyboard input | 23.222 / 23.489 / 23.489 | 19.239 / 27.277 / 28.408 |
| Apply/Create Another | 60.006 / 61.985 / 61.985 | 52.845 / 63.402 / 71.567 |
| Cancel | 43.713 / 46.925 / 46.925 | 42.168 / 47.673 / 60.606 |

Each row measures a complete scripted transition, including multiple Qt events where applicable,
not individual key/frame latency. Native process peak RSS is 225488 / 225588 / 226596 KiB. The
artifact's Qt inventory SHA-256 is `4192e5803b87b7ccf8c85bd06007b8c6e5b0ce5f91605151ea80051e615a5cba`;
all entries match the corresponding paths in the 956-file freeze. Original 145A evidence is retained.
These results establish a reference for the current bounded workflow, not budgets for the future
P01 workloads; those producers and the final budget gate remain assigned to their numbered blocks.

Final acceptance completed on **2026-11-07** after the requested pause, with all 956 runtime
fingerprints unchanged:

- Resume build is a no-op. The final full offscreen GUI run completes successfully: **682 cases,
  648 passed and 34 native-only skips / 87784 passed assertions** (`/tmp/blcad145c3-gui-full.log`).
- Native 100%: **80 cases / 11609 assertions**, no skips or failed cases
  (`/tmp/blcad145c3-native-100.log`). The first attempted process start waits on an automatic
  permission-review timeout; the permitted single start retry succeeds. This is not a failed or
  retried test scenario.
- Native 150%: **36 cases / 6931 assertions**, no skips or retries
  (`/tmp/blcad145c3-native-150.log`). Ordinary/compact and empty/populated selector captures are
  reviewed; final representative captures are checked in under `docs/images/`.
- Separate native application startup reaches expected timeout **124**. Native logs retain the known
  Medium depth/stencil warning; no High-severity or BadDrawable failure is observed.
- Promoted implementation/API/proof inventory: **5 cases / 494 assertions**, no skips
  (`/tmp/blcad145c3-final-inventory.log`). Its initial status check correctly rejects keeping
  `current_block` at the just-completed boundary; metadata now identifies 146 as planned/unstarted,
  while `implemented_through` is 145. No validation code is relaxed.

The full GUI count precedes the documentation-only capability promotion; the final inventory run
separately validates that promotion, exact API declarations, proof registration and negative cases.
Both runtime inventories remain unchanged. Scoped C++ formatting, new-file formatting and diff
checks pass. Block 145 is accepted; subsequent feature blocks remain planned. Delivery uses a normal
protected PR and requires refreshed main/tree comparison before the next implementation branch.

Delivery: [PR #143](https://github.com/GideonBa/Better-Linux-CAD/pull/143), implementation `7f50220`.
The delivery-record commit changes documentation only and preserves the final runtime freeze.


## Block 146A executable inventory and existing order replay — 2026-11-07

This Core-only foundation adds a read-only executable-family inventory and centralizes the
existing generic JSON replay. It does not complete authored order/suppression (146B), effective
execution (147), or the native browser (148). Runtime source edits stop during builds and checks.
Use the existing GUI preset's Core, Geometry and GUI targets so every consumer links the same Core.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j8
./build/dev-gui/blcad_core_tests "[feature-history-inventory],[feature-order],[feature-update-command]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
```

`tests/fixtures/history/legacy-{all-families,base,mixed-chain,reference}.json` are independent
canonical byte oracles captured **before rebuilding Core**, using the previously accepted
`build/dev-gui/libblcad_core.a` from main `27256d7` (PR #143). The fixture builder is
`tests/support/part_history_fixture.hpp`; each captured document also roundtrips byte-identically
through that old reader/writer. They are checked in, never regenerated by a test. The all-family
catalog covers Core intent validation for all fifteen executable families, not physical feasibility;
existing Geometry suites supply actual shape/volume/semantic-reference proof. The mixed chain adds
a workplane/Sketch consumer and independently permuted family arrays to test preservation of both
dependency and collection-order contracts. Reference mode has Body outputs but no executable base.

| Legacy fixture | SHA-256 |
| --- | --- |
| all-families | `764e38b7ec8bba5c42d16ba81441b736c88a1f776d57fcd0073a43afc1cad614` |
| base | `9cd12052b6ab1faed9c5ba0587fc94b5e0d9457b130329dc85d34993f1acfe38` |
| mixed-chain | `1016b5aff38061051b36a7097b64a6a6c07dfc2e2d99f7e816aad2d5b8a710c7` |
| reference | `02e61aa5135b6a8ac92a332e6cbfec3a0f477dcddc60ca9f62abc23f89d4822e` |

The initial all-consumer build passes; a final focused rebuild follows review that preserves
historical JSON diagnostic identities/categories. No runtime source edits follow the freeze.
`/tmp/blcad146a-runtime-freeze.json` contains **964** source/test/asset/build-script files,
including hidden placeholders and the four JSON fixtures. It is compact UTF-8 JSON sorted by path;
SHA-256 is `2d02fbb038318524ddb51a47fe1bdcd0ffab1cfc95de466a44f5991ae83576f9`.
Every recorded fingerprint remains unchanged through the final checks.

| Check | Result | Log under `/tmp/` |
| --- | --- | --- |
| Final all-consumer build | Pass | `blcad146a-final-build.log` |
| Focused Core inventory/order/update | 12 cases / 627 assertions | `blcad146a-core-focused.log` |
| Full Core | 601 / 22,700 | `blcad146a-core-full.log` |
| Full Geometry | 564 / 49,787 | `blcad146a-geometry-full.log` |
| Full offscreen GUI with required filesystem access | 682 cases: 648 passed, 34 native-only skips / 87,895 passed assertions | `blcad146a-gui-full-filesystem.log` |
| Native 100% persistence/history/Extrude combinations | 36 / 2,708, no skips | `blcad146a-native-100.log` |
| Separate native startup | Expected timeout 124 after 6 seconds | `blcad146a-startup.log` |

The first full GUI process was mistakenly started inside the restricted filesystem sandbox and
is deliberately interrupted with exit **130**, after the known structured STEP atomic-publication
failures (`blcad146a-gui-full.log`). The unchanged full run with required filesystem access passes;
no code/test expectation is weakened. The final build retains existing warnings in unchanged
Geometry/GUI code; the new Core/history sources add no warnings.

Native commands, after the full GUI process exits:

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[document-session],[part-workflow],[source-sketch-workflow],[create-another],[create-another-workflow],[imported-body-preview],[feature-order]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

The native union executes 36 real existing scenarios; the GUI binary has no `[feature-order]` tag,
which is covered by the Core/Geometry suites above. Native logs retain the known Medium
incomplete depth/stencil Visual warning, without High-severity/BadDrawable/BadWindow failures.
This slice changes no rendering or interaction; fresh 150% layout captures are not claimed. Prior
Block-145 UI acceptance is retained, and future history-browser acceptance stays assigned to 148.
Scoped tracked-C++ formatting, complete new-file formatting and diff checks pass. Delivery uses a
normal protected PR, followed by merged-state verification, refreshed main and tree comparison.


Delivery: [PR #144](https://github.com/GideonBa/Better-Linux-CAD/pull/144), implementation `af40935`.
The delivery-record commit changes documentation only; all 964 runtime fingerprints remain intact.


## Block 146B explicit history intent and execution boundary — 2026-11-07

Starts from verified main `7367faa` after normal merge of PR #144 and exact tree comparison.
`feature-system.md` owns candidate/mutation behavior; `file-format.md` owns version-1/2 migration.
New history UI stays assigned to 148. This delivery stores complete intent, never a second tree.
Effective bypass, unavailable creator outputs, prefix recompute and fresh/incremental execution
remain 147; current model consumers reject explicit intent until that evaluator exists.

Focused proof covers all fifteen families, active/suppressed queries and preserved definitions,
independent cross-family positions, rebuilt same-Body predecessors, hard creator dependencies,
resource-mediated order, complete failure rollback, appending while explicit history exists,
removal/ID reuse including specialized Surface/import paths, strict malformed versioned metadata,
immutable imports and result-face prelude/Project embedding. Distinct output Body identities from
Join/Cut/Intersect are creators as well. Four accepted historical byte fixtures remain unchanged.

The temporary load graph initially caused a focused regression: creator prevalidation interpreted
historical family-loading predecessors as authored dependencies. Versioned loading now establishes
`feature_order` through the existing typed replay before validating persistent intent. The three
failed cases/seven assertions are retained in `blcad146b-core-review-focused.log`; the corrected
full focus passes without relaxing assertions (`blcad146b-core-order-corrected.log`). The first
expanded build also catches incorrect accessors in the new GUI test; tests now use the actual
`part_document()`/`part_shape_cache()` APIs and the established absent-cache repair contract.
The corrected all-consumer build passes; these are development attempts, not final acceptance.

Final candidate freeze: `/tmp/blcad146b-runtime-freeze.json`, **968** source/test/asset/build-script
files, compact UTF-8 JSON sorted by path. SHA-256:
`dad6e820e6de3e811894f68a27e09b847cf9620367d3eadc3cdd31cbe14c7eba`.
No runtime edits follow that freeze. Final candidate focused checks pass:

- Core inventory/order/update/intent: **27 cases / 1,116 assertions** (`blcad146b-core-focused.log`).
- Geometry boundary: **2 / 286** (`blcad146b-geometry-focused.log`). Both all-active explicit order
  and suppressed intent reject full/empty-incremental/direct execution, analytical/cache shortcuts,
  scene selection, inspection and STEP export. Existing 1600 mm³ cache and export bytes are intact.
- GUI boundary: **2 / 40** (`blcad146b-gui-focused.log`): rejected transaction/normal open preserve
  old intent/history/cache; repair open retains exact new intent with Failed status and no cache.

```bash
cmake --build build/dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app -j8
./build/dev-gui/blcad_core_tests "[history-intent],[feature-history-inventory],[feature-order],[feature-update-command]"
./build/dev-gui/blcad_geometry_tests "[history-evaluation-boundary]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[history-evaluation-boundary]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
```

The full GUI command requires regular filesystem access for atomic STEP/project publication as
documented above. Build/test/native processes run serially. Final broad/native results follow below.


Final broad/native checks on the unchanged freeze:

| Check | Result | Log under `/tmp/` |
| --- | --- | --- |
| Final all-consumer build | Pass | `blcad146b-build-final-candidate.log` |
| Full Core | 616 cases / 23,189 assertions | `blcad146b-core-full.log` |
| Full Geometry | 566 / 50,073 | `blcad146b-geometry-full.log` |
| Full offscreen GUI | 684 cases: 650 passed, 34 native-only skips / 87,935 assertions | `blcad146b-gui-full.log` |
| Native 100% combinations | 38 / 2,748; no skips | `blcad146b-native-100.log` |
| Separate native startup | Expected timeout 124 after 6 seconds | `blcad146b-startup.log` |

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[document-session],[part-workflow],[source-sketch-workflow],[create-another],[create-another-workflow],[imported-body-preview],[history-evaluation-boundary]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

No failed full/native cases or native skips. The known Medium incomplete depth/stencil Visual
warning remains, without High-severity/BadDrawable/BadWindow failures. This change adds no layout
or rendering feature; new 150% captures are not claimed. Formatting/diff checks pass; all 968
runtime fingerprints remain intact through tests and documentation-only status promotion.
Block 146 intent/Core/JSON acceptance is complete; 147 remains planned and owns actual effective
suppression/prefix/freshness behavior, replacing the tested unsupported-evaluator boundary. Native
browser/End of Part workflows remain 148. Normal protected PR delivery and refreshed-main/tree
comparison are required before that next implementation branch.


After documentation-only status promotion, the final capability/status inventory passes **5 cases /
495 assertions** (`blcad146b-final-inventory.log`). The full GUI count precedes that promotion;
this separate focused check validates implemented-through 146/current block 147 without advancing
planned history UI capabilities. All runtime fingerprints remain unchanged.


Delivery: [PR #145](https://github.com/GideonBa/Better-Linux-CAD/pull/145), implementation `9329de2`.
The delivery-record commit is documentation only and preserves all 968 runtime fingerprints.


## Block 147A read-only effective availability plan — 2026-11-07

Starts from verified main `a659016` after normal merge of PR #145 and exact reviewed-tree comparison.
The query derives execution states and Body availability from existing typed records, never source
mutation or a second persistent model. All Geometry guards and execution paths from 146B remain
unchanged. Effective Geometry/prefix/cache/native acceptance is assigned to 147B, not this slice.

```bash
cmake --build build/dev-gui --target blcad_core_tests -j8
./build/dev-gui/blcad_core_tests "[history-evaluation-plan],[history-intent],[feature-history-inventory],[feature-order],[feature-update-command]"
./build/dev-gui/blcad_core_tests
```

Initial focused projection passes **9 cases / 507 assertions**. Review then identifies that resource
IDs can legally resemble Body aliases; ownership now uses actual Body records, with a regression
for Sketch/parameter IDs using `body:`. A separate negative case covers declared Bodies without
creators. No existing assertion is relaxed. Final Core build passes without new warnings
(`/tmp/blcad147a-core-build-review.log`). The new projection cases pass **11 / 538**, combined history
focus **38 / 1,654** (`/tmp/blcad147a-core-focused.log`), full Core **627 / 23,727**
(`/tmp/blcad147a-core-full.log`).

Proof includes all fifteen families and every prefix boundary, modifier predecessor snapshots,
semantic/resource output loss versus Body bypass, blocked active chains without silent cascading,
explicit dependent closure, independent and imported Bodies, Boolean tool consumption, immutable
reference ownership, missing creators and source JSON/graph/invalidation immutability. A complete
Core scope is not a fresh Geometry result. No JSON field/version/default changes. Existing four
historical byte oracles remain exact through the full Core suite.

Runtime freeze `/tmp/blcad147a-runtime-freeze.json`: **969** source/test/asset/build-script files,
compact UTF-8 JSON sorted by path; SHA-256
`0a5751712bd050d6807cfcfba81b1e6dd59dde72ef5e32071112e1ad068c458e`.
All fingerprints remain unchanged through final tests and documentation updates. Scoped/new-file
formatting and diff checks pass. Geometry, GUI, native and startup checks are not rerun for this
new unused read-only Core method; their previous execution paths and conservative boundary are
unchanged. B must build/link all consumers and run full physical/freshness/native acceptance when
it replaces that boundary. Whole Block 147 remains open. Normal protected PR delivery and verified
merged/refreshed main are required before the next branch.


Delivery: [PR #146](https://github.com/GideonBa/Better-Linux-CAD/pull/146), implementation `90a836d`.
The delivery-record commit changes documentation only and preserves all 969 runtime fingerprints.


## Block 147B scoped Geometry and cache ownership — 2026-11-07

Starts from verified main `f1b8c61` after normal merge of PR #146 and reviewed-tree comparison.
The Core availability plan drives all fifteen existing families on fresh disposable caches.
Per-row Body predecessors supply modifier bypass; only successful full/prefix evaluation publishes.
Private source/scope/lifetime stamps distinguish complete Geometry from logical Core completeness.
Explicit-history incremental entry points deliberately use a fresh full fallback; unscoped legacy
incremental behavior is retained. Browser/marker UI remains 148.

Initial box proof found that fixed datum planes are intentionally absent from the dependency graph;
owned datum geometry now follows that contract. Initial physical matrix covered twelve native
families; two fixture ID collisions (Sketch/feature and Sketch/PathCurve) were corrected without
relaxing assertions. All fourteen native families then passed independent legacy volume/bounds,
suppression/restore and every prefix boundary. Review added retained partial scope after public
cache clear, finishing selection validation, transient executable-lifetime invalidation including
identical ID/JSON reuse, and ownership validation before Undo/Redo cache reuse. The first GUI pass
found successful STEP preflight with zero output Bodies; preflight now validates complete available
visible results and rejects zero. Invalid suffix transactions retain the previous complete cache;
repair opening/explicit suppression/Undo/Redo are covered separately.

```bash
cmake --build build/dev-gui --target blcad_geometry_tests blcad_gui_tests blcad_core_tests blcad -j8
./build/dev-gui/blcad_geometry_tests "[history-evaluation-boundary],[history-physical]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[history-evaluation-boundary]"
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
```

Build/test/native/startup processes run serially. GUI commands use regular filesystem access for
atomic project/STEP publication, including offscreen runs. Final linked candidate builds pass
(`/tmp/blcad147b-final-build.log`, `blcad147b-final-include-build.log`); earlier broad integration build
`blcad147b-integration-build.log` also passes. Existing unrelated compiler warnings are unchanged.
Final focused Geometry passes **9 cases / 1,923 assertions** (`blcad147b-final-focused-geometry.log`),
GUI **3 / 142** (`blcad147b-final-focused-gui.log`), full Core **627 / 23,727**
(`blcad147b-core-full.log`) and full Geometry **573 / 51,710** (`blcad147b-geometry-full.log`).

Proof covers independent and consumed Bodies, legal independent reorder/rejected dependent order,
explicit dependent closure, suppressed creator output absence versus modifier bypass, every native
family, imported base/boss/cut/Hole/Fillet/transform and STEP readback, immutable Reference prefix
basis, assembly overlap/posed export, changed supplier bytes, failed suffixes, no direct-operation
resurrection, public cache mutation/clear and stale/partial/full consumers. GUI combines native-capable
scene ownership, Measure, STEP preflight, Save/reopen, one-entry Undo/Redo and repair. Source canonical
JSON and authored intent remain intact; the four historical JSON oracles pass with full Core.

Final runtime freeze `/tmp/blcad147b-runtime-freeze.json`: **969** source/test/asset/build-script files,
compact sorted-key UTF-8 JSON; SHA-256
`1d10348199ccfb406fa86cb96f28a44ad74b7b7f19f3dbc5556d4c8aba632a0c`.
Full GUI/native/startup results and final unchanged-fingerprint confirmation follow below.


Final checks on the same freeze:

| Check | Result | Log under `/tmp/` |
| --- | --- | --- |
| Full offscreen GUI | 685 cases: 651 passed, 34 expected native-only skips / 88,037 assertions | `blcad147b-gui-full.log` |
| Native 100% combinations | 39 / 2,851; no skips | `blcad147b-native-100.log` |
| Separate native startup | Expected timeout 124 after 6 seconds | `blcad147b-startup.log` |

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[document-session],[part-workflow],[source-sketch-workflow],[create-another],[create-another-workflow],[imported-body-preview],[history-evaluation-boundary]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

No failed full/native cases or native skips. The known Medium incomplete depth/stencil Visual
warning remains, without High-severity/BadDrawable/BadWindow failures. No layout feature changes
or new 150% captures are claimed; native marker/browser layout acceptance remains 148. All 969
runtime fingerprints remain unchanged after broad/native tests and documentation updates. Whole
147 is accepted; a final documentation inventory, protected PR delivery and refreshed-main/tree
comparison precede the next implementation branch. GitHub's effective main rules require PRs,
prohibit deletion/non-fast-forward updates and require no approving reviews/status checks; normal
merge methods remain allowed. The classic branch-protection endpoint reports 404 because these
requirements are delivered through repository rulesets, verified via the effective branch rules API.


After documentation-only status promotion, the final capability/status inventory passes **5 cases /
494 assertions** (`blcad147b-final-inventory.log`). The full GUI result precedes that promotion;
this separate check validates implemented-through 147/current block 148 while history browser
capabilities remain planned. Runtime fingerprints remain unchanged.


Delivery: [PR #147](https://github.com/GideonBa/Better-Linux-CAD/pull/147), implementation `9de9dd9`.
The delivery-record commit changes documentation only and preserves all 969 runtime fingerprints.

## Block 148A ordered browser and staged history commands — 2026-11-07

Starts from verified merged main `6d8179d` (PR #147) on `feat/history-browser-commands`.
This slice uses the existing browser, feature editor, task controls and complete-history evaluator.
Suppression/restore and dependency-checked reorder stage disposable complete candidates; Apply
publishes one existing session transaction. Explicit dependent inclusion is reversible before Apply.
Context commands and Ctrl+K share registered actions. Browser folding and semantic selection survive
refresh; clicking an already-current Sketch explicitly reselects the next command's input.
Unavailable supporting resources are omitted from the evaluated scene while their intent remains.
End of Part insertion/editing and whole-block capability promotion remain 148B/C.

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad -j8
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[shell-combined-revolve-controls],[history-controller],[history-shell],[model-browser],[shell-coverage],[block136-feature-edit]"
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
```

Run builds, CAD tests, native processes and startup serially. Full GUI commands need regular
filesystem access for the existing atomic project/STEP publication tests even with offscreen Qt.
Final incremental build passes (`/tmp/blcad148a-reclick-build.log`). Focused GUI regression passes
**31 cases: 30 passed, one expected native-only skip / 2,767 assertions**
(`blcad148a-reclick-focused.log`). Full Geometry passes **574 / 51,745**
(`blcad148a-geometry-full.log`), including the new **1 / 35** supporting-resource suppression/restore
case. The later browser click correction changes GUI files only; Geometry sources stay identical.
Core implementation and tests are unchanged from accepted 147B, whose **627 / 23,727** result is
retained; a new full Core run is not claimed for this GUI/scene slice.

Controller/browser proof covers explicit dependent closure and unchecking, independent legal order,
dependent/malformed rejection, source/lifetime changes, all fifteen authored families, immutable
Reference/imported bases, actual context menus and browser drag/drop, foreign/stale payloads,
keyboard correction, no-op Apply, invalid restore followed by existing-editor repair, committed Save,
one-entry Undo/Redo, source/fold/camera/selection restoration and command discovery. The all-family
catalog is an intent fixture; physical execution evidence remains the Geometry tests from 147B.

The initial full GUI run found one existing combined Revolve workflow failure after retaining the
browser's current Sketch. A repeated click now reactivates its consumed profile input without
duplicating selection handling when the row changes. The existing combined test passes unchanged
in the focused result above. Final broad/native checks use the corrected build.

Runtime freeze `/tmp/blcad148a-runtime-freeze.json`: **975** source/test/asset/build-script files,
compact sorted-key UTF-8 JSON; SHA-256
`4634e7df4c8cc5fc70b854fe7e57fe441305bb8cb9a710cd64de19cb3fd03ba5`.
Final full GUI/native/startup results and unchanged-fingerprint confirmation follow below.

Final full offscreen GUI passes **697 cases: 663 passed, 34 expected native-only skips /
88,526 assertions**, with no failed cases (`/tmp/blcad148a-gui-full-final.log`).

| Final native check | Result | Log under `/tmp/` |
| --- | --- | --- |
| Native 100% history/edit/source Sketch/imported Body combinations | 52 / 4,401; no skips or failures | `blcad148a-native-100-final.log` |
| Native compact 150% history commands | 7 / 299; no skips or failures | `blcad148a-native-150-final.log` |
| Separate native startup | Expected timeout 124 after six seconds | `blcad148a-startup.log` |

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad148a-accepted-100 ./build/dev-gui/blcad_gui_tests "[history-controller],[history-shell],[model-browser],[history-evaluation-boundary],[block136-feature-edit],[source-sketch-workflow],[create-another],[create-another-workflow],[part-workflow],[imported-body-preview],[shell-combined-revolve-controls]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_COMPACT=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad148a-accepted-150 ./build/dev-gui/blcad_gui_tests "[history-shell]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

Representative raw captures are inspected and retained in `docs/images/`, linked from the existing
UI contract. Compact sizing respects the actual available desktop height instead of triggering
window-manager maximization. No High-severity/BadDrawable/BadWindow failures appear; the known
Medium incomplete depth/stencil Visual warning remains. All 975 runtime fingerprints stay unchanged
after broad/native/startup checks. This accepts slice A only; full 148 remains open. Final
documentation inventory and protected PR delivery follow before starting the marker lifecycle.

After documentation-only slice status updates, `[shell-coverage]` passes **5 / 494**
(`/tmp/blcad148a-final-inventory.log`). Diff checks, new-file formatting and modified-line formatting
pass. The final fingerprint audit confirms all 975 runtime files unchanged; no whole-block history
capability is promoted by this bounded delivery.

Delivery: [PR #148](https://github.com/GideonBa/Better-Linux-CAD/pull/148), implementation `8d802d3`.
This delivery-record update changes documentation only and preserves all 975 runtime fingerprints.
Normal merge and refreshed-main/reviewed-tree comparison precede slice B's implementation branch.

## Block 148B session foundation — 2026-11-07

Starts from verified merged main `1179001` (PR #148), with an exact reviewed-tree comparison before
`feat/end-of-part-context`. This first serial part of B extends the existing Part workflow; it does
not expose a native End of Part marker or complete B. Complete intent remains in the session while
the evaluated cache is explicitly partial. New rows are inserted at the transient marker through
Core replay; complete suffix publication is one existing workflow transaction. Prefix readiness
and full-result freshness remain distinct, including at the final authored row.

```bash
cmake --build build/dev-gui --target blcad_gui_tests blcad -j8
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[history-prefix-session],[part-workflow],[document-session],[history-controller],[history-shell],[history-evaluation-boundary],[shell-combined-revolve-controls],[shell-coverage]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --durations yes
```

Build/test/native/startup processes run serially; runtime edits stop during checks. GUI commands
use regular filesystem access for atomic project/STEP publication, including offscreen runs.
The initial 134-object consumer build passes (`/tmp/blcad148b1-initial-build.log`). Review expands
coverage with actual imported suppliers, sidecars and suppressed rows; an optional/pointer mistake
in the new test is corrected before final compilation. The transient history entry initializer is
explicit, introducing no compiler warnings. Existing unrelated Sketch annotation warnings remain.
Final builds pass (`blcad148b1-final-build.log`, `blcad148b1-final-entry-build.log`).

Final focused regression passes **55 cases / 3,694 assertions**, no skips or failures
(`/tmp/blcad148b1-final-focused-regression.log`). The new ten session scenarios cover every marker
boundary; retained suffix definitions; rejected complete scene/inspection/STEP access; failed suffix
and failed marker movement; correction and independent full volume/bounds; exact Body predecessor
replay; future-creator rejection; insertion among suppressed rows; local Undo/Redo and one complete
publication; committed Save/Save As and byte-exact Sketch sidecars; original redo restoration;
immutable Reference bases; supplier reobservation even for marker-only visits; copied-session
independence; executable ID/lifetime reuse; and partial cache loss/recompute. Existing document,
Part workflow and browser/combined-Revolve regressions run in the same focused process.

Core and Geometry implementation/tests are unchanged from accepted main. Their prior full results
(Core **627 / 23,727** from 147B; Geometry **574 / 51,745** from 148A) are retained, not rerun or
claimed as new checks of this GUI-session change. New session tests execute the actual Geometry
prefix/full evaluator and real STEP reader/writer, with independent fresh full comparisons.
Native marker placement, child-command UI integration and new layout captures remain the next
serial part of 148B; no new marker/UI capability is promoted by this foundation.

Runtime freeze `/tmp/blcad148b1-runtime-freeze.json`: **976** source/test/asset/build-script files,
compact sorted-key UTF-8 JSON; SHA-256
`8b6107e0e50596bbf2a3e1db15c5896083e8657c8d59a7253727cad8950bbf1a`.
Full GUI/native/startup checks and final unchanged-fingerprint confirmation follow below.

Final full offscreen GUI passes **707 cases: 673 passed, 34 expected native-only skips /
89,135 assertions**, no failures (`/tmp/blcad148b1-gui-full.log`). Native 100% session/browser/source
Sketch combinations pass **36 / 2,241**, no skips or failures (`blcad148b1-native-100.log`):

```bash
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests "[history-prefix-session],[part-workflow],[source-sketch-workflow],[history-shell],[history-evaluation-boundary]"
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

These native runs combine the new headless session API with existing actual browser/source-Sketch
interaction; they do not claim interaction with an unimplemented native marker. No new layout or
150% capture is claimed by the foundation. The accepted 148A layout evidence is retained.

Separate native startup reaches expected timeout **124** after six seconds
(`/tmp/blcad148b1-startup.log`). The known Medium incomplete depth/stencil Visual warning remains;
no High-severity/BadDrawable/BadWindow failure appears in native/startup logs. Source/format/diff
review passes and all 976 runtime fingerprints remain unchanged after final checks. The session
foundation is verified; B and whole 148 remain open for native integration and combined acceptance.

After documentation-only status updates, `[shell-coverage]` passes **5 / 494**
(`/tmp/blcad148b1-final-inventory.log`), with no failures or skips. All 976 runtime fingerprints
remain unchanged; no whole-block capability is promoted. Protected PR delivery follows.
