# Sketch Plane Interaction MVP-8

Status: implemented in Block 107. Blocks 108–109 supply persistent topology and solving. Block 110 is
implemented as the first direct-manipulation consumer of fresh mapped/snapped pointer state.

This document is the canonical GUI interaction contract for Block 107. It extends the contextual
Sketch workspace with device-independent mapping and one deterministic transient authority for hover,
hit testing, stacked-hit cycling, Window/Crossing selection, grid presentation, snapping, and inference
preview.

Block 107 does not change persistent Sketch JSON and does not solve constraints.

## Authority boundary

Current interaction path:

```text
persistent Part + historical Sketch compatibility intent
  -> GuiSketchInteractionSceneBuilder
  -> transient plane-space interaction scene
  -> GuiSketchPlaneMapping
  -> GuiSketchInteractionController
  -> OcctViewport overlay / semantic selection
  -> GuiSketchWorkspace status and GuiSelectionModel
```

Core path introduced by later blocks:

```text
SketchTopology / SketchPointId         Block 108
  -> SketchConstraintSystem            Block 109
  -> SketchConstraintSolver            Block 109
  -> semantic drag candidate            Block 110
```

The current Block-107 scene builder still projects existing historical `Sketch` representation. It
does not invent `SketchPointId` values from screen positions, sampled curves, snap candidates, or hit
landmarks.

`GuiSketchInteractionScene`, sampled polylines, screen coordinates, hover products, snap candidates,
grid lines, hit cycles, inference guides, and rubber-band rectangles are transient GUI products. They
are never serialized and never become feature/topology/solver identity.

For example, an authored `line.web` remains semantic Sketch entity intent. Its Block-107 endpoint,
midpoint, nearest-point, screen polyline, and hover highlight are disposable interaction products.
Block-108 migration may establish a persistent `SketchPointId` for a shared endpoint; the Block-107
landmark itself is not that identity. Block-109 solver variables address the persistent point id.

## Device-independent plane mapping

`GuiSketchPlaneMapping` is the only Block-107 mapping boundary between:

```text
Qt screen position in device-independent pixels (DIP)
  <-> model-space view ray
  <-> active workplane coordinates
  <-> model-space point
```

The general mapping receives screen-DIP-to-ray and model-to-screen providers. Ray/plane intersection
uses resolved `GuiSketchPlaneView`; plane/model conversion reuses Block-99 workplane axes.

The native OCCT provider uses `V3d_View::ConvertWithProj(...)` and `V3d_View::Convert(...)`. Qt
coordinates are multiplied by `devicePixelRatioF()` before OCCT pixel APIs and divided on return. Hit
and snap tolerances therefore remain DIP-based.

A deterministic orthographic provider exists for offscreen GUI tests/fallback. It uses explicit plane
center and model-units-per-DIP scale and is not persistent camera intent.

Block 109 does not consume this mapping directly. Block 110 maps the pointer to plane space and then
constructs a semantic drag target over existing Block-108 identities before asking Block 109 to solve.

## Transient interaction scene

`GuiSketchInteractionSceneBuilder` projects supported historical planar Sketch intent into:

```text
curves
points / snap landmarks
annotations
curve intersections
```

The scene covers lines, three-point arcs, cubic Bezier splines, projected points/lines,
reference-generated helper lines where Geometry resolves them, parameter-driven rectangle/circle
profiles, circular hole-pattern circles, driving-dimension anchors, and reference/geometric/tangent
glyph anchors.

Lost projected references do not create replacement geometry. The scene increments
`unresolved_reference_count` and omits the unavailable transient primitive.

Curved interaction sampling is:

```text
three-point arc  48 segments
cubic Bezier     64 segments
circle           72 segments
```

Samples are not Geometry output, profile identity, STEP output, Block-108 point identity, or Block-109
solver geometry. Solver families evaluate exact topology definitions represented by their Core model;
for example Arc residuals use the three persistent defining points, not the sampled 48-segment polyline.

## Hit testing and priority

`GuiSketchInteractionController::hits_at(...)` uses zoom-stable DIP tolerances. Frozen hit priority:

```text
Point
Curve
Dimension
Glyph
```

Within one class:

```text
screen distance in DIP
model-space distance
stable candidate id
```

Repeated selection at the same screen position cycles the deterministic hit stack. The cycle resets
when pointer movement exceeds tolerance or hit signature changes. Signature uses hit kind plus stable
transient candidate id, never AIS owner address or OCCT traversal order.

Block 110 renders semantic handles in a separate overlay collection and performs deterministic handle
hit testing within 9 DIP, ordered by screen distance then stable handle id. This does not modify the
frozen Block-107 Point/Curve/Dimension/Glyph hit stack or `GuiSelectionModel`; every handle still
resolves explicitly to Block-108 point/entity roles.

## Window and Crossing selection

Empty-canvas drag creates a transient rectangle:

```text
left -> right  Window
right -> left  Crossing
```

