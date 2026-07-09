# Future Roadmap: Inventor-like Sketcher and Feature Parity

Status: planned long-term roadmap. This document is not an implementation claim.

This document records the target that BLCAD should eventually grow from its current minimal CAD-kernel MVP into a serious parametric sketch and feature system. The reference quality bar is an Inventor-like workflow: rich 2D sketches, 3D sketches, constraints, dimensions, automatic profile detection, feature creation from profiles, and advanced curve/surface workflows.

The goal is not to clone Autodesk Inventor's UI, names, or proprietary behavior. The goal is to document the broad CAD capabilities that users expect from an Inventor-class sketch environment and to map them into BLCAD's own architecture.

## Current implemented baseline

The current implementation is intentionally much smaller than this roadmap.

Implemented today:

- `RectangleProfile` fast-path primitive
- `CircleProfile` fast-path primitive
- `LineSegment` sketch entity
- `ClosedProfile` from ordered line-segment IDs
- validation for connected, closed, non-self-intersecting line loops
- JSON persistence for line segments and closed profiles
- additive extrude from one rectangle or one closed line profile
- subtractive through-all cut from one circle or one closed line profile

Not implemented yet:

- automatic region detection from unordered sketch curves
- arcs as sketch entities
- splines as sketch entities
- ellipses, slots, polygons, text, projected geometry
- sketch constraints and dimensions
- trim, extend, offset, mirror, pattern, fillet, or chamfer inside the sketcher
- revolve or revolve cut
- sweep, loft, guide-curve features, boundary surfaces, or surface-to-solid conversion
- GUI sketch editing

## Design principles

The sketcher must follow the same core rule as the rest of BLCAD:

```text
core model = design intent
geometry kernel = computed cache
```

Sketch entities, dimensions, constraints, detected regions, construction geometry, and feature inputs must be stored as parametric model intent. OCCT edges, wires, faces, and solids must remain geometry-layer results.

A mature sketch must be represented as a constraint graph:

```text
SketchEntity[]
SketchConstraint[]
SketchDimension[]
SketchRegion[]
  -> solved sketch state
  -> profile regions
  -> feature inputs
```

## 2D sketch entity targets

BLCAD should eventually support these 2D sketch entities.

### Points

Target entities:

```text
SketchPoint2D
CenterPoint2D
ConstructionPoint2D
ProjectedPoint2D
IntersectionPoint2D
```

Required behavior:

- usable as constraint anchors
- usable as spline fit/control points
- usable as centers for circles, arcs, ellipses, rectangles, and patterns
- optionally construction-only
- optionally projected from model edges, vertices, axes, or construction geometry

### Lines and centerlines

Target entities:

```text
LineSegment2D
ConstructionLine2D
CenterLine2D
AxisLine2D
ProjectedLine2D
```

Required behavior:

- normal solid-profile line segments
- construction-only lines that do not create profile regions
- centerlines usable for symmetry and revolve axes
- projected lines from model references
- support for horizontal, vertical, parallel, perpendicular, collinear, equal-length, tangent, coincident, midpoint, and dimensional constraints

The current `LineSegment` is only the first narrow version of this category.

### Rectangles

Target rectangle tools:

```text
TwoPointRectangle
CenterPointRectangle
ThreePointRectangle
RotatedRectangle
ConstructionRectangle
```

Required behavior:

- generate four line entities with appropriate coincident and perpendicular constraints
- preserve user intent as a rectangle feature in the sketch where useful
- allow conversion into independent line entities later
- allow construction or normal profile geometry
- support dimensions for width, height, angle, and center placement

The current `RectangleProfile` is a fast-path primitive, not a full rectangle sketch tool.

### Circles

Target circle tools:

```text
CenterRadiusCircle
CenterDiameterCircle
TwoPointCircle
ThreePointCircle
TangentCircle
ConstructionCircle
ProjectedCircle
```

Required behavior:

- store a circle as a sketch entity, not only as a profile primitive
- support radius and diameter dimensions
- support center constraints
- support tangent, concentric, equal-radius, coincident, and projected constraints
- allow circles to contribute to profile regions when normal geometry
- allow construction circles that do not create regions

The current `CircleProfile` is a fast-path primitive, not a full circle sketch entity.

### Arcs

Target arc tools:

```text
ThreePointArc
CenterPointArc
TangentArc
ConstructionArc
ProjectedArc
```

Required behavior:

- store arc center, radius, start angle, end angle, and orientation or an equivalent robust representation
- support tangent and curvature-continuity constraints in later versions
- support radius, diameter, angle, and arc-length dimensions
- allow arcs to participate in closed profile detection
- support trimming and extension

### Ellipses and elliptical arcs

Target entities:

