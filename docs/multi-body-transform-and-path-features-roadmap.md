# Multi-body transforms and path-feature roadmap

Status: target architecture. Multi-body/result/Boolean/transform foundations are implemented
through Block 57; path features and the remaining richer feature families stay planned.

This document records future model intent for multi-body part files, body transforms, boolean operations between bodies, path-following extrudes, path-following lofts, and associative sketch/body transform behavior. It is written as an AI-readable implementation roadmap.

## Goal

A future `PartDocument` should be able to contain several solid bodies, not only one final body.

The model should support:

- multiple independent and dependent solid bodies inside one part file
- body-level translation, rotation, and scaling operations
- boolean add / subtract / intersect between bodies
- extrude and extruded-cut features that can optionally follow a path curve
- loft and loft-cut features through two or more profile sketches
- loft features whose sections may live on arbitrarily oriented sketch planes
- path curves made from connected line, arc, spline, projected, or 3D-sketch segments
- associative behavior where transforming a body can also transform the sketches and construction references that belong to that body

The core rule remains unchanged: the file stores parametric model intent; OCCT shapes are recomputable cache data.

## Terminology

Use these terms consistently in future implementation:

```text
PartDocument
  owns one or more Body records

Body
  semantic solid/surface body identity inside one part file
  owns or references the features that generate it
  may have a BodyTransformStack

BodyId
  stable identifier for one body in a part document

BodyTransform
  model-intent transform applied to a body and optionally to its owning sketches/references

BodyBooleanFeature
  feature that combines source bodies into a target/new body

PathCurve
  ordered connected curve chain used by path-following features

ProfileSection
  one selected sketch profile used by loft/sweep/path-extrude features
```

## Multi-body PartDocument model

Block 48 implements the first body collection boundary. The fields after `visibility` remain target
architecture for later blocks:

```text
PartDocument
  bodies[]
    BodyId
    name
    body_kind = solid | surface
    visibility

    # planned extensions
    source_features[]
    transform_stack[]
    material_override
    cached_shape_key
```

`construction_only` is not a Body kind in the numbered Part Construction contract; construction
geometry remains separate semantic model intent.

Feature outputs should explicitly target bodies:

```text
Feature
  operation_mode = new_body | join | cut | intersect
  target_body = optional BodyId
  tool_bodies[] = optional BodyId list
  produced_body = optional BodyId
```

Rules:

- `new_body` creates a new `Body` identity.
- `join` adds generated volume to `target_body`.
- `cut` subtracts generated volume from `target_body`.
- `intersect` keeps the intersection with `target_body`.
- Boolean operations between existing bodies should be explicit `BodyBooleanFeature` records.
- Deleting a body should not silently delete sketches; ownership and references must be explicit.

## Body transforms

Future body transforms should be first-class model intent:

```text
BodyTransform
  id
  body
  kind = translate | rotate | scale | mirror | transform_matrix
  coordinate_space = world | body_local | sketch_local | construction_reference
  parameters
  apply_to_owned_sketches = true | false
  apply_to_owned_construction_geometry = true | false
```

Required transform kinds:

- translation by vector or by parameterized x/y/z distances
- rotation about datum axis, construction line, semantic edge, or explicit axis
- uniform scale
- non-uniform scale only if intentionally supported and clearly marked because it can invalidate dimensions
- optional matrix transform as an internal representation, not as the preferred user-facing intent

Rules:

- Transforms must participate in the dependency graph.
- Transform order is significant and should be preserved in a `BodyTransformStack`.
- A transform should affect the body cache only through recompute.
- Transform features should not rewrite the original feature definitions unless explicitly requested by a repair/apply command.

## Associative transform behavior for sketches

When a body is created from one or more sketches, those sketches should be able to remain associated with that body.

Future model concept:

```text
SketchOwnership
  sketch
  owning_body
  association = drives_body | consumed_by_body | reference_only
```

When a transform is applied to a body:

- if `apply_to_owned_sketches = true`, the sketch workplane frame should be transformed together with the body
- sketch-local 2D coordinates should remain unchanged when the workplane frame is transformed
- construction geometry owned by the body should transform when `apply_to_owned_construction_geometry = true`
- projected references should remain semantic references and be re-resolved after transform
- sketches not owned by the transformed body must not move silently

This behavior is required for workflows where a user creates a body from sketches and then moves, rotates, or scales that body while expecting the defining sketches to follow it.

## Boolean operations between bodies

Multi-body boolean features should support:

```text
BodyBooleanFeature
  id
  operation = add | subtract | intersect
  target_body
  tool_bodies[]
  result_mode = modify_target | new_body
  keep_tool_bodies = true | false
```

Rules:

- Add/union combines one target body with one or more tool bodies.
- Subtract removes the tool bodies from the target body.
- Intersect keeps only overlapping volume.
- Tool bodies may optionally remain visible or be consumed, but the choice must be explicit.
- Boolean features must keep semantic body references, not raw OCCT shape references.