Window selects a curve only when its complete interaction polyline lies inside. Crossing selects when
any sampled point is inside or any interaction segment crosses a rectangle edge. Point/annotation
landmarks select when contained.

Result is a lexicographically ordered semantic-id list represented as
`GuiSelectionKind::SketchEntity`. Ordinary click replaces, Shift adds, Ctrl toggles. The binder writes
the set to the existing session selection model.

A Block-107 semantic entity may later map explicitly to a Block-108 topology entity. Selection never
promotes a point hit/screen coordinate directly to `SketchPointId`.

## Grid

`GuiSketchGridConfig` controls:

```text
visible
snap_enabled
spacing
major_every
```

Default shell policy:

```text
spacing      10 mm
major_every  5
visible      true
grid snap    true
```

Visible range comes from mapping viewport corners to plane coordinates. Display lines may be
subsampled deterministically to stay within budget; configured spacing remains grid-snap authority.

Sketch menu exposes contextual `Show grid` and `Snap to grid`.

## Snap and inference candidates

Implemented families:

```text
Origin
Axis
Endpoint
Midpoint
Center
Quadrant
Intersection
Nearest
Grid
HorizontalInference
VerticalInference
AlignmentX
AlignmentY
```

Origin/axes are implicit scene references. Geometry landmarks derive from authored intent.
Intersections are deterministic pairwise intersections of interaction-polyline segments. Nearest is
nearest point on an interaction polyline. Grid uses configured plane spacing.

Horizontal/vertical inference may consume a command anchor. Alignment compares pointer plane
coordinates with existing landmarks. Creation tools in Blocks 111–113 are primary clients.

Snap candidates are presentation/query products. An accepted drag or creation command must resolve or
create persistent Block-108 topology through explicit Core identity/edit boundaries. Snap ids are not
saved as `SketchPointId` and are never passed directly to Block-109 solver targets.

## Deterministic snap choice

Only candidates within configured DIP tolerance are eligible. Ordering:

```text
1. model-space distance
2. screen-space distance in DIP
3. stable snap-family priority
4. stable candidate id
```

Selected result publishes raw/snapped plane coordinates, family, candidate id, inference text, and
model/screen distance. Block-106 status receives raw cursor and separate snap/inference text. Overlay
shows snap marker and optional dashed inference guide.

## Viewport overlay and shell binder

`OcctViewport` owns transparent `SketchInteractionOverlay` while interaction is active. It renders grid,
highest-priority hover curve/point, active snap marker, and Window/Crossing preview. The shell binder
owns dashed inference-guide presentation.

`install_sketch_interaction_binder(...)`:

- rebuilds scene after Enter Sketch, numeric commit, repair, or recompute;
- clears interaction on Finish Sketch;
- publishes raw plane coordinates and snap/inference text;
- synchronizes semantic Sketch selection with session/browser;
- disables direct hit/box selection while Block-106 task stages are active;
- owns contextual grid actions and inference-guide presentation.

The binder contains no curve mathematics, solver mathematics, Core mutation, topology migration, or
undo logic.

## Relationship to Blocks 108 and 109

Block 108 establishes persistent:

```text
SketchPointId
SketchTopologyPoint
SketchTopologyEntity
SketchTopologyDependency
SketchEditCommandExecutor
blcad.sketch_topology.mvp8
```

Block 109 establishes derived:

```text
SketchConstraintSystem
canonical point-variable order
normalized residual vector
central-difference Jacobian
SketchConstraintSolver
Jacobian rank and remaining DOF
redundant/conflicting/non-convergent/invalid-reference diagnostics
```

The separation is deliberate. A pixel-near endpoint hit may choose a visual candidate or location; only
Core topology establishes shared point identity and only stable topology point/entity ids can become
solver targets.

Block 109 evaluates exact Core topology definitions. Block 110 adds separate drag-move and Press/Release
callbacks to `OcctViewport`: pointer/snap/hit state is refreshed before Press and Release, moves may be
coalesced, and Release synchronously flushes the exact final snapped point. The solver still does not
consume interaction samples, approximated curves, or screen distances.

## Failure policy

Block 107 fails closed for missing providers, non-finite workplane/ray values, ray parallel to active
plane, invalid tolerances/grid spacing, Sketch outside supplied Part, degenerate authored three-point
arc, or native OCCT mapping failure.

Mapping/scene failure publishes GUI diagnostic and does not mutate Part. Lost projected references are
counted and omitted. Snap failure never creates geometry and box-selection failure never publishes a
partial selection set.

Block-108 topology failures and Block-109 solver failures remain separate Core diagnostics. GUI
interaction must not convert either into a partial persistent Sketch mutation.

## Focused proof

Block 107:

```text
[gui][sketch-hit-test]
[gui][sketch-snap]
[gui][sketch-box-selection]
```

Block 108:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

Block 109:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

## Next boundary

Block 111 consumes the same active-plane mapping and snap/inference authority for multi-click creation.
Accepted picks must create or reference explicit Block-108 topology identity; transient snap candidate
ids remain presentation/query state.
