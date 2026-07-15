# Shared Planar Sketch Topology MVP-8

Status: implemented in Block 108.

This document is the canonical Core contract for the shared planar point/entity topology introduced
by Block 108. It replaces equal-floating-point-coordinate coincidence as the identity model used by
future Interactive Sketcher solver and edit consumers.

The historical planar `Sketch` records remain a compatibility/materialization representation for
current Geometry and PartDocument workflows. Block 108 adds a canonical topology snapshot and an
explicit migration boundary over those records; it does not move solver authority into Qt and does
not implement the Block-109 nonlinear constraint solver.

## Authority boundary

The new Core path is:

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

The future Block-109 solver consumes `SketchTopology` point and entity identity. It must not infer
connectivity by comparing two `Point2` values.

For example, the historical records:

```text
line.a.end   = (20, 0)
line.b.start = (20, 0)
```

migrate to:

```text
entity/line.a.points[1] = entity/line.a/end
entity/line.b.points[0] = entity/line.a/end
```

Both entities therefore reference the same `SketchPointId`. Moving that point changes one point
record, not two disconnected endpoint coordinates.

## Stable planar point identity

Block 108 adds `SketchPointId` as a typed Core identity.

A `SketchTopologyPoint` contains:

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
- reference and construction flags are not both true;
- one canonical point identity for coincident coordinates with the same flags.

The canonical coincidence tolerance for legacy migration and duplicate-identity validation is
`1e-9` Sketch units.

Points with different semantic flags are not silently merged. In particular, a reference landmark
and an authored point at the same numeric coordinate remain distinct authorities.

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

Defining point arity is frozen as:

```text
Line                 2  start, end
Arc                  3  start, mid, end
Spline               4  start, control1, control2, end
RectangleProfile     1  center
CircleProfile        1  center
CircularHolePattern  1  center
```

Closed-profile families reference their ordered curve dependencies instead of duplicating point
coordinates. Projected/reference-generated entities remain explicit reference entities and do not
invent Core-local resolved coordinates.

Global point and entity collections are canonical lexicographic id order. Ordered point references
and ordered profile dependencies are semantic order and are never sorted. For example, a
`ClosedProfile` keeps `line.a -> line.b -> line.c`; canonicalization must not rewrite that contour to
lexicographic order if its authored traversal differs.

## Legacy Sketch migration

`SketchTopology::migrate_legacy(...)` is the deterministic migration from the existing embedded
`Point2` representation.

Migration first creates candidate entities with stable prefixes:

```text
SketchEntityId  -> entity/<id>
ProfileId       -> profile/<id>
```

Candidates are processed in lexicographic topology-id order. A point usage proposes:

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

When a same-flag point already exists at the candidate coordinate within `1e-9`, migration reuses
that canonical `SketchPointId`. The migration report records:

```text
previous_identity
canonical_identity
reason
```

The current reason for a collapsed legacy endpoint is:

```text
coincident legacy coordinates now share one canonical Sketch point
```

This makes unavoidable identity collapse explicit. Migration never rewrites the source `Sketch`.

For example, a connected triangle historically stores six line endpoint usages. Block-108 migration
produces three canonical point records and three identity-change records. The result is independent
of the line insertion order because candidate processing and global collections are canonical.

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

The dependency table is deletion authority for Block-108 entity commands. It is not a replacement
for the PartDocument-wide dependency graph.

## Construction and reference flags

`SketchTopologyFlags` makes construction/reference state explicit on points and entities.

```text
construction = false | true
reference    = false | true
```

`reference && construction` is rejected. Reference points/entities are read-only for Block-108
move/remove/replace commands.

Historical projected points, projected lines, and reference-generated lines migrate as reference
entities. Current legacy `LineSegment`/Arc/Spline records have no construction bit, so they migrate
as authored non-construction entities.

The canonical topology JSON persists both flags. A topology state that uses a flag not representable
by the historical PartDocument Sketch JSON is not silently flattened through the compatibility
bridge.

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

`SketchEditCommandExecutor::apply(...)` always operates on disposable copies of the point, entity,
and dependency collections. The candidate is published only after complete `SketchTopology::create`
validation succeeds.

### Add

Adding a point rejects:

- duplicate point id;
- a second same-flag point identity at canonical coincident coordinates.

Adding an entity rejects:

- duplicate entity id;
- unknown point references;
- unknown entity dependencies;
- self-dependency.

### Move

Move addresses exactly one `SketchPointId` and changes its coordinate.

Move rejects:

- missing point;
- reference point;
- non-finite coordinates;
- collision with another same-flag canonical point identity.

Because connected entities reference the same point id, moving one junction point changes all
connected entities without endpoint search or coordinate fan-out.

### Replace

Replace keeps the entity id and replaces its kind-compatible topology record. The complete candidate
is validated again.

Replace rejects:

- missing entity;
- reference entity;
- conversion of authored geometry into reference geometry;
- unknown point/entity references;
- self-dependency.

