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
Extend's chooser admits complete supported native line/arc/derived edges; Trim admits closed paths and profiles.
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
Modern Extend supports bounded analytic planes/cylinders, complete free boundaries and explicit
natural/tangent continuation under the [curved Surface contract](part-trim-extend-surface-geometry-mvp6.md).
Missing, stale, ambiguous or incompatible references fail closed.
Arbitrary OCCT topology indices are never persisted or guessed. Open raw Sketch chains first become
PathCurves through **Pfad erstellen**. Advanced controls add transient free-edge/gap overlays and
native measurements as described below; the original replacement-shell slice supplied text diagnostics.

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
Historical editing and explicit hidden/multiple inputs are covered below. Constrained Sweep/Loft
authoring remains required for acceptance of Block 173.

### Advanced Surface editing and historical input geometry

Browser editing of Offset/Thicken and typed Trim/Extend routes to the same owned controls as
creation. The editor loads complete typed source/contour references, side, projection vector,
continuation, construction policy, budgets and Body result context. Full-precision numeric text
preserves authored vector components and unchanged records, including formula bindings.

A disposable cache evaluates the history **before** the edited feature. Face/edge catalogs and
normal/extension frames use that cache with its matching history prefix. Historical input faces
and quilts have separate transient selection identities; hidden predecessors can be selected
without changing document visibility. The accepted model/cache stays unchanged until one checked
update transaction recomputes the complete suffix. In particular, editing a Cut from the original
box top measures its source at z=5 mm although the accepted top is already at z=3 mm.

Changed computed values require the explicit independent-value option. It creates new bindings
for changed fields and retains shared values/formulas. Fields sharing one parameter cannot submit
different values silently: the UI requires equal values or explicit separate bindings. Inactive
budgets remain excluded from the typed candidate. Chooser highlighting does not append Stitch/Fill
list entries; only explicit collection actions do. Invalid field changes clear stale handles, and
Cancel restores the entry selection, filters and scope. Repair References invokes the existing
owned repair tool after cancelling the draft.

Proof: all five targets passed. Combined advanced/legacy Surface and generic feature-edit Qt
regression **25 passed / 4 native skips / 16,791 assertions**; native browser editing, creation,
legacy Surface and generic editing **4 / 727** at both 100% and compact 150%. Cases retain exact
records through unchanged Apply, exact shared formulas, explicit budget splits, historical source
coordinates, independently integrated Cut/Thicken volumes, precise native drag/release and history.
Runtime **1,201 files**, `457f06711982e1cf574aa7d00688ab22baee92eacbf459b1e8ee6f8b8f0991dc`.
Explicit creation-time hidden/multiple-face selectors are covered below. Constrained Sweep/Loft
controls remain open Block 173 gates.
Both-scale editing/creation captures were reviewed after CAD/build processes exited. The standalone
native application initialized during an eight-second startup smoke and was deliberately stopped
by the timeout; the existing X11 visual depth/stencil warning also appears in passing native tests.

### Explicit hidden inputs and owned face sets

The Surface panel exposes hidden Body inputs explicitly, without changing persisted or presentation
visibility. Commands remain reachable when all compatible inputs are hidden. Reloading the catalog
retains typed selections and ordered collection entries by reference identity, rather than old row
numbers. Boolean targets must have geometry in the same input history. Historical editing starts
with its available predecessors; source controls are disabled during an active handle drag.

Offset/Thicken can consume an explicit set of one to 64 unique faces of one Body. Whole quilts and
individual faces remain separate choices. Adding a duplicate or a face from another owner leaves
the draft set intact. Editing reloads the complete set and bindings; switching off collection cannot
silently retain a saved multi-face fallback. Native source picking stays active during preview.
Readable Body/near-position labels and tooltips replace encoded reference keys in face choosers;
these positions are display hints, never persisted identities. Numeric loads use shortest roundtrip
text. Frames also evaluate staged existing parameter changes when a shared value drives the source.

Proof: all five targets passed. Affected Qt **21 passed / 5 native skips / 15,682 assertions**;
native Surface regression **5 / 779** at both 100% and compact 150%, with reviewed captures after
all CAD/build processes exited. Two cylindrical faces selected by actual model clicks produce an
Offset→Thicken solid with independently expected volume `154 acos(0.8) mm³`. A hidden cylinder
face goes through projected Trim Keep/Remove (independent native face-distance probes), actual
native arc picking, Natural/Tangent switching and precise Extend→Thicken. Its independent volume
is `10 acos(0.8) × (4 + 1.23456789012345) mm³`. Both workflows check exact history, Save/Open,
STEP reimport, integrated volume and material points. Additional cases reject duplicate/foreign
faces, restore visibility/filter/scope on Cancel and verify a shared source driver moves the grip
from z=5 to z=3 while its offset preview lies at z=6.

Frozen runtime: **1,203 files**,
`2e6fbcbee8f250fa3764e6e0b8e37b6f73f1f82110ae6173099a16f690423be3`.
Sweep material-region/native path selectors and constrained Loft creation/editing remain required
before accepting the complete Block 173 workflow.

### Surface parameter namespace reservation

New Surface recipes reserve every generated parameter node (distance, tolerance and deviation),
Body alias and display name before choosing their feature number. An unrelated user parameter
cannot be adopted or overwritten because its identifier happens to match a generated suffix.
Repeated independent-value overrides allocate unique display names and retain prior bindings.
All five targets passed; focused allocation/creation Qt **4 passed / 1 native skip / 185 assertions**
checks preserved user values, distinct new bindings and exact Undo. Runtime **1,203 files**,
`0e21237493cd354fe707519de7746a59750412aae3e34a1954f265211c226001`.

## Dynamic property-row layout — 173

Mode switches retain the owned controls and rebuild only their form layout. Wrapped fields and
nested forms reserve their actual preferred row heights inside the scroll area. This prevents
Curve/Contour, Path/Guide and direction controls from overlapping after task changes or when a
scrollbar narrows the panel. Selection, focus and authored feature state remain with the controls.

All five targets passed. The previously failing combined spatial/Surface context workflows now
pass **2 / 1,354**, including exact history and physical STEP export; final Qt captures were
reviewed. Complete native scale checks and full Qt regression remain the Block 173 final gate.
