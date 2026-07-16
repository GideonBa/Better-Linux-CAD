# GUI Sketch Constraint Authoring MVP-8

Status: implemented in Block 114.

This contract exposes the non-dimensional Block-109 planar constraint families through stable
Block-108 topology targets, selection-driven compatibility, automatic snap inference, semantic glyphs,
disposable solver preview, session-owned persistence, and one atomic Part/catalog transaction. Driving
and reference dimensions remain owned by Block 115.

## Authority boundary

```text
manual selection or automatic snap inference
  -> SketchConstraintIntent candidate
  -> GuiDocumentSession-owned accepted SketchConstraintCatalog
  -> SketchConstraintAuthoringService disposable solve
  -> SketchSolveResult + solved Sketch candidate + conflict/redundancy ids
  -> GuiSketchConstraintController preview/glyph
  -> one commit_part_constraint_transaction("Add sketch constraint", ...)
  -> PartDocument + all SketchConstraintCatalogs in one history snapshot
  -> later solver-backed drag/edit consumers rebuild from the same catalogs
```

Core owns stable constraint identity, target signatures, catalog ordering, persistence, solver
composition, and acceptance policy. Geometry owns deterministic glyph anchors/tokens. GUI owns
selection compatibility, inference conversion, transient preview publication, freshness checks,
sidecar file coordination, and document/catalog history. Qt widgets do not own constraint mathematics.

## Persistent constraint intent

`SketchConstraintIntent` contains:

```text
SketchConstraintId
SketchSolverConstraintKind
ordered targets[]
source = manual | automatic
```

A target is exactly one of:

```text
point  -> stable SketchPointId
entity -> stable SketchTopologyEntity id
```

Coordinates, screen hits, sampled curves, OCCT edges, temporary solver variables, and glyph positions
are never target identity. Target order is persistent because families such as Midpoint, Symmetric, and
PointOnObject assign different roles to their entries.

`SketchConstraintCatalog` is scoped to one `DocumentId` and one `SketchId`. Constraint ids are unique
and records are stored in lexicographic id order. `GuiDocumentSession` owns the document's complete
lexicographically ordered catalog collection.

## Implemented geometric families

Block 114 accepts every non-dimensional Block-109 family:

| Family | Ordered semantic targets |
|---|---|
| Coincident | point, point |
| Fixed | point or entity with defining points |
| Horizontal | line entity |
| Vertical | line entity |
| Parallel | line, line |
| Perpendicular | line, line |
| Collinear | line, line |
| Equal | measurable line/arc, measurable line/arc |
| Tangent | two line/arc/spline curves sharing one persistent endpoint |
| Concentric | arc/circle-profile/hole-pattern, same center-bearing capability |
| Midpoint | point, line |
| Symmetric | point, point, line axis |
| PointOnObject | point, line/arc |

Horizontal/vertical distance, aligned distance, radial, diameter, and angular constraints are rejected by
this authoring API. They require value/parameter and driving/reference semantics and therefore remain in
Block 115.

## Selection-driven command compatibility

`GuiSketchConstraintController::compatible_kinds(...)` derives the available command set from selected
stable topology targets. Unsupported combinations publish no command rather than guessing a target
role. In particular:

- one line offers Fixed, Horizontal, and Vertical;
- one point offers Fixed;
- two points offer Coincident;
- two lines offer Parallel, Perpendicular, Collinear, and Equal;
- two curves additionally offer Tangent only when they share a persistent `SketchPointId`;
- two measurable line/arc entities offer Equal;
- two center-bearing entities offer Concentric;
- one point and one line offer Midpoint and PointOnObject;
- one point and one arc offer PointOnObject;
- two points and one line offer Symmetric;
- point-less profile containers are not offered as Fixed targets.

The returned family list is deterministic enum order and contains no duplicates. Numeric coordinate
equality alone never makes two curves tangent-compatible.

## Automatic constraint candidates

Creation and edit tools may convert accepted Block-107 snap/inference results into the same persistent
intent used by manual commands:

