# Shared Planar Sketch Topology MVP-8

Status: implemented in Block 108.

This document is the canonical Core contract for shared planar point/entity topology introduced by
Block 108. It replaces equal-floating-point-coordinate inference as the connectivity identity model
used by future Interactive Sketcher solver and edit consumers.

The historical planar `Sketch` records remain a compatibility/materialization representation for
current Geometry and PartDocument workflows. Block 108 adds a canonical topology snapshot and an
explicit migration boundary over those records; it does not move solver authority into Qt and does
not implement the Block-109 nonlinear constraint solver.

## Authority boundary

The Core path is:

```text
legacy/current planar Sketch intent
  -> SketchTopology::migrate_legacy(...)
  -> canonical SketchTopology
  -> SketchEditCommandExecutor
  -> candidate SketchTopology
  -> topology JSON persistence
     or
  -> lossless legacy materialization check
  -> PartDocument::update_sketch(...)
```

Block 109 consumes `SketchTopology` point and entity identity. It must not infer connectivity by
comparing two `Point2` values.

For example, a validated ordered closed profile may historically contain:

```text
line.a.end   = (20, 0)
line.b.start = (20, 0)
profile.triangle = [line.a, line.b, line.c]
```

The explicit ordered profile connectivity migrates to:

```text
entity/line.a.points[1] = entity/line.a/end
entity/line.b.points[0] = entity/line.a/end
```

Both entities reference one `SketchPointId`. Moving that point changes one point record rather than
fan-out updates over two endpoint coordinate values.

Two unrelated point usages that merely have the same coordinates are **not** merged. They remain two
point identities and can later participate in a `Coincident` constraint as distinct solver variables.

## Stable planar point identity

Block 108 adds typed `SketchPointId`.

`SketchTopologyPoint` contains:

```text
id
position : Point2
construction
reference
```

Point ids are stable semantic Sketch-topology identity. Screen coordinates, hit candidates, sampled
Block-107 polylines, OCCT vertices, and memory addresses are not point identity.

Point validation requires:

- non-empty `SketchPointId`;
- finite X/Y coordinates;
- `reference && construction` is false;
- point id uniqueness within one topology.

Distinct point ids may occupy the same coordinate. Numeric coincidence does not create connectivity.
This is required for future explicit coincidence constraints and for geometry that intentionally
contains overlapping but topologically separate points.

Points with different semantic flags are likewise never merged merely because coordinates match.

## Entity topology

`SketchTopologyEntity` contains:

```text
id
kind
ordered point references
ordered entity dependencies
construction
reference
```

Implemented topology kinds are:

```text
Line
Arc
Spline
RectangleProfile
CircleProfile
ClosedProfile
ArcClosedProfile
CompositeClosedProfile
CircularHolePattern
ProjectedPoint
ProjectedLine
ReferenceGeneratedLine
```

Defining point arity is:

```text
Line                 2  start, end
Arc                  3  start, mid, end
Spline               4  start, control1, control2, end
RectangleProfile     1  center
CircleProfile        1  center
CircularHolePattern  1  center
```

Closed-profile families reference ordered curve dependencies instead of duplicating endpoint
coordinates. Projected/reference-generated entities remain explicit reference entities and do not
invent Core-local resolved coordinates.

Global point and entity collections are canonical lexicographic id order. Ordered point references
and ordered profile dependencies are semantic order and are never sorted. A profile authored as
`line.c -> line.a -> line.b` retains that traversal order even though the global entity collection is
lexicographically ordered.

## Deterministic legacy Sketch migration

`SketchTopology::migrate_legacy(...)` converts the existing embedded-`Point2` representation.

Candidate topology ids use:

```text
SketchEntityId  -> entity/<id>
ProfileId       -> profile/<id>
```

A point usage proposes:

```text
<entity-or-profile-topology-id>/<role>
```

Examples:

```text
entity/line.web/start
entity/line.web/end
entity/arc.outer/mid
profile/profile.hole/center
```

Candidates are processed in lexicographic topology-id order.

### Connectivity-derived point sharing

Migration derives shared point identity only from existing explicit ordered profile connectivity:

```text
ClosedProfile
ArcClosedProfile
CompositeClosedProfile outer contour
CompositeClosedProfile each inner contour
```

For each supported editable curve in an ordered contour, the current curve end usage is paired with
the next curve start usage, including last-to-first closure. A pair is shareable only when its legacy
coordinates agree within `1e-9` Sketch units and its flags are compatible. The already validated
historical profile path normally guarantees this condition; an inconsistent migration candidate
fails closed.

The canonical id of a connected point-equivalence group is the lexicographically smallest proposed
point-usage id in that group. This removes source entity insertion-order dependence.

For every non-canonical usage in a shared group, `SketchTopologyMigrationReport` records:

```text
previous_identity
canonical_identity
reason
```

