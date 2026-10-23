# Reference Recovery and Sketch Placement

This document describes the implemented seed for robust semantic reference recovery and sketch placement.

The goal is to make lost generated-face, generated-edge, and generated-vertex references explicit model state instead of silently reassigning sketches or projected references to arbitrary replacement topology.

## Implemented scope

The core model now has `SemanticReferenceTarget` as a uniform semantic target handle for generated references:

- generated face targets
- generated edge targets
- generated vertex targets

A target stores the source feature and the semantic face, edge, or vertex name. It does not store raw OCCT shape IDs, BRep handles, sub-shape indices, or cache references.

The recovery model has three first records:

- `ReferenceStatusRecord`
- `ReferenceRemapRecord`
- `SketchOriginOverrideRecord`

`ReferenceStatusRecord` stores whether a semantic target is currently `resolved` or `lost`. A lost record carries a message explaining why the target is unavailable.

`ReferenceRemapRecord` stores explicit user/model intent for remapping one semantic target to another valid semantic target of the same kind. The core does not silently remap a lost face, edge, or vertex by itself.

`SketchOriginOverrideRecord` stores an optional 2D local origin override for a sketch. If no override exists, a sketch still uses the origin of its resolved workplane frame.

## Reference evaluator

`ReferenceRecoveryEvaluator` currently evaluates a semantic target against the implemented rectangular additive-extrude seed. A target is reported as resolved when its source feature exists and is an additive extrude. If the source feature is missing or no longer supported by the implemented generated-topology seed, the evaluator returns a lost reference status.

The current lost-reference message is:

```text
semantic reference source is not available in the current generated topology
```

This is intentionally conservative. The kernel reports a lost reference; it does not guess a replacement.

## Sketch origin behavior

A sketch's default local origin remains the origin of its resolved workplane frame.

`WorkplaneResolver::resolve` returns the raw workplane frame for datum planes, construction planes, and derived generated-face workplanes.

`WorkplaneResolver::resolve_for_sketch` applies the optional `SketchOriginOverrideRecord` for that sketch. The override is interpreted in the resolved workplane's local coordinates. The resolved sketch frame origin becomes:

```text
origin = resolved_workplane.origin
       + override.x * resolved_workplane.x_axis
       + override.y * resolved_workplane.y_axis
```

If the resolved workplane has rectangular bounds, the local bounds center is shifted by the negative override. This keeps the bounded valid area correct in the sketch's shifted local coordinate system.

`SketchReferenceProjector` and `GeometryRecomputeExecutor` now use `resolve_for_sketch`, so projected references and feature recompute respect the same sketch-origin override semantics.

## Dimension-change behavior

Generated-face workplanes are still reconstructed from source feature parameters. A top-face sketch on a rectangular additive extrude moves in Z when the extrusion depth changes. Side-face workplanes move according to width, height, and depth changes.

The recovery tests explicitly check that a top-face workplane origin moves from `z = 10` to `z = 25` when the source extrusion depth parameter changes from 10 mm to 25 mm.

## JSON format

Reference statuses are stored at document level:

```json
{
  "reference_statuses": [
    {
      "id": "status.old_top_face",
      "status": "lost",
      "target": {
        "kind": "face",
        "source_feature": "feature.old",
        "face": "top"
      },
      "message": "old source feature was removed before this model was saved"
    }
  ]
}
```

Reference remaps are stored explicitly:

```json
{
  "reference_remaps": [
    {
      "id": "remap.old_top_to_base_top",
      "original": {
        "kind": "face",
        "source_feature": "feature.old",
        "face": "top"
      },
      "replacement": {
        "kind": "face",
        "source_feature": "feature.base",
        "face": "top"
      },
      "reason": "manual replacement selected by the user"
    }
  ]
}
```

Sketch origin overrides are stored at document level:

```json
{
  "sketch_origin_overrides": [
    {
      "sketch": "sketch.top_references",
      "local_origin": {"x": 0.0, "y": 0.0}
    }
  ]
}
```

Supported semantic target kinds are:

```text
face
edge
vertex
```

Supported reference status values are:

```text
resolved
lost
```

The checked-in example `examples/projected_sketch_references.blcad.json` includes reference status metadata, an explicit remap record, and a sketch-origin override record.

## Dependency behavior

Reference status records and remap records are stored in `PartDocument` and participate in dependency graph state where the referenced source feature is available.

A lost record may reference a missing source feature. This is allowed by design so a document can preserve broken model intent and present it for later repair instead of dropping the reference.

A remap replacement target must validate against the current document. The original target may be missing.

## Tests

The implemented test coverage includes:

- core tests for resolved and lost semantic reference evaluation
- core tests for storing reference status records, remap records, and sketch origin overrides in `PartDocument`
- JSON roundtrip tests for status, remap, and origin-override metadata
- geometry tests proving a generated-face workplane follows source feature dimensions
- geometry tests proving `resolve_for_sketch` shifts the resolved frame according to a sketch-origin override
- geometry tests proving missing generated sources are reported as lost instead of silently remapped

