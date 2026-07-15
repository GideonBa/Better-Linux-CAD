# Sketch Plane Interaction MVP-8

Status: implemented in Block 107.

This document is the canonical GUI interaction contract for Block 107. It extends the contextual
Sketch workspace from Block 106 with one device-independent mapping and one deterministic transient
interaction authority for hover, hit testing, stacked-hit cycling, window/crossing selection, grid
presentation, snapping, and inference preview.

Block 107 does not change persistent Sketch JSON, does not add shared planar point topology, and does
not solve constraints. Blocks 108 and 109 own those model and solver boundaries.

## Authority boundary

The interaction path is:

```text
persistent Part + Sketch intent
  -> GuiSketchInteractionSceneBuilder
  -> transient plane-space interaction scene
  -> GuiSketchPlaneMapping
  -> GuiSketchInteractionController
  -> OcctViewport overlay / semantic selection
  -> existing GuiSketchWorkspace status and GuiSelectionModel
```

The Core `Sketch`, parameters, projected-reference intent, and stable semantic ids remain the source
of truth. `GuiSketchInteractionScene`, sampled curve polylines, screen coordinates, hover products,
snap candidates, grid lines, hit cycles, and rubber-band rectangles are transient GUI products.
They are never serialized and never become feature or topology identity.

For example, an authored `line.web` remains the persistent line entity. Its Block-107 endpoint,
midpoint, nearest-point, screen-space polyline, and hover highlight are disposable interaction
products derived again from `line.web` whenever the active Sketch projection is rebuilt.

## Device-independent plane mapping

`GuiSketchPlaneMapping` is the only Block-107 mapping boundary between:

```text
screen position in Qt device-independent pixels (DIP)
  <-> view ray in model space
  <-> active workplane coordinates
  <-> model-space point
```

The general mapping receives two view providers:

- screen DIP -> model-space view ray;
- model-space point -> screen DIP.

Ray/plane intersection is performed against the resolved `GuiSketchPlaneView`. Plane/model
conversion then reuses the Block-99 workplane axes.

The native OCCT viewport provider uses `V3d_View::ConvertWithProj(...)` for pixel-to-ray conversion
and `V3d_View::Convert(...)` for model-to-pixel projection. Qt coordinates are multiplied by
`devicePixelRatioF()` before entering the OCCT pixel API and divided by the same ratio on the return
path. Hit and snap tolerances therefore remain specified in DIP instead of physical device pixels.

A deterministic orthographic provider exists for offscreen/headless GUI tests and non-native
viewport fallback. It uses an explicit plane center and model-units-per-DIP scale; it is not a
second persistent camera model.

## Transient interaction scene

`GuiSketchInteractionSceneBuilder` projects currently supported planar Sketch intent into four
transient products:

```text
curves
points / snap landmarks
annotations
curve intersections
```

The scene currently covers:

- line segments;
- three-point arcs;
- cubic Bezier splines;
- projected Sketch points and lines;
- reference-generated helper lines where the existing Geometry authority resolves them;
- parameter-driven rectangle profiles;
- parameter-driven circle profiles;
- circular hole-pattern circles;
- driving-dimension anchor products;
- existing reference/geometric/tangent constraint glyph anchors.

Projected geometry is resolved through the existing `SketchReferenceProjector`. Lost projected
references do not create replacement geometry and do not mutate the Sketch. The scene records an
`unresolved_reference_count` and simply omits the unavailable transient primitive.

Curved entities are sampled only for interaction queries and overlay presentation:

```text
three-point arc   48 segments
cubic Bezier      64 segments
circle            72 segments
```

These samples are not Geometry output, profile identity, STEP output, or persistent topology. The
exact authored curve intent remains authoritative. Later blocks may replace interaction sampling
with more specialized analytic hit tests without a file-format migration.

## Hit testing and priority

`GuiSketchInteractionController::hits_at(...)` evaluates the active scene with zoom-stable DIP
tolerances. The frozen hit priority is:

```text
Point
Curve
Dimension
Glyph
```

Within one priority class, candidates are ordered by:

```text
screen distance in DIP
model-space distance
stable candidate id
```

This keeps a handle or snap landmark selectable in front of its owning curve and keeps curves ahead
of annotation text/glyph products.

Repeated selection at the same screen position cycles through the deterministic ordered hit stack.
The cycle resets when the pointer moves beyond the configured DIP reset tolerance or when the hit
signature changes. The cycle signature uses hit kind plus stable transient candidate id; AIS owner
addresses and OCCT traversal order are not selection identity.

For example, when a midpoint, its owning line, a dimension anchor, and a constraint glyph overlap,
the first four repeated clicks select them in exactly that order.

## Window and crossing selection

Dragging from empty Sketch canvas creates a transient rubber-band rectangle.

Direction freezes the selection mode:

```text
left -> right   Window
right -> left   Crossing
```

