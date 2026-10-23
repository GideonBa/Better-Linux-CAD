# Projected Sketch Reference Geometry

This document describes the implemented seed for sketch-local projected reference geometry and the first reference-driven sketch constraint path.

The goal is to let a sketch consume stable model-intent references to already defined construction geometry and semantic generated geometry without storing raw OCCT topology IDs and without introducing a full sketch constraint solver yet.

## Sketch origin and workplane placement

A sketch does not currently store an independent origin override. Its local origin is the origin of its resolved workplane frame.

For `datum.xy`, that origin is the stored datum-plane origin.

For a sketch placed on a generated face, the origin is reconstructed by `WorkplaneResolver` from semantic model intent. In the current rectangular additive-extrude seed, the top face origin is at the rectangle center translated to the extrusion depth, side-face origins are placed at the center of the corresponding generated side face, and their axes are deterministic face-local axes.

Because generated-face workplanes are resolved from source feature parameters, the sketch frame moves when the dimensions of the source body change. For example, a top-face sketch on `feature.base.top` moves in Z when the extrusion depth parameter changes. Side-face workplanes move when width, height, or depth changes according to the corresponding semantic face frame.

If a generated reference no longer resolves, the replacement shell offers explicit rebinding for
loadable Parts within the bounds documented in `docs/gui-sketch-workbench-mvp7.md`. The user selects
a replacement and checks a disposable recompute preview before applying it. Loading documents
already rejected by source validation/recompute and repairing multiple interdependent references
remain planned reference-robustness work.

## Implemented scope

The core sketch model has two projected reference-entity types:

- `ProjectedSketchPoint`
- `ProjectedSketchLine`

Both use `SketchEntityId` as sketch-local reference entity IDs. This keeps projected reference entities in the same sketch-local identity namespace as regular sketch entities while keeping them separate from profile-owning `LineSegment` geometry.

`ProjectedSketchPoint` can reference:

- a `ConstructionPointId`
- a `SemanticVertexReference`

`ProjectedSketchLine` can reference:

- a `ConstructionLineId`
- a `SemanticEdgeReference`

These references are model intent. They do not store raw OCCT faces, edges, vertices, or shape sub-indexes.

## Reference targets and constraints

The core model now has `SketchReferenceTarget` handles for:

- `line_segment`
- `line_segment_start`
- `line_segment_end`
- `projected_point`
- `projected_line`

The first reference-driven sketch constraints are:

- `coincident_to_projected_point`
- `parallel_to_projected_line`
- `collinear_with_projected_line`

These are intentionally small deterministic constraints, not a full sketch solver. They are enough to represent that a helper line endpoint should sit on a projected point, or that a helper line should be parallel or collinear with a projected line.

The optional geometry layer contains `ReferenceDrivenSketchHelper`, which can resolve the first constraint records against projected reference geometry. It can also create a deterministic profile-helper `LineSegment` from two `coincident_to_projected_point` endpoint constraints when both projected endpoints are fully resolvable.

## JSON format

Sketch JSON supports optional `projected_points`, `projected_lines`, and `constraints` arrays.

A projected construction point reference has the form:

```json
{
  "id": "ref.point.top_front_mid",
  "source": "construction_point",
  "construction_point": "point.top_front_mid"
}
```

A projected semantic vertex reference has the form:

```json
{
  "id": "ref.vertex.top_front_right",
  "source": "semantic_vertex",
  "semantic_vertex": {
    "source_feature": "feature.base",
    "vertex": "top_front_right"
  }
}
```

A projected construction line reference has the form:

```json
{
  "id": "ref.line.top_front_axis",
  "source": "construction_line",
  "construction_line": "line.top_front_axis"
}
```

A projected semantic edge reference has the form:

```json
{
  "id": "ref.edge.top_front",
  "source": "semantic_edge",
  "semantic_edge": {
    "source_feature": "feature.base",
    "edge": "top_front"
  }
}
```

A reference-driven constraint has the form:

