# Planar Assembly Constraint Equations MVP-5

Status: implemented read-only equation/residual construction for active planar Mate and Distance assembly constraints over the supported generated-face target family. The first rigid-body solver now consumes these descriptors as a separate downstream layer.

## Goal

This block connects persistent assembly relationship intent to deterministic geometric residual data.

It answers:

1. Which active assembly constraint is evaluated?
2. Which component occurrences and semantic targets does it reference?
3. What are those targets in component-local geometry?
4. What are their planes in assembly coordinates under current component transforms?
5. What deterministic Mate or Distance residual values describe the current relationship error?

The returned equation descriptor is derived data. It is not model intent, is not serialized, and does not update component transforms.

## API

The geometry-layer API lives in:

```text
include/blcad/geometry/assembly_constraint_equation_builder.hpp
```

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

PlanarConstraintResidualDescriptor
  variant<PlanarMateResidualDescriptor,
          PlanarDistanceResidualDescriptor>

AssemblyConstraintEquationDescriptor
  constraint
  type
  target_a
  target_b
  residual

AssemblyConstraintEquationBuilder
  build(Project, AssemblyConstraint)
```

Constraint id/type and both target identities remain explicit in the returned descriptor. Target planes are expressed in assembly coordinates.

## Construction pipeline

```text
AssemblyConstraint
  -> target A
     -> AssemblyConstraintTargetResolver
     -> ResolvedAssemblyConstraintTarget
     -> AssemblyTransformEvaluator
     -> AssemblySpaceConstraintTargetDescriptor A
  -> target B
     -> AssemblyConstraintTargetResolver
     -> ResolvedAssemblyConstraintTarget
     -> AssemblyTransformEvaluator
     -> AssemblySpaceConstraintTargetDescriptor B
  -> canonical planar residual construction
  -> AssemblyConstraintEquationDescriptor
```

Target order is preserved exactly. Target A is resolved before target B, so target-resolution error precedence is deterministic.

Generated-face frame construction remains owned by `WorkplaneResolver`/`AssemblyConstraintTargetResolver`. Rigid-transform evaluation remains owned by `AssemblyTransformEvaluator`. This builder does not duplicate either geometry path.

## Canonical planar Mate residual convention

For assembly-space target planes:

```text
oA = target A origin
nA = target A unit normal

oB = target B origin
nB = target B unit normal
```

Mate residuals are:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate has:

```text
normal_opposition = (0, 0, 0)
signed_separation_mm = 0
```

`normal_opposition` requires opposed unit normals.

`signed_separation_mm` requires target B's plane origin to have zero normal offset from target A's plane.

Tangential origin differences are intentionally not residuals. The current planar relationship is between infinite supporting planes, so coincident planes may use different frame origins inside the plane.

Example:

```text
A top face
  origin = (0, 0, 8)
  normal = (0, 0, 1)

B bottom face
  origin = (15, -9, 8)
  normal = (0, 0, -1)
```

The Mate residual is zero even though the frame origins differ tangentially by `(15, -9, 0)`.

## Canonical signed planar Distance convention

Planar Distance residuals are:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

A satisfied Distance has:

```text
normal_parallelism = (0, 0, 0)
distance_residual_mm = 0
```

`cross(nA, nB) = 0` accepts equal or opposed normal direction. Distance therefore enforces parallel supporting planes without imposing Mate-style normal opposition.

The signed separation is deliberately target-order dependent:

```text
A -> B uses nA
```

Positive separation means B lies in target A's positive normal direction. Negative separation means B lies in the negative direction.

Example:

```text
oA = (0, 0, 8)
nA = (0, 0, 1)

oB = (25, -15, 20)
nB = (0, 0, 1)

target_distance_mm = 12
```

Then:

```text
normal_parallelism = (0, 0, 0)
signed_separation_mm = 12
distance_residual_mm = 0
```

Swapping the targets while keeping a positive `10 mm` target can produce:

```text
signed_separation_mm = -12
distance_residual_mm = -22
```

This is intentional. Target A/B order is persistent semantic intent and must not be normalized away by later consumers.

## Why residuals are explicit descriptors

The builder does not reduce the relationship to a boolean `is_satisfied` value.

It exposes geometric error components:

```text
Mate
  3 normal-opposition components
  1 signed-separation component

