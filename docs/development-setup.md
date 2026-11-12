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

## Build and test presets

Run commands from the repository root. Choose the smallest relevant configuration; `dev-gui`
includes Core and Geometry and registers all three test executables.

| Scope | Complete configure/build/test workflow | Build directory |
| --- | --- | --- |
| Core | `cmake --workflow --preset dev-build-test` | `build/dev` |
| Core + OCCT Geometry | `cmake --workflow --preset dev-geometry-build-test` | `build/dev-geometry` |
| Core + Geometry + Qt GUI | `cmake --workflow --preset dev-gui-build-test` | `build/dev-gui` |

The workflows in `CMakePresets.json` are equivalent to these individual steps (substitute `dev`
or `dev-geometry` when appropriate):

```bash
cmake --preset dev-gui
cmake --build --preset dev-gui
ctest --preset dev-gui --output-on-failure
```

For a focused desktop change, build the affected targets before running their binaries. Include
`blcad_export_step` when changing exchange workflows. The app target is `blcad_app`; its executable
is named `blcad`.

```bash
cmake --build --preset dev-gui --target blcad_core_tests blcad_geometry_tests blcad_gui_tests blcad_app blcad_export_step -j6
./build/dev-gui/blcad_core_tests
./build/dev-gui/blcad_geometry_tests
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests
./build/dev-gui/blcad
```

`CMakeLists.txt` sets `QT_QPA_PLATFORM=offscreen` on discovered GUI cases. CTest therefore needs
no platform prefix; direct GUI-binary invocation does. To run only GUI cases through CTest:

```bash
ctest --preset dev-gui -R '^gui\.' --output-on-failure
```

## Verification discipline

- Finish source/test/asset/build-script edits before starting builds or acceptance runs. Keep those
  runtime inputs frozen while builds, CAD tests or application startup are active; documentation
  records may be updated independently. Rebuild affected targets after any runtime change.
- Use one GUI process at a time: offscreen suites, native suites and separate app startup run
  serially. Core/Geometry correctness binaries may run alongside one GUI suite on the same frozen
  sources. Timing measurements run alone, without concurrent builds or tests.
- Start with focused regression tests, then run the broader suites and combinations required by the
  existing feature plan. A test-only correction invalidates the affected binary's earlier evidence;
  unchanged binaries can retain their results only when their inputs are verified unchanged.
- For a numbered acceptance gate, record a runtime-input manifest/digest covering `src/`, `include/`,
  `tests/`, assets and build scripts; verify it again before delivery. Keep old and corrected evidence
  distinguishable. A partial/interrupted/failed run is not a passing full suite.
- Record actual cases, assertions, skips and unavailable environments. Native-only/optional skips in
  offscreen runs remain unproved unless the relevant cases are executed natively. A focused native
  filter does not establish full native-suite coverage.
- Inspect working and staged diffs, preserve unrelated changes and follow `AGENTS.md` for protected
  PR delivery. `mvp-plan.md` alone owns numbered status; individual slices do not close a whole block.

If STEP atomic-publication tests fail under a restricted filesystem, check the actual file-access
failure and rerun with the required access; offscreen Qt does not remove filesystem requirements.
Do not weaken the export assertions. If an incremental build crashes after a public type-layout
change, inspect Ninja's header-dependency records: stale objects with zero recorded dependencies
have caused stack corruption here. Rebuild affected artifacts and rerun the relevant suites.

## Find and select focused tests

`tests/{core,geometry,gui}/` owns assertions/tags; `CMakeLists.txt` owns source registration and
CTest discovery. Discover current files/tags instead of copying historical acceptance commands:

```bash
rg --files tests | rg '(hole|history|resource|step)'
rg -n 'TEST_CASE|\[parity-b154\]' tests/core tests/geometry tests/gui
./build/dev-gui/blcad_core_tests --list-tags
./build/dev-gui/blcad_geometry_tests --list-tags
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests --list-tags
```

Catch2 adjacent tags mean AND; commas mean OR. Check that every selected filter matches tests.
Examples for the implemented Hole boundary:

```bash
./build/dev-gui/blcad_core_tests '[parity-b152],[parity-b154]'
./build/dev-gui/blcad_geometry_tests '[parity-b153],[parity-b154]'
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests '[hole-history-shell],[hole-result-reference-shell],[hole-manufacturing-shell]'
QT_QPA_PLATFORM=offscreen BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 ./build/dev-gui/blcad_gui_tests '[shell-coverage]'
```

