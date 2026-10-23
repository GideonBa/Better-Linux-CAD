# GUI Interactive Surface Authoring and Surface-to-Solid MVP-9

Status: headless authority implemented in Block 128; replacement-shell integration implemented as a
Block-136 slice. Block 136 remains open for Measure, Assembly, browser feature editing and acceptance.

## Authority and transaction boundary

`ShellFeatureTools`, owned explicitly by `ShellWindow`, presents the six Surface commands in the
3D-Modell ribbon's **Flächen** group and the shared feature task panel. It calls the existing
`GuiInteractiveFeatureCoordinator` and `GuiInteractiveSurfaceController`; it installs no binder,
searches no foreign widget tree and uses no polling timer.

Inputs resolve to the existing Block-88..92 references and `SurfaceFeature` records. The controller
accepts an existing result Body or an optional new candidate Body; Extend/Stitch additionally accept
candidate Length parameters. Identity, name and parameter-type collisions fail before authoring.
Preview provisions these records on a disposable Part clone and runs actual Geometry recompute.
`preview_shape` returns the resulting Body shape for a nonselectable viewport ghost. The session,
history and persistence stay unchanged until Apply commits all inputs and the feature in one
`commit_part_transaction`. Cancel discards everything. No save-format change is introduced.

## Commands and input roles

- **Begrenzungsfläche / Boundary:** two boundary curves span a surface; three or four ordered curves
  must form one closed contour. The task list supports dragging, up/down and removal.
- **Füllfläche / Fill:** one closed path, or ordered connected paths/edges forming one closed contour.
  Planar contours produce an exact analytic plane; spatial contours retain the existing filling
  algorithm.
- **Fläche trimmen / Trim:** select a target and a closed path/profile contour. Separate model-click
  roles select each input. The retained result is the region inside the contour, under the existing
  deterministic Trim contract.
- **Fläche erweitern / Extend:** select a target and one matching linear boundary. A live arrow and
  an explicit millimeter field drive distance. Its origin/direction come from the same Geometry
  outward-frame calculation as actual extension; changing the target/boundary rebuilds that frame.
- **Flächen vernähen / Stitch (Knit/Sew):** collect duplicate-free surface inputs and type a positive
  Length tolerance. Geometry diagnoses gaps, disconnected or non-manifold inputs before Apply. The
  result remains a Surface Body, including a closed stitched shell.
- **Hülle zu Volumen / Closed shell to solid:** select an existing Surface Body. Geometry requires
  a closed, orientable shell with no free edges and produces a new Solid Body.

All commands provide an actual result preview and a fixed Apply/Cancel footer. Failed previews clear
that ghost and disable Apply; invalid numeric candidates restore the preceding valid value and
preview. Extend supports pointer dragging and numeric refinement of an active drag. Escape restores
the value at drag start, then a further Escape cancels the command. Stitch tolerance is typed only.

The task exposes existing PathCurves, visible recomputed Surface Bodies, materialized Sketch profiles
for Trim, and supported unambiguous semantic box faces/linear edges on visible solid Bodies. The latter
reuse stable finishing-selection resolution, including four-line drawn rectangle extrusions.
Extend's path chooser admits only single linear segments; Trim admits closed paths and profiles.
Selection masks and an explicit semantic whitelist restrict native picking to the active input role,
then restore the previous viewport context on Apply/Cancel. The result is a new Body; source Bodies
remain under the existing Surface feature contract.

## Geometric limits and diagnostics

The shell preserves the established contracts in `part-surface-feature-intent-mvp6.md`,
`part-boundary-fill-surface-geometry-mvp6.md`, `part-trim-extend-surface-geometry-mvp6.md`,
`part-surface-stitch-geometry-mvp6.md` and `part-closed-shell-to-solid-geometry-mvp6.md`.
Extend currently requires one unambiguous face with a planar linear polygon boundary; it does not
extend arbitrary curved surfaces. Missing, stale, ambiguous or incompatible references fail closed.
Arbitrary OCCT topology indices are never persisted or guessed. Open raw Sketch chains first become
PathCurves through **Pfad erstellen**. Color-coded free-edge/gap overlays are not implemented by this
slice; the task displays the authoritative diagnostic and valid result geometry.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-surface],[interactive-surface]"
./build/dev-gui/blcad_geometry_tests "[surface-boundary-fill],[surface-trim-extend],[surface-stitch],[surface-to-solid]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-surface]"
```

`gui_shell_surface_tools_tests.cpp` verifies ribbon reachability, actual Boundary/Fill/Trim/Extend,
Stitch and shell-to-solid results, pending-record isolation, invalid-candidate rollback, role-specific
selection, cancellation, identity/type collisions, exact Undo/Redo and Save/Open. Native proof changes
target and boundary through actual model clicks, drags the extension arrow, refines its value, and
restores a second drag with Escape and clicks the reachable Apply button. Geometry proof verifies
exact planar Fill and an outward frame
that survives reversed boundary direction and source-parameter recompute. Existing headless Surface
proof continues to cover tolerance-driven gap diagnostics and coordinator routing.
