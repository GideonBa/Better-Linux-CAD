# Semantic References

Status: partially implemented. Generated planar faces are proven by the MVP-2 workplane path and the MVP-5 assembly target/residual/solver path; a complete general reference system remains future work.

This document is the canonical rule for persistent geometry references shared by sketch, feature, construction, assembly, and future engineering layers.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or transient kernel ids such as `Face17` or `Edge7`.

Persistent references must describe constructive meaning rather than incidental topology numbering.

```text
Bad:
  Sketch starts on Face_12
  Constraint targets Face_17
  Chamfer applies to Edge_3

Good:
  Sketch starts on feature.base_extrude.top
  Assembly target is feature.mount_plate.bottom
  Chamfer targets outer_top_edges
```

## Reference objects

BLCAD owns its own semantic reference records.

Current and planned families include:

- `SemanticFaceReference`
- semantic edge references and named edge groups
- semantic axis references
- semantic point/vertex references
- future analytic surface references where needed

Features that create geometry should expose stable semantic meaning for downstream consumers.

Example target architecture:

```text
HoleFeature_M6_01
  exposed_references
    hole_axis
    entry_face
    bottom_face
```

## Current implemented generated-face scope

The implemented `AdditiveExtrude` generated-face family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The implemented path is:

```text
SemanticFaceReference
  -> WorkplaneResolver::resolve_generated_face
  -> deterministic generated-face frame
```

`DerivedWorkplane` uses that path for sketch placement.

`AssemblyConstraintTargetResolver` parses supported persistent assembly target tokens and resolves component/project/part ownership to a component-local planar descriptor.

`AssemblyTransformEvaluator` maps the local descriptor into assembly coordinates using the exact persisted `RigidTransform` convention.

`AssemblyConstraintEquationBuilder` consumes active Mate/Distance records and creates canonical planar residual descriptors.

`AssemblyRigidBodySolver` repeatedly consumes those residuals on private project copies while evaluating current and finite-difference candidate transforms.

The full implemented assembly path is therefore:

```text
semantic target token
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residual descriptor
  -> AssemblyRigidBodySolver
  -> proposed component transforms
  -> explicit fresh-result application
```

No raw OCCT face id is stored in `PartDocument` or assembly constraint model intent.

Target resolution keeps transform interpretation, residual semantics, and solver policy in their separate downstream layers.

See:

- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`

## Target order as semantic intent

For planar Distance constraints, target A/B order is semantically observable because signed separation is:

```text
dot(oB - oA, nA)
```

A reference consumer must not silently reorder the two targets even when the graph edge connects the same component pair.

`AssemblyConstraintGraph` preserves constraint identity but does not own semantic target geometry.

`AssemblyConstraintEquationBuilder` reads target order from the persistent `AssemblyConstraint` record.

`AssemblyRigidBodySolver` orders constraints by graph edge id but preserves each constraint's internal target A/B semantics through the equation builder.

## Future semantic-reference scope

Still required:

- named edge groups produced by features for fillet/chamfer
- semantic axis references produced by hole and shaft features for Concentric/Insert
- generated edge and vertex assembly target families
- analytic surface references where needed
- broader rebinding after recompute with explicit lost-reference diagnostics
- stronger reference dependency/invalidation integration across assembly solve consumers

Semantic axis references remain the current blocker for Concentric residual construction and Concentric solving.

## Dependency and solve integration

A semantic reference creates a conceptual dependency from a geometry producer to a consumer:

```text
source feature -> exposed reference -> consuming feature/constraint
```

When source geometry changes, the reference must be re-resolved. Failed resolution should invalidate or fail the consumer rather than silently bind to another kernel entity.

The current assembly solver does not persist resolved targets or move reference ownership into solver state. Every residual evaluation reuses the semantic resolution path from current project intent.

The repository still does not have a dedicated assembly solve-dependency graph. `AssemblyConstraintGraph` represents relationship connectivity, not geometry-producer invalidation dependencies.

## Out of scope for the first versions

- a complete general topological naming system
- automatic repair of every lost reference chain
- raw OCCT handles as core model references

## Current downstream boundary

The next assembly block is Jacobian-rank and remaining-degree-of-freedom diagnostics over the implemented solver model.

Semantic-reference expansion remains a separate prerequisite for Concentric and richer constraint families.
