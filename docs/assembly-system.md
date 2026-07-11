# Assembly System with Constraints

Status: component instances, explicit free-placement/state updates, solver-independent Mate/Concentric/Distance constraint records, a deterministic read-only active-constraint graph, generated-face semantic target resolution, and explicit read-only rigid-transform evaluation are implemented. Constraint equation construction and rigid-body solving are not implemented yet.

The assembly system composes project-owned part models into component occurrences. Persistent records store design intent; connectivity, resolved target geometry, and evaluated assembly-space frames remain regenerable derived data.

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
  -> future equation construction
  -> future rigid-body solver
```

No implemented derived layer replaces or mutates persisted component placement intent.

## Implemented component instances and free placement

The canonical detailed document is `docs/component-instance-mvp5.md`.

Implemented behavior:

- `AssemblyDocument` owns component instances by value
- each component instance references a registered member part document
- several component instances may reference the same project-owned `PartDocument`
- free-placement transforms contain finite translation values in millimeters and finite rotation values in degrees
- visibility, suppression, and grounding state are persisted model intent
- transform, visibility, suppression, and grounding can be updated explicitly
- component identity and referenced part intent are preserved across updates
- direct placement edits do not infer constraints, compute DOF, or trigger part geometry recompute

The transform record is:

```text
RigidTransform
  translation_mm
  rotation_deg
```

The exact geometry-evaluation convention for this record is now implemented separately in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Implemented assembly constraint model intent

The canonical detailed document is `docs/assembly-constraint-model-intent-mvp5.md`.

The current record layer includes:

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

Implemented rules:

- constraint ids use `AssemblyConstraintId`
- target component ids and semantic reference tokens must be non-empty
- both target components must already exist in the owning `AssemblyDocument`
- constraint ids are unique within the assembly
- Distance requires a length quantity
- Mate and Concentric reject distance values
- inactive constraints remain persisted but disabled as model intent
- adding or loading a constraint does not change component transforms
- semantic tokens remain opaque at the record layer
- assembly/project JSON persist the records through additive `assembly_constraints`
- older assembly JSON without `assembly_constraints` remains loadable
- project structure validation includes constraint component targets

The record layer stores relationship intent only. It does not resolve geometry or construct equations.

## Implemented read-only constraint graph

The canonical detailed document is `docs/assembly-constraint-graph-mvp5.md`.

```text
AssemblyConstraintGraph
  nodes = every ComponentInstanceId
  edges = active AssemblyConstraintId records

adjacent_constraints(component)
connected_components()
```

Graph rules:

- every component instance is a node, including isolated instances
- active constraints create distinct edges
- inactive constraints create no graph edge
- multiple constraints between the same component pair remain legal multi-edges
- edge records preserve constraint id and component endpoints without copying target geometry
- graph construction revalidates active edge endpoints
- node order is lexicographic by `ComponentInstanceId`
- edge and adjacency order is lexicographic by `AssemblyConstraintId`
- component ids inside connected groups are lexicographically ordered
- connected groups are ordered by their first component id
- graph construction and queries are read-only

The graph is derived data and is not serialized.

## Implemented semantic assembly target resolution

The canonical detailed document is `docs/assembly-constraint-target-resolution-mvp5.md`.

The geometry-layer path is:

```text
AssemblyConstraintTargetResolver
  resolve(Project, AssemblyConstraintTarget)
    -> component occurrence
    -> referenced PartDocument
    -> supported SemanticFaceReference
    -> component-local planar descriptor
    -> separate persisted RigidTransform
```

Implemented rules:

- `ComponentInstanceId` resolves through the project assembly
- the component's `referenced_part_document` resolves to the project-owned `PartDocument`
- the supported token family is `feature.<feature-id>.{top,bottom,right,left,front,back}`
- the source feature must resolve in the referenced part and currently be a supported `AdditiveExtrude`
- `WorkplaneResolver::resolve_generated_face` supplies the shared generated-face frame implementation
- the local descriptor contains origin, x-axis, y-axis, and normal
- the component `RigidTransform` is returned separately
- malformed and unsupported target families fail explicitly
- semantic axes, generated edges, and generated vertices remain unsupported assembly target families
- resolution does not mutate component transforms, constraints, part model intent, or geometry cache ownership
- resolved descriptors are not serialized

Target resolution deliberately does not apply component placement. That responsibility belongs to the transform evaluator.

## Implemented rigid-transform evaluation

The canonical detailed document is `docs/assembly-rigid-transform-evaluation-mvp5.md`.

The evaluator API is:

```text
AssemblyTransformEvaluator
  evaluate_point(RigidTransform, Point3)
  evaluate_vector(RigidTransform, Vector3)
  evaluate_plane(RigidTransform, ComponentLocalPlanarDescriptor)
    -> AssemblySpacePlanarDescriptor