| Snap/inference | Candidate family | Required supplied targets |
|---|---|---|
| Horizontal inference | Horizontal | authored line entity |
| Vertical inference | Vertical | authored line entity |
| Endpoint | Coincident | authored point, reference point |
| Intersection | Coincident | authored point, reference point |
| Midpoint | Midpoint | authored point, reference line |
| Center | Concentric | authored center-bearing entity, reference center-bearing entity |
| Nearest | PointOnObject | authored point, reference line/arc |

`automatic_candidate(...)` only creates a candidate and marks `source=automatic`. It never silently
commits. Grid, quadrant, axis, alignment-only, or insufficiently identified snaps return no automatic
constraint.

## Disposable solve and acceptance

`SketchConstraintAuthoringService::preview(...)` performs these ordered steps:

1. Validate catalog/Part/Sketch identity and candidate id uniqueness.
2. Migrate the current historical Sketch to stable `SketchTopology`.
3. Adapt all historical embedded constraints into a `SketchConstraintSystem`.
4. Add every accepted sidecar constraint with stable `intent/<id>` solver identity.
5. Add the candidate and solve through the existing Block-109 solver.
6. Publish stable conflict/redundancy ids with the internal prefix removed.
7. Materialize a complete solved Sketch only for `under_constrained` or `fully_constrained`.

Accepted status is exactly:

```text
under_constrained | fully_constrained
```

The following remain preview-only and are not commit-capable:

```text
redundant
conflicting
invalid_reference
non_convergent
```

For every rejected state, `catalog_after` equals the original catalog and no solved Sketch candidate is
published. The source `PartDocument`, source Sketch, and session catalog collection are never mutated
during preview.

## Semantic glyphs and interaction

`SketchConstraintGlyphLayoutResolver` maps one constraint to:

```text
semantic_id = sketch/<SketchId>/constraint/<SketchConstraintId>
candidate_id = <SketchConstraintId>
token
plane-space anchor
state = accepted | preview | conflict | redundant
ordered target ids
```

Point targets anchor at the persistent point. Entity targets anchor at the centroid of their defining
persistent points. Multi-target glyphs anchor at the centroid of target anchors. Accepted catalogs can
be regenerated as a complete glyph list after every solve, document restore, or file open.

`GuiSketchConstraintController` converts glyphs into `GuiSketchAnnotationPrimitive` with
`GuiSketchHitKind::Glyph`. Existing Block-107 hit priority and annotation tolerance therefore make
accepted, preview, conflict, and redundancy glyphs semantic hit targets without introducing raw widget
identity.

## Atomic commit and freshness

Before commit, the GUI controller verifies:

- the active Part is the Part used for preview;
- the session-owned constraint catalog equals the preview catalog snapshot;
- the target Sketch still exists;
- migrated current topology equals the preview source topology;
- the preview is accepted and contains a complete materialized Sketch.

A successful operation performs exactly one:

```text
GuiDocumentSession::commit_part_constraint_transaction(
    "Add sketch constraint",
    PartDocument + complete catalog collection mutation)
```

The transaction clones both Part and catalog collection, applies the solved Sketch and accepted catalog,
validates their scope, recomputes Geometry, then publishes them together as one history entry. A failed
mutation, validation, recompute, or stale check publishes neither half.

Generic Part transactions preserve and validate the current catalog collection. Deleting or replacing a
Sketch that still owns a catalog fails before publication.

## Constraint removal

`GuiSketchConstraintController::remove_accepted(session, sketch, id)` deletes one accepted
session-owned catalog constraint as its own atomic `Remove sketch constraint` history entry. The
transaction re-verifies the target Sketch and rejects a catalog that changed since the caller read
it; Sketch geometry is left exactly as solved. The Qt binder exposes
`blcad.action.sketch_constraint.delete`, enabled when exactly one accepted constraint glyph
(`sketch/<SketchId>/constraint/<id>`) is selected. Historical embedded Sketch constraint records
remain Block-99 Sketch-edit intent and are not deletable through this command. Constraint
suppression has no Core intent and remains an explicit deferral.

