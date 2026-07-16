# Shared Planar Sketch Topology MVP-8

Status: implemented in Block 108. Block 109 is the general solver consumer and Block 110 is the first direct-manipulation consumer.

This document is the canonical Core contract for shared planar point/entity topology. It replaces
floating-point-coordinate inference as the connectivity identity model used by Interactive Sketcher
solver and edit consumers.

The historical planar `Sketch` records remain a compatibility/materialization representation for
current Geometry and PartDocument workflows. `SketchTopology` is the stable point/entity identity
boundary consumed directly by the Block-109 constraint solver and later direct-manipulation blocks.

## Authority boundary

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

canonical SketchTopology
  -> SketchConstraintSolver
  -> derived solve result / DOF / diagnostics
```

The solver must not infer connectivity by comparing two `Point2` values.

For example, a validated ordered closed profile may historically contain:

```text
line.a.end   = (20, 0)
line.b.start = (20, 0)
profile.triangle = [line.a, line.b, line.c]
```

Explicit ordered profile connectivity migrates to:

```text
entity/line.a.points[1] = entity/line.a/end
entity/line.b.points[0] = entity/line.a/end
```

Both entities reference one `SketchPointId`. Moving or solving that point changes one point record.

Two unrelated point usages at the same coordinates are not merged. They remain two point identities
and Block 109 may relate them with an explicit `Coincident` residual as distinct solver variables.

## Stable planar point identity

`SketchTopologyPoint` contains:

```text
id : SketchPointId
position : Point2
construction
reference
```

Point ids are stable semantic topology identity. Screen coordinates, hit candidates, sampled Block-107
polylines, OCCT vertices, and memory addresses are not point identity.

Validation requires:

- non-empty point id;
- finite X/Y coordinates;
- `reference && construction` is false;
- point-id uniqueness within one topology.

Distinct point ids may occupy the same coordinate. Numeric coincidence does not create connectivity.

Block 109 consumes every non-reference point as two canonical solver variables, X then Y. Reference
points remain constant and do not contribute solver variables.

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

Implemented kinds are:

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

Global point and entity collections are canonical lexicographic id order. Ordered point references and
ordered profile dependencies are semantic order and are never sorted.

Block 109 addresses entity targets by this canonical entity id. Solver family/type compatibility is
explicit: for example Tangent consumes supported curve entities, Concentric consumes center-bearing
entities, and Radial/Diameter consume Arc. A wrong kind reports `InvalidReference` rather than
reinterpreting the entity.

## Deterministic legacy Sketch migration

`SketchTopology::migrate_legacy(...)` converts the existing embedded-`Point2` representation.

Candidate topology ids use:

```text
SketchEntityId -> entity/<id>
ProfileId      -> profile/<id>
```

A point usage proposes `<owner>/<role>`, for example:

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

For each supported editable curve in an ordered contour, current curve end is paired with next curve
start, including last-to-first closure. Coordinates must agree within `1e-9` Sketch units and flags
must be compatible. Inconsistent explicit connectivity fails closed.

The canonical id of a connected usage group is the lexicographically smallest proposed usage id in
that group. For every non-canonical usage, `SketchTopologyMigrationReport` records:

```text
previous_identity
canonical_identity
reason = "legacy profile connectivity now shares one canonical Sketch point"
```

Migration never rewrites the source `Sketch`.

A connected triangle historically stores six endpoint usages. Its ordered ClosedProfile links three
adjacent pairs, so migration produces three shared points and three identity-change records independent
of line insertion order.

Two independent line endpoints at `(20, 0)` without an ordered profile relation remain distinct ids.
This distinction is required by Block-109 Coincident semantics and exact DOF accounting.

Reference-generated curves without persistent editable endpoint coordinates remain reference entities
and receive no invented point usages.

## Derived dependency records

`SketchTopologyDependency` records:

```text
consumer_id
source_entity_id
role
```

Migration derives dependencies for reference constraints, geometric constraints, driving dimensions,
trim/extend operations, tangent continuity, and profile topology references.

Records are sorted by consumer/source/role and deduplicated. The table is Block-108 entity-deletion
authority; it does not replace the PartDocument dependency graph.

Block 109 consumes constraint targets through `SketchConstraintSystem`, not by parsing topology
dependency prose. The dependency table remains edit/delete authority.

## Construction and reference flags

`SketchTopologyFlags` makes construction/reference state explicit:

```text
construction = false | true
reference    = false | true
```

`reference && construction` is rejected. Reference points/entities are read-only for Block-108 edit
commands. Historical projected points, projected lines, and reference-generated lines migrate as
reference entities.

Canonical topology JSON persists both flags. A state not representable by historical PartDocument
Sketch JSON is not silently flattened through the compatibility bridge.

For Block 109, reference points are constants. Current projected point/line topology has no invented
resolved point coordinates, so historical projected-reference constraints adapted to the solver report
`InvalidReference` until the later associative-reference owner supplies a coordinate-capable target.

## Editable Core commands

`SketchEditCommand` has Add, Move, Replace, and Remove families with payloads:

```text
AddPoint
AddEntity
MovePoint
ReplaceEntity
RemovePoint
RemoveEntity
```

`SketchEditCommandExecutor::apply(...)` operates on disposable point/entity/dependency collections and
publishes a candidate only after complete `SketchTopology::create(...)` validation.

Add rejects duplicate ids and invalid references. Move addresses exactly one point id, rejects missing
or reference points and non-finite coordinates, and may move one distinct id onto another id's
coordinate without merging identity. Replace preserves entity id and validates complete candidate
references/dependencies. Remove rejects a point still referenced by an entity and rejects an entity
with active dependency consumers or reference state.

A failed command leaves source topology unchanged.

## Exact undo and redo

Every successful command produces `SketchEditTransaction` with complete canonical `before` and `after`
`SketchTopology` snapshots plus kind/object id.

`SketchTopologyUndoStack` stores those snapshots, clears redo after a new command, and never
reconstructs inverse commands heuristically.

This exact snapshot model is the topology-side rollback authority Block 110 reuses for drag preview and
cancellation.

## Canonical topology JSON

Block 108 adds:

```text
schema  = blcad.sketch_topology.mvp8
version = 1
```

Root fields are `schema`, `version`, `sketch`, `points[]`, `entities[]`, and `dependencies[]`.

The loader applies the same Core invariants as direct construction. Distinct point ids with equal
coordinates are valid and round-trip exactly.

`serialize_sketch_topology_to_json(...)` and `deserialize_sketch_topology_from_json(...)` round-trip
one canonical topology structurally exactly.

Blocks 109–110 add no topology-schema fields for solver variables, residuals, Jacobians, rank, DOF,
convergence, drag handles, pointer samples, temporary drag point/entity ids, or live previews. Those
values are derived/transient. Block 110 strips `__gui.drag.pointer` / `__gui.drag.center` from solver
output and rebuilds a topology containing exactly the source point/entity/dependency identities before
preview or commit.

## Existing PartDocument JSON migration

The historical save schema remains `blcad.part_document.mvp1`.

`migrate_legacy_part_document_sketch_json(...)` loads the historical document, selects the requested
Sketch, and runs `SketchTopology::migrate_legacy(...)`. No GUI or solver session state participates.

The topology schema is persistence authority for shared point identity and explicit topology flags.
The historical PartDocument schema remains a compatibility carrier for current Geometry consumers.

## Lossless PartDocument compatibility bridge

`SketchTopologyLegacyMaterializer` projects a topology candidate back into current Line/Arc/Spline and
profile-center records while preserving existing semantic ids and parameter references.

`SketchTopologyPartDocumentEditor` performs:

```text
current PartDocument Sketch
  -> migrate topology
  -> apply one topology command
  -> materialize historical Sketch candidate
  -> migrate candidate back to topology
  -> exact topology equality check
  -> PartDocument::update_sketch(...)
