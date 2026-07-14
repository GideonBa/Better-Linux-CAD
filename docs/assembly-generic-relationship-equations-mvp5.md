# Assembly Generic Relationship Equations and Shared Solve Integration MVP-5

Status: implemented as Block 39 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for Coincident, Parallel, and Perpendicular capability compatibility, residual semantics, local/root-space equation construction, shared numeric integration, freshness/application behavior, Jacobian-rank diagnostics, and the Block-40 handoff.

## Scope

Implemented:

- Block-37 compatibility entries for Coincident, Parallel, and Perpendicular;
- shared capability-driven equation construction over typed resolved targets;
- component-local target normalization through exact authored transform chains;
- root-space hierarchy target reuse without a second coordinate authority;
- Point/Point, Point/Line, Line/Point, Point/Plane, and Plane/Point Coincident residuals;
- Line/Axis direction-pair and Plane-normal Parallel residuals;
- Line/Axis direction-pair and Plane-normal Perpendicular residuals;
- Line/Axis non-planar Angle execution through the same capability builder;
- local `AssemblyConstraintGraph` participation;
- Project-level `AssemblyCrossHierarchyConstraintGraph` participation;
- cross-hierarchy motion-connectivity participation;
- existing deterministic relationship ordering;
- shared residual flattening and length scaling;
- shared central finite differences and damped Gauss-Newton solve;
- existing semantic PartDocument freshness and relationship/group freshness;
- existing atomic local and authority-qualified application;
- existing local and cross-hierarchy Jacobian-rank diagnostics.

Not implemented:

- Point/Point or Point/Plane Distance equations;
- Circle/Cylinder/Frame generic relationship pairs beyond existing historical families;
- tangent, symmetric, midpoint, lock, gear, rack, path, cam, contact, or limit relationships;
- joint target compatibility migration;
- a second graph, optimizer, finite-difference implementation, transform authority, or endpoint model;
- any persistence or JSON change.

## Compatibility boundary

The implemented generic relationship matrix is:

```text
Coincident
  Point <-> Point
  Point <-> Line
  Line  <-> Point
  Point <-> Plane
  Plane <-> Point

Parallel
  Line <-> Line
  Axis <-> Axis
  Line <-> Axis
  Axis <-> Line
  Plane <-> Plane

Perpendicular
  Line <-> Line
  Axis <-> Axis
  Line <-> Axis
  Axis <-> Line
  Plane <-> Plane
```

Capability choice remains deterministic and ordered through `AssemblyTargetCompatibilityResolver`.

For multi-capability targets the first supported matrix pair wins exactly as in Block 37. No source-kind-specific special case is added to equation construction.

## Shared target projection into equation space

`AssemblyGenericRelationshipEquationBuilder` consumes typed `AssemblyResolvedGeometricTarget` values and validates them without changing endpoint or coordinate-space identity.

For a component-local target the selected capability projection is evaluated as:

```text
persistent semantic endpoint
  -> validated typed component-local target
  -> selected Point/Line/Axis/Plane projection
  -> exact transforms_inner_to_outer
  -> AssemblyHierarchyTransformEvaluator
  -> root-assembly equation value
```

An already root-space hierarchy target reuses its selected projected value directly after typed target validation.

The authoritative resolved target remains unchanged and retains:

```text
exact endpoint identity
original ComponentLocal or RootAssembly coordinate space
source kind
source metadata
capability vector
exact transform chain
```

Equation construction transforms only the selected projected point, origin, direction, or normal needed by the residual. It does not relabel a local endpoint as a hierarchy endpoint and does not mutate the resolved target record.

No Euler-angle recomposition, OCCT topology identity, or persisted Geometry product is introduced.

## Coincident residuals

### Point to Point

For ordered target points `pA` and `pB`:

```text
r = pB - pA
```

Residual components: 3 length components.

A satisfied relationship has:

```text
pA == pB
```

### Point to Line

For point `p`, line origin `o`, and unit direction `d`:

```text
r = cross(p - o, d)
```

Residual components: 3 length components.

The ordered `Point <-> Line` and `Line <-> Point` pairs use the same geometric distance semantics. `CoincidentPointLineResidualDescriptor::point_target` records whether the point is Target A or Target B so target order remains explicit in derived diagnostics.

### Point to Plane

For point `p`, plane origin `o`, and unit normal `n`:

```text
r = dot(p - o, n)
```

Residual components: 1 length component.

The ordered `Point <-> Plane` and `Plane <-> Point` pairs likewise record which target owns the point.

## Parallel residual

For two selected unit directions `dA` and `dB`:

```text
r = cross(dA, dB)
```

Residual components: 3 dimensionless components.

For Plane targets:

```text
d = plane.normal
```

For Line and Axis targets:

```text
d = selected line/axis direction
```

Parallel and anti-parallel directions are both accepted.

## Perpendicular residual

For two selected unit directions `dA` and `dB`:

```text
r = dot(dA, dB)
```

Residual components: 1 dimensionless component.

Plane targets use normals; Line and Axis targets use directions.

## Directional Angle closure

Block 37 already selected:

```text
Line <-> Line
Axis <-> Axis
Line <-> Axis
Axis <-> Line
```

for Angle, but pre-Block-39 planar equation construction rejected those non-Plane pairs.

Block 39 closes that compatibility/execution gap through the shared generic equation builder:

```text
direction_dot = dot(dA, dB)
angle_alignment = direction_dot - cos(target_angle_deg)
```

