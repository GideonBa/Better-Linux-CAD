# Semantic References

Status: partially implemented. Generated planar faces, selected generated edges/vertices, the first feature-produced axis family, and the first feature-produced seating-plane family are implemented. A complete general topological naming system remains future work.

This document is the canonical rule for persistent geometry references shared by sketch, feature, construction, assembly, and future engineering layers.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or transient kernel ids such as `Face17` or `Edge7`.

Persistent references describe constructive meaning rather than incidental topology numbering.

```text
Bad:
  Sketch starts on Face_12
  Constraint targets Face_17
  Hole axis is CylinderFace_8
  Insert seat is OpeningFace_4

Good:
  Sketch starts on feature.base_extrude.top
  Assembly target is feature.mount_plate.bottom
  Circular cut exposes feature.hole.axis
  Circular feature exposes feature.hole.seat
```

## Reference families

BLCAD owns semantic reference records and tokens.

Implemented or planned families include:

- `SemanticFaceReference`
- `SemanticEdgeReference`
- `SemanticVertexReference`
- `SemanticAxisReference`
- `SemanticSeatingPlaneReference`
- future named groups and analytic surface references

Geometry-producing features should expose stable constructive meaning for downstream consumers.

## Generated-face family

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

Assembly path:

```text
semantic face token
  -> AssemblyConstraintTargetResolver::resolve
  -> component-local plane + separate RigidTransform
  -> AssemblyTransformEvaluator::evaluate_plane
  -> Mate/Distance residual builder
  -> shared numeric solver/DOF path
```

## Generated-axis family

Canonical detail: `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

First stable token:

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

The first producer is a `SubtractiveExtrude` whose source sketch contains exactly one `CircleProfile` and exactly one total profile.

The semantic axis derives from part intent:

```text
origin = CircleProfile.center mapped through the source-sketch workplane
direction = workplane normal adjusted by ExtrudeDirection
```

Assembly path:

```text
feature.hole.axis
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> component-local axis + separate RigidTransform
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
  -> shared numeric residual/Jacobian system
  -> solver and DOF diagnostics
```

No raw cylindrical face or OCCT topology id participates in axis identity.

## Generated seating-plane family

Canonical detail: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

First stable token:

```text
feature.<feature-id>.seat
```

Core identity:

```text
SemanticSeatingPlane::Primary
SemanticSeatingPlaneReference
  source_feature
  plane
  node_id()
```

Example:

```text
SemanticSeatingPlaneReference(feature.hole, Primary)
  -> feature.hole.seat
```

The first producer uses the same single-circle `SubtractiveExtrude` family as the primary semantic axis.

The seating plane derives from the exact source feature/profile and workplane:

```text
seat.origin = mapped CircleProfile.center
seat.normal = primary semantic axis direction
```

The local frame follows the source workplane. For `OppositeSketchNormal`, Y and the normal are both reversed so the seating frame remains right-handed.

One persistent `.seat` Insert endpoint derives both:

```text
primary axis
oriented seating plane
```

from the same exact circular feature/profile identity.

Assembly path:

```text
feature.hole.seat
  -> AssemblyConstraintTargetResolver::resolve_insert
  -> local axis + local seat + separate RigidTransform
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> InsertResidualDescriptor
```

Insert numeric solver/DOF integration is not implemented yet.

No OCCT opening face, cylindrical face, or topology index participates in seating identity.

## Why one pattern does not equal one axis or seat

`CircularHolePattern` produces several distinct holes.

The tokens:

```text
feature.pattern.axis
feature.pattern.seat
```

would be ambiguous.

A later pattern reference family must define stable per-instance semantic identity. It must not use transient OCCT face order or assume internal vector position is permanent identity.

## Target order as semantic intent

Some residual descriptors expose persistent target A/B order in their raw values.

Distance:

```text
dot(oB - oA, nA)
```

Concentric axis offset:

```text
cross(oB - oA, dA)
```

Insert seating separation:

```text
dot(sB - sA, nA)
```

A graph, numeric system, solver, cache, or diagnostic consumer must not silently reorder endpoints.

`AssemblyConstraintGraph` orders constraints by id but does not own geometry or reconstruct target order.

Residual builders read A/B order directly from persistent `AssemblyConstraint` intent.

## Concentric semantic dependency

Concentric consumes two semantic assembly-space axis lines.

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

Equal and opposed directions are accepted. Axial translation and rotation about the common axis remain free.

The shared numeric system flattens the descriptor in stable order and re-resolves `.axis` targets for every finite-difference evaluation.

A regular one-free-body Concentric system is proven as rank four with two remaining local DOF.

## Insert semantic dependency

Insert consumes two composite semantic endpoints derived from `.seat` targets.

Each endpoint supplies:

```text
one primary assembly-space axis
one assembly-space seating plane
```

Canonical residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The seating normal is canonically aligned with the endpoint axis direction. Axis parallelism therefore already owns the regular tilt constraints; no duplicate seating-normal residual is required.

A direct central finite-difference test of the seven Insert scalar residuals over six rigid-transform variables proves regular rank five and one remaining rotation-about-axis DOF.

This Insert rank proof is currently outside `AssemblySolveDiagnosticsAnalyzer` because Insert is not yet a shared numeric family.

## Solve and DOF integration rule

Semantic references remain the source of target geometry for every residual evaluation.

The solver must not persist resolved target geometry.

For current Mate, Distance, and Concentric solving, every central finite-difference perturbation re-resolves semantic targets and rebuilds assembly-space geometry/residuals from current project intent.

The next Insert integration must follow the same rule for `.seat`: every residual/Jacobian evaluation must regenerate the composite endpoint from current feature/profile/workplane intent.

When source geometry changes, targets are re-resolved. Failed resolution must fail or invalidate the consumer. A target must never silently bind to another kernel entity because topology numbering changed.

## Persistence rule

The existing assembly target fields store semantic references as strings:

```text
assembly_constraints[].target_a.semantic_reference
assembly_constraints[].target_b.semantic_reference
```

`feature.hole.axis` and `feature.hole.seat` require no new JSON fields.

Derived data includes:

- semantic token interpretation
- local axis and seat descriptors
- assembly-space axis and seat descriptors
- Concentric and Insert residual descriptors
- flattened numeric residuals
- finite-difference Jacobians
- solve results and unapplied proposals
- rank and DOF diagnostics

Changing the meaning of an established semantic token family is a compatibility change even when the JSON schema shape remains identical.

## Future semantic-reference scope

Still required:

- broader axis/seat producers from shafts, revolves, cylindrical features, and construction geometry
- stable per-instance pattern axis/seat tokens
- named edge groups for fillet/chamfer
- generated edge/vertex assembly target families
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
- silently inferring semantic axes or seating planes from arbitrary kernel cylinders/faces

## Current downstream boundary

Generated planar faces, primary circular axes, and primary circular seating targets are implemented.

Mate, Distance, and Concentric participate in the shared numeric solver/DOF path.

Insert relationship intent and read-only composite residual semantics are implemented over `.seat` targets. The next assembly step is Insert integration into the same shared numeric residual/Jacobian, solver, application, and local DOF path.
