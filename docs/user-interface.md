# User Interface Architecture

> **GUI Shell Reset MVP-9R (Blocks 132–136):** the Qt shell layer described in parts of this
> document (MainWindow, workspace tab bars, binder installation) was **deleted and replaced** in
> Block 133 by `ShellWindow` + `ShellRibbon` (`src/gui/shell/`, contract:
> `docs/gui-shell-reset-sequence-mvp9r.md`). The headless authorities documented here (session,
> transactions, browser model, selection semantics, viewport contracts, sketch workspace) remain
> canonical and are consumed unchanged by the new shell. Shell-widget specifics below are
> historical until this document is rewritten at the end of MVP-9R (Block 136).

Status: MVP-7 and Interactive Sketcher MVP-8 are accepted through Block 121. Blocks 95–105 provide
the optional Qt shell, document transactions, OCCT viewport, deterministic browser/property surfaces,
and semantic selection synchronization. Blocks 106–120 establish the contextual Sketch workspace,
device-independent interaction, stable shared planar topology, deterministic solving, direct creation
and modification, profile/Finish-Sketch workflows, and Interactive Sketch3D. Block 121 completes
integrated coverage, equivalence, atomicity, and measured performance acceptance. Block 122 implements
the selection-first Part/Surface/Assembly workspace. Block 123 implements reusable candidate-only
viewport manipulators and numeric-HUD coupling. Block 124 implements interactive Extrude, path
Extrude, and Revolve authoring; Block 125 implements interactive Fillet, Chamfer, Shell, and Draft
authoring; Block 126 implements interactive Pattern, Mirror, Body Boolean, and Body Transform
authoring; Block 127 implements interactive PathCurve, Sweep, and Loft authoring; Block 128 implements
interactive Surface authoring and surface-to-solid conversion; Block 129 adds the feature edit
lifecycle and Core feature-update commands; and Block 130 implements direct Assembly placement,
compatibility-filtered relationships, joint-frame preview, and coordinate motion. Block 131 adds
transient Measure and completes integrated MVP-9 acceptance. GUI Shell Reset MVP-9R is implemented
through Block 135; Block 136 completes the replacement surface. STEP Import MVP-10 begins with
Block 137.

The UI is deliberately not built like FreeCAD. The goal is a modern, consistent, reduced interface
with clear separation between model, parameters, features, Sketch topology, solver results, and
Assembly. The UI operates Core/Geometry authorities; it must not own CAD or solver mathematics.

## Main areas

| UI area | Purpose |
|---------|---------|
| 3D viewport | display and interaction with Parts, Sketches, faces, edges, axes, and Assemblies |
| Feature tree | chronological/logical feature list of a Part |
| Assembly tree | Parts, occurrences, relationships, and joints |
| Parameter window | parameter name, value, unit, formula, scope, and usage sites |
| Property panel | edit selected object/feature through validated commands |
| Command palette | quick access to Sketch, Part, Surface, Assembly, Inspect, and Exchange commands |
| Measure readout | transient distance, angle, radius/diameter, volume, solids, and witness overlay |
| Engineering panel | later technical assistants for mechanical engineering domains |

The Block-131 Measure command is an Inspect action over fresh derived Geometry. Point/edge/face and
Body picks produce a transient readout and optional witness overlay. Cancel, incompatible targets,
or stale Geometry produce no document mutation; Measure state is never serialized.

## Selection-first modeling workspace

Block 122 adds a second, model-context command surface over the accepted MVP-7 shell. The global
workspace row remains `Part | Assembly | Inspect | Exchange`; the modeling context row is:

```text
Part | Surface | Assembly | Selection filter | Repeat | ViewCube | Camera bookmarks
```

`GuiModelingWorkspace` owns transient command-selection state. `GuiModelingWorkspaceShellBinder`
rendered it through the stable Qt object names below until the Block-133 shell reset deleted it;
Block 136 re-establishes this surface on `ShellWindow`, and the names below are historical:

```text
blcad.modeling_toolbar
blcad.modeling_tabs
blcad.modeling_selection_filter
blcad.modeling_repeat
blcad.modeling_mini_toolbar
blcad.view_cube
blcad.camera_bookmarks
```