```text
Ellipse2D
EllipticalArc2D
ConstructionEllipse2D
```

Required behavior:

- major and minor axis definition
- center, axis, and endpoint constraints
- dimensions for major/minor axes and orientation
- optional participation in profile-region detection

### Splines

Target spline tools:

```text
FitPointSpline2D
ControlVertexSpline2D
BezierSpline2D
ConstructionSpline2D
ProjectedSpline2D
```

Required behavior:

- support interpolation/fitted splines
- support control-vertex splines
- support endpoint constraints
- support tangent handles
- support curvature handles later
- support fit-point and control-point dimensions where useful
- support trimming, splitting, and region detection where robust
- support conversion into OCCT curve representations in the geometry layer

### Polygons

Target polygon tools:

```text
InscribedPolygon
CircumscribedPolygon
EdgePolygon
ConstructionPolygon
```

Required behavior:

- generate constrained line loops
- support side count as a parameter
- support center, radius, angle, and side-length dimensions
- preserve polygon design intent where possible

### Slots

Target slot tools:

```text
CenterToCenterSlot
OverallSlot
CenterPointSlot
ThreePointArcSlot
ConstructionSlot
```

Required behavior:

- create profile geometry made from lines and arcs
- preserve slot design intent where useful
- support length, width, centerline, and orientation dimensions
- support tangent constraints between slot arcs and side lines

Slots are important for mechanical parts and should be treated as first-class sketch intent rather than only loose curves.

### Sketch fillet and sketch chamfer

Target tools:

```text
SketchFillet2D
SketchChamfer2D
```

Required behavior:

- operate on two sketch curves or a sketch corner
- create arc or line geometry
- preserve radius or distance design intent
- support equal-radius/equal-distance reuse
- update automatically when parent sketch curves move

### Sketch text

Target tool:

```text
SketchText2D
```

Required behavior:

- place text on a sketch plane
- support text parameters, font metadata, alignment, height, width factor, and rotation
- optionally convert text outlines into curves for emboss/extrude workflows
- keep font dependency and outline conversion explicit

### Projected and included geometry

Target tools:

```text
ProjectGeometry
ProjectCutEdges
ProjectSilhouette
IntersectGeometry
Include3DGeometry
```

Required behavior:

- project model edges, vertices, axes, construction geometry, and semantic references into a sketch
- preserve semantic references instead of raw OCCT topology
- support associative projections where source geometry changes
- allow projected geometry to be normal or construction-only depending on tool mode
- detect broken references and report them clearly

Projected geometry is essential for Inventor-like sketch workflows because later sketches often reference earlier features.

## 2D sketch editing tools

BLCAD should eventually support these editing operations in the sketcher.

### Trim, extend, split, and break

Target tools:

```text
TrimCurve
ExtendCurve
SplitCurve
BreakCurve
```

Required behavior:

- operate on lines, arcs, circles, ellipses, and splines
- update affected constraints or reject impossible edits
- preserve design intent when trimming a generated rectangle, slot, polygon, or projected curve
- maintain valid profile-region detection after edits

### Offset

Target tools:

```text
OffsetCurve
OffsetLoop
OffsetProfile
```

Required behavior:

- offset a single curve
- offset a chain of connected curves
- offset a closed profile loop
- support inside/outside direction
- support distance as a parameter or dimension
- handle corner modes such as sharp, round, or chamfered joins in later versions

### Mirror and pattern inside sketches

Target tools:

```text
SketchMirror
SketchRectangularPattern
SketchCircularPattern
```

Required behavior:

- preserve pattern design intent
- use construction lines or center points as mirror/pattern references
- support count and spacing/angle as parameters
- expose generated entities as dependent sketch entities
- avoid expanding semantic pattern intent into unrelated loose curves unless explicitly requested

### Move, copy, rotate, scale, and stretch

Target tools:

```text
MoveSketchEntities
CopySketchEntities
RotateSketchEntities
ScaleSketchEntities
StretchSketchEntities
```

Required behavior:

- update entity coordinates and affected constraints
- optionally copy constraints and dimensions
- reject edits that overconstrain or break the sketch
- allow explicit transformation parameters in later versions

### Convert normal/construction geometry

Target tool:

```text
ToggleConstructionGeometry
```

Required behavior:

- switch eligible entities between normal profile geometry and construction geometry
- update profile-region detection accordingly
- preserve constraints and dimensions where possible

## 2D constraints

BLCAD should eventually support a full 2D geometric constraint solver.

Target geometric constraints:

```text
Coincident
Collinear
Concentric
Parallel
Perpendicular
Tangent
Horizontal
Vertical
Equal
Symmetric
Midpoint
Fix / Ground
Smooth / CurvatureContinuous
PointOnCurve
PointOnLine
PointOnCircle
PointOnArc
PointOnSpline
PointOnProjectedGeometry
```

