# Assembly System with Constraints

Status: component instances, explicit placement/state updates, solver-independent Mate/Concentric/Distance records, deterministic active-constraint connectivity, generated-face target resolution, explicit rigid-transform evaluation, planar Mate/Distance residual construction, and the first deterministic rigid-body assembly solver plus explicit result-application boundary are implemented. Remaining-DOF and constraint-state diagnostics are not implemented yet.

The assembly system composes project-owned part models into component occurrences. Persistent records store design intent; graph connectivity, resolved target geometry, evaluated assembly-space frames, residual values, Jacobians, and solve-result descriptors remain regenerable derived data.

## Core idea

A part document defines part model intent once. An assembly holds occurrences of parts, so one owned `PartDocument` can be referenced by several component instances.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

The current assembly pipeline is deliberately layered:

```text
persistent component and constraint intent
  -> active constraint connectivity
  -> semantic target resolution
  -> component-local target frames
  -> explicit rigid-transform evaluation
  -> assembly-space target frames
  -> planar Mate/Distance residual construction
  -> rigid-body solve on a private Project copy
  -> proposed component transforms
  -> explicit atomic successful-result application
  -> future DOF and constraint-state analysis
```

Only the final explicit application boundary changes persisted component placement intent.

## Implemented component instances and free placement

Canonical document: `docs/component-instance-mvp5.md`.

Implemented behavior:

- `AssemblyDocument` owns component instances by value
- each component references a registered member part document
- several component instances may reference one project-owned `PartDocument`
- free-placement translation values are finite millimeters
- free-placement rotation values are finite degrees
- visibility, suppression, and grounding are persisted model intent
- transform and state values can be updated explicitly
- updates preserve component identity and referenced part intent
- direct placement edits do not infer constraints, compute DOF, or trigger part recompute

The transform record is:

```text
RigidTransform
  translation_mm
  rotation_deg
```

Its geometry-evaluation semantics are canonicalized in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Implemented assembly constraint model intent

Canonical document: `docs/assembly-constraint-model-intent-mvp5.md`.

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference

AssemblyConstraint
  id
  name
  type = mate | concentric | distance
  target_a
  target_b
  state = active | inactive
  distance = optional length quantity
```

Record-layer rules:

- constraint ids use `AssemblyConstraintId`
- target component ids and semantic tokens must be non-empty
- both target components must exist in the owning assembly
- constraint ids are unique
- Distance requires a positive length quantity
- Mate and Concentric reject distance values
- inactive constraints remain persisted model intent
- adding or loading a constraint does not change component transforms
- semantic tokens remain opaque at the record layer
- assembly/project JSON persist records through additive `assembly_constraints`
- older JSON without `assembly_constraints` remains loadable
- project structure validation checks component targets

The record layer does not resolve geometry, evaluate transforms, construct residuals, or solve placement.

## Implemented read-only constraint graph

Canonical document: `docs/assembly-constraint-graph-mvp5.md`.

```text
AssemblyConstraintGraph
  nodes = every ComponentInstanceId
  edges = active AssemblyConstraintId records

adjacent_constraints(component)
connected_components()
```

Graph rules:

- every component is a node, including isolated occurrences
- active constraints create distinct edges
- inactive constraints create no edge
- multiple constraints between one component pair remain legal multi-edges
- edge records preserve constraint id and component endpoints without copying geometry
- graph construction defensively revalidates active endpoints
- node order is lexicographic by component id
- edge and adjacency order is lexicographic by constraint id
- connected-group member ids are lexicographically ordered
- connected groups are ordered by their first component id
- construction and queries are read-only

The graph is derived data and is not serialized. Its exact connected groups are the implemented solver partition boundary.

## Implemented semantic assembly target resolution

Canonical document: `docs/assembly-constraint-target-resolution-mvp5.md`.

```text
AssemblyConstraintTargetResolver
  resolve(Project, AssemblyConstraintTarget)
    -> component occurrence
    -> referenced PartDocument
    -> supported SemanticFaceReference
    -> ComponentLocalPlanarDescriptor
    -> separate persisted RigidTransform
