# Semantic References

Status: partially implemented. Generated planar faces are proven by the MVP-2 workplane path and the MVP-5 assembly target/residual/solver/DOF-diagnostic path. A complete general topological naming system remains future work.

This document is the canonical rule for persistent geometry references shared by sketch, feature, construction, assembly, and future engineering layers.

## Rule

The core must not depend permanently on raw OCCT `TopoDS_Face`, `TopoDS_Edge`, or `TopoDS_Vertex` handles or transient kernel ids such as `Face17` or `Edge7`.

Persistent references describe constructive meaning rather than incidental topology numbering.

```text
Bad:
  Sketch starts on Face_12
  Constraint targets Face_17
  Chamfer applies to Edge_3

Good:
  Sketch starts on feature.base_extrude.top
  Assembly target is feature.mount_plate.bottom
  Hole exposes feature.hole.main_axis
  Chamfer targets outer_top_edges
```

## Reference families

BLCAD owns semantic reference records and tokens.

Current and planned families include:

- `SemanticFaceReference`
- semantic edge references and named edge groups
- semantic axis references
- semantic point/vertex references
- future analytic surface references where needed

Geometry-producing features should expose stable constructive meaning for downstream consumers.

Example target architecture:

```text
HoleFeature_M6_01
  exposed_references
    main_axis
    entry_face
    bottom_face
```

## Implemented generated-face scope

The implemented additive-extrude face family is:

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

The implemented assembly path is:

```text
persistent semantic target token
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + separate RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> planar Mate/Distance residual descriptor
  -> shared assembly numeric system
  -> AssemblyRigidBodySolver
  -> proposed component transforms
  -> explicit fresh-converged-result application
  -> AssemblySolveDiagnosticsAnalyzer
  -> local Jacobian rank and remaining DOF
```

No raw OCCT face id is stored in part or assembly model intent.

Target resolution, transform interpretation, residual semantics, numeric solving, and DOF diagnostics remain separate layers.

See:

- `docs/derived-workplane-mvp2-seed.md`
- `docs/workplane-resolver-mvp2.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`

## Target order as semantic intent

Planar Distance signed separation is:

```text
dot(oB - oA, nA)
```

Target A/B order is therefore semantically observable.

A graph, numeric system, solver, cache, or diagnostic consumer must not silently reorder endpoints even when two constraints connect the same component pair.

The graph orders constraints by id but does not own target geometry.

The equation builder reads target A/B order from persistent `AssemblyConstraint` intent.

The shared numeric system and solver preserve each constraint's internal target order while using deterministic graph constraint-id ordering.

## Implemented solve/DOF integration rule

Semantic references remain the source of target geometry for every residual evaluation.

The solver does not persist resolved target geometry.

Central finite-difference perturbations re-evaluate residuals from current project intent through the same target resolver.

`AssemblySolveDiagnosticsAnalyzer` evaluates local Jacobian rank through the same shared numeric residual/Jacobian path as the solver.

Therefore no solver or DOF cache takes ownership of semantic reference bindings.

When source geometry changes, semantic targets are re-resolved from model intent.

A failed resolution must fail or invalidate the consumer. It must never silently bind to another kernel entity because a raw topology index happened to move.

## Next semantic-reference block: generated axes

Semantic axes are now the direct next assembly blocker.

The next block should define a stable generated-axis family produced by supported feature intent.

Target properties:

```text
semantic axis token
  -> source feature identity
  -> named axis role
  -> component-local axis descriptor
       point/origin
       unit direction
```

A first stable token family should be feature-produced and explicit. Example shape:

```text
feature.<feature-id>.main_axis
```

The exact supported source feature family must be documented before implementation. The resolver must reject unsupported feature/axis families explicitly rather than guess from OCCT topology.

Assembly resolution should follow:

```text
AssemblyConstraintTarget
  -> component occurrence
  -> project-owned referenced PartDocument
  -> supported semantic axis token
  -> source feature intent
  -> ComponentLocalAxisDescriptor
  + separate component RigidTransform
```

Assembly-space evaluation follows the existing transform convention:

```text
axis point     -> rotate then translate
axis direction -> rotate only
```

The assembly-space result should use a distinct axis descriptor type rather than overloading planar descriptors.

## Concentric residual dependency

Concentric must not be implemented by comparing raw cylindrical OCCT faces.

It should consume two resolved assembly-space semantic axes.

The residual block must define canonical:

- axis-direction parallelism/alignment semantics
- shortest line-to-line offset semantics
- target A/B behavior where sign or direction becomes observable
- degeneracy/normalization validation

Only after those residual conventions are stable should `AssemblyRigidBodySolver` and `AssemblySolveDiagnosticsAnalyzer` consume Concentric constraints.

Insert remains downstream because it combines axis alignment with axial seating.

## Future semantic-reference scope

Still required after the first generated-axis family:

- broader axis families from hole, shaft, revolve, cylindrical, and construction geometry intent
- named edge groups for fillet/chamfer
- generated edge and vertex assembly target families
- analytic surface references where needed
- broader reference rebinding after recompute
- explicit lost-reference diagnostics
- stronger geometry-producer invalidation integration across assembly consumers

## Dependency integration

A semantic reference creates a conceptual dependency:

```text
source feature -> exposed reference -> consuming feature or assembly constraint
```

`AssemblyConstraintGraph` currently represents relationship connectivity only. It is not a geometry-producer invalidation or solve-dependency graph.

A future dependency model may connect feature/reference changes to affected assembly constraints and solved placements, but reference ownership must remain with model intent and resolvers rather than solver state.

## Out of scope for the first versions

- a complete general topological naming system
- automatic repair of every lost reference chain
- raw OCCT handles as persistent model references
- silently inferring semantic axes from arbitrary kernel cylinders

## Current downstream boundary

Jacobian-rank and remaining-DOF diagnostics are implemented.

The next assembly step is semantic generated-axis resolution and a read-only Concentric target/residual pipeline. Solver and DOF-diagnostic Concentric integration follows only after that semantic geometry contract is stable.