```

The exact convention is:

- translation values are millimeters
- rotation values are degrees
- positive rotations follow the right-hand rule
- rotations are active fixed-axis rotations
- rotation order is X, then Y, then Z
- for column vectors the composition is `Rz * Ry * Rx`
- points are rotated and then translated
- vectors, axes, and normals are rotated without translation
- arbitrary vector magnitude is preserved
- unit frame vectors remain unit length and orthogonal within floating-point tolerance

The transform bridge from semantic targets is now explicit:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver::resolve(...)
  -> ResolvedAssemblyConstraintTarget.local_plane
     + ResolvedAssemblyConstraintTarget.component_transform
  -> AssemblyTransformEvaluator::evaluate_plane(...)
  -> AssemblySpacePlanarDescriptor
```

`AssemblySpacePlanarDescriptor` is distinct from `ComponentLocalPlanarDescriptor` so coordinate space remains part of the type-level API vocabulary.

Evaluation is read-only and unpersisted. It never updates the stored `RigidTransform`.

## Degrees of freedom

A rigid body has six DOF: translation along X/Y/Z and rotation about X/Y/Z.

Face-on-face, axis-on-axis, and other relationships eventually reduce those DOF. The current record, graph, target-resolution, and transform-evaluation layers do not claim any DOF effect.

DOF analysis remains unimplemented.

## Part features vs. assembly constraints

A part feature changes part geometry. An assembly constraint describes intended pose relationships between component occurrences or, later, between a component and an assembly reference.

Part features include extrudes, cuts, future holes, fillets, and chamfers. Assembly constraints must not modify part feature intent.

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

The implemented record seed is limited to Mate, Concentric, and Distance.

Persistent targets use semantic references such as `feature.base_extrude.top`; raw OCCT topology names such as `Face17` are not core model intent.

The geometry resolver currently interprets only the generated planar face family. Semantic axes required for Concentric remain future work.

## Next block: read-only planar Mate/Distance equation construction

The next assembly block should construct deterministic geometric equation/residual data over the now-available assembly-space planar target frames.

Target seed:

```text
AssemblyConstraintEquationBuilder
  build(Project, AssemblyConstraint)
    -> resolve target A
    -> resolve target B
    -> evaluate target A plane in assembly space
    -> evaluate target B plane in assembly space
    -> construct supported equation/residual descriptor
```

First-scope rules:

- operate read-only on one persisted constraint record
- support active Mate and Distance constraints only
- require both targets to resolve through the implemented generated planar face family
- reuse `AssemblyConstraintTargetResolver`
- reuse `AssemblyTransformEvaluator`
- define one canonical planar Mate residual convention explicitly
- define one canonical signed planar Distance separation convention explicitly
- preserve constraint id/type and target/component identities in returned descriptors
- reject inactive constraints explicitly in the first seed
- reject Concentric until semantic axis references are implemented
- keep equation/residual descriptors derived and unpersisted
- do not solve equations or mutate transforms

The exact residual semantics must be documented together with the implementation rather than inferred later by a solver.

## Solver pipeline

After equation construction is stable, a future solver can operate over connected rigid-body groups.

```text
AssemblySolvePipeline
  read component instances
  -> read active constraints
  -> identify fixed components
  -> build constraint graph
  -> resolve semantic target geometry
  -> evaluate resolved targets in assembly space
  -> build constraint equations
  -> solve component transforms
  -> detect underdefined / fully constrained / overconstrained state
  -> update assembly transforms
  -> update viewport
```

Independent connected groups identified by `AssemblyConstraintGraph` can later be solved separately.

Rigid-body solving and remaining-DOF computation are not implemented.

## Grounding

`ComponentGroundingState::Grounded` is persisted model intent but is not yet enforced.