```

Implemented rules:

- component ids resolve through the project assembly
- component part references resolve to project-owned `PartDocument` objects
- the supported token family is `feature.<feature-id>.{top,bottom,right,left,front,back}`
- the source feature must be a supported `AdditiveExtrude`
- `WorkplaneResolver::resolve_generated_face` supplies the shared generated-face frame implementation
- local descriptors contain origin, x-axis, y-axis, and normal
- component transforms remain separate placement intent
- malformed and unsupported target families fail explicitly
- semantic axes, generated edges, and generated vertices remain unsupported assembly target families
- resolution does not mutate component, constraint, part, or geometry-cache ownership state
- resolved descriptors are not serialized

Target resolution owns semantic lookup and component-local geometry only.

## Implemented rigid-transform evaluation

Canonical document: `docs/assembly-rigid-transform-evaluation-mvp5.md`.

```text
AssemblyTransformEvaluator
  evaluate_point(RigidTransform, Point3)
  evaluate_vector(RigidTransform, Vector3)
  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
    -> AssemblySpacePlanarDescriptor
```

Canonical convention:

- translation is measured in millimeters
- rotation is measured in degrees
- positive rotations follow the right-hand rule
- rotations are active fixed-axis rotations
- rotations are applied X, then Y, then Z
- column-vector composition is `Rz * Ry * Rx`
- points are rotated and then translated
- vectors, axes, and normals are rotated without translation
- rigid rotation preserves vector magnitude
- unit frame vectors remain unit length and orthogonal within floating-point tolerance

The semantic-target bridge is:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
```

Transform evaluation is read-only and unpersisted.

## Implemented planar Mate/Distance residual construction

Canonical document: `docs/assembly-planar-constraint-equations-mvp5.md`.

```text
AssemblyConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> resolve target A
    -> resolve target B
    -> evaluate target A plane in assembly space
    -> evaluate target B plane in assembly space
    -> construct Mate or Distance residual descriptor
```

Target A and target B order is preserved exactly.

For target planes `oA,nA` and `oB,nB`, Mate residuals are:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

A satisfied Mate has zero normal-opposition and zero signed separation. Tangential origin differences are not residuals because the relationship is between infinite supporting planes.

Planar Distance residuals are:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

A satisfied Distance has zero normal-parallelism and zero distance residual. Signed separation is deliberately target-order dependent and measures B from A along target A's normal.

The builder:

- supports active Mate and Distance records only
- resolves the implemented generated planar face family
- rejects inactive constraints before target resolution
- rejects Concentric before target resolution
- resolves target A before target B
- propagates target-resolution diagnostics unchanged
- preserves constraint id/type and both target identities
- creates no JSON or cache record
- does not solve or mutate transforms

## Implemented first rigid-body assembly solver

Canonical document: `docs/assembly-rigid-body-solver-mvp5.md`.

The geometry-layer API is:

```text
AssemblyRigidBodySolver
  solve(Project, connected_group, options)
    -> AssemblySolveResult

AssemblySolveResultApplier
  apply(Project, converged_result)
    -> applied transform count
```

The solve input must exactly match one deterministic group returned by `AssemblyConstraintGraph::connected_components()`.

### Fixed and variable participation

Solver policy is explicit:

```text
Grounded -> fixed
Free     -> six numeric transform variables
Suppressed -> rejected by the first solver seed
Hidden/Visible -> no solver participation effect
```

At least one component in the group must be grounded. The first solver does not invent a floating-group gauge condition.

Multiple grounded components are allowed and every grounded transform remains fixed.

An all-grounded group returns:

- `Converged` when current residual RMS is within tolerance
- `FixedGeometryInconsistent` when current residual RMS exceeds tolerance

### Variable ordering

Free components use deterministic lexicographic component-id order.

Each free component contributes:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

These are the persisted `RigidTransform` coordinates directly. The solver does not hide a quaternion, matrix, or alternative Euler convention at the API boundary.

Residual evaluation continues through `AssemblyTransformEvaluator`, so the existing X-then-Y-then-Z convention remains authoritative.

### Residual flattening and weighting

Active constraints are consumed in lexicographic `AssemblyConstraintId` order inherited from the graph.

Each supported constraint contributes four numeric residuals.

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

The default length scale is `1.0 mm`. This is the explicit first weighting policy between dimensionless orientation residuals and length residuals.

### Numeric solve method

The first solver uses deterministic damped Gauss-Newton iteration:

```text
evaluate residual r
-> central finite-difference Jacobian J
-> form J^T J + lambda I
-> form -J^T r
-> partial-pivot Gaussian elimination
-> deterministic backtracking line search
-> damping escalation when a step does not decrease RMS
```

Default numeric policy:

