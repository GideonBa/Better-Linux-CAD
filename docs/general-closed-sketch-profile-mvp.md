# Future MVP: General Closed Sketch Profiles

Status: planned future block after the current controlled semantic-face workplane sequence.

This document records a deliberately missing capability so it does not get lost while MVP 2 focuses on semantic generated-face workplanes.

## Current state

The current sketch model supports only primitive profiles:

```text
RectangleProfile
CircleProfile
```

The current geometry recompute path is intentionally narrow:

- `AdditiveExtrude` requires exactly one rectangle profile.
- `SubtractiveExtrude` requires exactly one circle profile.
- Sketches can be placed on standard datum planes or on the currently supported derived face workplanes.

This means the system does not yet support a general CAD sketch made from connected curve entities.

## Explicitly not implemented yet

The following are not implemented yet:

- free line chains
- polylines
- arcs
- splines
- connected sketch entities
- general closed sketch loops
- closed wires
- multiple contours in one sketch
- inner holes in the same sketch profile
- profile selection from several closed regions
- validation of closed loops
- validation against self-intersections
- general OCCT face creation from arbitrary sketch wires
- additive extrude from arbitrary `TopoDS_Wire` / `TopoDS_Face`
- subtractive extrude from arbitrary `TopoDS_Wire` / `TopoDS_Face`

Advanced 3D sketch curves, guide splines, sweep, loft, boundary surfaces, and closed-surface-to-solid workflows are intentionally tracked in `docs/advanced-surfacing-and-3d-sketch-mvp.md` rather than inside this first planar closed-profile MVP.

## Goal

The future goal is to support arbitrary closed sketch profiles as feature input, while still keeping model intent in the core and OCCT geometry in the geometry layer.

The target user path should become:

```text
Sketch entities
  -> SketchLoop / ClosedProfile
  -> validated planar wire
  -> OCCT face
  -> additive or subtractive extrude
```

This should allow a user to draw a closed 2D profile from sketch entities and use it as an extrude profile, instead of being limited to the current rectangle and circle special cases.

## Proposed model concepts

A minimal first version should introduce explicit sketch entities before full constraints:

```text
LineSegment
ArcSegment
SketchLoop
ClosedProfile
```

Later versions may add:

```text
SplineSegment
TrimmedCurveSegment
ConstructionGeometry
SketchConstraint
ProfileRegion
```

The first version should keep the scope small and testable.

## Proposed implementation sequence

1. Add a `LineSegment` sketch entity with stable IDs and endpoint coordinates.
2. Add a `SketchLoop` or `ClosedProfile` model that references ordered sketch entities.
3. Validate that the loop is closed within tolerance.
4. Validate that consecutive entity endpoints connect.
5. Reject self-intersecting loops in the first implementation.
6. Convert the closed loop into an OCCT `TopoDS_Wire` in the geometry layer.
7. Convert the wire into an OCCT `TopoDS_Face`.
8. Add additive extrude support for one closed profile.
9. Add subtractive through-all extrude support for one closed profile.
10. Add JSON serialization and roundtrip tests for the new sketch entities and closed profile.
11. Add geometry tests for a non-rectangular polygon extrude.
12. Add geometry tests for a non-circular closed-profile cut.

## First useful acceptance tests

A minimal implementation should prove:

- a triangle sketch loop can be stored in `PartDocument`
- the triangle loop survives JSON roundtrip
- a triangle loop can be converted into an OCCT face
- an additive triangle prism can be recomputed and exported as STEP
- a pentagon or L-shaped loop can be used as a through-all cut
- an open loop is rejected before OCCT execution
- a self-intersecting loop is rejected before OCCT execution
- a loop with duplicate profile/entity IDs is rejected by the core model

## Deliberate limitations for the first version

The first version should not attempt to implement everything at once.

Out of scope for the first closed-profile MVP:

- full sketch constraint solver
- automatic region detection from unordered lines
- multiple independent closed profiles in one feature
- inner holes in the same profile
- spline support
- tangent/parallel/perpendicular constraints
- offset profiles
- sketch trimming UI
- arbitrary non-planar sketch geometry
- 3D sketch splines connecting points from sketches on different planes
- loft, sweep, boundary surface, surface stitching, or closed-shell-to-solid features
- GUI sketch editing

## Relationship to current MVP 2

The current MVP 2 work is about semantic generated-face workplanes:

```text
top
bottom
right
left
front
back
```

The closed-profile work should come after that controlled workplane sequence, because arbitrary profiles still need reliable workplane placement, bounds handling, JSON persistence, recompute ordering, and STEP export.

The closed-profile MVP should not replace the current rectangle and circle primitives immediately. Rectangle and circle can remain fast-path primitives while general closed profiles are added as a broader feature path.

This planar block remains separate from the later advanced surfacing block:

```text
Planar closed profiles answer: how does a single sketch define a closed 2D area?
Advanced surfacing answers: how do spatial curves, multiple sketches, guide curves, and surfaces define freeform geometry?
```
