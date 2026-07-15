# Sketch Data Model

Status: implemented historical planar Sketch/PartDocument representation; superseded as the
Interactive Sketcher connectivity identity model by Block-108 shared topology.

This document describes the original Core records for `DatumPlane`, `Sketch`, `LineSegment`,
`RectangleProfile`, `CircleProfile`, and `ClosedProfile`. These records remain supported, remain
serialized by the historical `blcad.part_document.mvp1` format, and remain current Geometry
compatibility inputs.

They are no longer the canonical identity model for future planar constraint solving and direct
manipulation. Block 108 introduces stable shared `SketchPointId` and `SketchTopology`; its canonical
contract is `docs/sketch-shared-topology-mvp8.md`.

## Historical design goal

The MVP-1 Sketch model captured planar design intent through:

- one workplane reference;
- primitive rectangle and circle profiles;
- explicit line segments;
- ordered closed profiles referencing line-segment IDs;
- parameter references for parameterized primitive sizes;
- PartDocument validation of workplane and parameter references.

Later blocks extended `Sketch` with arcs, splines, projected/reference-driven geometry, constraints,
dimensions, trim/extend, tangent continuity, additional profile families, and repair workflows. Those
extensions remain persistent historical Sketch intent and are consumed by current Part/Geometry
pipelines.

## Historical coordinate representation

`Point2` is a simple planar value:

```text
Point2
  x
  y
```

The original curve records embed those values directly. For example:

```text
LineSegment
  id = "line.a"
  start = (0, 0)
  end = (20, 0)
```

Validation requires non-empty line id, distinct endpoints within the historical tolerance, and
unique line ids in one Sketch.

Line segments are Core model intent; they are not OCCT edges. However, an embedded endpoint value is
not a shared point identity. Historically:

```text
line.a.end   = (20, 0)
line.b.start = (20, 0)
```

stores two coordinate usages whose numeric equality is interpreted by profile/geometry validation.
That is the limitation Block 108 addresses.

## Block-108 shared topology supersession

For Interactive Sketcher solver and edit consumers, the canonical identity path is now:

```text
historical Sketch records
  -> SketchTopology::migrate_legacy(...)
  -> stable SketchPointId records
  -> topology entities reference shared point ids
```

The previous example may migrate to:

```text
entity/line.a.points[1] = entity/line.a/end
entity/line.b.points[0] = entity/line.a/end
```

The two lines then reference one point identity. Future constraint solving and drag logic must consume
that topology identity instead of searching for numerically equal `Point2` values.

`SketchTopologyMigrationReport` records every historical endpoint usage collapsed into an existing
canonical shared point id. Migration is deterministic and does not mutate the source `Sketch`.

The canonical topology persistence schema is:

```text
blcad.sketch_topology.mvp8
version 1
```

The historical `blcad.part_document.mvp1` schema remains load-compatible. It is not silently
reinterpreted as if shared point ids had always been serialized.

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

Datum-plane id and name must not be empty. User-defined construction planes and broader workplane
resolution are documented in the later workplane/construction contracts.

## RectangleProfile and CircleProfile

The rectangle fast path stores:

```text
RectangleProfile
  id
  center : Point2
  width_parameter
  height_parameter
```

The circle fast path stores:

```text
CircleProfile
  id
  center : Point2
  diameter_parameter
```

Profile-local constructors validate required ids. The owning `PartDocument` validates referenced
parameters and their required types.

Block-108 legacy migration represents each primitive center through one shared topology point. The
parameter references remain authored profile intent and are not copied into point identity.

## ClosedProfile

The historical line-loop profile stores an ordered curve list:

```text
ClosedProfile
  id = "profile.triangle"
  line_segments = ["line.a", "line.b", "line.c"]
```

Validation requires at least three unique non-empty line ids, existing referenced lines, an ordered
connected loop, closure from last to first, and no non-adjacent self-intersection.

The curve order is semantic. Block-108 migration converts the references to ordered topology entity
dependencies and deliberately does not lexicographically sort this contour order.

Later ArcClosedProfile and CompositeClosedProfile families follow the same principle of ordered
semantic curve dependencies.

## Sketch

The historical `Sketch` stores:

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

Sketch ids, names, and workplane ids must be non-empty. Entity/profile ids obey their family and
cross-family uniqueness rules. Profile references and constraint/dimension targets are validated by
the existing Sketch and PartDocument authorities.

## PartDocument integration

`PartDocument` owns planar Sketch compatibility records and validates their document-level references.
`PartDocument::update_sketch(...)` performs candidate-based atomic Sketch replacement: it validates
the new Sketch through `add_sketch(...)`, restores the authored Sketch position and downstream graph
edges, synchronizes invalidation state, marks the Sketch changed, and commits only the complete
candidate.

Block 108 reuses that authority through `SketchTopologyPartDocumentEditor` only for topology edits
that are exactly representable by the historical records. The bridge performs:

```text
migrate
-> edit topology candidate
-> materialize historical Sketch candidate
-> re-migrate
-> require exact topology equality
-> PartDocument::update_sketch(...)
```

If shared point identity, explicit topology flags, or ordered dependencies would be lost, the bridge
fails closed. The canonical topology state remains persistable in `blcad.sketch_topology.mvp8`.

## Current proof

Historical Sketch behavior remains covered by `[core][sketch]`, PartDocument JSON, profile, and
Geometry tests.

Block-108 topology/migration proof is:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

For example, the topology proof migrates a connected triangle created in different line insertion
orders and obtains the same three shared point records and migration report in both cases.

## Current next boundary

Block 109 adds the deterministic general planar constraint solver over Block-108 shared point/entity
topology. The historical embedded-coordinate records remain compatibility inputs; solver connectivity
must not regress to equal-coordinate inference.
