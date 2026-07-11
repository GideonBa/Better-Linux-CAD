# Assembly System with Constraints

Status: component instances, explicit placement/state updates, solver-independent Mate/Concentric/Distance records, deterministic active-constraint connectivity, generated-face target resolution, explicit rigid-transform evaluation, and read-only planar Mate/Distance equation-residual construction are implemented. Rigid-body solving is not implemented yet.

The assembly system composes project-owned part models into component occurrences. Persistent records store design intent; graph connectivity, resolved target geometry, evaluated assembly-space frames, and current residual values remain regenerable derived data.

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
  -> future rigid-body solver
  -> future DOF and constraint-state analysis
```

The implemented derived layers do not replace or mutate persisted component placement intent.

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
- Distance requires a length quantity
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

The graph is derived data and is not serialized. Connected groups are the intended future solver partition boundary.

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

The geometry-layer API is:

```text
AssemblyConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> resolve target A
    -> resolve target B
    -> evaluate target A plane in assembly space
    -> evaluate target B plane in assembly space
    -> construct Mate or Distance residual descriptor
```

The builder returns:

```text
AssemblyConstraintEquationDescriptor
  constraint
  type
  target_a
    component_instance
    semantic_reference
    assembly-space plane
  target_b
    component_instance
    semantic_reference
    assembly-space plane
  residual
```

Target A and target B order is preserved exactly.

### Mate convention

For assembly-space target planes:

```text
oA, nA = target A origin and unit normal
oB, nB = target B origin and unit normal
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

Tangential differences between target frame origins are not Mate residuals because the current relationship is between infinite supporting planes.

### Distance convention

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

The signed separation is deliberately target-order dependent. It measures B from A along target A's normal. Swapping targets can therefore change the sign and the resulting Distance residual.

`cross(nA, nB) = 0` accepts parallel planes with equal or opposite normal direction. Distance does not impose Mate-style normal opposition.

### Builder participation and failures

The first residual builder:

- supports active Mate and Distance records only
- resolves only the implemented generated planar face family
- rejects inactive constraints before target resolution
- rejects Concentric before target resolution because semantic axis targets are not implemented
- resolves target A before target B, giving deterministic failure precedence
- propagates target-resolution diagnostics unchanged
- defensively rejects a Distance record without a length quantity
- preserves constraint id/type and both target identities
- creates no JSON or cache record
- does not solve or mutate transforms

The vector residuals remain explicit `Vector3` values. Residual flattening, component weighting, Jacobians, convergence tolerances, and nonlinear solve policy are deliberately deferred to the solver layer.

## Degrees of freedom

A rigid body has six DOF: translation along X/Y/Z and rotation about X/Y/Z.

Face and axis relationships eventually reduce those DOF. The current record, graph, target-resolution, transform-evaluation, and residual-construction layers do not claim a remaining-DOF result.

DOF analysis remains unimplemented.

## Part features vs. assembly constraints

A part feature changes part geometry. An assembly constraint describes intended pose relationships between component occurrences or, later, between a component and an assembly reference.

Part features include implemented extrudes/cuts and future holes, fillets, and chamfers. Assembly constraints must not modify part feature intent.

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

The persistent record seed includes Mate, Concentric, and Distance. The current residual builder supports planar Mate and Distance only.

Persistent targets use semantic references such as `feature.base_extrude.top`; raw OCCT names such as `Face17` are not core model intent.

Semantic axes required for Concentric remain future work.

## Next block: first rigid-body solver seed

The next assembly block should consume deterministic connected groups and the implemented planar residual descriptors.

Target architecture:

```text
AssemblyRigidBodySolver
  solve(Project, AssemblyConstraintGraph connected group)
    -> identify fixed and variable components
    -> collect supported active constraints in deterministic order
    -> evaluate planar Mate/Distance residual descriptors
    -> optimize variable rigid transforms
    -> return solve-result descriptor

AssemblySolveResult
  state
  iterations
  final residual/error summary
  proposed component transforms
```

First-scope rules to define before implementation:

- use one deterministic connected graph group as solver input
- treat grounded components as fixed references in the first seed
- define explicit behavior for groups with zero grounded components
- define explicit behavior for groups with multiple grounded components
- preserve the documented persisted `RigidTransform` convention at the solver API boundary
- define the internal variable transform representation explicitly
- consume only active Mate/Distance constraints supported by `AssemblyConstraintEquationBuilder`
- flatten residuals in deterministic constraint-id and component order
- document residual component ordering
- define residual weighting before mixing orientation and millimeter components numerically
- define convergence tolerance, maximum iterations, and non-convergence behavior
- return proposed transforms before applying model mutation
- provide a separate explicit application/update boundary
- leave project model state unchanged on failed or non-converged solves
- keep Concentric outside the first solver seed

The solver must not silently infer semantics from grounding, suppression, or residual units. Those policies belong in explicit solver contracts.

## Solver pipeline

The intended sequence is now:

```text
AssemblySolvePipeline
  read component instances
  -> read active constraints
  -> build AssemblyConstraintGraph
  -> select one connected group
  -> identify fixed components
  -> resolve semantic target geometry
  -> evaluate targets in assembly space
  -> build Mate/Distance residual descriptors
  -> solve variable component transforms
  -> return proposed transform updates
  -> explicitly apply successful updates
  -> later compute remaining DOF and constraint state
```

Independent connected groups can be solved separately.

Rigid-body solving and remaining-DOF computation are not implemented yet.

## Grounding

`ComponentGroundingState::Grounded` is persisted model intent but is not enforced by current storage or geometry consumers.

A grounded component can still receive a direct explicit transform update. Constraint creation, graph construction, target resolution, transform evaluation, and residual construction also do not enforce grounding.

The first solver seed should define grounded components as fixed solve participants. This must be a solver policy, not a retroactive side effect in the storage or geometry layers.

## Visibility and suppression

Visibility and suppression are persisted editable states.

They do not currently affect assembly geometry execution, export, graph connectivity, target resolution, transform evaluation, residual construction, or solver participation.

Constraint active/inactive state is separate from component suppression state. The graph and residual path filter by constraint state, not component suppression.

A future solver must define suppression participation explicitly.

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

Assembly features and collision checks remain future additions. They are outside the current component, record, graph, target-resolution, transform-evaluation, and residual-construction blocks.

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
- canonical Mate normal-opposition and signed-separation residuals
- canonical Distance normal-parallelism and signed A-to-B separation residuals
- deterministic target order and failure precedence
- no persisted graph, resolved-target, evaluated-frame, or residual cache data
- focused tests for every implemented assembly block

Next MVP-5 steps:

- first rigid-body solver producing proposed component transforms
- explicit successful solve-result application boundary
- remaining-DOF display and under/fully/overconstrained analysis

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
9. Add a first rigid-body solver producing proposed transforms with explicit fixed/variable participation. This is the next step.
10. Add explicit successful solve-result application and remaining-DOF analysis.
11. Add solved-state JSON records only where data is not safely regenerable.
12. Add semantic axis references and Concentric/Insert integration.
13. Add joints and limits, then motion via the solver.
14. Add rigid subassemblies first, flexible subassemblies later, and collision checks.

## Out of scope for the first versions

- implicit solver mutation before fixed/variable participation, residual weighting, convergence, and application semantics are explicit
- joints, motion, and limits before the rigid solver is stable
- flexible subassemblies
- collision/interference analysis beyond a later simple static check

## Role in the system

The assembly system connects part modeling, parameters, future hole and shaft assistants, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable semantic relationships and deterministic solve logic, not only visually stacked bodies.
