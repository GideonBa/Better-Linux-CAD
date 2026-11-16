# GUI Interactive Surface Authoring and Surface-to-Solid MVP-9

Status: headless authority implemented in Block 128; replacement-shell integration and selection-first
contexts accepted in Block 136. Block 144 places these commands on Surface; `mvp-plan.md` owns status.

## Authority and transaction boundary

`ShellFeatureTools`, owned explicitly by `ShellWindow`, presents the six Surface commands in the
**Surface** ribbon’s **Create and Modify** group and the shared feature task panel. It calls the existing
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

## Replacement-shell Surface contexts — 2026-10-25

Exactly one verified PathCurve or stable semantic edge exposes Boundary and Fill. A visible fresh
Surface Body or supported stable semantic solid face exposes Trim, Extend and Stitch; only a Surface
Body exposes shell-to-solid. Capability comes from current typed Surface inputs and reference keys,
selection kind/filter/scope and the standalone Part/cache, never from parsing an ID or chooser order.
The original action retains remaining-input prerequisites and Geometry suitability checks. In
particular, an open Surface Body may start conversion but its invalid preview blocks Apply.

Typed handoff idempotently seeds the exact first ordered curve/surface or target. Trim/Extend switch
to contour/edge collection with no arbitrary default boundary. Additional list inputs remain explicit;
duplicate insertion is rejected. Browser curve/surface/profile picks reach the same active owner as
viewport selection. Role scopes and prior filter/scope restoration use the existing Surface task.
Context Cancel, task Cancel and Escape restore complete still-valid consumed selection. Save writes
accepted intent without closing drafts; New/Open follow the common replacement lifecycle. All context
state remains transient and this slice adds no save-format field.

Cancel now rebuilds the idle scene after clearing Surface targets, removing temporary solid Edge/Face
presentations even when the previous All filter did not change. Stable idle targets remain available
under the explicit Edge/Face filters. Wrapped guidance lives outside dynamic form rows to keep
Boundary/Fill, Trim/Extend and Stitch/conversion controls usable across task switches.

Six focused `[block136-surface-context]` cases cover exact handoff, kind/key guards, nondefault
sources, stable solid roles, idempotence, all cancellation routes, scene cleanup, atomic Apply/history,
Save and replacement. The twenty-ninth combined Qt-input workflow chains Fill, Trim, Extend, Stitch
and solid conversion, cancels Boundary and an invalid open-shell conversion, rejects duplicates,
removes/reorders list entries, edits upstream depth, checks six-step history, reopens canonically and
exports STEP. Active browser curve and Surface Body collection is exercised as well. Existing native
Surface tests additionally cover actual target/edge mouse picking and extension dragging.
The native ribbon test opens the current **Surface** tab before checking Extend visibility and
bounds its ordinary/compact window to the actual screen. The native picking regression passes at
100% and 150%; this is the existing Surface workflow, with no new Surface capability claim.
Reproduction: `docs/development-setup.md`; current full/native evidence and remaining work:
`docs/gui-shell-acceptance-mvp9r.md` and `docs/mvp-plan.md`.

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


## Advanced candidate foundation

The headless Surface authority now accepts typed Offset/Thicken and modern Trim/Extend options
from the existing V13 records. Creation and `begin_edit` share candidate construction, exact
reference/parameter retention, preview validation and one atomic add/update transaction. Pending
Length parameters and native edge PathCurves are provisioned only inside the candidate. Untouched
computed values remain formula-bound; direct computed-value changes retain the existing explicit
rejection rather than removing a formula implicitly.

Offset/Thicken direction handles follow the actual source normal, reverse for OppositeNormal and
use half the authored total distance for Symmetric display/drag scaling. The normal frame is a
transient regular interior station near the quilt centroid; it never becomes a persisted face
identity or changes per-face signed-side semantics. Modern Trim retains its explicit side and
projection; Extend retains natural/tangent mode and deviation policy during editing.

[Candidate tests](../tests/gui/gui_advanced_surface_controller_tests.cpp) check independent
cylindrical thickness volumes, signed/symmetric handles, creation/editing, failed excessive
thickness, formula preservation and exact session Undo/Redo. The same file checks independently
sampled Airfoil sections through a GUI Loft candidate with explicit G2 supports and rejects global
longitudinal G2 separately. Native widgets/selectors and their complete workflow gates remain in
progress; this foundation does not enable new ribbon commands by itself.

### Advanced Surface authoring controls

Block 173 adds native Offset Surface/Thicken actions and typed creation controls shared with
Trim/Extend: local-normal side, symmetric **total** separation, exact/verified construction,
physical tolerance/deviation and all four Thicken Body operations. Trim exposes retained region
and projection vector; Extend accepts complete native curved edges and natural/tangent continuation.
Selected result edges provision curve-preserving Path inputs in the same transaction. Named Paths
keep picking priority over coincident generated edges. Numeric handles use shortest roundtrip
formatting, preserving the entered double through drag and release.

Transient free-edge overlays remain unselectable and clear on cancellation. Feedback reports the
actual normal-distance criterion and native station/error budget before the Body operation; it
makes no global thickness claim about a Boolean result. Absence of complete seam pairs is stated
explicitly, rather than reported as a zero measured gap. Invalid tolerance/deviation disables Apply;
Cancel restores input selection, filters and semantic scope without changing the document.

Creation proof: all five build targets passed; offscreen Surface regression **14 passed / 2 native
skips / 15,397 assertions**. Native Surface, viewport and Sweep/Loft regression **11 / 959** passed
at both 100% and compact 150%. The symmetric Stitch→Thicken flow verifies exact
`1.23456789012345 mm` release, two-step Undo/Redo, Save/Open and independently integrated STEP
material/volume. Both-scale captures were reviewed after all CAD processes exited. Frozen runtime:
**1,199 files**, `cfec44bc42a7306d250c7c9732f7c34818245249a79f5fa1bb05d447db2a13e5`.
Full advanced editing, hidden/multiple inputs and constrained Sweep/Loft authoring remain required
for acceptance of Block 173.