A semantic viewport/browser selection becomes a command source only when its verified capability
matches a command's first input. A Body can expose Boolean/Transform/Pattern/Mirror starts; an Edge can
expose Fillet/Chamfer; a materialized profile region can expose Extrude/Revolve/Sweep/Loft/path
Extrude. The UI does not infer capability from OCCT topology, display labels, or semantic-id spelling.

The mini-toolbar appears near the latest viewport click, orders commands deterministically, consumes
the selected object on command start, and exposes Cancel while the existing task state is active.
`Esc` and Cancel restore the complete semantic selection plus capability context. Repeat starts the
same command id without replaying hidden parameter or selection state.

Finish Sketch keeps the existing camera, cursor, workspace, and selection-filter restoration. A
single unambiguous bounded line region is materialized into the persistent Sketch before the feature
handoff; an already materialized profile is handed off unchanged. Multiple or ambiguous detected
regions still require an explicit contour/profile selection.

The selection-filter combo publishes one mask to both `GuiSelectionModel` and `OcctViewport`. The
ViewCube reuses viewport standard-camera authority; Home and named bookmarks store complete transient
camera snapshots and never enter Part, Project, sidecar, undo history, or exchange persistence.

Canonical contract: `docs/gui-modeling-workspace-mvp9.md`.

## Viewport manipulators and numeric HUD

Block 123 adds one common direct-manipulation surface for later Part, Surface, and Assembly commands:

```text
linear arrow | angular wheel | radial handle
translation triad | rotation triad
pattern count | pattern spacing
```

`GuiViewportManipulatorLayer` owns only transient handle descriptors, camera/model mapping, hit
products, pointer measurements, numeric text, and candidate values. It does not receive a
`GuiDocumentSession` and cannot mutate a feature, Body, component, relationship, joint, or save format.
The owning Block-124–130 command supplies handles and turns the returned candidate into its own
validated Geometry preview and Apply transaction.

Handle presentation has a fixed DIP size and tolerance, so arrows and rings remain visible across
zoom levels. Value mapping remains in model space: linear families use a model axis, angular/radial
families use an explicit model plane. Overlapping handle hits sort by screen distance and stable handle
id. Drag start freezes the initial measurement, avoiding a value jump to the visual endpoint, and
release evaluates the exact final pointer sample.

The deleted Qt overlay binder exposed the historical object names below; Block 135 re-establishes the
equivalent shell-owned `ShellFeatureTools` overlay without cross-component discovery:

```text
blcad.viewport_manipulator_overlay
blcad.manipulator_numeric_hud
```

Dragging updates the HUD. Valid typed `mm`, `deg`, or unitless count input updates the same candidate
and overrides later pointer motion until explicitly cleared. `Esc`, window deactivation, invalid
mapping, or invalid release clears transient interaction without a compensating document transaction.
In the Block-135 Extrude/Revolve shell, rejected text or an incompatible unit restores the last valid
displayed value. Cancelling only the active drag restores its drag-start value and keeps the feature
task open; a second `Esc` cancels the complete task. Selecting a non-profile object clears any prior
Finish-Sketch handoff, so commands never start from a stale hidden profile.

Canonical contract: `docs/gui-viewport-manipulators-mvp9.md`.

## Contextual Sketch workspace

Block 106 promotes planar Sketch editing to `GuiWorkspace::Sketch`, a transient contextual Part mode.
The global workspace row remains `Part | Assembly | Inspect | Exchange`; entering Sketch does not create
persistent project mode or Part feature.

Sketch command surface:

```text
Create | Constrain | Dimension | Modify | Project
```

Sketch browser contract:

```text
Entities | Constraints | Dimensions | Diagnostics
```

The command bar renders group strip/numeric HUD. Existing model browser remains semantic selection
authority and Properties/Tasks remains contextual task surface. Status bar exposes tool hint, raw plane
cursor coordinates, snap/inference, remaining DOF, and solve state.

Producer boundaries are explicit:

```text
Block 107  cursor / hover / hit / box selection / grid / snap / inference
Block 108  persistent shared SketchPointId / SketchTopology identity
Block 109  deterministic solve result / exact local remaining DOF / solver diagnostics
Block 110  semantic handles / live drag solve invocation / status publication / release commit
```

