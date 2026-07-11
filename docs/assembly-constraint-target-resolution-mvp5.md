# Assembly Constraint Target Resolution MVP-5

Status: implemented read-only generated-planar-face target resolution. The first rigid-body solver now consumes this resolver indirectly through the planar residual builder.

## Goal

`AssemblyConstraintTargetResolver` bridges persistent semantic assembly target intent to deterministic component-local planar geometry.

It owns semantic target lookup and local frame construction only.

It does not apply component placement, construct constraint residuals, decide solver participation, or mutate transforms.

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

The resolved descriptor preserves component, part, feature, and semantic-face identity plus the component-local plane and separate persisted `RigidTransform`.

## Resolution path

```text
AssemblyConstraintTarget
  -> component id
  -> Project.assembly().find_component_instance(...)
  -> component.referenced_part_document
  -> Project.find_part_document(...)
  -> parse generated-face semantic token
  -> validate source feature
  -> WorkplaneResolver::resolve_generated_face(...)
  -> ComponentLocalPlanarDescriptor
  + separate component RigidTransform
```

Component occurrence identity is resolved before part geometry.

The resolver never copies or duplicates the project-owned `PartDocument` as new model intent.

## Supported semantic target family

The first assembly target family is:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The suffix selects one of the six implemented `SemanticFace` values.

The source feature id is the token prefix before the final dot. The parser deliberately does not invent a second naming grammar beyond the existing typed `FeatureId` value.

The referenced source feature must exist in the component's project-owned part and currently must be an `AdditiveExtrude` supported by the shared generated-face workplane path.

## Shared generated-face geometry

The resolver delegates face geometry to:

```text
WorkplaneResolver::resolve_generated_face
```

Derived workplanes and assembly targets therefore share one implementation of generated-face origin, basis axes, and normal.

The six face frames retain the existing formulas for top, bottom, right, left, front, and back faces of the supported rectangle-profile additive extrude.

The assembly resolver does not duplicate these formulas.

## Component-local versus assembly-space geometry

Resolver output is intentionally component-local:

```text
ComponentLocalPlanarDescriptor
```

The persisted component placement is returned separately:

```text
ResolvedAssemblyConstraintTarget.component_transform
```

The downstream transform path is:

```text
ResolvedAssemblyConstraintTarget.local_plane
+ ResolvedAssemblyConstraintTarget.component_transform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
```

This preserves the explicit X-then-Y-then-Z `RigidTransform` convention owned by the transform evaluator.

## Downstream use

The complete implemented assembly geometry/solve path is:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
  -> AssemblyConstraintEquationBuilder
  -> Mate/Distance residual descriptor
  -> AssemblyRigidBodySolver
  -> proposed component transforms
  -> AssemblySolveResultApplier
  -> explicit successful transform application
```

The resolver itself remains independent from constraint type, residual semantics, grounding, residual weighting, numeric Jacobians, and solve policy.

The solver re-resolves targets for every residual evaluation, including finite-difference perturbations on its private project copies. No resolved-target cache becomes solver state.

## Explicit failure behavior

Resolution fails when:

- the target component instance does not exist in the project assembly
- the component's referenced part is not owned by the project
- the semantic token is malformed
- the token suffix is not one of the six supported planar faces
- the source feature does not exist
- the source feature is not a supported additive extrude
- the shared generated-face resolver rejects the source profile/geometry family

Semantic axis targets remain unsupported.

Generated edge and vertex target families also remain unsupported for assembly constraints.

Failures use the existing `Result<T>`/`Error` path and are propagated unchanged by the equation builder and first rigid-body solver.

## Read-only and persistence boundary

Resolution does not:

- mutate component transforms
- change grounding, visibility, or suppression
- change assembly constraints
- add generated references or derived workplanes to the original part
- run a solver
- take ownership of `ShapeCache`
- persist a resolved target descriptor

The descriptor is regenerated from current project model intent and part parameters.

The solver also leaves target descriptors unpersisted. Explicit solver-result application changes only existing component transform fields, not semantic target ownership.

## Tests

`tests/geometry/assembly_constraint_target_resolver_tests.cpp` covers:

- all six generated-face targets
- component and referenced-part identity
- separate component transform preservation
- missing component targets
- missing project-owned parts
- missing source features
- malformed semantic tokens
- unsupported semantic axis targets
- unsupported generated-edge targets
- deterministic repeated resolution
- unchanged component transforms, constraint records, semantic tokens, and part derived-workplane count

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
```

Transform, equation, and solver suites separately verify downstream coordinate mapping, residual semantics, repeated target resolution during numeric solving, and transform application boundaries.

## Deliberate resolver-layer limitations

This resolver itself does not implement:

- component-local-to-assembly transform math
- constraint residual semantics
- rigid-body optimization
- solved transform application
- remaining-DOF analysis
- grounding or suppression solver policy

Transform evaluation, planar Mate/Distance residual construction, rigid-body solving, and explicit successful-result application are implemented as separate downstream layers.

## Current downstream boundary

The repository-wide next assembly step is read-only Jacobian-rank and remaining-degree-of-freedom diagnostics over the implemented solver model.

Semantic axis references and Concentric residual construction remain deferred.