Residual components: 1 dimensionless component.

Existing Plane/Plane `PlanarAngleResidualDescriptor` remains a public compatible direct-equation contract. Local numeric execution may use the equivalent shared directional formulation; cross-hierarchy Plane/Plane direct equation construction retains the historical planar descriptor.

## Residual flattening and scaling

The shared numeric system now recognizes:

```text
Coincident Point/Point  -> 3 scaled length rows
Coincident Point/Line   -> 3 scaled length rows
Coincident Point/Plane  -> 1 scaled length row
Parallel                -> 3 dimensionless rows
Perpendicular           -> 1 dimensionless row
Directional Angle       -> 1 dimensionless row
```

Length components divide by the existing:

```text
length_residual_scale_mm
```

Direction/dot/angle components are already dimensionless and are appended unchanged.

All relationship blocks remain concatenated in existing deterministic graph order.

## Graph and authority integration

The shared graph participation authority now classifies all eight relationship families as equation-enabled:

```text
Mate
Concentric
Distance
Insert
Angle
Coincident
Parallel
Perpendicular
```

Therefore generic families enter:

```text
AssemblyConstraintGraph
AssemblyCrossHierarchyConstraintGraph
AssemblyCrossHierarchyMotionGraph
```

No graph type changes.

Local solving still derives one six-variable block per free active local component in one exact connected group.

Cross-hierarchy solving still derives one six-variable block per unique free active:

```text
ComponentTransformAuthority =
  (assembly_document,
   local ComponentInstanceId)
```

Repeated rooted child occurrences that resolve to the same authority still share one direct transform variable block and at most one proposal.

## Shared numeric execution

Block 39 reuses unchanged:

```text
absolute candidate transform vectors
central finite differences
shared residual evaluator contract
damped Gauss-Newton normal equations
partial-pivot dense solve
damping escalation
backtracking line search
solve-state classification
```

No generic-target-specific optimizer exists.

The local numeric adapter routes Coincident, Parallel, Perpendicular, and Angle through `AssemblyGenericRelationshipEquationBuilder`.

Project-level cross-hierarchy generic relationships are resolved through exact occurrence paths into root space, built by the same generic equation semantics, and flattened through the same residual implementation.

## Freshness and atomic application

No new freshness model is introduced.

Participating generic relationships automatically enter existing relationship snapshots because complete records already store:

```text
id
name
type
target A
target B
state
optional Distance value
optional Angle value
```

Generic relationship types carry neither scalar value.

Existing solve results already snapshot every participating transform authority and the canonical serialized PartDocument intent for every participating referenced part.

Therefore edits to semantic target-producing part model intent stale old generic solve results through the same exact byte-for-byte PartDocument snapshot contract.

Local application remains copy-then-replace after complete freshness validation.

Cross-hierarchy application remains:

```text
validate Project structure
validate authority/proposal freshness
validate relationship snapshots
validate exact current solve group
validate hierarchy boundaries
validate semantic PartDocument snapshots
copy Project
apply direct authority transform proposals
replace source Project
```

No component or authority transform is mutated during equation construction or solving.

## Diagnostics

Local and cross-hierarchy diagnostics continue to build the actual central-difference Jacobian at the converged solution and call:

```text
compute_assembly_matrix_rank
```

No relationship-family DOF table is added.

Example: one Point/Point Coincident relationship against one free rigid component yields three residual rows and, for a regular configuration, Jacobian rank 3. With six transform variables the diagnostics report three constrained DOF and three remaining local DOF because that is the measured Jacobian rank, not because Coincident is assigned a hard-coded DOF count.

## Focused acceptance coverage

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
```

`tests/geometry/assembly_generic_relationship_equation_tests.cpp` proves:

- Point/Point Coincident residual semantics;
- ordered Point/Line and Line/Point Coincident semantics;
- ordered Point/Plane and Plane/Point Coincident semantics;
- Parallel Line/Axis and Plane pairs, including anti-parallel acceptance;
- Perpendicular Line/Axis and Plane pairs;
- Line/Line Angle through Block-37 compatibility;
- local generic relationship graph participation and solve convergence;
- local source immutability and atomic stale-result rejection;
- cross-hierarchy generic relationship graph participation and solve convergence;
- authority-qualified application;
- repeated occurrence endpoints sharing one child transform authority;
- semantic PartDocument edits staling old generic solve results;
- local and cross-hierarchy diagnostics using measured Jacobian rank.

The complete existing Geometry suite remains the regression proof for historical Mate, Distance, Angle, Concentric, Insert, DatumPlane-to-generated-face Mate, and the unified Axis semantics used by DatumAxis, cylindrical-face, circular-edge, and current `.axis` Concentric targets.

## Handoff

Block 40 joint target compatibility and the oriented Frame contract are implemented in `docs/assembly-joint-target-compatibility-mvp5.md`.

Current Revolute remains `Frame <-> Frame`; Axis alone remains incompatible because it has no deterministic reference X direction.

Blocks 41–43 typed coordinates, additive compatible JSON, and shared vector drives are implemented in their canonical joint documents. Blocks 44–47 Prismatic, Cylindrical, Planar, and passive Spherical are implemented in their canonical documents. Blocks 48–63 Body identity, body-scoped recompute/inspection, Body Booleans, and BodyTransform/SketchOwnership intent are implemented; Block 64 General Linear Pattern Geometry is next.