Block 109 provides the headless producer and Block 110 continuously publishes baseline/live drag
`SketchSolveResult` status and remaining DOF through the existing status row. The UI must render
`SketchSolveResult`; it must not count endpoints or glyphs to estimate DOF.

`Enter Sketch` captures workspace, semantic selection, full transient camera state, and viewport
selection filter. It resolves workplane, enters normal orthographic view, restricts picking to
SketchEntity/Edge/Vertex. `Finish Sketch` cancels unfinished creation previews and restores the
workspace, camera and selection filtering after validation. Open contours remain editable and do
not become profiles; invalid closed/branched contours and document errors reject completion. It does not
invent an Extrude or another downstream feature; the handoff publishes an existing profile or the
single bounded line region materialized as part of finishing the Sketch.

The command lifecycle is canonical in `docs/gui-interactive-sketch-workspace-mvp8.md`.

## Sketch transformations and helper geometry

The Transformieren editor is nonmodal. Select several curves by repeated clicks or a selection
rectangle, then drag in the canvas or enter numeric values. Move/Copy use a translation drag;
Rotate/Scale and circular patterns allow picking the center; rectangular patterns retain numeric
counts and the second direction. Offset accepts multiple disconnected line chains. Numeric values
and the live preview share one operation request; Apply creates one undo step, and Escape cancels.

Mirror and Symmetric expose separate Symmetrieachse and Elemente pick controls. Mirror copies the
selected curves across the picked line. Symmetric constrains existing curves in nearest reflected
pairs, including their endpoints and spline control points, while keeping the picked axis stationary
during authoring. A single selected line constrains its endpoints symmetrically. Existing dimensions
and constraints participate in the solve; conflicting edits cannot be applied.

Linie offers ordinary, dashed construction and dash-dot center lines in the sketch itself. Helper
lines can serve as axes and do not contribute to automatic profiles. The Spline tool collects any
number of fit points until right-click or Enter. Every click extends a smooth cubic Bezier chain
through the picked points; the preview remains transient until completion, which creates one undo
step. Clicking near the first point closes the chain exactly,
with a continuous tangent at the seam. Finish Sketch also commits a pending spline before leaving
the editor. Closed spline chains and mixed spline/line contours become selectable extrusion
profiles; construction and center lines do not participate in region discovery. The elliptical-arc
creation command is removed; exact rational ellipse spans remain available internally for ellipses
and projected curves.

## Sketch canvas interaction

Block 107 provides one mapping between Qt DIP, OCCT view rays, active-plane coordinates, and model
points. Hit/snap tolerances are DIP-based and remain zoom/high-DPI stable. Native mapping accounts for
device-pixel ratio; offscreen tests use deterministic orthographic mapping.

Hover/repeated-click priority:

```text
Point -> Curve -> Dimension -> Glyph
```

Repeated clicks at the same location cycle the stable overlapping-hit stack. Empty-space drag uses:

```text
left -> right   Window: fully contained curves
right -> left   Crossing: contained or crossed curves
```

A transparent overlay draws grid, highest-priority hover product, active snap marker, and rubber-band
selection, and dashed horizontal/vertical/alignment inference guides. These are transient view
products; since Block 134 the shell's sketch tool surface owns them.

Sketch menu exposes `Show grid` and `Snap to grid`. Default spacing is 10 mm with every fifth line
major. Visible grid range is bounded; visual subsampling never changes snap spacing.

Implemented snap/inference families:

```text
origin | axis | endpoint | midpoint | center | quadrant
intersection | nearest | grid
horizontal | vertical | X alignment | Y alignment
```

Eligible candidates are ordered by model-space distance, screen-space distance, stable family priority,
then candidate id. Status displays raw plane coordinates separately from selected snap/inference text.

Block 107 interaction samples are not Block-109 solver geometry. For example, an Arc hit uses a sampled
interaction polyline, while the solver's arc residuals use the three persistent Block-108 defining
points.

Canonical contract: `docs/gui-sketch-plane-interaction-mvp8.md`.

## Shared point/entity topology and UI identity

Block 108 introduces Core `SketchPointId` and `SketchTopology`, the persistent identity model for solver
and direct-manipulation workflows.

Historical line/arc/spline records embed `Point2` values. Legacy migration converts them to topology
entities referencing persistent point ids. Explicit ordered closed-profile connectivity shares endpoint
ids; unrelated equal-coordinate points remain distinct identities.