Distance
  3 normal-parallelism components
  1 signed-distance component
```

Vector residuals remain `Vector3` values and scalar lengths retain `_mm` naming.

The descriptor layer does not choose numeric weighting, Jacobian construction, convergence tolerance, or optimization method.

Those decisions are now owned by `AssemblyRigidBodySolver`, documented in `docs/assembly-rigid-body-solver-mvp5.md`.

The first solver currently flattens each descriptor orientation-first and length-last, scales the length component by its explicit millimeter residual scale, and evaluates a central finite-difference Jacobian. Those are solver policies rather than equation-builder semantics.

## Supported constraints and targets

The builder supports:

```text
AssemblyConstraintType::Mate
AssemblyConstraintType::Distance
```

Both targets must resolve through the implemented generated planar face family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The source-feature restrictions remain those of `AssemblyConstraintTargetResolver` and `WorkplaneResolver::resolve_generated_face`.

## Explicit failure behavior

Construction fails explicitly when:

- the constraint is inactive
- the constraint type is Concentric
- target A cannot be resolved
- target B cannot be resolved
- either semantic token is malformed
- either target belongs to an unsupported semantic family
- a target component does not exist
- a referenced part is not project-owned
- a generated-face source feature cannot be resolved
- a defensive Distance record reaches the builder without a length quantity

Inactive constraints fail before target geometry resolution.

Concentric fails before target resolution with:

```text
concentric equation construction requires semantic axis target support
```

Target-resolution diagnostics are propagated unchanged.

The implemented rigid-body solver also propagates these builder errors. It does not silently ignore unsupported active constraints in a selected graph group.

## Read-only and persistence boundary

Equation construction does not:

- mutate a component `RigidTransform`
- update a component instance
- enforce grounding
- infer suppression participation
- change constraint state or target tokens
- add, remove, or rewrite constraints
- modify part parameters, sketches, features, or derived workplanes
- run part recompute
- own or mutate `ShapeCache`
- solve a constraint system
- apply a proposed transform
- compute remaining degrees of freedom
- persist equation or residual data

The descriptor is regenerated from:

```text
persistent AssemblyConstraint intent
+ current project/part model intent
+ persisted component RigidTransform values
```

No assembly or project JSON field is added.

The downstream solver also keeps residual descriptors and numeric Jacobians unpersisted. Only explicit application of a fresh converged solve result may update the existing persistent component transform fields.

## Tests

`tests/geometry/assembly_constraint_equation_builder_tests.cpp` covers:

- satisfied planar Mate
- constraint and target identity preservation
- tangential Mate frame-origin differences
- unsatisfied Mate normal/separation residuals
- transformed target geometry
- satisfied planar Distance
- nonparallel Distance plane residuals
- explicit target-distance preservation
- signed A-to-B separation
- positive Distance residual error
- reversed target-order sign behavior
- inactive rejection
- Concentric rejection
- unsupported generated-edge target propagation
- deterministic repeated construction
- unchanged component transforms, constraint records, target tokens, and part workplane intent

Targeted test command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deliberate block boundary

This builder itself does not implement:

- rigid-body optimization
- transform mutation
- residual weighting
- Jacobian construction
- convergence or iteration policy
- remaining-DOF computation
- Jacobian-rank diagnostics
- underconstrained, fully constrained, or overconstrained classification
- grounding or suppression solver policy
- semantic axis references
- Concentric equations
- richer constraint/joint families
- motion, collision, subassemblies, geometry instancing, or assembly STEP export

The first rigid-body optimization, grounding policy, residual weighting, numeric Jacobian, convergence semantics, proposed-transform result, and explicit application boundary are implemented downstream in `AssemblyRigidBodySolver` and `AssemblySolveResultApplier`.

## Next technical step

The repository-wide next assembly block is read-only solve diagnostics and remaining-degree-of-freedom analysis.

It should reuse the solver's deterministic variable/residual ordering and local numeric Jacobian model, define a canonical Jacobian-rank tolerance, and report variable count, local rank, constrained DOF, remaining DOF, and underconstrained versus locally fully constrained state without persisting DOF cache data.

Concentric remains deferred until semantic axis targets and Concentric residual construction exist.
