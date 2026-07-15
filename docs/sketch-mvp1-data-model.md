# Sketch Data Model

Status: implemented historical planar Sketch/PartDocument representation; superseded as the
Interactive Sketcher connectivity identity model by Block-108 shared topology and consumed by the
Block-109 general planar constraint solver through explicit migration/adaptation.

This document describes the original Core records for `DatumPlane`, `Sketch`, `LineSegment`,
`RectangleProfile`, `CircleProfile`, and `ClosedProfile`. These records remain supported, remain
serialized by historical `blcad.part_document.mvp1`, and remain current Geometry compatibility inputs.

They are not the canonical identity model for planar solving/direct manipulation. Block 108 introduces
stable shared `SketchPointId`/`SketchTopology`; Block 109 solves constraints over that topology.
Canonical contracts are:

- `docs/sketch-shared-topology-mvp8.md`
- `docs/sketch-planar-constraint-solver-mvp8.md`

## Historical design goal

The MVP-1 Sketch model captured planar design intent through:

- one workplane reference;
- primitive rectangle/circle profiles;
- explicit line segments;
- ordered closed profiles referencing line ids;
- parameter references for parameterized primitive sizes;
- PartDocument validation of workplane/parameter references.

Later blocks extended `Sketch` with arcs, splines, projected/reference-driven geometry, constraints,
dimensions, trim/extend, tangent continuity, additional profile families, and repair workflows. Those
extensions remain persistent historical Sketch intent and are consumed by current Part/Geometry
pipelines.

## Historical coordinate representation

`Point2` is a planar value:

```text
Point2
  x
  y
```

Original curve records embed values directly:

```text
LineSegment
  id = "line.a"
  start = (0, 0)
  end = (20, 0)
```

Line segments are Core intent, not OCCT edges. However, an embedded endpoint value is not shared point
identity. Historically:

```text
line.a.end   = (20, 0)
line.b.start = (20, 0)
```

stores two coordinate usages. Profile validation may interpret equality for ordered connectivity, but
Block 109 must not treat arbitrary equal coordinates as solver connectivity.

## Block-108 shared topology supersession

For Interactive Sketcher solver/edit consumers:

```text
historical Sketch records
  -> SketchTopology::migrate_legacy(...)
  -> stable SketchPointId records
  -> topology entities reference persistent point ids
```

A validated connected profile may migrate to:

```text
entity/line.a.points[1] = entity/line.a/end
entity/line.b.points[0] = entity/line.a/end
```

Both lines then reference one point identity. Migration derives sharing only from explicit ordered
historical profile connectivity and reports every collapsed endpoint usage.

Two unrelated equal-coordinate point usages remain two `SketchPointId` values. This is required so
Block 109 can express `Coincident` between distinct variables.

Canonical topology persistence:

```text
blcad.sketch_topology.mvp8
version 1
```

Historical `blcad.part_document.mvp1` remains load-compatible and is not reinterpreted as if shared ids
had always been serialized.

## Block-109 solver consumption

`SketchConstraintSolver` consumes canonical Block-108 topology. Every non-reference point contributes
X then Y variables in canonical point-id order. Constraint targets explicitly address persistent point
or topology entity ids.

Therefore the solver never performs endpoint fan-out by searching for equal `Point2` values. Connected
entities move/solve together because they already reference the same `SketchPointId`; distinct
coincident points remain distinct variables until a constraint relates them.

`SketchConstraintSystemBuilder::from_legacy(...)` adapts current persistent geometric constraints,
TangentContinuity, and parameter-backed distance dimensions to the Block-109 solve request. This is an
explicit compatibility adapter, not a replacement of historical JSON schema.

Solver variables, residuals, Jacobians, rank, remaining DOF, convergence state, and conflict/
redundancy diagnostics are derived and are not added to the MVP-1 Sketch record.

## DatumPlane

The original fixed XY datum is:

```text
DatumPlane XY
  id = "datum.xy"
  name = "XY"
  origin = (0, 0, 0)
  x_axis = (1, 0, 0)
  y_axis = (0, 1, 0)
  normal = (0, 0, 1)
```

Datum-plane id/name must not be empty. User-defined construction planes and broader workplane
resolution are documented in later workplane/construction contracts.

## RectangleProfile and CircleProfile

Rectangle fast path stores:

```text
RectangleProfile
  id
  center : Point2
  width_parameter
  height_parameter
```

Circle fast path stores:

```text
CircleProfile
  id
  center : Point2
  diameter_parameter
```

Profile constructors validate required ids; owning `PartDocument` validates parameter references/types.
Block-108 migration represents each primitive center through one topology point. Parameter references
remain authored profile intent and are not copied into point identity.

Block 109 can consume center-bearing topology entities for Concentric where the topology kind provides a
persistent center point. Current direct Radial/Diameter residuals target three-point Arc; user-facing
circle/radius dimension expansion remains Block 112/115 territory.

## ClosedProfile

Historical line loop stores an ordered curve list:

```text
ClosedProfile
  id = "profile.triangle"
  line_segments = ["line.a", "line.b", "line.c"]
```

Validation requires at least three unique non-empty ids, existing lines, ordered connected loop, closure,
and no non-adjacent self-intersection.

Curve order is semantic. Block-108 migration converts references to ordered topology entity dependencies
and never sorts contour traversal order. ArcClosedProfile/CompositeClosedProfile follow the same
ordered-dependency principle.

## Sketch

Historical `Sketch` stores:

```text
id
name
workplane
line / arc / spline records
projected and reference-generated records
constraint and dimension records
trim / extend and tangent-continuity records
primitive and general profile records
```

Sketch ids, names, and workplane ids must be non-empty. Entity/profile ids obey family/cross-family
uniqueness. Profile references and constraint/dimension targets are validated by existing Sketch and
PartDocument authorities.

The historical Sketch object remains persistence/Geometry compatibility authority. It is not the
Block-109 solver variable store.

## PartDocument integration

`PartDocument` owns historical planar Sketch compatibility records and validates document-level
references. `PartDocument::update_sketch(...)` performs candidate-based atomic replacement: validate
through `add_sketch(...)`, restore authored Sketch position/downstream graph edges, synchronize
invalidation, mark changed, and commit only the complete candidate.

Block 108 reuses that authority through `SketchTopologyPartDocumentEditor` only for exactly representable
topology edits:

```text
migrate
-> edit topology candidate
-> materialize historical Sketch candidate
-> re-migrate
-> require exact topology equality
-> PartDocument::update_sketch(...)
```

If point identity, topology flags, orphan points, or ordered dependencies would be lost, the bridge
fails closed.

Block 109 solving does not automatically call `update_sketch(...)`. It returns derived solved topology.
Block 110 and later interaction/command owners must explicitly choose one validated persistent commit
boundary.

## Current proof

Historical behavior remains covered by `[core][sketch]`, PartDocument JSON, profile, and Geometry tests.

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

The solver proof verifies canonical variable/constraint ordering, exact local remaining DOF, all initial
residual families, convergence, redundancy/conflict attribution, invalid reference, non-convergence, and
current historical constraint/dimension adaptation.

## Current next boundary

Block 110 connects semantic Sketch handles and Block-107 pointer mapping to Block-108 topology candidates
and Block-109 solving. Direct manipulation must continue to use persistent point/entity identity rather
than coordinate-equality inference.