Example:

```text
historical
  line.a.end   = (20, 0)
  line.b.start = (20, 0)
  profile.loop = [line.a, line.b, ...]

topology
  entity/line.a.points[1] = entity/line.a/end
  entity/line.b.points[0] = entity/line.a/end
```

One moved or solved topology point remains one shared junction.

A Block-107 point hit/snap landmark is not automatically persistent point identity. Accepted drag/
creation commands must resolve or create `SketchPointId` through explicit Core topology operations.

Block-108 Core commands support Add, Move, Replace, Remove with dependency-safe validation. Reference
topology is read-only. Successful commands provide exact complete before/after topology snapshots.
Canonical topology persistence is `blcad.sketch_topology.mvp8`; historical PartDocument JSON remains
load-compatible through explicit migration and only accepts losslessly materializable topology edits.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## General planar solver and UI contract

Block 109 adds `SketchConstraintSolver` in Core. The direct headless solver supports:

```text
coincident | fixed | horizontal | vertical
parallel | perpendicular | collinear | equal | tangent | concentric
midpoint | symmetric | point-on-object
horizontal distance | vertical distance | aligned distance
radial | diameter | angular
```

Solver identity is semantic:

```text
point target      -> SketchPointId
entity target     -> canonical SketchTopologyEntity id
variable          -> (SketchPointId, X|Y)
constraint        -> stable SketchSolverConstraint id
```

Constraint records are sorted lexicographically by id. Every non-reference point contributes X then Y
variables in canonical point-id order. Reference points remain constant.

Linear values are millimeters; angular values are degrees. Length residuals use deterministic Sketch
scale; the Jacobian uses central differences and the numeric engine uses deterministic damped
Gauss-Newton execution.

Result states are:

```text
fully constrained
under constrained + remaining DOF
redundant + stable constraint ids
conflicting + stable constraint ids
non-convergent
invalid reference
```

Remaining DOF is exact local differential DOF from final Jacobian rank, not a GUI endpoint-count
heuristic. Redundancy uses canonical incremental rank attribution. Conflict uses deterministic
single-removal re-solving in canonical constraint order.

`SketchSolveResult` is derived. It contains disposable solved topology, variable order, rank, remaining
DOF, diagnostics, iterations, and residual summary. It is not persistent model intent and solve does not
mutate source topology.

The current persisted Sketch adapter maps existing geometric constraints, TangentContinuity, and
parameter-backed distance dimensions into this solver. The direct solver supports more families than
historical Sketch JSON can currently author; Blocks 114/115 own persistence/UI authoring expansion.

Projected-reference constraints without coordinate-capable Block-108 topology report invalid reference.
The UI must not fill this with sampled endpoints or OCCT subshapes.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Solver-backed Sketch direct manipulation through Block 110

Block 110 implements the first GUI consumer that composes Blocks 107–109:

```text
screen pointer
  -> active-plane position and snap/inference
  -> explicit semantic handle
  -> existing SketchPointId/entity role
  -> disposable topology candidate + transient drag target
  -> Block-109 solve
  -> live viewport preview
  -> release: one validated document transaction
```

Preview never mutates `PartDocument`. Semantic handles are drawn as a separate cyan overlay and hit
tested within 9 DIP by screen distance then stable handle id, without changing Block-107 hit priority.
`Esc`, lost capture/window deactivation, reference geometry, incompatible fully constrained geometry,
or failed solve restores the source preview and creates no history entry. Pointer moves coalesce to the
latest pending sample; release synchronously flushes the exact final snapped position.

The block exposes endpoint, midpoint, center, radius, arc, spline, and dimension handles only where
semantic topology/constraint authority exists. Handle screen position is presentation; handle identity
must resolve to stable Core point/entity roles.

## Parameter window

The parameter model remains Core authority; UI is an editor over it. Target shape:

```text
Assembly Parameters
Name                     Value  Unit  Formula          Used by
bolt_circle_radius       50     mm    -                3 parts
bolt_count               8      -     -                3 parts
bolt_clearance_diameter  6.6    mm    from bolt_size   2 parts
plate_thickness          8      mm    -                BasePlate
gasket_thickness         2      mm    -                Gasket
```

`Used by` comes from parameter usage tracking.