The reason is:

```text
legacy profile connectivity now shares one canonical Sketch point
```

Migration never rewrites the source `Sketch`.

For example, a connected triangle historically stores six endpoint usages. Its ordered
`ClosedProfile` explicitly links three adjacent endpoint pairs, so Block-108 migration produces three
shared points and three identity-change records. Reordering the source line insertion sequence does
not change the result.

By contrast, two independent lines ending at `(20, 0)` without an ordered profile relation retain
two distinct point ids. This is deliberate and is the central distinction between **coincident
coordinates** and **shared topology identity**.

Reference-generated curves without persistent editable endpoint coordinates remain reference
entities and do not receive invented point usages during migration.

## Derived dependency records

`SketchTopologyDependency` records:

```text
consumer_id
source_entity_id
role
```

Migration derives dependencies for:

- reference constraints;
- geometric constraints;
- driving dimensions;
- trim/extend operations;
- tangent continuity;
- closed/profile topology references.

Dependency records are sorted by consumer, source entity, then role and deduplicated. They are
queryable through `dependents_of(...)`.

The table is Block-108 entity-deletion authority. It is not a replacement for the PartDocument-wide
dependency graph.

## Construction and reference flags

`SketchTopologyFlags` makes construction/reference state explicit on points and entities:

```text
construction = false | true
reference    = false | true
```

`reference && construction` is rejected. Reference points/entities are read-only for Block-108
move/remove/replace commands.

Historical projected points, projected lines, and reference-generated lines migrate as reference
entities. Current legacy Line/Arc/Spline records have no construction bit and migrate as authored
non-construction entities.

Canonical topology JSON persists both flags. A topology state using a flag not representable by the
historical PartDocument Sketch JSON is not silently flattened through the compatibility bridge.

## Editable Core commands

`SketchEditCommand` freezes four command families:

```text
Add
Move
Replace
Remove
```

Concrete payloads are:

```text
AddPoint
AddEntity
MovePoint
ReplaceEntity
RemovePoint
RemoveEntity
```

`SketchEditCommandExecutor::apply(...)` operates on disposable point/entity/dependency collections.
A candidate is published only after complete `SketchTopology::create(...)` validation succeeds.

### Add

Adding a point rejects duplicate point id. It does **not** reject another point at the same numeric
coordinate because coincident coordinates may represent distinct topology identities.

Adding an entity rejects duplicate entity id, unknown point references, unknown entity dependencies,
and self-dependency.

### Move

Move addresses exactly one `SketchPointId` and changes only that point record.

Move rejects:

- missing point;
- reference point;
- non-finite coordinates.

Move may place one point at another point's coordinate without merging their ids. A future Block-109
constraint may require or diagnose that coincidence. If the moved point is shared by connected
entities, every connected entity still references the same moved id.

### Replace

Replace keeps entity id and replaces its topology record. Complete candidate topology is validated.

Replace rejects missing entity, reference entity, conversion of authored geometry into reference
geometry, unknown point/entity references, and self-dependency. Topology dependency records owned by
the replaced entity are regenerated from ordered entity dependencies.

### Remove

Point removal rejects a point still referenced by any topology entity.

Entity removal rejects a reference entity and any entity with an active dependency-table consumer.
For example, `entity/line.a` cannot be removed while `profile/profile.triangle` depends on it.

A failed command returns `Validation` or `Dependency` and leaves source topology unchanged.

## Exact undo and redo

Every successful command produces one `SketchEditTransaction` containing:

```text
before : complete canonical SketchTopology
after  : complete canonical SketchTopology
kind
object_id
```

`undo()` returns exact `before`; `redo()` returns exact `after`.

`SketchTopologyUndoStack` stores transaction snapshots, clears redo after a new command, and never
reconstructs inverse commands heuristically.

For example:

```text
initial
  -> MovePoint(shared.corner, 30, 0)
  -> moved
  -> undo == initial
  -> redo == moved
```

Equality is structural equality of canonical point/entity/dependency records.

## Canonical topology JSON

Block 108 adds a separate Core topology schema:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

Root fields:

```text
schema
version
sketch
points[]
entities[]
dependencies[]
```

Point record:

```json
{
  "id": "entity/line.a/end",
  "position": {"x": 20.0, "y": 0.0},
  "construction": false,
  "reference": false
}
```

Entity record:

```json
{
  "id": "entity/line.b",
  "kind": "line",
  "points": ["entity/line.a/end", "entity/line.b/end"],
  "entity_dependencies": [],
  "construction": false,
  "reference": false
}
```

Dependency record:

```json
{
  "consumer": "profile/profile.triangle",
  "source_entity": "entity/line.a",
  "role": "topology"
}
```

The loader applies the same Core invariants as direct construction. Duplicate point ids, duplicate
entity ids, unknown references, invalid flags, wrong point arity, duplicate ordered dependencies,
self-dependencies, malformed records, and unsupported kinds fail closed.