Representative entry points (select the layer where the tag exists):

| Area | Tags / source or contract owner |
| --- | --- |
| Sketch topology, solver, constraints, dimensions | `[sketch-topology]`, `[sketch-solver]`, `[sketch-constraints]`, `[sketch-dimensions]`; `interactive-sketcher-sequence-mvp8.md` |
| Sketch creation/modification, region and Finish Sketch | `[sketch-create]`, `[sketch-modify]`, `[sketch-regions]`, `[sketch-finish]`; matching `tests/` files |
| Shell, browser, ribbon, viewport | `[shell]`, `[model-browser]`, `[ribbon]`, `[viewport]`, `[native-viewer-lifecycle]`; `gui-shell-acceptance-mvp9r.md` |
| Extrude/Revolve and finishing | `[extrude-revolve]`, `[interactive-finishing]`, `[interactive-shell-draft]`; `gui-interactive-extrude-revolve-mvp9.md`, `gui-interactive-finishing-mvp9.md` |
| Body operations, Pattern/Mirror, paths/Loft, surfaces | `[interactive-body-operation]`, `[interactive-pattern-mirror]`, `[interactive-path-sweep]`, `[interactive-loft]`, `[interactive-surface]` |
| Feature editing and combined workflows | `[feature-update-command]`, `[feature-edit]`, `[shell-combined-workflow]`; `gui-feature-update-inventory-mvp9.json`, `gui-feature-coverage-manifest-mvp9.json` |
| Assembly | `[interactive-assembly-placement]`, `[interactive-relationships]`, `[interactive-joint-motion]`, `[block136-assembly-analysis]`; `gui-interactive-assembly-mvp9.md` |
| STEP | `[step-import-source]`, `[step-import-reader]`, `[native-step-editable-parts]`; `step-import-sequence-mvp10.md` |
| Shared workflow controls | `[parity-b145]`, `[property-controls]`, `[source-sketch-workflow]`, `[create-another-workflow]`; `user-interface.md` |
| History intent, scoped evaluation and browser | `[parity-b146]`, `[parity-b147]`, `[parity-b148]`, `[history-prefix-session]`, `[history-combined-acceptance]` |
| Engineering resources and physical properties | `[parity-b149]`, `[parity-b150]`, `[parity-b151]`, `[material-physical-combined]`; `engineering-modules.md` |
| Semantic and manufacturing Holes | `[parity-b152]`, `[parity-b153]`, `[parity-b154]`; `hole-wizard.md`, `tests/support/hole_*`, `tests/{core,geometry}/*hole*`, `tests/gui/gui_shell_history_tests.cpp` |
| Shared Thread definitions, persistence and execution | `[parity-b155]`; `hole-wizard.md`, `tests/core/{thread_feature,part_thread_feature}_tests.cpp`, `tests/geometry/thread_feature_*tests.cpp`; `[thread-history-shell]` / `[thread-geometry-shell]` in `tests/gui/gui_shell_history_tests.cpp` |
| Existing Hole/Thread property editing | `[parity-b157]`, `[hole-thread-edit-shell]`, `[native-hole-thread-edit]`; `hole-wizard.md`, `gui-feature-edit-mvp9.md`; combine with `[block136-feature-edit]` and `[history-prefix-session]` for the shared editor regression. Full Wizard authoring remains 157B/C. |
| Semantic Plain Hole creation | `[hole-create-shell]`, `[native-hole-create]`; the same current shell test file covers pending inputs, target/support changes, physical cuts, exact history/reopen and scoped native model picks. Combine with the existing edit/Pattern/prefix/inventory regression; full Wizard remains 157B/C. |
| Hole recipes and dimensional sources | `[hole-recipe-shell]`, `[native-hole-recipe]`; independent physical volume oracles cover nine recipes and six extents, pinned/custom and Angle sources, unused inputs and exact history/reopen. Combine with existing creation/edit regressions. Full Thread/support selectors remain 157B2b. |
| Imported/result Hole creation supports | `[hole-support-shell]`, `[native-hole-support]`; placed imported plane/axis, supplier freshness, pending/unused workplanes, real on-screen model picks and exact history/reopen. Complete support editing/limit/Thread workflows remain within 157B2b. |
| External Thread support inspection | Geometry `[thread-support-query],[thread-import]` plus `[parity-b155]`; independent entry frame/radius/span/taper oracles, both directions, rigid placement, current cache and supplier-byte rejection. The shared resolver also requires the existing native Thread edit regression. |
| New internal/external Thread authoring | Qt `[thread-create-shell]` and native `[native-thread-create]`; pending/unused inputs, physical ideal-V cuts, imported placement/source guards, exact history/reopen and STEP. Combine with existing Hole creation/recipe/support/edit, shared feature editor and icon/inventory regressions. Complete selectors remain 157B2b. |
| Thread profile and interval controls | `[thread-source-shell]` / `[native-thread-source]`; exact revision/row/typed columns, incomplete/invalid correction, custom ownership, source provenance, optional start/taper removal and native mapped modeled geometry. Combine with creation/edit regressions and preserve full 157B2b/C scope. |
| Explicit Pattern placements and Hole consumers | `[parity-b156]`, `[pattern-placement]`, `[hole-pattern-combined]`; `part-pattern-core-mvp6.md`, `hole-wizard.md`, `file-format.md`; `[hole-pattern-consumer-shell]` exercises the existing Assembly task |