## Feature and Assembly command surfaces

Feature-creation workflows share consistent task/preview/commit structure and consume existing Core/
Geometry authorities. Current validation workbenches expose Part, Surface, Assembly, analysis, and
exchange families. Block 122 supplies their common selection-first command-start and navigation
surface. Block 123 supplies reusable candidate-only manipulators. Blocks 124–130 add implemented
family-specific preview and authoring in the order frozen by
`docs/interactive-modeling-sequence-mvp9.md`; the Assembly interaction contract is
`docs/gui-interactive-assembly-mvp9.md`.

Constraint suggestions remain deterministic from compatible geometry/capability types; they do not
require AI.

## Recompute and solve feedback

After a committed change, UI updates viewport, browser, properties, and diagnostics from authoritative
recompute/solve results. Failed candidates do not publish partial persistent state.

During Block-110 drag, solve results are transient preview feedback. A failed live solve may keep the
last valid visual preview or restore the pre-drag display according to the Block-110 frozen contract,
but release cannot commit a failed candidate. Block-123 candidates are even narrower: they are numeric
input only and do not claim a valid feature preview until the owning later command validates Geometry.

## Implementation sequence

1. Keep model/recompute/serialization headless-first. Implemented.
2. Add read-only OCCT viewport and semantic presentation. Implemented.
3. Add browser/property projections and synchronized semantic selection. Implemented.
4. Add typed command/task transactions and feature-validation workbenches. Implemented through Block
   105 and accepted.
5. Add contextual Sketch workspace and device-independent canvas interaction. Implemented in 106–107.
6. Replace equal-coordinate connectivity with stable shared planar Core topology. Implemented in 108.
7. Add deterministic general planar solving and exact local DOF over that topology. Implemented in 109.
8. Add solver-backed semantic-handle drag and atomic release commit. Implemented in 110.
9. Add creation, constraints, dimensions, modify/project tools, regions, Interactive Sketch3D, and
   integrated acceptance through Block 121. Implemented and accepted.
10. Add the selection-first Part/Surface/Assembly workspace, contextual commands, filters, and
    navigation aids. Implemented in Block 122.
11. Add reusable transient viewport manipulators and numeric coupling. Implemented in Block 123.
12. Add family-specific interactive Part/Surface/Assembly authoring through Blocks 124–131.
13. Rehost Extrude/Revolve with shell-owned manipulators. Implemented in Block 135.
14. Restore full replacement-shell feature and acceptance coverage in Block 136.
15. Add STEP Reference/EditableBody import through Blocks 137–143.

## Current boundary

Blocks 106–121 are implemented and Interactive Sketcher MVP-8 is accepted. Interactive Modeling
MVP-9 is accepted through Block 131. Replacement-shell Blocks 132–135 are implemented, including
Sketch authoring and shell-owned interactive Extrude/Revolve. Block 136 full feature/acceptance
coverage is next.

No widget may implement substitute constraint mathematics. Sketch and modeling interactions map
transient picks to explicit semantic capabilities and public Core/application commands, use existing
solver/Geometry authorities for disposable candidates, and commit through the validated document
transaction/history boundary.

## Solid finishing in the replacement shell

The 3D-Modell **Bearbeiten** group provides **Verrunden**, **Fase**, **Aushöhlen** and **Formschräge**.
Choose the Body in the Feature panel and select edges or faces in the model or target list. Multiple
picks toggle independently. Only unambiguous current-body semantic targets are offered; an empty
selection disables Apply. The numeric value and viewport handle edit the same live geometry preview.
Fase exposes its three modes and a second distance/angle when needed; Aushöhlen chooses inward or
outward; Formschräge chooses a datum pull axis and neutral plane. Cancel/Escape leaves the model
unchanged, while Apply creates one undo entry including its new driving parameters.

This first Block-136 slice supports stable box roles from axis-aligned Sketch rectangles. The
remaining feature ribbon families and browser feature editing remain in the plan.

## Body patterns and Mirror

The 3D-Modell **Muster** group provides **Lineares Muster**, **Kreismuster** and **Körper spiegeln**.
Toggle multiple source Bodies in the model or list. Choose a pattern axis or mirror plane in the
reference chooser, or click **Referenz im Modell wählen** to pick it in the viewport. Source picks
survive reference changes. Linear patterns support spacing or total length and either direction;
circular patterns use equal spacing over a total angle up to 360 degrees. Count includes the original
placement. Spacing, angle and integer count have numeric editors and separate viewport handles.

