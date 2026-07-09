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

This block does not implement a GUI remapping dialog.

It does not implement automatic topology matching after arbitrary feature edits. The current evaluator only knows whether the source feature is available in the implemented rectangular additive-extrude semantic topology seed.

It does not rewrite sketches or projected references through a remap record automatically. Remap records are explicit model intent for later application by a dedicated repair operation.

It does not add a full sketch solver, automatic region detection, or arbitrary generated topology evaluation.