## Path curves

Path-following features need a connected path model.

Target model:

```text
PathCurve
  id
  segments[]
    PathSegmentReference
  closure = open | closed
  continuity_hint = C0 | G1 | G2 optional
```

Segment references may point to:

- explicit sketch line segments
- explicit sketch arc segments
- explicit sketch spline segments
- reference-generated helper lines
- projected sketch lines
- construction lines
- 3D sketch line/polyline/spline segments
- semantic generated edges in later stages

Validation rules:

- segments must be ordered and connected within tolerance
- branch points are rejected in the first implementation
- self-intersecting paths may be rejected initially
- the path coordinate frame / orientation rule must be explicit for sweep-like behavior
- path curves should be reusable by extrude, sweep, loft, and surface features

## Path-following extrude and extruded cut

Future extrude features should support both straight and path-following modes.

Target model:

```text
AdditiveExtrude
  input_profile
  direction_mode = sketch_normal | opposite_sketch_normal | vector | path
  path_curve = optional PathCurveId
  operation_mode = new_body | join

SubtractiveExtrude
  input_profile
  target_body
  direction_mode = sketch_normal | opposite_sketch_normal | vector | path
  path_curve = optional PathCurveId
  operation_mode = cut
```

Rules:

- Straight extrude remains the fast path.
- Path-following extrude uses a connected path curve instead of a single normal direction.
- Path-following cut removes the swept/extruded volume from the target body.
- The profile plane may be arbitrary, but the feature must define how the profile frame is oriented along the path.
- Early implementation may start with a planar path or a single 3D polyline path and explicit error messages for unsupported twist cases.

This overlaps with sweep. The intended distinction is:

```text
ExtrudeAlongPath = extrude semantics with a profile and path extension option
SweepFeature     = richer dedicated path-sweep feature with orientation/twist/rail controls
```

If implementation later decides these should share the same internal geometry engine, the model intent should still keep user-facing feature types clear.

## Loft through two or more sketches

A future loft feature should support two or more profile sections.

Target model:

```text
LoftFeature
  sections[]
    ProfileSectionReference
  guide_curves[] optional
  continuity = C0 | G1 | G2 optional
  operation_mode = new_body | join | cut | intersect
  target_body optional
```

Required behavior:

- support two profile sketches
- support three or more profile sketches
- profile sketches may be on arbitrarily oriented planes
- section ordering must be explicit and stable
- section matching / vertex correspondence should be deterministic
- guide curves may be added later to control flow between sections
- loft cut should remove the lofted volume from a target body

For two or more sketches on arbitrary planes, the feature must resolve every section into its own workplane frame and then map local profile geometry into model space before lofting.

## Loft following a path

Loft features should optionally accept a path or guide curve chain:

```text
LoftFeature
  sections[]
  path_curve optional
  guide_curves[] optional
```

Use cases:

- pipe-to-pipe transitions that bend along a route
- duct transitions with a centerline path
- blade/wings where sections follow a span curve
- organic fairings controlled by one or more rails

The path must be connected and semantic. It must not be stored as raw OCCT edges.

## Arbitrarily oriented sketch planes

Future path and loft features must not assume parallel sketch planes.

Rules:

- every sketch section resolves through `WorkplaneResolver` or the future 3D sketch system
- each profile section stores local sketch coordinates
- loft/sweep/path-extrude geometry maps every section into model space before OCCT operations
- orientation matching between sections must be explicit enough to avoid random twisting
- section start point, seam, or alignment reference should be part of model intent for closed profiles

Possible future model field:

```text
ProfileSectionReference
  sketch
  profile
  alignment_reference = optional SketchEntityId | SketchPoint | SemanticReference
  flip_normal = true | false
  rotation_offset = optional angle parameter
```

## Dependency graph integration

The dependency graph must include:

- body depends on source features
- transform depends on body and transform parameters
- transformed body depends on transform stack order
- boolean feature depends on target body and tool bodies
- path-following extrude depends on profile sketch, path curve, body target, and parameters
- loft depends on all section sketches, profile references, guide curves, path curves, body target, and parameters
- sketch ownership edges from body to owned sketch transform behavior

No body operation should bypass the dependency graph.

## File-format target

Future JSON/project files should persist model intent similar to:

```json
{
  "bodies": [
    {"id": "body.base", "name": "Base", "kind": "solid"},
    {"id": "body.tool", "name": "Tool", "kind": "solid"}
  ],
  "body_transforms": [
    {
      "id": "transform.move_tool",
      "body": "body.tool",
      "kind": "translate",
      "vector": {"x": 10.0, "y": 0.0, "z": 0.0},
      "apply_to_owned_sketches": true,
      "apply_to_owned_construction_geometry": true
    }
  ],
  "body_booleans": [
    {
      "id": "boolean.subtract_tool",
      "operation": "subtract",
      "target_body": "body.base",
      "tool_bodies": ["body.tool"],
      "result_mode": "modify_target",
      "keep_tool_bodies": false
    }
  ]
}
```

