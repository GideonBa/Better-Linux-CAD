# GUI Sketch Constraint Authoring MVP-8

Status: implemented in Block 114.

This contract exposes the non-dimensional Block-109 planar constraint families through stable
Block-108 topology targets, selection-driven compatibility, automatic snap inference, semantic glyphs,
disposable solver preview, and one atomic Part transaction. Driving and reference dimensions remain
owned by Block 115.

## Authority boundary

```text
manual selection or automatic snap inference
  -> SketchConstraintIntent candidate
  -> existing accepted SketchConstraintCatalog
  -> SketchConstraintAuthoringService disposable solve
  -> SketchSolveResult + solved Sketch candidate + conflict/redundancy ids
  -> GuiSketchConstraintController preview/glyph
  -> one Add sketch constraint Part transaction
  -> coordinated constraint-catalog snapshot
```

Core owns stable constraint identity, target signatures, catalog ordering, persistence, solver
composition, and acceptance policy. Geometry owns deterministic glyph anchors/tokens. GUI owns
selection compatibility, inference conversion, transient preview publication, freshness checks, and
coordinated document/catalog undo and redo. Qt widgets do not own constraint mathematics.

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
and records are stored in lexicographic id order.

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
| Tangent | line/arc/spline curve, line/arc/spline curve |
| Concentric | arc/circle-profile/hole-pattern, same family capability |
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
- two lines offer Parallel, Perpendicular, Collinear, Equal, and Tangent;
- two measurable line/arc entities offer Equal;
- two center-bearing entities offer Concentric;
- one point and one line offer Midpoint and PointOnObject;
- one point and one arc offer PointOnObject;
- two points and one line offer Symmetric;
- point-less profile containers are not offered as Fixed targets.

The returned family list is deterministic enum order and contains no duplicates.

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

Accepted status is therefore exactly:

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

The source `PartDocument`, source Sketch, and source catalog are never mutated during preview.

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
be regenerated as a complete glyph list after every solve or document restore.

`GuiSketchConstraintController` converts glyphs into `GuiSketchAnnotationPrimitive` with
`GuiSketchHitKind::Glyph`. Existing Block-107 hit priority and annotation tolerance therefore make
accepted, preview, conflict, and redundancy glyphs semantic hit targets without introducing raw widget
identity.

## Atomic commit, freshness, undo, and redo

Before commit, the GUI controller verifies:

- the active Part is the Part used for preview;
- the current constraint catalog equals the preview catalog snapshot;
- the target Sketch still exists;
- migrated current topology equals the preview source topology;
- the preview is accepted and contains a complete materialized Sketch.

A successful operation performs exactly one:

```text
GuiDocumentSession::commit_part_transaction("Add sketch constraint", ...)
```

The solved complete Sketch and `catalog_after` snapshot are published together by the controller. Its
undo/redo bridge requires matching `GuiDocumentSession` labels and matching catalog states before it
restores the corresponding `catalog_before` or `catalog_after` snapshot. A stale or out-of-order bridge
fails closed.

The sidecar is explicit because historical `blcad.part_document.mvp1` cannot represent stable arbitrary
point/entity targets. Applications save/load the catalog alongside the Part and include its snapshot in
their document-level history bundle.

## Persistence

Canonical sidecar marker:

```text
schema  = blcad.sketch_constraints.mvp8
version = 1
```

Representative record:

```json
{
  "id": "constraint.auto.horizontal.1",
  "kind": "horizontal",
  "source": "automatic",
  "targets": [
    {"kind": "entity", "id": "entity/line.1"}
  ]
}
```

The sidecar stores only document/sketch scope and authored constraint records. It does not store solved
coordinates, residuals, Jacobians, rank, DOF, conflicts, redundant sets, glyph tokens, glyph anchors,
hit-test products, or preview state.

## Failure policy

No persistent mutation occurs for:

- duplicate/empty constraint ids;
- unsupported dimensional families;
- wrong target count or target-kind signature;
- duplicate non-Fixed targets;
- missing point/entity targets;
- stale Part, topology, or catalog snapshots;
- redundant/conflicting/invalid/non-convergent solve results;
- failed solved-Sketch materialization or Part recompute;
- malformed/unsupported sidecar JSON;
- undo/redo history that no longer matches the catalog bridge.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-constraints]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-constraints]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-auto-constraint]"
```

The proof covers topological target validation, Sidecar JSON roundtrip, manual and automatic provenance,
selection compatibility, stable solver/conflict ids, source immutability, deterministic glyph anchors,
glyph hit primitives, accepted commit, refused conflict commit, and exact coordinated undo/redo.

## Next boundary

Block 115 owns driving/reference dimensions, value parameters, in-canvas dimension editing, and
parameter/expression binding. Block 116 owns trim, extend, split, Sketch fillet, and Sketch chamfer.