A grounded component can still receive an explicit free-placement transform update. Constraint creation, graph construction, target resolution, and transform evaluation also do not enforce grounding.

A future solver should treat grounded components as fixed rigid-body references and should diagnose solve groups without an adequate fixed reference where required.

## Visibility and suppression

Visibility and suppression are persisted, editable component states.

They do not yet affect assembly geometry execution, export, graph connectivity, target resolution, transform evaluation, or solver participation. Future consumers must define those semantics explicitly.

Constraint active/inactive state is separate from component suppression state. The graph filters on constraint state only.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag becomes solver input projected onto the valid constraint space.

Future joint families include Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear relation, and Screw relation. Joints and constraints may later carry angle, position, or distance limits.

## Assembly parameters

Assembly parameters already drive member part parameters through the MVP-4 `ParameterBinding` seed.

Future top-down assembly control may bind parameters to constraints and joints:

```text
DistanceConstraint.distance = Assembly.bearing_distance
PrismaticJoint.position    = Assembly.slider_position
RevoluteJoint.angle        = Assembly.crank_angle
```

Direct constraint-to-assembly-parameter binding is not implemented by the current Distance record.

## Assembly features and collision

Assembly features and collision checks remain future additions. They are outside the current component, constraint-record, graph, target-resolution, and transform-evaluation blocks.

## Dependency-graph integration

Future solved constraints should integrate with a solve dependency model:

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

The implemented `AssemblyConstraintGraph` is relationship connectivity, not the part recompute `DependencyGraph` and not yet a solve dependency graph.

## MVP scope (MVP 5)

Implemented:

- component instances owned by `AssemblyDocument`
- references from component instances to project-owned part documents
- finite free-placement transforms plus visibility, suppression, and grounding state
- explicit transform/state update APIs
- solver-independent Mate, Concentric, and Distance records
- semantic component target tokens
- active/inactive constraint state
- Distance value validation
- assembly/project JSON roundtrip for components and constraints
- project structure validation for constraint component targets
- read-only active-constraint graph
- deterministic graph nodes, edges, adjacency, and connected groups
- generated-face target resolution through project/component/part ownership
- component-local planar target descriptors
- shared generated-face workplane frame implementation
- explicit rigid-transform evaluation convention
- point, vector, and planar-frame component-local-to-assembly-space evaluation
- distinct assembly-space planar descriptor type
- no persisted graph, target-resolution, or transform-evaluation cache data
- focused test coverage for every implemented assembly block

Next MVP-5 steps:

- planar Mate/Distance equation/residual construction
- rigid-body solver producing transforms
- remaining-DOF display and under/fully/overconstrained analysis

Concentric equation construction remains blocked on a stable semantic axis-reference family. Later steps include Insert, Angle, Tangent, joints, limits, collision checks, and subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented: `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs. Implemented: `docs/component-instance-mvp5.md`.
3. Add semantic component targets and Mate/Concentric/Distance records. Implemented: `docs/assembly-constraint-model-intent-mvp5.md`.
4. Persist and inspect constraints without changing component transforms. Implemented.
5. Add a read-only active-constraint graph. Implemented: `docs/assembly-constraint-graph-mvp5.md`.
6. Resolve supported generated-face targets to component-local planar descriptors. Implemented: `docs/assembly-constraint-target-resolution-mvp5.md`.
7. Define and test explicit rigid-transform evaluation for points, vectors, and planar frames. Implemented: `docs/assembly-rigid-transform-evaluation-mvp5.md`.
8. Build first supported planar Mate/Distance equation/residual descriptors. This is the next step.
9. Add a rigid-body solver producing transforms and remaining DOF.
10. Add solved-state JSON records only where data is not safely regenerable.
11. Add semantic axis references and Concentric/Insert integration.
12. Add joints and limits, then motion via the solver.
13. Add rigid subassemblies first, flexible subassemblies later, and collision checks.

## Out of scope for the first versions

- solver mutation before stable records, semantic targets, connectivity, transform evaluation, and equation construction exist
- joints, motion, and limits before the rigid solver is stable
- flexible subassemblies
- collision/interference analysis beyond a later simple static check

## Role in the system

The assembly system connects part modeling, parameters, future hole and shaft assistants, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable semantic relationships and deterministic solve logic, not only visually stacked bodies.
