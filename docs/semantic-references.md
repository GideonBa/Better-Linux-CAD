# Semantic References

Status: partially implemented. Generated planar faces, selected generated edges/vertices, and the first feature-produced semantic axis family are implemented. A complete general topological naming system remains future work.

This document is the canonical rule for persistent geometry references shared by sketch, feature, construction, assembly, and future engineering layers.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or transient kernel ids such as `Face17` or `Edge7`.

Persistent references describe constructive meaning rather than incidental topology numbering.

```text
Bad:
  Sketch starts on Face_12
  Constraint targets Face_17
  Hole axis is CylinderFace_8

Good:
  Sketch starts on feature.base_extrude.top
  Assembly target is feature.mount_plate.bottom
  Circular cut exposes feature.hole.axis
```

## Reference families

BLCAD owns semantic reference records and tokens.

Current and planned families include:

- `SemanticFaceReference`
- `SemanticEdgeReference`
- `SemanticVertexReference`
- `SemanticAxisReference`
- future named groups and analytic surface references

Geometry-producing features should expose stable constructive meaning for downstream consumers.

## Implemented generated-face family

The additive-extrude face family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

Shared part-geometry path:

```text
SemanticFaceReference
  -> WorkplaneResolver::resolve_generated_face
  -> deterministic generated-face frame
```

`DerivedWorkplane` consumes this path for sketch placement.

The assembly path is:

```text
semantic face token
  -> AssemblyConstraintTargetResolver::resolve
  -> ComponentLocalPlanarDescriptor + separate RigidTransform
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> planar Mate/Distance residuals
```

The current numeric solver and DOF analyzer consume this planar residual path.

## Implemented generated-axis family

Canonical detail: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

The first stable feature-produced axis token is:

```text
feature.<feature-id>.axis
```

Core identity:

```text
SemanticAxis::Primary
SemanticAxisReference
  source_feature
  axis
  node_id()
```

Example:

```text
SemanticAxisReference(feature.hole, Primary)
  -> feature.hole.axis
```

The first supported producer is a `SubtractiveExtrude` whose input sketch contains exactly one `CircleProfile` and exactly one total profile.

This mirrors the existing circular through-all cut execution path.

The semantic axis derives from part intent:

```text
origin
  = CircleProfile.center mapped through the resolved source-sketch workplane

direction
  = resolved workplane normal
    or its negation for OppositeSketchNormal
```

Assembly path:

```text
feature.hole.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> ComponentLocalAxisDescriptor + separate RigidTransform
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblySpaceAxisDescriptor
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
```

No raw cylindrical face or OCCT topology id participates in the reference identity.

## Why one pattern does not equal one axis

`CircularHolePattern` produces several distinct hole axes.

The token:

```text
feature.pattern.axis
```

would therefore be ambiguous.

A later pattern-axis family must define stable per-instance semantic identity. It must not use transient OCCT face order or silently assume vector position is permanent identity.

## Target order as semantic intent

Some residual descriptors expose target order in their raw values.

Planar Distance uses:

```text
dot(oB - oA, nA)
```

Concentric axis offset uses:

```text
cross(oB - oA, dA)
```

A graph, numeric system, solver, cache, or diagnostic consumer must not silently reorder endpoints.

`AssemblyConstraintGraph` orders constraints by id but does not own target geometry.

Residual builders read target A/B order directly from persistent `AssemblyConstraint` intent.

## Concentric semantic dependency

Concentric consumes two semantic assembly-space axis lines.

Canonical residuals are:

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted.

Axial origin separation is absent from the offset residual, so Concentric leaves translation along the common axis free.

Rotation about the common axis is also unconstrained by axis-line coincidence.

These semantics are implemented in the read-only Concentric builder.

The shared numeric system, rigid-body solver, and DOF diagnostics do not yet consume Concentric residuals. That integration is the next assembly block.

## Solve and DOF integration rule

Semantic references remain the source of target geometry for every residual evaluation.

The solver does not persist resolved target geometry.

For current Mate/Distance solving, central finite-difference perturbations re-evaluate residuals from current project intent through the same target resolver.

`AssemblySolveDiagnosticsAnalyzer` evaluates local Jacobian rank through the same shared numeric path as the solver.

The next Concentric integration must follow the same rule: each residual/Jacobian evaluation resolves `feature.<feature-id>.axis` from current project model intent rather than caching kernel bindings in solver state.

When source geometry changes, semantic targets are re-resolved. Failed resolution must fail or invalidate the consumer. It must never silently bind to another kernel entity because a topology index changed.

## Persistence rule

The existing assembly target field already stores semantic references as strings:

```text
assembly_constraints[].target_a.semantic_reference
assembly_constraints[].target_b.semantic_reference
```

`feature.hole.axis` needs no new JSON field.

The following remain derived:

- `SemanticAxisReference` interpretation of a supported token
- local axis descriptors
- assembly-space axis descriptors
- Concentric residual descriptors
- numeric Jacobians and DOF diagnostics

Changing the meaning of an established semantic token family is a compatibility change even when the JSON schema shape remains identical.

## Future semantic-reference scope

Still required:

- broader axis producers from shafts, revolves, cylindrical features, and construction geometry
- stable per-instance pattern-axis tokens
- named edge groups for fillet/chamfer
- generated edge and vertex assembly target families
- analytic surface references where needed
- broader reference rebinding after recompute
- explicit lost-reference diagnostics
- stronger geometry-producer invalidation integration across assembly consumers

## Dependency integration

A semantic reference creates a conceptual dependency:

```text
source feature -> exposed semantic reference -> consuming feature or assembly constraint
```

`AssemblyConstraintGraph` currently represents relationship connectivity only. It is not a geometry-producer invalidation or solve-dependency graph.

A future dependency model may connect feature/reference changes to affected assembly constraints and solved placements, but reference ownership must remain with model intent and resolvers rather than solver state.

## Out of scope for the first versions

- a complete general topological naming system
- automatic repair of every lost reference chain
- raw OCCT handles as persistent model references
- silently inferring semantic axes from arbitrary kernel cylinders

## Current downstream boundary

Generated-axis resolution and read-only Concentric residual construction are implemented.

The next assembly step is to integrate the Concentric residual family into the shared numeric residual/Jacobian system, rigid-body solver, and local remaining-DOF diagnostics. Insert remains downstream until axial seating semantics are explicit.