Previews use the actual computed result. Apply creates a new result Body and any driving parameters
in the same undo step; source Bodies remain unchanged. Cancel/Escape discards all pending inputs.

## Body Boolean and Transform

The 3D-Modell **Körper** group provides **Kombinieren**, **Verschieben**, **Drehen** and **Skalieren**.
For Boolean, choose the target separately and toggle one or more tool Bodies in the model or source
list. **Zielkörper im Modell wählen** changes the role of the next Body pick. Choose Vereinen,
Abziehen or Schnittmenge, modify the target or create a new result Body, and choose whether to keep
the tools. An empty/invalid result disables Apply.

For transforms, select multiple Bodies, then drag the handles or enter X/Y/Z distances, a rotation
angle about a chosen axis, or a unitless scale factor about an explicit center. Both ownership
checkboxes start off and only affect geometry with existing associations. The source list remains
available while original Bodies are hidden by the live preview. Escape during a drag restores its
start value; Cancel discards the task. Apply records the entire multi-Body operation in one Undo
step and preserves the ordered transform stack.

## PathCurve, Sweep and Loft

The 3D-Modell **Sweep / Loft** group provides **Pfad erstellen**, **Sweep** and **Loft**. Build a path
from connected segments, reorder them in the list, and check a row to reverse its direction. Closure,
orientation and a fixed up-vector are explicit. Sources remain references to their original curves.

Sweep follows a selected path with a closed Sketch profile or an open path profile for a surface.
**Modellklick wählt** separates profile, trajectory and guide selection. Twist has an angle grip and a
numeric field. A guide and nonzero twist cannot be combined. Loft uses an ordered section list:
drag rows or use Hoch/Runter, check normals to flip them, and optionally choose a center path,
ordered guides and continuity. Both commands provide solid, cut and surface modes. Cut requires a
visible solid target; unsupported or invalid results disable Apply.

The scrollable task panel shares actual live previews and Apply/Cancel with the other features.
New Bodies and twist parameters appear only on Apply and are removed together by one Undo.

## Surface tools

In **3D-Modell → Flächen**, **Begrenzungsfläche** connects two curves or closes three/four ordered
boundary curves; **Füllfläche** closes an ordered contour. Add inputs to the task list, drag them to
reorder, or use Hoch/Runter and Entfernen. Open Sketch chains first become paths using **Pfad erstellen**.

For **Fläche trimmen** and **Fläche erweitern**, use **Modellklick wählt** to select the target and
then the contour/boundary. The same inputs are available in the task choosers. Trim keeps the region
inside a closed path or Sketch profile. Extend requires a matching linear outer edge: drag its arrow
for live preview and refine the distance in millimeters. Escape restores the drag's initial value;
a further Escape cancels the command.

**Flächen vernähen** collects adjacent surfaces with an explicit gap tolerance. A successfully
stitched closed shell can become a solid through **Hülle zu Volumen**. Incompatible/open inputs show
an error before Apply. A valid preview enables **Anwenden**, which creates the new result Body and
its feature in one undoable step; **Abbrechen** discards the candidate. Source Bodies remain available.

## Measure

Open **3D-Modell → Prüfen** and choose **Abstand**, **Winkel**, **Radius / Durchmesser** or
**Körperdaten**. Select elements in the model or through **Element A/B** in the **Messen** panel.
After A, the next model click selects B; **Modellklick wählt** changes that role explicitly.
**Auswahlfilter** isolates points, edges or faces when geometry overlaps.

Distance uses the supporting straight lines and planes, including their extensions beyond selected
edges/faces. Angle shows the smaller angle from 0° to 90°. Circular edges and cylindrical faces
provide radius/diameter. Part Bodies provide volume in mm³ and solid count; Project geometry can be
measured in its displayed assembly position.

Distance witnesses and A/B markers follow the camera. **Kopieren** copies a current result and
**Neu auswählen** clears the inputs. Model changes or stale geometry clear the readout and require
new picks. **Schließen**, Escape or the dock close button exits Measure and restores the previous
selection filters. Measurement creates no model change or history entry.