```

Only an exactly representable candidate reaches `update_sketch(...)`. Identity, flags, ordered
relationships, or orphan point records that historical Sketch JSON would lose cause fail-closed
rejection.

Block 109 solving does not automatically call this bridge. Block 110 is one explicit interaction owner:
it requires source-only solved topology to materialize and re-migrate exactly for preview, and repeats
the equality check inside one freshness-checked document transaction on release.

## Persistence and regeneration

Persist in `blcad.sketch_topology.mvp8`:

```text
SketchPointId
point coordinates and flags
entity id/kind/ordered point references/ordered entity dependencies/flags
canonical dependency records
```

Regenerate:

```text
historical embedded Point2 compatibility representation
legacy profile-connectivity equivalence groups and migration report
PartDocument compatibility materialization candidate
Block-107 interaction samples and screen mappings
Block-109 solver variables/residuals/Jacobians/rank/DOF/results/diagnostics
Block-110 drag equations and live preview candidates
```

No OCCT shape, sampled polyline, screen coordinate, hover state, hit stack, snap candidate, or solver
cache is topology persistence.

## Failure policy

Block 108 fails closed for invalid ids/coordinates/flags, wrong entity point arity, unknown point/entity
references, duplicate ordered dependencies, self-dependency, inconsistent explicit profile connectivity,
read-only reference mutation, active deletion consumers, malformed topology JSON, missing requested
historical Sketch, or lossy PartDocument materialization.

Block 109 separately fails or reports `InvalidReference` when a solver request cannot consume a valid
topology target. Solver failure does not weaken topology invariants.

## Focused proof

Block-108 tags:

```text
[core][sketch-topology]
[core][sketch-edit-command]
[core][sketch-json-migration]
```

They cover insertion-order-independent migration, profile-connected point sharing, identity-change
reporting, unrelated coincident ids remaining distinct, ordered dependencies, shared-point movement,
dependency-safe removal, Add/Replace/Remove validation, exact snapshot undo/redo, historical JSON
migration, topology JSON, and exact round-trip.

Block-109 consumer proof is documented in `docs/sketch-planar-constraint-solver-mvp8.md`.

## Next boundary

Block 111 is implemented (`docs/gui-sketch-basic-creation-mvp8.md`). Basic creation expands into
ordinary lines plus explicit ordered closed profiles, so shared corner identity still derives only
from profile connectivity under this contract; snap positions seed coordinates, never identity.
Block 112 extends the entity kinds for circles, arcs, ellipses, and slots.