These records are future target architecture. They are not part of the current `.blcad.json` implementation.

## Proposed implementation sequence

1. **Implemented in Block 48:** add `BodyId` and the first immutable `Body` records to `PartDocument` without changing geometry behavior. The seed covers id, name, Solid/Surface kind, visibility, canonical ordering, and ownership; feature outputs, transforms, material/cache metadata, and JSON remain later steps.
2. **Implemented through Block 51:** add feature operation mode plus target/effective-result Body
   intent, compatible persistence, producer/consumer graph edges, invalidation, and cycle/removal
   behavior.
3. Add `ShapeCache` support for multiple body shapes instead of only a single final shape.
4. Add JSON roundtrip tests for body identity and feature output body intent.
5. Add `BodyTransform` records and transform-stack ordering.
6. Add geometry-layer transform application for translation, rotation, and uniform scaling.
7. Add sketch ownership records and tests for body transforms moving owned sketch workplanes.
8. Add body boolean features for add/subtract/intersect.
9. Add path curve records for connected 2D/3D curve chains.
10. Add path-following extrude and extruded-cut feature records.
11. Add loft through two sketches on arbitrary planes.
12. Add multi-section loft through three or more sketches.
13. Add path/guide-curve loft support.
14. Add STEP export of multi-body parts with deterministic body naming where supported.

Feature Body-operation persistence, body-scoped recompute/inspection, Body Booleans,
BodyTransform/SketchOwnership intent plus associative Geometry execution, reusable semantic inputs,
and richer Extrude/Cut intent/JSON plus Geometry, persistent plus executed Revolve/RevolveCut, and
general Pattern Core intent plus Linear and Circular Pattern Geometry, and persistent MirrorFeature
intent and Edge Treatment Core/JSON are implemented through Block 68. The current handoff is Block
69 Fillet Geometry.
Canonical implemented contracts include `docs/part-body-identity-mvp6.md`,
`docs/part-body-json-mvp6.md`,
`docs/part-feature-body-operation-mvp6.md`, `docs/part-feature-body-dependency-mvp6.md`, and
`docs/part-multi-body-recompute-mvp6.md`, `docs/part-multi-body-inspection-mvp6.md`,
`docs/part-body-boolean-mvp6.md`, `docs/part-body-boolean-geometry-mvp6.md`, and
`docs/part-body-transform-ownership-mvp6.md`, and
`docs/part-body-transform-geometry-mvp6.md`, `docs/part-feature-input-reference-mvp6.md`, and
`docs/part-extrude-extent-intent-mvp6.md`, and
`docs/part-extrude-extent-geometry-mvp6.md`, and
`docs/part-revolve-intent-mvp6.md`, and
`docs/part-revolve-geometry-mvp6.md`, and
`docs/part-pattern-core-mvp6.md`, and
`docs/part-linear-pattern-geometry-mvp6.md`, and
`docs/part-circular-pattern-geometry-mvp6.md`, and
`docs/part-mirror-intent-mvp6.md`; the
numbered Part Construction sequence is authoritative where this older roadmap groups work
differently.

GUI Feature Validation is deliberately sequenced first in
`docs/gui-feature-validation-sequence-mvp7.md`. STEP import then follows in
`docs/step-import-sequence-mvp9.md`: Blocks 122–128 reuse this roadmap's body and feature-result
authority for Reference and EditableBody import modes instead of adding foreign BRep authority
prematurely.

## First acceptance tests

The first implementation should eventually prove:

- one `PartDocument` can hold two independent solid bodies
- a body can be translated, rotated, and uniformly scaled through model intent
- transforming a body can also transform the workplane frames of sketches owned by that body
- two bodies can be joined into one body
- one body can be subtracted from another body
- a path curve made of multiple connected segments validates as one path
- an extrude can follow a connected path curve
- an extruded cut can follow a connected path curve
- a loft can create a body between two profile sketches on non-parallel planes
- a loft can create a body through three or more profile sketches
- a loft can follow a path or guide curve while preserving explicit section ordering
- all records survive JSON roundtrip as model intent

## Deliberate limitations for the first version

Do not implement all of this at once.

Early versions may defer:

- non-uniform scale
- arbitrary transform matrices as user-facing operations
- automatic sketch ownership inference
- automatic body merge detection
- body-level material and appearance
- multi-body STEP naming details
- path twist minimization
- rail/guide-curve lofts
- full surface continuity controls
- automatic section correspondence for complex closed profiles
- assembly-level component transforms

Assembly transforms are related but separate: assemblies place component instances, while multi-body transforms operate on bodies inside one part document.
