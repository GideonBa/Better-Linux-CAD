# Planar Assembly Constraint Equations MVP-5

Status: implemented read-only equation/residual construction for active planar Mate and Distance assembly constraints over the supported generated-face target family.

## Goal

This block connects persistent assembly relationship intent to deterministic geometric residual data without introducing a rigid-body solver.

The implemented path now answers:

1. Which active assembly constraint record is being evaluated?
2. Which two component occurrences and semantic targets does it reference?
3. What are the supported target planes in component-local coordinates?
4. What are those planes in assembly coordinates under the persisted component transforms?
5. What deterministic residual values describe the current Mate or Distance error?

The returned equation descriptor is derived data. It is not model intent, is not serialized, and does not update component transforms.

## API

The geometry-layer API lives in `include/blcad/geometry/assembly_constraint_equation_builder.hpp`.

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

`AssemblyConstraintEquationDescriptor` preserves the persistent constraint id and type plus both target component ids and semantic reference tokens. The returned target planes are expressed in assembly coordinates.

## Construction pipeline

For each supported active constraint:

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

Target order is preserved exactly. Target A is always resolved and evaluated before target B.

The builder reuses the existing target-resolution and transform-evaluation layers. It does not duplicate generated-face frame construction or rigid-transform rotation math.

## Canonical planar Mate residual convention

For target planes A and B, define:

```text
oA = target A assembly-space origin
nA = target A assembly-space unit normal

oB = target B assembly-space origin
nB = target B assembly-space unit normal
```

The implemented planar Mate convention is:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate requires:

```text
normal_opposition = (0, 0, 0)
signed_separation_mm = 0
```

The first equation requires the two unit normals to oppose each other.

The second equation requires target B's plane origin to have zero signed offset from target A's plane along target A's normal.

Tangential origin differences inside the plane are intentionally not residuals. Infinite coincident planes may use different frame origins without violating a planar Mate relationship.

Example:

```text
A top face
  origin = (0, 0, 8)
  normal = (0, 0, 1)

B bottom face
  origin = (15, -9, 8)
  normal = (0, 0, -1)
```

Then:

```text
normal_opposition = (0, 0, 0)
signed_separation_mm = 0
```

The planes satisfy the implemented Mate residual convention even though their frame origins differ tangentially by `(15, -9, 0)`.

## Canonical signed planar Distance convention

For the same target definitions, the implemented planar Distance convention is:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

A satisfied planar Distance requires:

```text
normal_parallelism = (0, 0, 0)
distance_residual_mm = 0
```

`normal_parallelism = cross(nA, nB)` accepts parallel planes with either equal or opposite normal direction. Distance does not impose Mate-style normal opposition.

The signed separation is deliberately target-order dependent:

```text
A -> B uses nA
```

Positive separation means target B lies in the positive target-A normal direction from target A.

Negative separation means target B lies in the negative target-A normal direction.

For example:

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

If the two targets are reversed while the positive distance remains `10 mm`, the same geometry gives:

```text
signed_separation_mm = -12
distance_residual_mm = -22
```

This behavior is intentional. A later solver must preserve the persisted target order rather than silently converting planar Distance into an unsigned absolute-distance equation.

## Why the residuals are explicit descriptors

This seed does not hide the equations behind a boolean `is_satisfied` result.

The returned values preserve the geometric error components needed by later solver and diagnostic layers:

```text
Mate
  3 normal-opposition components
  1 signed-separation component

Distance
  3 normal-parallelism components
  1 signed-distance component
```

The vector residuals are represented as `Vector3` values rather than an opaque numeric array. The scalar length residuals retain explicit `_mm` naming.

This block does not yet construct Jacobians, choose residual weighting, define convergence tolerances, or reduce mathematically redundant vector components. Those are solver-layer decisions.

## Supported constraint and target families

The first builder supports:

```text
AssemblyConstraintType::Mate
AssemblyConstraintType::Distance
```

Both targets must resolve through the currently implemented generated planar face family:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
```

The source feature restrictions remain those of `AssemblyConstraintTargetResolver` and `WorkplaneResolver::resolve_generated_face`.

## Explicit failure behavior

Construction fails explicitly when:

- the constraint state is inactive
- the constraint type is Concentric
- target A cannot be resolved
- target B cannot be resolved
- either semantic target token is malformed
- either target belongs to an unsupported semantic reference family
- either target component does not exist
- either referenced part cannot be resolved to a project-owned `PartDocument`
- either generated-face source feature cannot be resolved by the existing target-resolution path
- a defensive Distance record reaches the builder without a length quantity

Inactive constraints fail before target geometry resolution.

Concentric fails before target geometry resolution with:

```text
concentric equation construction requires semantic axis target support
```

This preserves the architecture boundary that full Concentric construction requires a stable semantic axis-reference family.

Target-resolution failures are propagated without rewriting their diagnostics. Target A is resolved before target B, so error precedence is deterministic.

## Read-only and persistence boundary

Equation construction does not:

- mutate either component `RigidTransform`
- update a component instance
- enforce grounding
- infer suppression participation
- change constraint state or target tokens
- add, remove, or rewrite assembly constraint records
- modify part parameters, sketches, features, or derived workplanes
- run part recompute
- own or mutate a `ShapeCache`
- solve an equation
- write a solved transform
- compute remaining degrees of freedom
- persist equation or residual data

The descriptor is regenerated from:

```text
persistent AssemblyConstraint intent
+ current project/part model intent
+ persisted component RigidTransform values
```

No assembly or project JSON schema field is added.

## Tests

`tests/geometry/assembly_constraint_equation_builder_tests.cpp` covers:

- a satisfied planar Mate between a top face and translated bottom face
- preservation of constraint id, type, target component ids, and semantic tokens
- tangentially different Mate frame origins with zero signed plane separation
- unsatisfied Mate normal-opposition and signed-separation residuals
- transformed target geometry after component translation and Z rotation
- a satisfied planar Distance residual
- Distance normal-parallelism residual construction
- explicit target distance preservation
- signed A-to-B separation
- positive Distance residual error
- reversed target order producing the documented negative signed separation
- inactive-constraint rejection
- Concentric rejection before semantic-axis target resolution
- unsupported generated-edge target-family error propagation
- deterministic repeated construction
- unchanged component transforms
- unchanged constraint count and semantic target tokens
- unchanged part derived-workplane count

Targeted test command after a geometry build:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
```

Complete geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

## Deliberate limitations

This block does not implement:

- a rigid-body solver
- transform mutation from residuals
- residual Jacobians
- residual weighting or solver tolerances
- remaining-DOF computation
- underconstrained, fully constrained, or overconstrained analysis
- grounding enforcement
- suppression participation rules
- semantic axis references
- Concentric equation construction
- Insert or other richer constraint families
- joints or motion
- collision/interference analysis
- subassemblies
- component geometry instancing
- assembly-level STEP export

## Next technical step

The next assembly block is a first rigid-body solver seed over the now-stable active-constraint graph, supported target-resolution path, explicit rigid-transform convention, and planar Mate/Distance residual descriptors.

That solver block must define its fixed-component/grounding participation rule, variable transform representation, convergence/error behavior, and transform-update boundary explicitly before mutating component placement.

Concentric remains outside the first solver seed until semantic axis targets and Concentric residual construction exist.
