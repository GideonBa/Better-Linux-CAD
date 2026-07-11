# Planar Assembly Constraint Equations MVP-5

Status: implemented read-only residual construction for active planar Mate and Distance constraints over the supported generated-face target family. The shared numeric system, rigid-body solver, and local DOF diagnostics consume these planar descriptors downstream.

Semantic axes and Concentric residual construction are now implemented separately in `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

## Goal

This block converts persistent planar assembly relationship intent plus current target geometry into deterministic geometric residual descriptors.

It does not choose solver participation, residual scaling, Jacobian policy, optimization, transform mutation, or DOF classification.

## API

```text
AssemblySpaceConstraintTargetDescriptor
  component_instance
  semantic_reference
  plane : AssemblySpacePlanarDescriptor

PlanarMateResidualDescriptor
  normal_opposition
  signed_separation_mm

PlanarDistanceResidualDescriptor
  normal_parallelism
  target_distance_mm
  signed_separation_mm
  distance_residual_mm

AssemblyConstraintEquationDescriptor
  constraint
  type
  target_a
  target_b
  residual

AssemblyConstraintEquationBuilder
  build(Project, AssemblyConstraint)
```

`residual` is the planar Mate/Distance variant only.

## Construction pipeline

```text
AssemblyConstraint
  -> target A
     -> AssemblyConstraintTargetResolver::resolve
     -> ComponentLocalPlanarDescriptor
     -> AssemblyTransformEvaluator::evaluate_plane
     -> AssemblySpaceConstraintTargetDescriptor A
  -> target B
     -> same planar path
     -> AssemblySpaceConstraintTargetDescriptor B
  -> canonical planar residual construction
```

Target A resolves before target B, so error precedence is deterministic.

Generated-face geometry remains owned by `WorkplaneResolver`/`AssemblyConstraintTargetResolver`. Transform semantics remain owned by `AssemblyTransformEvaluator`.

## Canonical Mate residuals

For planes `(oA,nA)` and `(oB,nB)`:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate has zero vector and scalar residual.

Tangential origin differences are intentionally absent because the relationship is between infinite supporting planes.

## Canonical Distance residuals

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

`cross(nA,nB) = 0` accepts equal or opposed normals.

Signed separation measures B from A along target A's normal. The raw value is target-order dependent and target A/B order must not be normalized away.

## Numeric-system boundary

The builder returns geometric residual descriptors. It does not choose numeric weighting.

The current shared numeric system flattens:

```text
Mate:
  normal_opposition.x
  normal_opposition.y
  normal_opposition.z
  signed_separation_mm / length_residual_scale_mm

Distance:
  normal_parallelism.x
  normal_parallelism.y
  normal_parallelism.z
  distance_residual_mm / length_residual_scale_mm
```

These are solver/numeric-system policies rather than planar builder semantics.

## Concentric boundary

`AssemblyConstraintEquationBuilder` remains deliberately planar and supports:

```text
Mate
Distance
```

It still rejects Concentric through the established planar path.

Concentric now has a dedicated builder:

```text
AssemblyConcentricConstraintEquationBuilder
```

and dedicated axis descriptor/residual types.

This separation avoids overloading planar descriptors and allows semantic axis/residual conventions to be stable before the shared numeric system changes.

The current solver and DOF analyzer still use the planar builder through `AssemblyConstraintNumericSystem`; therefore an active Concentric constraint still fails in those consumers until the next integration block.

## Read-only and persistence boundary

Planar equation construction does not:

- mutate component transforms or state
- enforce grounding/suppression policy
- change target strings or target order
- modify part parameters, sketches, features, or workplanes
- run a solver
- compute DOF
- persist residual descriptors

No assembly/project JSON field is added.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
```

Coverage includes satisfied and unsatisfied Mate/Distance cases, transformed target geometry, target identity, tangential Mate freedom, signed Distance order, unsupported targets, deterministic repeated construction, and read-only behavior.

Concentric residual tests are separate:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
```

## Current downstream boundary

Planar residual construction remains stable and already feeds the current numeric solver/diagnostic system.

The next assembly step is to add the dedicated Concentric residual family to that shared numeric system without changing existing Mate/Distance scalar ordering or semantics.
