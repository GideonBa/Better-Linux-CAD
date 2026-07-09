# Future MVP: Construction Geometry and Relation-Driven Datum System

Status: planned future block. This is not implemented yet.

This document records the requirement that BLCAD must support construction geometry: auxiliary planes, axes/lines, and points that can be placed freely in 3D and can also be defined through geometric relationships to existing model elements.

## Goal

The future goal is to let users create explicit helper geometry and use it as stable parametric reference geometry.

The target user path should include:

```text
ConstructionPlane
  -> Sketch on ConstructionPlane
  -> Feature using that sketch
```

It should also include reference geometry such as:

```text
ConstructionPoint
ConstructionLine / ConstructionAxis
ConstructionPlane
```

These objects must be part of model intent, not only temporary UI artifacts.

## Required capabilities

BLCAD should eventually support:

- auxiliary planes / construction planes / datum planes created by the user
- auxiliary lines / construction lines / datum axes created by the user
- auxiliary points / construction points / datum points created by the user
- free placement of those objects in 3D space
- sketches on user-created construction planes
- parametric dependencies between construction geometry and other model objects
- recompute of dependent sketches and features when construction geometry changes
- JSON persistence of construction geometry and its defining relationships
- STEP export through normal recompute, without serializing construction geometry as final BRep unless explicitly requested later

## Free placement

The first useful version should allow construction geometry to be placed directly by numeric parameters.

Examples:

```text
ConstructionPoint
  origin = (x, y, z)
```

```text
ConstructionLine
  point = p
  direction = d
```

```text
ConstructionPlane
  origin = p
  x_axis = u
  y_axis = v
  normal = n
```

The direct placement path is important because it gives deterministic tests before relation solving is introduced.

## Relationship-based placement

After direct placement works, construction geometry should be definable by relationships to other reference objects.

The required relationship types include at least:

- parallelism
- orthogonality / perpendicularity
- angle constraints
- coincident point-on-point relationships
- point-on-line relationships
- point-on-plane relationships
- line-on-plane relationships
- plane-through-point relationships
- plane-through-line relationships
- line-through-two-points construction
- plane-through-three-points construction
- plane offset from another plane
- line parallel to another line through a point
- plane parallel to another plane through a point
- plane normal to a line
- plane tangent or normal to a generated surface in later stages

These relationships should become part of model intent and should create dependency graph edges.

## Relation targets

Construction geometry relationships should eventually be able to reference:

- standard datum planes
- user-created construction planes
- user-created construction lines / axes
- user-created construction points
- generated semantic faces
- generated semantic edges
- generated vertices
- later, selected surfaces or analytic surface references

References to generated topology must remain semantic. The core must not store raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles.

## Sketch integration

A sketch should be able to reference a construction plane exactly as it can reference a standard datum plane or a derived semantic-face workplane.

Target path:

```text
construction_plane.offset_front
  -> sketch.profile_on_offset_plane
  -> additive or subtractive feature
```

A construction plane used by a sketch must participate in recompute ordering:

```text
reference object -> construction plane -> sketch -> feature
```

If a construction plane moves because a referenced point, line, face, or parameter changes, dependent sketches and features must be invalidated and recomputed.

## Proposed model concepts

A minimal first version should introduce these core model concepts:

```text
ConstructionPoint
ConstructionLine
ConstructionPlane
ConstructionRelation
ConstructionGeometryId
```

Possible placement definitions:

```text
ExplicitPoint3
ExplicitLine3
ExplicitPlane3
PlaneOffsetFromPlane
PlaneThroughThreePoints
LineThroughTwoPoints
PlaneParallelToPlaneThroughPoint
LineParallelToLineThroughPoint
PlaneNormalToLineThroughPoint
```

Later versions may add:

```text
SurfaceTangentPlane
SurfaceNormalLine
EdgeReferenceLine
MidpointPoint
IntersectionPoint
IntersectionLine
```

## Proposed implementation sequence

1. Add stable typed IDs for construction points, construction lines, and construction planes.
2. Add explicit 3D placement models for point, line, and plane.
3. Add `PartDocument` storage and validation for construction geometry.
4. Add dependency graph nodes for construction geometry.
5. Allow sketches to reference construction planes as workplanes.
6. Add JSON serialization and roundtrip tests for explicit construction geometry.
7. Add geometry-layer resolution from construction planes into `ResolvedWorkplane` frames.
8. Add recompute tests proving that a sketch on an explicit construction plane can drive a feature.
9. Add relation objects for simple dependencies such as plane offset, line through two points, and plane through three points.
10. Add validation for degenerate definitions: identical points, zero-length directions, parallel vectors where an intersection is required, or non-orthonormal plane frames.
11. Add relation-driven invalidation so changes to a referenced point, line, plane, parameter, or semantic face mark dependent construction geometry dirty.
12. Add relationship types for parallel, orthogonal, and angle-based construction.
13. Only after that, add relations to generated semantic faces, edges, vertices, or analytic surfaces.

## First useful acceptance tests

A minimal implementation should prove:

- an explicit construction point can be stored in `PartDocument`
- an explicit construction line can be stored in `PartDocument`
- an explicit construction plane can be stored in `PartDocument`
- a sketch can use a construction plane as its workplane
- the construction plane survives JSON roundtrip
- a document restored from JSON can recompute a feature whose sketch is placed on a construction plane
- a construction plane depending on parameters invalidates dependent sketches when those parameters change
- invalid plane frames are rejected
- invalid zero-length construction lines are rejected
- a plane-through-three-points definition rejects collinear points

## Deliberate limitations for the first version

The first construction-geometry MVP should not attempt to implement everything at once.

Out of scope for the first version:

- full 3D geometric constraint solver
- GUI manipulators
- automatic face/edge/vertex picking UI
- raw OCCT topological references in the core
- arbitrary surface relationships
- tangent planes to arbitrary NURBS surfaces
- automatic repair of invalid reference chains
- assembly-level construction geometry
- exposing construction geometry as exported final solids or curves by default

## Relationship to existing roadmap

The current MVP 2 semantic-face workplane seed proves that sketches can be placed on generated planar faces without storing raw OCCT face IDs.

The construction-geometry block generalizes the workplane concept from generated faces to user-defined reference geometry. It should remain separate from the general closed sketch profile block:

```text
Construction geometry answers: where can a sketch live?
General closed sketch profiles answer: what shape can a sketch describe?
```

Both are needed for a serious CAD system, but they should be implemented as separate, testable increments.