Required behavior:

- constraints are model intent
- constraints create dependency graph relationships where they reference parameters or external geometry
- constraint solver reports underdefined, fully defined, and overdefined states
- solver reports conflicting and redundant constraints
- automatic constraint inference may be added, but explicit constraints must remain visible and editable
- deleting geometry must delete or repair dependent constraints safely

The first constraint solver should be 2D only. 3D constraints and surfacing constraints must remain later work.

## Dimensions and parameters

Target dimension types:

```text
LinearDimension
AlignedDimension
HorizontalDimension
VerticalDimension
AngularDimension
RadiusDimension
DiameterDimension
ArcLengthDimension
DistancePointToLineDimension
DistancePointToPointDimension
DrivenReferenceDimension
```

Required behavior:

- dimensions are parameterized model intent
- dimensions can be driving or driven/reference-only
- dimensions can be named and reused in expressions
- dimensions can reference global, part, sketch, or feature parameters
- changing a dimension invalidates dependent sketch regions and features
- dimension conflicts must be reported instead of silently distorting geometry

## Automatic profile and region detection

The current `ClosedProfile` requires ordered line segments. A mature sketcher must detect closed regions automatically from a graph of curves.

Target concepts:

```text
SketchCurveGraph
SketchRegion
ProfileRegion
OuterLoop
InnerLoop
IslandLoop
RegionSelection
```

Required behavior:

- detect closed loops from unordered lines, arcs, circles, ellipses, splines, and trimmed curves
- support multiple independent regions in one sketch
- support inner loops / holes in a profile
- support overlapping and nested regions where unambiguous
- reject or report ambiguous/self-intersecting regions
- allow user selection of one or more profile regions as feature input
- keep region selection stable across sketch edits where possible
- store selected regions semantically, not as OCCT face IDs

This is the real Inventor-like step beyond the current ordered `ClosedProfile` MVP.

## 3D sketcher targets

BLCAD should eventually support a separate 3D sketch mode.

Target entities:

```text
SketchPoint3D
SketchLine3D
SketchPolyline3D
SketchArc3D
SketchSpline3D
SketchHelix3D
SketchIntersectionCurve3D
SketchProjectedCurve3D
SketchGuideCurve3D
```

Required behavior:

- curves live in model space, not only on one planar sketch
- 3D splines can connect points from different planar sketches or construction planes
- 3D curves can be used as sweep paths, loft guide rails, surface boundaries, and reference curves
- 3D sketch entities must support explicit coordinates first, then relation-driven references later
- 3D sketch geometry must remain core model intent and convert to OCCT edges/wires only in the geometry layer

## Feature targets from sketches

BLCAD should eventually support these sketch-driven feature families.

### Extrude and cut

Already partially implemented:

```text
AdditiveExtrude
SubtractiveExtrude / ThroughAllCut
```

Future scope:

- extrude one or more selected profile regions
- one-sided, symmetric, two-sided, and to-object extents
- distance, through-all, to-next, to-face, and between extents
- taper/draft angle
- thin extrude
- join, cut, intersect, and new-body operation modes

### Revolve and revolve cut

Target features:

```text
RevolveFeature
RevolveCutFeature
```

Required behavior:

- use one selected closed profile region or open thin-profile region
- use a sketch centerline, construction axis, datum axis, or semantic model axis as revolution axis
- support angle extent such as 360 degrees, partial angle, symmetric angle, or to-object in later versions
- support join, cut, intersect, and new-body operation modes
- reject self-intersecting revolution results before or during geometry execution with clear errors
- preserve axis and profile references semantically

Revolve is required for shafts, bushings, pulleys, cones, turned parts, grooves, undercuts, and rotational cuts.

### Sweep and sweep cut

Target features:

```text
SweepFeature
SweepCutFeature
SweepSurfaceFeature
```

Required behavior:

- use a profile region and a path curve
- path may be a line, polyline, arc chain, spline, helix, 3D sketch curve, construction line, or semantic edge reference
- support solid sweep from closed profile
- support surface sweep from open profile
- support path orientation, twist control, and guide/rail curves in later versions
- support join, cut, intersect, and new-body operation modes

### Loft and loft cut

Target features:

```text
LoftFeature
LoftCutFeature
LoftSurfaceFeature
MultiSectionLoft
GuidedLoft
```

Required behavior:

- use two or more profile sketches
- support arbitrary many parallel profiles
- support non-parallel profiles where well-defined
- support guide curves and rails
- support continuity modes such as C0, G1, and G2
- allow a middle sketch to shape the transition smoothly without forcing a visible hard edge when smooth continuity is requested
- support join, cut, intersect, and new-body operation modes