The shared GUI executable owns one Qt application instance. `ShellWindow` consumes the existing
headless controllers under `include/blcad/gui/gui_interactive_*`; new tests must exercise current
shell owners rather than the removed MainWindow/binders. Part/Project Apply uses the existing
transaction authorities; consult the feature contracts before introducing persistent state.

## Native Qt/OCCT and layout checks

Offscreen GUI tests use a logical viewport without X11/GLX. Rendering and interaction changes also
need the directly invoked test binary under `QT_QPA_PLATFORM=xcb`, with a working `DISPLAY`, and a
separate app-startup smoke run. CTest's offscreen environment cannot prove native rendering.

Native fixtures must fit the available screen in device-independent coordinates. At 150%, an
unbounded 1280×800-DIP request exceeds a 1920×1080 display. The STEP Source fixture honors compact
mode/screen bounds, synchronizes native layout before projection, and asserts that imported topology
pick targets are actually on screen; keep the physical selection assertions intact.
Native keyboard fixtures activate the main window and await activation with a bounded event loop.
Assert actual focus before input and on the rebuilt selector afterward; reacquire widgets that a
form rebuild replaces. Do not weaken focus assertions to accommodate an inactive test window.

Use the relevant focused filter at ordinary 100% and compact 150% scale. This runnable example
covers the existing Hole/shared-History UI; select additional affected families from their tests:

```bash
mkdir -p /tmp/blcad-native-100 /tmp/blcad-native-150
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-native-100 ./build/dev-gui/blcad_gui_tests '[hole-history-shell],[hole-result-reference-shell],[hole-manufacturing-shell]'
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1.5 BLCAD_UI_COMPACT=1 BLCAD_REQUIRE_SHELL_ACCEPTANCE=1 BLCAD_UI_CAPTURE_DIR=/tmp/blcad-native-150 ./build/dev-gui/blcad_gui_tests '[hole-history-shell],[hole-result-reference-shell],[hole-manufacturing-shell]'
QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=1 timeout 6s ./build/dev-gui/blcad
```

The final command intentionally returns 124 after six seconds; confirm the real window appeared
and inspect its output. A timeout alone is not startup proof. When native execution is unavailable,
record that explicitly rather than treating the offscreen result as its replacement.

`BLCAD_UI_COMPACT` is honored by supporting fixtures (commonly 1024×640 DIP); ordinary captures
commonly use 1280×800. At 150%, 1024×640 DIP yields 1536×960 physical pixels. Inspect original,
unmodified captures for visible labels/selectors, scroll reachability, pinned Apply/Cancel, preview,
selection and overlays. Model Fit All currently includes datum planes; small geometry in a full
window capture must not be hidden by editing the image. Fonts/icons are bundled Qt resources.

Keep native dialog/popup watchdogs scoped to the awaited interaction: popup dismissal or the intended
action ends its watchdog; `QDialog::finished` ends the dialog watchdog. Existing STEP helpers use
five-second limits. Retain action/dialog/document assertions and failure diagnostics when fixing
harness timing. Known OCCT Medium visual/depth/stencil notices can be environment output; inspect
High-severity, BadDrawable/BadWindow and actual assertion failures separately.

## Performance measurements

Correctness tests must not assert portable wall-clock budgets. Run opt-in timing profiles in
isolation, retaining environment, source/build identity, raw samples and measurement boundaries.
`[complex-part-profile]` is excluded from default GUI/CTest selection and requires native rendering
for publication/redraw measurements:

