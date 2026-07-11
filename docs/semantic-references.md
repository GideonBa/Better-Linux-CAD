# Semantic References

Status: partially implemented. The generated-face family is proven by the MVP-2 workplane path and the MVP-5 assembly target/residual path; the broader reference system remains future work.

This document defines how BLCAD refers to geometry. Sketch, feature, hole-wizard, fillet/chamfer, pattern/mirror, and assembly documents share this rule.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or kernel labels such as `Face17` or `Edge7`. Those identifiers can change after feature edits and create the topological-naming problem.

Every persistent geometry reference must instead be semantic: it names constructive meaning rather than incidental kernel position.

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

BLCAD owns its reference objects. A reference describes what a face, edge, axis, or point means.

- `SemanticFaceReference` — named generated face such as `feature.base_extrude.top`
- edge references / named edge groups such as `outer_top_edges`
- axis references such as a hole axis or shaft main axis
- point/vertex references

Features that create geometry should expose semantic references for downstream consumers:

```text
HoleFeature_M6_01
  exposed_references:
    hole_axis
    entry_face
    bottom_face
```

## Current implemented scope

The generated-face seed proves the principle for a supported `AdditiveExtrude`:

- `SemanticFaceReference` identifies `feature.<feature-id>.{top,bottom,right,left,front,back}`.
- `DerivedWorkplane` exposes supported semantic faces as sketch workplanes.
- `WorkplaneResolver` maps those faces to deterministic local frames.
- `WorkplaneResolver::resolve_generated_face` exposes one shared generated-face frame path.
- `AssemblyConstraintTargetResolver` parses supported persistent assembly target tokens and resolves component/project/part ownership to a component-local planar descriptor.
- `AssemblyTransformEvaluator` maps those local descriptors into assembly coordinates using the explicit persisted `RigidTransform` convention.
- `AssemblyConstraintEquationBuilder` consumes active Mate/Distance records whose targets use the generated planar face family and constructs canonical residual descriptors.
- No raw OCCT face ids are stored in `PartDocument` or assembly constraint model intent.

The current assembly path is:

```text
semantic target token
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residual descriptor
```

Target resolution itself still keeps `RigidTransform` separate. Transform interpretation and residual semantics belong to their own downstream layers.

See:

- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`

## Future scope

- named edge groups produced by features for fillet/chamfer
- semantic axis references produced by hole and shaft features for Concentric/Insert
- generated edge and vertex assembly target families
- analytic surface references where needed
- broader reference rebinding after recompute with explicit lost-reference diagnostics

Semantic axis references are the current blocker for Concentric residual construction.

## Dependency and solve integration

A semantic reference creates a conceptual dependency from geometry producer to consumer:

```text
source feature -> exposed reference -> consuming feature/constraint
```

When source geometry changes, the reference must be re-resolved. A failed semantic resolution should invalidate the consumer rather than silently bind to another kernel entity.

The current assembly target/residual path is read-only and does not yet create a solve dependency graph.

The first rigid-body solver seed should consume current semantic resolution and residual construction without moving persistent reference ownership into the solver.

## Out of scope for the first versions

- a complete general topological naming system
- automatic repair of every lost reference chain
- raw OCCT handles as core model references