```text
length residual scale                 1.0 mm
convergence RMS                       1.0e-8
translation finite-difference step    1.0e-4 mm
rotation finite-difference step       1.0e-4 deg
initial damping                        1.0e-6
maximum iterations                     100
maximum damping attempts               8
maximum line-search steps              12
```

The central-difference column for variable `x_j` is:

```text
J[:,j] = (r(x + h_j e_j) - r(x - h_j e_j)) / (2 h_j)
```

No analytic Jacobian or sparse linear algebra is implemented yet.

### Solve states

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

`MaximumIterationsReached` and `NumericalFailure` retain the current best proposals for diagnostics, but they cannot be applied.

Validation and unsupported-family problems remain `Result<T>` failures.

### Read-only solve and explicit application

`AssemblyRigidBodySolver::solve` changes transforms only on a private `Project` copy.

The source project remains unchanged until `AssemblySolveResultApplier::apply` is explicitly called with a converged result.

The solve result snapshots every group component's:

```text
component id
grounding state
suppression state
source transform
```

This includes fixed grounded anchors.

The applier rejects a stale result when any snapshotted solve input changed after solve. A grounded anchor cannot move between solve and apply without invalidating the old result.

After prevalidation, proposed free-component transforms are applied to another project copy. The caller's project is replaced only after all transform updates succeed.

The application boundary is therefore atomic at the current `Project` value boundary.

Solver results, proposed transforms, Jacobians, damping state, and residual summaries are derived and unpersisted.

## Degrees of freedom

A rigid body has six DOF: translation along X/Y/Z and rotation about X/Y/Z.

The solver now has a deterministic variable ordering and a local numeric Jacobian model, but it does not yet claim a remaining-DOF result.

Jacobian rank tolerance, constrained-DOF count, remaining-DOF count, and under/fully/overconstrained classification remain the next assembly block.

## Part features vs. assembly constraints

A part feature changes part geometry. An assembly constraint describes intended pose relationships between component occurrences or, later, between a component and an assembly reference.

Part features include implemented extrudes/cuts and future holes, fillets, and chamfers. Assembly constraints and the assembly solver must not modify part feature intent.

## Constraint types

| Constraint | Description |
|-----------|-------------|
| Mate | two planar faces together, normals opposed. |
| Flush | two planar faces aligned, normals same direction. |
| Coincident | points, axes, or planes coincide. |
| Concentric | two cylindrical/circular references share an axis. |
| Distance | fixed distance between two references. |
| Angle | fixed angle between planes/axes/faces. |
| Tangent | one face tangent to another. |
| Insert | concentric plus axial seating. |
| Lock | fully fix one component relative to another. |

The persistent record seed includes Mate, Concentric, and Distance. The residual builder and first solver support planar Mate and Distance only.

Persistent targets use semantic references such as `feature.base_extrude.top`; raw OCCT names such as `Face17` are not core model intent.

Semantic axes required for Concentric remain future work.

## Solver pipeline

The implemented sequence is now:

```text
AssemblySolvePipeline
  read component instances
  -> read active constraints
  -> build AssemblyConstraintGraph
  -> select one exact connected group
  -> identify grounded fixed and free variable components
  -> resolve semantic target geometry
  -> evaluate targets in assembly space
  -> build Mate/Distance residual descriptors
  -> flatten deterministic weighted residuals
  -> evaluate numeric Jacobian
  -> solve variable component transforms on a Project copy
  -> return proposed transform updates
  -> explicitly apply a fresh converged result atomically
  -> future remaining-DOF and constraint-state diagnostics
```

Independent connected groups can be solved separately.

## Grounding

`ComponentGroundingState::Grounded` remains persisted model intent.

Storage-level direct transform updates are still allowed while grounded. This preserves the distinction between explicit model edits and solver policy.

The first solver is the first consumer that enforces grounding semantics: grounded components are fixed solve participants.

A group with no grounded component is rejected. Multiple grounded components are allowed and remain fixed.

## Visibility and suppression

Visibility and suppression remain persisted editable states.

Visibility does not affect the first solver.

Suppression still does not alter graph connectivity or residual construction. The first solver therefore rejects a selected connected group containing a suppressed component instead of silently changing graph participation.

A later suppression-aware assembly solve path must define graph and constraint participation together.

Constraint active/inactive state remains separate from component suppression state.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag becomes solver input projected onto valid constraint space.