Topology dependency records owned by the replaced entity are regenerated from its ordered entity
dependencies.

### Remove

Point removal rejects a point still referenced by any topology entity.

Entity removal rejects a reference entity and any entity with an active dependency-table consumer.
For example, `entity/line.a` cannot be removed while `profile/profile.triangle` depends on it.

A failed command returns `Validation` or `Dependency` error and leaves the source topology unchanged.

## Exact undo and redo

Every successful command produces one `SketchEditTransaction` containing:

```text
before : complete canonical SketchTopology
after  : complete canonical SketchTopology
kind
object_id
```

`undo()` returns the exact `before` snapshot. `redo()` returns the exact `after` snapshot.

`SketchTopologyUndoStack` keeps transaction snapshots, clears redo history after a new command, and
never reconstructs an inverse command heuristically.

For example:

```text
initial
  -> MovePoint(shared.corner, 30, 0)
  -> moved
  -> undo == initial
  -> redo == moved
```

Equality is structural equality of the canonical point/entity/dependency records.

## Topology JSON

Block 108 adds a separate canonical Core topology schema:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

Root fields are:

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

The loader validates the same canonical topology invariants as direct Core construction. Duplicate
point ids, duplicate same-flag coincident identities, unknown references, malformed flags, and
unsupported entity kinds fail closed.

`serialize_sketch_topology_to_json(...)` and `deserialize_sketch_topology_from_json(...)` round-trip
one canonical topology exactly.

## Existing PartDocument JSON migration

The existing save schema remains:

```text
blcad.part_document.mvp1
```

Block 108 does not silently reinterpret that schema as if shared point ids had always existed.
Instead:

```text
migrate_legacy_part_document_sketch_json(...)
  -> existing PartDocument JSON loader
  -> exact requested legacy Sketch
  -> SketchTopology::migrate_legacy(...)
  -> topology + migration report
```

This is the explicit migration boundary required by Block 108. No GUI session state participates.
Existing files continue to load through the historical PartDocument loader.

The canonical topology schema is the persistence authority for shared point identity and explicit
construction/reference flags. The legacy PartDocument schema remains a compatibility carrier for
current Geometry consumers until the remaining Interactive Sketcher sequence adopts topology
natively.

## Lossless PartDocument compatibility bridge

`SketchTopologyLegacyMaterializer` projects a topology candidate back into the current
`LineSegment`, `ArcSegment`, `SplineSegment`, and profile-center representation while preserving
existing semantic entity/profile ids and parameter references.

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
update authority then validates the complete candidate, rebuilds Sketch dependencies, preserves the
authored Sketch position, restores downstream graph edges, marks affected nodes changed, and commits
the candidate atomically.

If round-trip materialization changes point identity, flags, ordered dependencies, or any other
canonical topology record, the bridge fails with:

```text
topology edit cannot be represented by legacy PartDocument Sketch JSON without identity loss
```

For example, moving a migrated shared line junction is representable: both legacy endpoint
coordinates materialize to the new position and migration reconstructs the same shared point id.
Setting a construction flag on a historical line is not representable because the old Sketch record
has no construction field; that state must remain in topology JSON rather than being silently lost.

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
derived dependency records
```

Regenerate or adapt:

```text
legacy embedded Point2 endpoint representation
migration identity-change report
PartDocument compatibility materialization candidate
Block-107 interaction samples and screen mappings
future Block-109 solver variables/residuals/results
```

No OCCT shape, sampled polyline, screen coordinate, hover state, hit stack, snap candidate, or solver
result is part of Sketch topology persistence.

## Failure policy

Block 108 fails closed for:

- empty or duplicate point/entity ids;
- non-finite point coordinates;
- duplicate same-flag canonical coincident point identities;
- reference + construction flag combination;
- wrong point arity for an entity kind;
- unknown point or entity references;
- duplicate ordered entity dependencies;
- self-dependency;
- read-only reference mutation;
- removal with active point/entity consumers;
- malformed or unsupported topology JSON;
- requested legacy Sketch missing during migration;
- lossy PartDocument compatibility materialization.

A failure does not mutate the source topology or PartDocument.

## Focused proof

Focused tags are:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

The tests prove:

- insertion-order-independent migration;
- six triangle endpoint usages collapse to three shared points;
- exact migration identity-change reporting;
- ordered profile dependencies remain ordered;
- connected entities reference the same `SketchPointId`;
- one shared-point move is visible through every connected entity reference;
- dependent point/entity removal fails with `Dependency`;
- Add/Replace/Remove candidate validation;
- exact topology snapshot undo/redo;
- legacy PartDocument Sketch JSON migration;
- canonical topology JSON schema/flag publication;
- exact topology JSON round-trip;
- malformed/duplicate point identity rejection.

## Next boundary

Block 109 owns the deterministic general planar constraint solver over Block-108 point/entity
topology. It freezes solver-variable ordering, scale normalization, nonlinear residual families,
remaining-DOF accounting, conflict/redundancy attribution, convergence diagnostics, and stable
headless solve results.
