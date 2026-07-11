# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only generated-planar-face target resolution. The planar residual builder, rigid-body solver, shared numeric system, and solve/DOF diagnostics consume this target path indirectly.

## Goal

`AssemblyConstraintTargetResolver` bridges persistent semantic assembly target intent to deterministic component-local planar geometry.

It owns semantic target lookup and local frame construction only.

It does not apply component placement, construct residuals, decide solver participation, optimize transforms, or compute DOF.

## API

```text
ComponentLocalPlanarDescriptor
  origin
  x_axis
  y_axis
  normal

ResolvedAssemblyConstraintTarget
  component_instance
  referenced_part_document
  source_feature
  face
  local_plane
  component_transform

AssemblyConstraintTargetResolver
  resolve(Project, AssemblyConstraintTarget)
```

The resolved descriptor preserves component/part/feature/face identity, the component-local planar frame, and the separate persisted component transform.

## Resolution path

```text
AssemblyConstraintTarget
  -> component id
  -> project assembly component occurrence
  -> component.referenced_part_document
  -> project-owned PartDocument
  -> parse supported generated-face token
  -> validate source feature
  -> WorkplaneResolver::resolve_generated_face
  -> ComponentLocalPlanarDescriptor
  + separate RigidTransform
```

Component occurrence identity is resolved before part geometry.

The resolver never duplicates the project-owned `PartDocument` as new model intent.

## Supported target family

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The final suffix selects one implemented `SemanticFace`.

The token prefix before the final dot is the typed `FeatureId` value.

The source feature must exist in the referenced project-owned part and currently must be a supported `AdditiveExtrude`.

## Shared generated-face geometry

The resolver delegates geometry to:

```text
WorkplaneResolver::resolve_generated_face
```

Derived workplanes and assembly targets therefore share one implementation of generated-face origin, basis axes, and normal.

The assembly resolver does not duplicate top/bottom/right/left/front/back frame formulas.

## Component-local versus assembly-space geometry

Resolver output is intentionally component-local.

```text
ComponentLocalPlanarDescriptor
+ separate ResolvedAssemblyConstraintTarget.component_transform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
```

This keeps transform semantics in the dedicated evaluator and preserves the canonical X-then-Y-then-Z `RigidTransform` convention.

## Downstream implemented path

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residual descriptor
  -> shared assembly numeric system
  -> AssemblyRigidBodySolver
  -> AssemblySolveResult / explicit application
  -> AssemblySolveDiagnosticsAnalyzer
  -> local Jacobian rank and remaining DOF
```

The resolver remains independent from constraint type, residual semantics, grounding, residual weighting, finite-difference policy, optimization, and DOF classification.

The solver and diagnostics re-evaluate targets through the residual path on private project copies. No resolved-target cache becomes solver or DOF state.

## Failure behavior

Resolution fails when:

- the target component occurrence does not exist
- the component's referenced part is not project-owned
- the semantic token is malformed
- the token suffix is not one of the six supported planar faces
- the source feature does not exist
- the source feature is not a supported additive extrude
- the shared generated-face resolver rejects the source profile/geometry family

Semantic axis targets remain unsupported.

Generated edge and vertex assembly targets also remain unsupported.

Failures use the existing `Result<T>`/`Error` path and propagate through downstream residual, solver, and diagnostics consumers.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms or state
- change assembly constraints
- add generated references or derived workplanes to the original part
- run a solver
- compute DOF
- own a `ShapeCache`
- persist resolved target descriptors

Descriptors are regenerated from current project model intent and part parameters.

Explicit solver-result application changes only existing component transform fields, never semantic target ownership.

## Tests

`tests/geometry/assembly_constraint_target_resolver_tests.cpp` covers all six generated faces, identity preservation, separate transform preservation, missing component/part/source feature failures, malformed tokens, unsupported axis/edge families, determinism, and unchanged project intent.

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
```

Downstream transform, equation, solver, and diagnostics suites separately verify assembly-space mapping, residual semantics, repeated target evaluation, optimization, and local Jacobian-rank analysis.

## Deliberate resolver-layer limitations

This resolver does not own transform math, residual semantics, rigid-body optimization, transform application, or DOF analysis. Those responsibilities are implemented in separate downstream layers.

It also does not implement semantic axis, edge, or vertex assembly target descriptors.

## Current downstream boundary

Jacobian-rank and remaining-DOF diagnostics are implemented.

The next assembly step is a new semantic generated-axis target family and a read-only Concentric target/residual pipeline. That work should extend semantic target typing rather than force axis geometry into the planar descriptor used by this resolver.