## Session history, dirty state, save, and open

`GuiDocumentSession::HistoryEntry` stores:

```text
Part/Project JSON snapshot
complete SketchConstraintCatalog collection
history label
```

Global `undo()` and `redo()` therefore restore Sketch geometry and accepted constraint intent together;
no controller-local mutable catalog is required. Dirty-state comparison includes both the current Part
JSON and current catalog collection.

For a Part saved as:

```text
model.blcad.json
```

the session stores the catalog collection at:

```text
model.blcad.json.sketch-constraints.json
```

Opening a Part loads the sidecar when present, verifies its document id and unique per-Sketch scope,
and restores the catalog collection before creating the saved history baseline. Saving an empty
collection removes an obsolete sidecar. Project documents do not own this Part-edit-session sidecar.

## Reuse by later direct manipulation

`SketchConstraintCatalogSystemBuilder` rebuilds the effective solver system from:

```text
historical embedded constraints
+ accepted session-owned sidecar constraints
```

The real Block-110 viewport binder now creates `GuiSketchDragController` from `GuiDocumentSession`, not
from a bare `PartDocument`. Baseline and pointer solves therefore contain accepted `intent/<id>` records.
Release rechecks the current catalog and rebuilt full constraint system against the preview snapshots
before committing. A horizontal constraint, for example, remains active during every later endpoint
drag rather than merely preserving the once-solved coordinates.

The legacy bare-Part drag constructor and commit path remain available for existing headless tests and
callers that intentionally have no sidecar catalog.

## Persistence schema

Canonical sidecar marker:

```text
schema  = blcad.sketch_constraints.mvp8
version = 1
```

Document-level shape:

```json
{
  "schema": "blcad.sketch_constraints.mvp8",
  "version": 1,
  "document": "part.example",
  "catalogs": [
    {
      "sketch": "sketch.1",
      "constraints": [
        {
          "id": "constraint.auto.horizontal.1",
          "kind": "horizontal",
          "source": "automatic",
          "targets": [
            {"kind": "entity", "id": "entity/line.1"}
          ]
        }
      ]
    }
  ]
}
```

Catalogs are unique per `SketchId` and sorted by Sketch id. Constraints are unique and sorted by
constraint id. The sidecar stores only authored scope and intent. It does not store solved coordinates,
residuals, Jacobians, rank, DOF, conflicts, redundant sets, glyph tokens, glyph anchors, hit-test
products, preview state, or temporary drag equations.

## Failure policy

No persistent mutation occurs for:

- duplicate/empty constraint ids;
- unsupported dimensional families;
- wrong target count or target-kind signature;
- duplicate non-Fixed targets;
- missing point/entity targets;
- duplicate or wrong-document Sketch catalogs;
- stale Part, topology, catalog, or effective solver-system snapshots;
- redundant/conflicting/invalid/non-convergent solve results;
- failed solved-Sketch materialization or Part recompute;
- malformed/unsupported sidecar JSON;
- sidecar/catalog scope that does not match the loaded Part;
- undo/redo history that no longer matches the expected constraint snapshot.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-constraints]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-auto-constraint]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

The proof covers accepted-constraint removal with exact undo/redo and unknown-id rejection,
topological target validation, document-level sidecar roundtrip through
`GuiDocumentSession`, manual/automatic provenance, complete family compatibility, shared-topology
Tangent gating, stable solver/conflict/redundancy ids, source immutability, deterministic glyph anchors,
glyph hit primitives, accepted atomic commit, refused conflict/redundancy commit, global exact undo/
redo, and enforcement of accepted constraints during later solver-backed drag.

## Next boundary

Block 115 owns driving/reference dimensions, value parameters, in-canvas dimension editing, and
parameter/expression binding. Block 116 owns trim, extend, split, Sketch fillet, and Sketch chamfer.