```bash
BLCAD_PROFILE_SAMPLES=11 BLCAD_PROFILE_OUTPUT=/tmp/blcad-rotor-profile.json QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests '[complex-part-profile]'
BLCAD_PROFILE_SEQUENCE=1 BLCAD_PROFILE_SAMPLES=5 BLCAD_PROFILE_OUTPUT=/tmp/blcad-rotor-sequence.json QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests '[complex-part-profile]'
```

The rotor sizes are 5/20/40 blades; `BLCAD_PROFILE_BLADES=20` isolates one. Sequence mode adds
4/12/24 Body translations and measures actual Apply/Undo/Redo. Keep first-call and repeated samples
separate; five samples do not establish a reliable tail budget. JSON serialization/parsing is not
complete history latency; a queued zero-timer is not physical pointer latency. Fixture setup/export
is excluded. `[complex-part-sequence]` remains an ordinary correctness gate, with empty-cache oracles.
Geometry test support owns direct OCCT headers; do not add them to the GUI test target.

For the existing Extrude UI baseline, execute `[workflow-interaction-baseline]` alone in three
separate native processes with distinct `BLCAD_WORKFLOW_INTERACTION_BASELINE` JSON output paths.
Each measures its first workflow and ten reused-window trials; synchronous Qt input, kernel work
and event publication are included, startup/setup/oracle checks are excluded. Shape-only evidence
uses `[workflow-baseline]` and `BLCAD_WORKFLOW_BASELINE`. Compare inventories using their own
enumeration (Qt may omit hidden `.gitkeep` files).

Raw historical baselines remain in `docs/benchmarks/`; cache/measurement contracts live in
`gui-modeling-workspace-mvp9.md`. These results establish a reference for the current bounded workflow,
not budgets for the future P01 workloads; those producers and the final budget gate remain assigned
to their numbered blocks.

## Headless tools

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_project examples/component_instances.blcad.project.json assembly.parameter 1 build/export
./build/dev-geometry/blcad_export_posed_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_export_structured_assembly input.blcad.project.json output.step
./build/dev-geometry/blcad_move_joint input.blcad.project.json joint.id 30 output.blcad.project.json
./build/dev-geometry/blcad_analyze_assembly input.blcad.project.json
```

## Formatting and generated files

`.editorconfig` and `.clang-format` own formatting. Format new files fully and keep legacy edits
focused; avoid repository-wide formatting for an unrelated change. For a focused C++ change set:

```bash
git diff --name-only -z --diff-filter=ACM -- '*.hpp' '*.cpp' | xargs -0 -r clang-format -i
git diff --check
git diff --cached --check
```

Generated outputs live under `build/`; remove that directory only when a clean rebuild is needed.
Documentation-only compaction requires link/anchor/fence and preservation checks, not fresh CAD
builds or native runs. Runtime/schema/interaction changes still require their relevant gates.

## Documentation and status ownership

- `README.md`: repository entry point; `architecture-summary.md`: authority and module map.
- `mvp-plan.md`: current block, numbered acceptance and delivery records. Check it before work.
- `inventor-parity-sequence.md`, `inventor-parity-test-plan.md`: remaining implementation plan,
  combinations and future native/performance obligations. Do not reduce unimplemented planning.
- `file-format.md`: save-format contract and compatibility; feature documents own behavior.
- `user-interface.md`: shared current/planned UI contracts; `gui-shell-acceptance-mvp9r.md`: shell
  acceptance/inventory; `step-import-sequence-mvp10.md`: imported-source boundaries.
- `hole-wizard.md`: current Hole implementation and planned Hole/Thread authoring. The dedicated
  Inventor-like progressive Hole tool remains 157. Check numbered status before claiming UI support.

Older implemented-block build logs, failed intermediate attempts, duplicate suite counts and
per-delivery hashes were removed from this working guide. Their original record is available in Git:

```bash
git show 46ab49c:docs/development-setup.md
```

Use current tests, contracts and `mvp-plan.md` for new work. The following planning-delivery record is
retained verbatim to preserve its future obligations; its dated block numbers and counts are historical.

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

## Maintaining this guide

Keep reusable commands and current environment constraints here. Record concise feature-specific
acceptance and delivery evidence with its existing contract/status owner; use Git history for
superseded run logs. Preserve all planning for unimplemented features.
