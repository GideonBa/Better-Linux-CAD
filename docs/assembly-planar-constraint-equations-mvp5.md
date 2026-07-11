# Planar Assembly Constraint Equations MVP-5

Status: implemented read-only residual construction for active planar Mate and Distance constraints over the supported generated-face target family. The shared numeric system, rigid-body solver, and local DOF diagnostics consume these descriptors downstream.

## Goal

This block converts persistent assembly relationship intent plus current target geometry into deterministic geometric residual descriptors.

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

Constraint id/type and target A/B identity remain explicit.

## Construction pipeline

```text
AssemblyConstraint
  -> target A
     -> AssemblyConstraintTargetResolver
     -> AssemblyTransformEvaluator
     -> assembly-space plane A
  -> target B
     -> AssemblyConstraintTargetResolver
     -> AssemblyTransformEvaluator
     -> assembly-space plane B
  -> canonical planar residual construction
  -> AssemblyConstraintEquationDescriptor
```

Target A resolves before target B, giving deterministic error precedence.

Generated-face geometry remains owned by `WorkplaneResolver` and `AssemblyConstraintTargetResolver`.

Rigid-transform semantics remain owned by `AssemblyTransformEvaluator`.

## Mate residual convention

For assembly-space plane origins and unit normals:

```text
oA, nA = target A
oB, nB = target B
```

Mate residuals:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate has:

```text
normal_opposition = (0, 0, 0)
signed_separation_mm = 0
```

Tangential differences between plane frame origins are intentionally not residuals. The relationship is between infinite supporting planes.

The three scalar components of `normal_opposition` do not imply three independent rotational DOF constraints. The implemented Jacobian-rank diagnostics show one planar Mate has local rank 3 over six free rigid-body variables: two rotational directions plus one normal translation direction.

## Distance residual convention

Planar Distance residuals:

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

A satisfied Distance has:

```text
normal_parallelism = (0, 0, 0)
distance_residual_mm = 0
```

`cross(nA, nB) = 0` accepts equal or opposed normal direction. Distance therefore requires parallel supporting planes but does not impose Mate-style normal opposition.

Signed separation is deliberately target-order dependent:

```text
A -> B uses nA
```

Positive separation places B in A's positive-normal direction. Negative separation places B in A's negative-normal direction.

Target A/B order is persistent semantic intent and must not be normalized away by graph, numeric, solver, or diagnostic consumers.

## Explicit descriptors instead of booleans

The builder does not reduce a relationship to `is_satisfied`.

It preserves geometric error components:

```text
Mate
  3 normal-opposition components
  1 signed-separation component

Distance
  3 normal-parallelism components
  1 signed-distance component
```

Vector residuals remain `Vector3`; millimeter scalar residuals use `_mm` names.

This layer does not infer independent constraint count from residual component count.

The implemented `AssemblySolveDiagnosticsAnalyzer` measures local independent directions from the shared numeric Jacobian instead.

## Downstream numeric interpretation

The shared private assembly numeric system consumes these descriptors.

Constraint order is inherited lexicographically from `AssemblyConstraintGraph`.

Each residual descriptor is flattened orientation-first and scaled-length-last.

Mate:

```text
normal_opposition.x
normal_opposition.y
normal_opposition.z
signed_separation_mm / length_residual_scale_mm
```

Distance:

```text
normal_parallelism.x
normal_parallelism.y
normal_parallelism.z
distance_residual_mm / length_residual_scale_mm
```

Default length scale: `1.0 mm`.

The same flattened system is used by:

```text
AssemblyRigidBodySolver
AssemblySolveDiagnosticsAnalyzer
```

The diagnostics therefore compute Jacobian rank from the same equation interpretation that the solver optimizes.

Residual-row redundancy is reported separately and is not automatically called semantic overconstraint.

## Supported constraints and targets

Supported:

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

Source-feature restrictions are inherited from `AssemblyConstraintTargetResolver` and `WorkplaneResolver::resolve_generated_face`.

## Failure behavior

Construction fails explicitly when:

- the constraint is inactive
- the constraint type is Concentric
- target A cannot be resolved
- target B cannot be resolved
- a semantic token is malformed
- a target uses an unsupported semantic family
- a target component does not exist
- a referenced part is not project-owned
- a generated-face source feature cannot be resolved
- a defensive Distance record lacks its length quantity

Inactive constraints fail before target resolution.

Concentric currently fails before target resolution with:

```text
concentric equation construction requires semantic axis target support
```

Target-resolution diagnostics propagate unchanged.

Solver and diagnostics consumers do not silently ignore unsupported active constraints in a selected graph group.

## Read-only and persistence boundary

Equation construction does not:

- mutate component transforms or state
- enforce grounding or suppression policy
- rewrite constraints or target order
- mutate part intent
- run part recompute
- own a `ShapeCache`
- solve transforms
- apply proposals
- compute Jacobian rank or DOF
- persist equation/residual descriptors

Descriptors are regenerated from current project/part intent, persistent constraint intent, and current component transforms.

The downstream numeric system, solver, and diagnostics also keep residual/Jacobian/DOF products unpersisted.

Only explicit application of a fresh converged solve result may change existing persistent component transforms.

## Tests

`tests/geometry/assembly_constraint_equation_builder_tests.cpp` covers satisfied/unsatisfied Mate, target identity/order, tangential frame origin differences, transformed target geometry, satisfied/unsatisfied Distance, normal parallelism, target-distance preservation, signed A-to-B separation, reversed target order, inactive/Concentric rejection, unsupported target propagation, determinism, and unchanged project intent.

Targeted command:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
```

Solver and diagnostics tests verify the downstream shared numeric interpretation:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

## Deliberate builder-layer boundary

This builder does not own residual scaling, finite differences, Jacobian construction, solve participation, optimization, transform application, rank tolerance, or DOF classification.

Those capabilities are implemented downstream by the shared numeric system, solver, applier, and diagnostics analyzer.

This builder also does not support semantic axis targets or Concentric residuals.

## Next technical step

Jacobian-rank and remaining-DOF diagnostics are implemented.

The next assembly block is a semantic generated-axis reference/target family and read-only Concentric residual construction. The Concentric descriptor must remain a distinct axis-based residual family rather than being forced into the current planar residual variant.

Solver and DOF-diagnostic Concentric integration follows only after the semantic axis and residual conventions are stable.