Distinct point ids with equal coordinates are valid and round-trip exactly.

`serialize_sketch_topology_to_json(...)` and `deserialize_sketch_topology_from_json(...)` round-trip
one canonical topology structurally exactly.

## Existing PartDocument JSON migration

The historical save schema remains:

```text
blcad.part_document.mvp1
```

Block 108 does not reinterpret it as if shared point ids had always existed. Instead:

```text
migrate_legacy_part_document_sketch_json(...)
  -> existing PartDocument JSON loader
  -> exact requested legacy Sketch
  -> SketchTopology::migrate_legacy(...)
  -> topology + migration report
```

No GUI session state participates. Existing PartDocument files continue to load through the
historical loader.

The topology schema is persistence authority for shared point identity and explicit topology flags.
The historical PartDocument schema remains a compatibility carrier for current Geometry consumers
until later Interactive Sketcher blocks adopt topology natively.

## Lossless PartDocument compatibility bridge

`SketchTopologyLegacyMaterializer` projects a topology candidate back into current `LineSegment`,
`ArcSegment`, `SplineSegment`, and profile-center records while preserving existing semantic ids and
parameter references.

`SketchTopologyPartDocumentEditor` performs:

```text
current PartDocument Sketch
  -> migrate to topology
  -> apply one topology command
  -> materialize legacy Sketch candidate
  -> migrate candidate back to topology
  -> exact topology equality check
  -> PartDocument::update_sketch(...)
```

Only an exactly representable candidate reaches `PartDocument::update_sketch(...)`. The existing
update authority validates the complete candidate, rebuilds Sketch dependencies, preserves authored
Sketch position, restores downstream graph edges, marks affected nodes changed, and atomically
publishes the candidate.

If round-trip materialization changes point identity, flags, ordered dependencies, or another
canonical topology record, the bridge fails with:

```text
topology edit cannot be represented by legacy PartDocument Sketch JSON without identity loss
```

For example, moving a profile-connected shared line junction is representable: both historical
endpoint coordinates materialize to the new location and profile-based migration reconstructs the
same shared id. Setting a construction flag on a historical line is not representable because the
old curve record has no construction field; that state remains in topology JSON rather than being
silently lost.

A topology-only command such as removing an entity may also leave explicit orphan point records. If
historical materialization would omit those records and therefore change canonical topology, the
PartDocument bridge rejects the single edit. The topology command itself remains valid and exactly
persistable. A later command can remove the orphan point before a lossless materialization attempt.

## Persistence and compatibility

Persist in `blcad.sketch_topology.mvp8`:

```text
SketchPointId
point coordinates
point construction/reference flags
entity id and kind
ordered point references
ordered entity dependencies
entity construction/reference flags
canonical dependency records
```

Regenerate or adapt:

```text
historical embedded Point2 endpoint representation
legacy profile-connectivity migration equivalence groups
migration identity-change report
PartDocument compatibility materialization candidate
Block-107 interaction samples and screen mappings
future Block-109 solver variables/residuals/results
```

No OCCT shape, sampled polyline, screen coordinate, hover state, hit stack, snap candidate, or solver
result is part of topology persistence.

## Failure policy

Block 108 fails closed for:

- empty or duplicate point/entity ids;
- non-finite point coordinates;
- reference + construction flag combination;
- wrong point arity for entity kind;
- unknown point or entity references;
- duplicate ordered entity dependencies;
- self-dependency;
- inconsistent explicit legacy profile connectivity during migration;
- read-only reference mutation;
- removal with active point/entity consumers;
- malformed or unsupported topology JSON;
- requested legacy Sketch missing during migration;
- lossy PartDocument compatibility materialization.

A failure does not mutate source topology or PartDocument.

## Focused proof

Focused tags:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

The focused proof covers:

- insertion-order-independent migration;
- six connected-triangle endpoint usages collapse to three shared points through profile
  connectivity;
- explicit migration identity-change reporting;
- unrelated equal-coordinate point usages remain distinct identities;
- ordered profile dependencies remain ordered;
- connected entities reference the same `SketchPointId`;
- one shared-point move is visible through every connected entity reference;
- distinct points may be moved into numeric coincidence without identity merge;
- dependent point/entity removal fails with `Dependency`;
- Add/Replace/Remove candidate validation;
- exact topology snapshot undo/redo;
- legacy PartDocument Sketch JSON migration;
- topology JSON schema/flag publication;
- exact topology JSON round-trip;
- malformed and duplicate point-id rejection.

## Next boundary

Block 109 owns the deterministic general planar constraint solver over Block-108 point/entity
topology. It freezes solver-variable ordering, scale normalization, nonlinear residual families,
remaining-DOF accounting, conflict/redundancy attribution, convergence diagnostics, and stable
headless solve results.
