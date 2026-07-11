# Semantic References

Status: partially implemented. The generated-face family is proven by the MVP-2 workplane path and the MVP-5 read-only assembly target resolver; the full reference system remains future work.

This document is the canonical description of how BLCAD refers to geometry. It is referenced by the sketch, feature, hole-wizard, fillet/chamfer, pattern/mirror, and assembly documents so the rule is stated once.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or on kernel IDs such as `Face17` or `Edge7`. Kernel IDs change after feature edits and make models unstable. This is the classic topological-naming problem.

Every persistent reference to geometry must instead be semantic: it names the constructive meaning of a face, edge, axis, or point.

```text
Bad:
  Sketch starts on Face_12
  Hole direction uses Edge_7
  Chamfer applies to Edge_3

Good:
  Sketch starts on BasePlate.top_mounting_face
  Hole direction uses DatumAxis.main_bolt_axis
  Chamfer applies to BasePlate.outer_top_edges
```

## Reference objects

The system owns its own reference objects. A reference describes what a face/edge/axis means, not its incidental kernel index.

- `SemanticFaceReference` — a named generated face (for example `feature.base_extrude.top`).
- edge references / named edge groups (for example `outer_top_edges`, `outer_vertical_edges`).
- axis references (for example a hole axis, a shaft main axis).
- point/vertex references.

Features that create geometry should expose the semantic references they produce, so downstream features and constraints can bind to them:

```text
HoleFeature_M6_01
  exposed_references:
    hole_axis
    entry_face
    bottom_face
```

## Current implemented scope

The generated-face seed proves the principle for a simple `AdditiveExtrude`:

- `SemanticFaceReference` resolves generated faces such as `feature.base_extrude.{top,bottom,right,left,front,back}`.
- `DerivedWorkplane` exposes those faces as sketch workplanes.
- `WorkplaneResolver` maps them to concrete frames.
- `WorkplaneResolver::resolve_generated_face` exposes the same generated-face frame path directly to other geometry-layer consumers.
- `AssemblyConstraintTargetResolver` parses the supported generated-face semantic target family and resolves it through component occurrence and project-owned part identity to a component-local planar descriptor.
- The assembly target resolver keeps the component `RigidTransform` separate and does not silently invent assembly-space rotation semantics.
- No raw OCCT face IDs are stored in `PartDocument` or assembly constraint model intent.

See `docs/derived-workplane-mvp2-seed.md`, `docs/workplane-resolver-mvp2.md`, and `docs/assembly-constraint-target-resolution-mvp5.md`.

## Future scope

- named edge groups produced by features (needed by fillet/chamfer, see `docs/fillet-chamfer-features.md`).
- axis references produced by hole features (needed by assembly `Concentric`/`Insert`, see `docs/assembly-system.md`).
- references to generated edges, vertices, and later analytic surfaces, still without storing raw OCCT topology in the core.
- a broader resolution mechanism that re-binds semantic references to correct kernel entities after every recompute and reports a clear error if a reference can no longer be resolved.

## Dependency-graph integration

A semantic reference creates a dependency edge from the object that produces the geometry to the object that consumes it:

```text
source feature -> exposed reference -> consuming feature/constraint
```

When the source geometry changes, the reference is re-resolved. If it cannot be resolved, dependents are marked invalid with a clear error rather than silently binding to the wrong entity.

The current assembly target resolver is read-only and does not yet add assembly solve dependencies. That integration belongs to the later assembly solve pipeline.

## Out of scope for the first versions

- a full topological naming system.
- automatic repair of lost reference chains.
- raw OCCT handles anywhere in the core model.