## Deliberate limitations

The initial seed does not implement a GUI remapping dialog. The replacement-shell consumer below
now provides explicit per-consumer rebinding within the documented bounded scope.

It does not implement automatic topology matching after arbitrary feature edits. The current evaluator only knows whether the source feature is available in the implemented rectangular additive-extrude semantic topology seed.

It does not rewrite sketches or projected references through a remap record automatically. Remap records are explicit model intent for later application by a dedicated repair operation.

It does not add a full sketch solver, automatic region detection, or arbitrary generated topology evaluation.


## Replacement-shell repair consumer — 2026-10-23

The application now exposes explicit same-ID rebinding for projected Sketch points/lines and
generated-face workplanes. It diagnoses with Geometry resolvers, previews a recomputed Part copy,
and publishes one undoable session transaction. `Sketch::replace_reference` requires an existing
projected entity of the same kind; `PartDocument::update_sketch` validates the new source and rejects
cycles while preserving downstream edges. Persisted source fields change only for the selected
consumer. Stored `ReferenceRemapRecord` entries remain explicit historical intent, without automatic
global rewriting. Exact UI, tests and remaining load/recompute/batch limitations live in
`docs/gui-sketch-workbench-mvp7.md` under “Replacement-shell explicit reference repair”.


The shell repair task also supports an ordered transient list of replacements. Each stage validates
Core intent without requiring a complete scene; one final preview/recompute and transaction publish
the whole batch. This allows several lost references to be repaired together while preserving the
existing status/remap semantics. Each intermediate Core update must remain valid. Core-invalid missing-source
loading remains planned; see the same GUI contract and `docs/mvp-plan.md`.


## Repair-time loading and history — 2026-10-23

**3D-Modell → Dokument → Zur Reparatur öffnen** explicitly opens a Part for reference repair. The
session API is `open_file(path, discard_dirty, GuiDocumentOpenMode::ReferenceRepair)`. A Core-valid
Part may be retained when initial complete Geometry recompute fails. Normal Open remains strict.
Project files, malformed intent, missing Core sources and invalid constraint/dimension sidecars are
rejected before session replacement in either applicable path. A repair-open request whose full
recompute succeeds produces a normal fresh Part and can still open the repair task.

A failed repair-open Part retains authored intent, its file path, the saved JSON/catalog boundary
and the actual Geometry diagnostic. `reference_repair_mode()` is true, recompute status is Failed,
and the authoritative `part_shape_cache()` is absent. No partially computed cache is published.
The shell marks the title `[Reparatur]`, clears old derived presentation and opens the repair task.
Ordinary Part/constraint/dimension/Sketch transactions, Sketch entry and normal modeling/export
controls are blocked until complete recompute succeeds. A freshness setter cannot promote this
state to fresh. Opening/creating another document resets the repair state normally.

For repair target selection only, the shell builds a disposable cache of individually successful
additive-extrude producers. Each attempt uses a cache copy and is retained only on success. Typed
face/edge/vertex candidates still use the existing semantic resolver contract; no final-topology
ordinals or nearest-shape matching are introduced. The local cache is rebuilt against staged intent
when needed and never becomes session Geometry. Invalid or unavailable producers remain omitted.
Final Preview and `commit_part_repair_transaction` both require a fresh complete candidate recompute,
including all downstream features. Successful publication clears repair mode and installs its cache.
Opening for repair does not guarantee that unrelated Geometry failures can be fixed through the
currently supported reference editors. Failed repair transactions preserve the previous intent,
cache and status.

History entries admitted through failed repair-open carry an in-memory permission to restore that
failed snapshot. Undo computes the target on a disposable state and restores original intent with
Failed status and no cache when recompute still fails. Redo computes and publishes the repaired
snapshot normally. Other history targets require successful recompute. Failed history attempts do
not replace the current document or its valid cache/status. This adds no persistent JSON fields.

Save during staging/Preview writes committed intent and expires the task; it can therefore save an
unchanged failed Part without serializing the pending repair. Dirty state continues to compare
canonical JSON and both sidecar catalogs. Saving a repaired state and undoing to the failed original
makes it dirty; Redo returns to the saved repaired state. A saved failed copy requires explicit repair
opening until a complete repair has been published.

Proofs: three `[repair-open-session]` cases cover admission, strict source validation, transaction
and freshness guards, failed/complete repair, canonical saving, saved/dirty boundaries and failed
history/Redo. Three `[block136-repair-open]` shell cases cover cancellation, preview saves, protected
modeling contexts and real file-dialog/ribbon/popup/list/button/camera/history input with native
presentation. Structurally rejected missing-source documents remain the next separate Core audit;
constraint-diagnostic suggestion controls remain open as recorded in `docs/mvp-plan.md`.