```json
{
  "id": "constraint.helper_start_on_mid",
  "kind": "coincident_to_projected_point",
  "constrained": {"kind": "line_segment_start", "entity": "line.helper"},
  "reference": {"kind": "projected_point", "entity": "ref.point.top_front_mid"}
}
```

The checked-in example `examples/projected_sketch_references.blcad.json` shows projected references and the first reference-driven constraints in one model.

## Geometry projection

The optional geometry layer contains `SketchReferenceProjector`.

It resolves the sketch workplane through `WorkplaneResolver`, then resolves the referenced source through the appropriate resolver:

- construction point references use `ConstructionPointResolver`
- construction line references use `ConstructionLineResolver`
- semantic vertex references use `SemanticReferenceEvaluator::resolve_vertex`
- semantic edge references use `SemanticReferenceEvaluator::resolve_edge`

A resolved 3D point is projected into the sketch-local workplane frame as a 2D point.

A resolved 3D line is orthogonally projected as a local point plus a normalized 2D direction. Its anchor may lie off the sketch plane and its direction may be oblique to it. A direction perpendicular to the sketch plane is rejected because its projection collapses to a point.

A semantic generated edge uses its evaluated start point as the projected local anchor and its edge vector as the projected direction seed. Source references remain associative. Semantic edges retain finite projected endpoints for drawing and endpoint snapping; construction-line references remain axes. Committed projected references have native viewport presentations and stay visible after the preview is dismissed.

## Dependency and invalidation behavior

Projected references now participate in the dependency graph at the sketch level.

For construction-backed projected references, the dependency is:

```text
construction point/line -> sketch -> dependent feature
```

For semantic generated references, the dependency is:

```text
source feature -> semantic edge/vertex node -> sketch -> dependent feature
```

Relation-driven construction points also add dependencies from their relation sources, so a projected construction point that itself depends on a generated edge is invalidated when the source feature changes.

## Orthographic projection

Projection drops the component normal to the target workplane. Off-plane vertices and edges are valid sources. Nonfinite coordinates and directions whose plane projection collapses are rejected.

The shell source chooser also offers lines, arcs, circles and rational cubic splines from other sketches. These are projected as finite, editable snapshots, explicitly marked as unlinked in the editor. Arcs and circles use exact rational cubic spans, including ellipses when projected obliquely. Existing construction and semantic model references keep their source identity and dependency behavior.

## Tests

The implemented test coverage includes:

- core storage and source-type tests for projected point and line references
- core storage and validation tests for sketch reference targets and reference-driven constraints
- JSON roundtrip tests for construction-point, construction-line, semantic-vertex, semantic-edge, and constraint records
- dependency graph tests proving source feature and construction-relation changes invalidate projected-reference sketches and dependent features
- geometry projection tests for semantic generated vertices and edges on a top-face sketch
- geometry projection tests for construction points and construction lines on a top-face sketch, including orthographic off-plane projection
- geometry tests for resolving coincident and parallel projected-reference constraints
- geometry tests for deterministic profile-helper line generation from two projected point endpoint constraints
- collapsed-direction rejection tests for projection normal to the sketch plane

## Deliberate limitations

This block does not implement a full sketch solver. Reference-driven constraints are stored and deterministically evaluated only for the initial projected-point/projected-line cases.

The currently implemented semantic generated edge and vertex evaluation still supports only the controlled rectangular additive extrude topology. Arbitrary generated topology remains deferred.

Projected line references remain helper geometry for profile inference. Semantic edges also carry
finite derived endpoints for display and snapping. The helper path can produce a deterministic
profile `LineSegment` only when two projected endpoint constraints are available and fully resolved.

The replacement shell supports explicit same-ID point/line and generated-face-workplane rebinding
for loadable Parts. See `docs/gui-sketch-workbench-mvp7.md` for preview, transaction and remaining
recovery limits. Automatic reassignment after arbitrary topology changes remains unsupported.

GUI manipulators, live selection, associative dimensions, automatic region detection, and a general solver remain deferred.