Future joint families include Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear relation, and Screw relation. Constraints and joints may later carry angle, position, or distance limits.

## Assembly parameters

Assembly parameters already drive member part parameters through `ParameterBinding`.

Future top-down assembly control may bind parameters to constraints and joints:

```text
DistanceConstraint.distance = Assembly.bearing_distance
PrismaticJoint.position    = Assembly.slider_position
RevoluteJoint.angle        = Assembly.crank_angle
```

Direct constraint-to-assembly-parameter binding is not implemented by the current Distance record.

## Assembly features and collision

Assembly features and collision checks remain future additions. They are outside the current component, record, graph, target-resolution, transform-evaluation, residual-construction, and first solver blocks.

## Dependency-graph integration

Future solved constraints should integrate with a solve dependency model:

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

`AssemblyConstraintGraph` is relationship connectivity, not the part recompute `DependencyGraph` and not yet a solve dependency graph.

## MVP scope (MVP 5)

Implemented:

- component instances owned by `AssemblyDocument`
- references to project-owned part documents
- finite free-placement transforms plus component state
- explicit placement/state update APIs
- solver-independent Mate, Concentric, and Distance records
- semantic component target tokens
- active/inactive constraint state
- Distance value validation
- assembly/project JSON roundtrip
- project structure validation for constraint component targets
- deterministic read-only active-constraint graph
- generated-face semantic target resolution
- component-local planar target descriptors
- shared generated-face frame implementation
- explicit rigid-transform evaluation convention
- point, vector, and planar-frame assembly-space evaluation
- distinct assembly-space planar descriptor type
- active planar Mate/Distance equation-residual construction
- canonical Mate and Distance residual conventions
- deterministic target order and failure precedence
- exact connected-group solver input validation
- explicit grounded/fixed and free/variable participation
- explicit suppressed-component rejection and visibility policy
- deterministic residual and variable ordering
- explicit residual length scaling
- central finite-difference Jacobian
- damped Gauss-Newton solve with line search
- solve-state and residual-summary descriptors
- proposed transform results before mutation
- stale solve-input snapshots
- explicit atomic successful-result application
- no persisted graph, resolved-target, evaluated-frame, residual, Jacobian, or solver-result cache data
- focused tests for every implemented assembly block

Next MVP-5 steps:

- local Jacobian-rank and remaining-DOF diagnostics
- underconstrained and locally fully constrained group classification
- explicit initial overconstraint/inconsistency diagnostics

Concentric remains blocked on a stable semantic axis-reference family and Concentric residual construction. Later steps include Insert, Angle, Tangent, joints, limits, collision checks, and subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented: `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs. Implemented: `docs/component-instance-mvp5.md`.
3. Add semantic targets and Mate/Concentric/Distance records. Implemented: `docs/assembly-constraint-model-intent-mvp5.md`.
4. Persist and inspect constraints without changing component transforms. Implemented.
5. Add a read-only active-constraint graph. Implemented: `docs/assembly-constraint-graph-mvp5.md`.
6. Resolve generated-face targets to component-local planar descriptors. Implemented: `docs/assembly-constraint-target-resolution-mvp5.md`.
7. Define and test explicit rigid-transform evaluation. Implemented: `docs/assembly-rigid-transform-evaluation-mvp5.md`.
8. Build planar Mate/Distance equation-residual descriptors. Implemented: `docs/assembly-planar-constraint-equations-mvp5.md`.
9. Add a first rigid-body solver with explicit fixed/variable participation and proposed transforms. Implemented: `docs/assembly-rigid-body-solver-mvp5.md`.
10. Add explicit successful solve-result application. Implemented: `docs/assembly-rigid-body-solver-mvp5.md`.
11. Add read-only Jacobian-rank, remaining-DOF, and local solve-state diagnostics. This is the next step.
12. Add solved-state JSON records only where data is not safely regenerable.
13. Add semantic axis references and Concentric/Insert integration.
14. Add joints and limits, then motion via the solver.
15. Add rigid subassemblies first, flexible subassemblies later, and collision checks.

## Out of scope for the first versions

- persistent solver/Jacobian/DOF cache state before a stable diagnostic data model exists
- joints, motion, and limits before the rigid solver and DOF model are stable
- flexible subassemblies
- collision/interference analysis beyond a later simple static check

## Role in the system

The assembly system connects part modeling, parameters, future hole and shaft assistants, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable semantic relationships and deterministic solve logic, not only visually stacked bodies.