### Hole, thread, emboss, rib, shell, and web-like features

Target feature families:

```text
HoleFeature
ThreadFeature
EmbossFeature
EngraveFeature
RibFeature
WebFeature
ShellFeature
DraftFeature
```

Required behavior:

- consume sketch points, sketch profiles, or semantic faces as inputs
- preserve parametric design intent
- support standards databases where appropriate
- remain separate feature types rather than late raw BRep edits

### Patterns and mirrors

Target feature families:

```text
RectangularPatternFeature
CircularPatternFeature
MirrorFeature
CurveDrivenPatternFeature
SketchDrivenPatternFeature
```

Required behavior:

- pattern features, bodies, faces, or sketch regions depending on operation type
- preserve count, spacing, angle, axis, plane, or path as parametric model intent
- maintain semantic references
- avoid copying only raw geometry where a parametric pattern is intended

## Surface and solid conversion targets

The advanced surfacing roadmap remains the long-term block for surface-heavy parts.

Target features:

```text
BoundarySurface
FillSurface
NetworkSurface
PatchSurface
TrimSurface
ExtendSurface
StitchSurfaces
KnitSurfaces
SewShell
ConvertClosedShellToSolid
```

Required behavior:

- build surfaces from boundary curves, multiple sketches, projected curves, or 3D guide curves
- stitch several surfaces into a shell
- validate that a shell is closed, consistently oriented, and manifold enough to become a solid
- convert a closed stitched shell into a solid body
- keep surface construction operations as feature-tree intent

## Proposed staged implementation sequence

This roadmap is intentionally large. It should be implemented in stages.

### Stage A: explicit construction geometry

1. Add construction points, lines, and planes.
2. Allow sketches on user-created construction planes.
3. Resolve construction planes into workplane frames.
4. Persist construction geometry in JSON.

### Stage B: richer 2D sketch entities

1. Add arc sketch entities.
2. Add sketch circle entities separate from `CircleProfile`.
3. Add rectangle tools that generate constrained line entities.
4. Add basic spline entities.
5. Add projected geometry references.
6. Add JSON tests for each entity type.

### Stage C: profile detection

1. Build a sketch curve graph.
2. Detect closed regions from unordered curves.
3. Support multiple regions.
4. Support inner loops / holes.
5. Add region selection as feature input.
6. Keep the current explicit `ClosedProfile` path as a simple stable fallback.

### Stage D: constraint and dimension solver

1. Add geometric constraints.
2. Add driving and driven dimensions.
3. Detect underdefined, fully defined, and overdefined sketches.
4. Add automatic constraint inference only after explicit constraints are reliable.
5. Add solver diagnostics and repair paths.

### Stage E: revolve and richer extrude/cut features

1. Add `RevolveFeature`.
2. Add `RevolveCutFeature`.
3. Add selected profile-region inputs.
4. Add axis references to sketch centerlines, construction lines, and semantic axes.
5. Add partial-angle and symmetric-angle extents.
6. Expand extrude/cut extents beyond fixed distance and through-all.

### Stage F: sweep, loft, 3D sketches, and surfacing

1. Add 3D sketch points, lines, polylines, and splines.
2. Add sweep and sweep cut.
3. Add loft and loft cut.
4. Add guide-curve lofts.
5. Add boundary/fill surfaces.
6. Add surface stitching and closed-shell-to-solid conversion.

### Stage G: Inventor-class usability layer

1. Add GUI sketch editing.
2. Add visual constraints and dimensions.
3. Add profile highlighting and region picking.
4. Add command previews.
5. Add undo/redo and command transactions.
6. Add diagnostics for broken references and overdefined sketches.

## Deliberate non-goals for early stages

Do not attempt to implement the whole Inventor-like sketcher at once.

Early stages should not include:

- full GUI sketch editing
- all constraints at once
- all entity types at once
- automatic region detection before explicit entity validation works
- surface modeling before construction geometry and profile selection are reliable
- raw OCCT topology in the core model
- implicit feature behavior that is not stored as model intent

## Relationship to existing roadmap documents

This document ties together several existing roadmap blocks:

```text
Construction geometry:
  where sketches and references can be placed

General closed sketch profiles:
  how planar curves form selectable closed areas

Advanced surfacing and 3D sketching:
  how spatial curves, profiles, and surfaces build freeform geometry

Feature system:
  how sketch regions become features such as extrude, cut, revolve, sweep, loft, and surface operations
```

The current implementation should continue to move in small, testable increments. The immediate next step remains explicit construction geometry, because Inventor-like sketching depends on reliable user-defined planes, axes, and points.
