# Planar Assembly Constraint Equations MVP-5

Status: implemented read-only residual construction for active planar Mate and Distance constraints over the supported generated-face target family. The shared numeric system, rigid-body solver, and local DOF diagnostics consume these planar descriptors alongside the separate Concentric residual family. Insert now has a third dedicated composite residual builder and remains outside the shared numeric system until the next block.

Semantic axes and Concentric residuals are canonicalized in `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

Insert seating targets and composite residuals are canonicalized in `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

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

Target A resolves before target B, preserving deterministic error precedence.

Generated-face geometry remains owned by `WorkplaneResolver` and `AssemblyConstraintTargetResolver`. Transform semantics remain owned by `AssemblyTransformEvaluator`.

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

Signed separation measures B from A along target A's normal. The raw value is target-order dependent and A/B order must not be normalized away.

## Numeric-system boundary

The builder returns geometric residual descriptors. It does not choose numeric weighting.

The shared numeric system flattens planar descriptors as:

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

Each planar constraint contributes four scalar residuals.

These are numeric-system policies rather than planar builder semantics.

Concentric integration did not change the planar order or scaling. Insert integration must also preserve it exactly.

## Separate geometry-family builders

`AssemblyConstraintEquationBuilder` remains deliberately planar and supports:

```text
Mate
Distance
```

Concentric uses:

```text
AssemblyConcentricConstraintEquationBuilder
```

Insert uses:

```text
AssemblyInsertConstraintEquationBuilder
```

The planar builder rejects both nonplanar families before target resolution:

```text
Concentric
  -> semantic axis builder required

Insert
  -> dedicated composite target builder required
```

This separation prevents axis or composite seat semantics from leaking into planar descriptor types.

## Shared numeric selection

The private shared numeric system currently selects:

```text
Concentric
  -> AssemblyConcentricConstraintEquationBuilder

Mate / Distance
  -> AssemblyConstraintEquationBuilder
```

Insert residual construction is implemented but is not yet selected by the shared numeric evaluator.

Current active Insert solver input therefore reaches the explicit planar-builder error:

```text
Insert equation construction requires dedicated composite target support
```

The next block will add:

```text
Insert
  -> AssemblyInsertConstraintEquationBuilder
```

inside the one shared residual evaluator without changing the three existing family orders.

## Insert relationship to planar seating geometry

Insert contains one seating-plane scalar:

```text
signed_seating_separation_mm = dot(sB - sA, nA)
```

That does not make Insert a planar Mate or Distance relationship.

A `.seat` endpoint is a composite circular-feature target that also derives a primary semantic axis. Insert combines:

```text
axis direction parallelism
axis-line lateral offset
signed axial seating
```

The dedicated Insert builder therefore owns the complete composite semantics.

No second planar constraint is synthesized and no hidden Mate record is created.

## Read-only and persistence boundary

Planar equation construction does not:

- mutate component transforms or state
- enforce grounding/suppression policy
- change target strings or target order
- modify part parameters, sketches, features, or workplanes
- flatten residuals
- build Jacobians
- run a solver
- compute DOF
- persist residual descriptors

The same read-only architectural rule applies to Concentric and Insert builders.

No assembly/project JSON field is added for any residual descriptor.

## Tests

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Planar coverage includes satisfied/unsatisfied Mate/Distance cases, transformed target geometry, target identity, tangential Mate freedom, signed Distance order, unsupported targets, determinism, and read-only behavior.

Concentric and Insert suites separately cover their semantic geometry and residual contracts.

## Current downstream boundary

Planar Mate/Distance residual construction remains stable and feeds the shared numeric solver/diagnostic system together with the separate Concentric family.

Stable Insert intent, semantic seating targets, and read-only composite residual construction are implemented through a dedicated builder.

The next assembly block is Insert selection/flattening inside the shared numeric residual/Jacobian system, followed by solver/application/DOF integration. Existing planar semantics must remain unchanged.