`Window` selects a curve only when its complete interaction polyline lies inside the rectangle.
`Crossing` selects a curve when any sampled point lies inside or any interaction segment crosses a
rectangle edge. Point and annotation landmarks are selected when contained.

The result is a lexicographically ordered list of stable semantic ids represented as
`GuiSelectionKind::SketchEntity`. Ordinary click replaces selection, `Shift` adds, and `Ctrl`
toggles. The binder writes the resulting set into the existing session selection model. No second
persistent selection identity system is introduced.

## Grid

`GuiSketchGridConfig` controls the transient Sketch grid:

```text
visible
snap_enabled
spacing
major_every
```

The default shell policy is:

```text
spacing     = 10 mm
major_every = 5
visible     = true
grid snap   = true
```

The viewport maps its four screen corners into active-plane coordinates and generates only the
visible plane-grid range. If the requested range exceeds the display budget, display lines are
subsampled deterministically. This only reduces visual density; the configured grid spacing remains
the grid-snap authority.

The Sketch menu exposes `Show grid` and `Snap to grid`. Both actions are contextual and hidden when
no Sketch workspace is active.

## Snap candidates

The implemented snap families are:

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

Origin and active-plane axes are implicit scene references. Endpoints, midpoints, centers, and
quadrants are derived from authored Sketch intent. Intersections are deterministic pairwise
intersections of transient interaction polyline segments. `Nearest` is the nearest point on an
interaction polyline. Grid snap uses configured plane spacing.

Horizontal and vertical inference consume an optional command anchor. Alignment inference compares
the raw pointer plane coordinate with existing scene landmarks. Block 107 exposes the anchor API;
creation tools in Blocks 111–113 become its primary clients.

## Deterministic snap choice

Only candidates inside the configured DIP snap tolerance are eligible. Eligible candidates are
ordered by:

```text
1. model-space distance
2. screen-space distance in DIP
3. stable snap-family priority
4. stable candidate id
```

The model-space-first rule makes the geometric candidate authoritative. Screen distance is an
explicit tie breaker and preserves usable pixel behavior under zoom. Stable family and candidate-id
ordering remove collection/traversal-order dependence.

The selected result publishes:

- raw plane coordinate;
- snapped plane coordinate;
- snap family;
- candidate id;
- inference/status text;
- model-space distance;
- screen-space distance in DIP.

The Block-106 status bar receives raw plane cursor coordinates and separate snap/inference text. A
snap marker is drawn at the selected snapped coordinate.

## Viewport overlay and shell binding

`OcctViewport` owns one transparent `SketchInteractionOverlay` child while Block-107 interaction is
active. The overlay renders:

- minor and major grid lines;
- the highest-priority hover curve or point;
- the active snap marker;
- Window/Crossing rubber-band preview.

The overlay is a presentation client only. `GuiSketchInteractionController` performs the mapping and
query algorithms.

`install_sketch_interaction_binder(...)` binds an already-created `MainWindow` to this authority.
The application installs the binder after shell construction. The binder:

- rebuilds the active interaction scene after Enter Sketch, numeric commit, repair, or recompute;
- clears interaction on Finish Sketch;
- feeds raw plane coordinates and snap/inference text into `GuiSketchWorkspaceStatus`;
- synchronizes semantic Sketch selections with the existing session selection model and browser;
- disables direct hit/box selection while a Block-106 command task is active;
- owns the contextual grid actions.

The binder contains no curve mathematics, snapping algorithm, Core mutation, or undo logic.

## Failure policy

Block 107 fails closed when:

- a mapping provider is missing;
- a workplane or ray contains non-finite values;
- a ray is parallel to the active Sketch plane;
- interaction tolerances or grid spacing are non-positive/non-finite;
- the scene builder receives a Sketch outside the supplied Part;
- an authored three-point arc is degenerate;
- a native OCCT pixel/ray conversion fails.

A mapping or interaction-scene failure publishes a GUI diagnostic and does not mutate the Part.
Lost projected references are counted and omitted from the transient scene. Snap failure never
creates geometry and box-selection failure never publishes a partial selection set.

## Focused proof

The focused tags are:

```text
[gui][sketch-hit-test]
[gui][sketch-snap]
[gui][sketch-box-selection]
```

The tests cover:

- DIP -> ray -> plane -> model -> DIP roundtrip;
- Point/Curve/Dimension/Glyph priority;
- deterministic stacked-hit cycling and reset;
- origin/axis/geometry/intersection/nearest snap behavior;
- horizontal and alignment inference;
- grid snap and bounded grid display;
- Window versus Crossing semantics;
- interaction-scene projection of lines, arcs, splines, circles, dimensions, and constraints;
- contextual shell activation, grid actions, and interaction cleanup on Finish Sketch.

## Next boundary

Block 108 owns shared planar point/entity topology, dependency-safe mutation commands, construction
and reference flags, canonical ordering, JSON migration, and exact undo/redo semantics. Block 107
continues to consume the current Sketch model read-only until that migration is implemented.
