# Assembly System with Constraints

Status: component instances, explicit free-placement/state updates, solver-independent Mate/Concentric/Distance constraint records, and a deterministic read-only active-constraint graph are implemented. Semantic target geometry resolution and rigid-body solving are not implemented yet.

The assembly system composes parts into assemblies like Inventor or SolidWorks. The implemented MVP-5 blocks store component occurrences, direct placement/state intent, persistent semantic relationship records, and derived relationship connectivity. Later stages will resolve supported semantic target geometry and solve component positions from those relationships.

## Core idea

A part document defines part model intent once. An assembly holds occurrences of parts, so one owned `PartDocument` can be referenced by several component instances.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

The current core stores assembly occurrence and relationship intent plus a regenerable connectivity graph; assembly-level geometry instancing is not implemented yet.

Free-placement transforms are still stored and updated directly. Constraints are persistent records and graph edges expose active connectivity, but neither layer solves or replaces those transforms.

## Implemented component-instance and free-placement blocks

The canonical detailed document for the implemented component-instance records, states, update APIs, validation rules, JSON compatibility, inspection tool, and tests is `docs/component-instance-mvp5.md`.

Implemented assembly behavior relevant to this roadmap:

- `AssemblyDocument` owns component instances by value
- each component instance references a registered member part document
- several component instances may reference the same project-owned `PartDocument`
- free-placement transforms contain finite translation values in millimeters and finite rotation values in degrees
- visibility, suppression, and grounding state are persisted model intent
- transform, visibility, suppression, and grounding can be updated explicitly on an existing component instance
- updates preserve component identity and the referenced part document
- direct placement edits do not infer constraints, compute DOF, or trigger part geometry recompute

Field-by-field component record definitions are intentionally not duplicated here; `docs/component-instance-mvp5.md` is the source of truth for that implemented block.

## Implemented assembly constraint model intent

The canonical detailed document for the implemented constraint records, validation, JSON, project validation, tests, and headless inspection is `docs/assembly-constraint-model-intent-mvp5.md`.

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

- constraint ids are typed through `AssemblyConstraintId`
- target component ids and semantic reference tokens must be non-empty
- both target component instances must already exist in the owning `AssemblyDocument`
- constraint ids are unique within the assembly
- Distance requires a length quantity
- Mate and Concentric reject distance values
- inactive constraints remain persisted but disabled as model intent
- adding or loading a constraint does not change component transforms
- semantic tokens remain opaque at the record layer and are not resolved to OCCT topology
- assembly/project JSON persist the records through additive `assembly_constraints`
- older assembly JSON without `assembly_constraints` remains loadable
- project structure validation includes constraint component targets
- the existing component inspector prints stored constraint type, state, semantic targets, and optional distance

## Implemented read-only constraint graph

The canonical detailed document for graph construction, deterministic query behavior, tests, persistence boundaries, and headless inspection is `docs/assembly-constraint-graph-mvp5.md`.

The implemented core graph is:

```text
AssemblyConstraintGraph
  nodes = every ComponentInstanceId
  edges = active AssemblyConstraintId records

adjacent_constraints(component)
connected_components()
```

Graph rules:

- every component instance is a node, including isolated instances
- active constraints create distinct edges between target A and target B component instances
- inactive constraints create no graph edge
- multiple different constraints between the same component pair remain distinct legal edges
- edge records preserve the constraint id and both component endpoints but do not copy semantic target geometry
- graph construction defensively validates active edge endpoints against the source `AssemblyDocument`
- node order is lexicographic by `ComponentInstanceId`
- edge and adjacency order is lexicographic by `AssemblyConstraintId`
- component ids inside each connected group are lexicographically ordered
- connected groups are ordered by their lexicographically first component id
- graph construction and all queries are read-only with respect to transforms, grounding, constraint state, and part model intent

The graph is derived data. It is not stored in assembly or project JSON because it can be rebuilt exactly from `component_instances` and active `assembly_constraints`.

The headless component inspector prints node count, active-edge count, and deterministic connected groups after normal project structure validation.

## Target component instances

Later versions should extend the current occurrence records toward:

```text
ComponentInstance
  id, name
  referenced_document
  transform
  fixed_state
  visibility, suppression_state, color_override
  constraints
  exposed_references
  degrees_of_freedom
```

State display target: Fixed, Underdefined, Fully constrained, Overconstrained, Suppressed.

## Degrees of freedom

A rigid body has 6 DOF (translate x/y/z, rotate x/y/z). Constraints reduce them. Face-on-face removes translation along the normal and some rotations; axis-on-axis leaves translation along and rotation about the axis. The system should eventually show remaining DOF per component.

DOF analysis is not implemented. The current constraint records and connectivity graph do not claim any DOF effect.

## Part features vs. assembly constraints

A part feature changes a part's geometry (extrude, cut, hole, fillet, chamfer). An assembly constraint does not change part geometry; it describes intended pose relationships between two component instances or, in later versions, between a component and an assembly reference.

## Constraint types

| Constraint | Description |
|-----------|-------------|
| Mate       | two planar faces together, normals opposed. |
| Flush      | two planar faces aligned, normals same direction. |
| Coincident | points, axes, or planes coincide. |
| Concentric | two cylindrical/circular references share an axis. |
| Distance   | fixed distance between two references. |
| Angle      | fixed angle between planes/axes/faces. |
| Tangent    | a face tangent to another (cylinder-plane, cylinder-cylinder). |
| Insert     | concentric + axial seating (screw in hole). |
| Lock       | fully fix one component relative to another. |

The implemented record seed is limited to Mate, Concentric, and Distance. The other rows remain target architecture.

Constraints use semantic references (`docs/semantic-references.md`): `CoverPlate.bottom_mounting_face`, `Screw.main_axis`, `BasePlate.HoleFeature_01.axis` — never raw OCCT face names such as `Face17`.

The record layer validates persistent target tokens. The graph uses only the component endpoint portion of each target. Neither layer resolves semantic geometry or moves components.

## Next block: read-only semantic assembly target resolution

The next assembly step should resolve supported persistent semantic target intent to component-local geometric descriptors before constraint equations or a rigid-body solver are introduced.

Target seed:

```text
AssemblyConstraintTargetResolver
  resolve(Project, AssemblyConstraintTarget)
    -> component occurrence
    -> referenced PartDocument
    -> supported semantic reference
    -> component-local geometric descriptor
```

First-scope rules:

- resolve `ComponentInstanceId` through the project's `AssemblyDocument`
- resolve the component's `referenced_part_document` to the project-owned `PartDocument`
- support the currently implemented generated-face semantic reference family first
- reuse the existing semantic face/workplane geometry path rather than introducing raw OCCT topology ids
- return a component-local planar descriptor containing origin, basis axes, and normal
- keep the component `RigidTransform` available as separate placement intent
- do not silently invent an assembly rotation convention inside the target resolver
- malformed or unsupported semantic tokens must return explicit errors
- semantic axes required for full Concentric solving remain unsupported until a stable axis-reference family exists
- resolution is derived read-only data and must not be serialized
- resolution must not modify component transforms, constraint records, part intent, or cache ownership

## Solver and constraint graph

After supported semantic target resolution is stable, a future solver can compute component transforms from active constraints over connected rigid-body groups, resolved geometry, DOF, transforms, and nonlinear geometric relations.

```text
AssemblySolvePipeline
  read component instances -> read active constraints -> identify fixed components
  -> build constraint graph -> resolve semantic target geometry
  -> build constraint equations -> solve component transforms
  -> detect underdefined -> detect overconstrained
  -> update assembly transforms -> update viewport
```

Independent connected groups identified by `AssemblyConstraintGraph` can later be solved separately.

Rigid-body solving, assembly-space target evaluation, constraint equations, and remaining-DOF computation are not implemented yet.

## Grounding

The current implementation stores and updates `ComponentGroundingState::Grounded` as model intent, but it does not enforce grounding behavior.

A component marked `grounded` can still receive an explicit free-placement transform update. Adding or loading constraints and building the constraint graph also do not enforce grounding. This is intentional for the no-solver MVP boundary.

A later solver stage should warn if no component is grounded and should use grounded components as fixed references.

## Visibility and suppression

Visibility and suppression are persisted and explicitly editable component states.

They do not yet affect assembly geometry execution, export, collision analysis, graph connectivity, or solver participation because those assembly consumers or participation rules are not implemented. Later consumers must define those semantics explicitly instead of inferring them inside storage or graph APIs.

Constraint active/inactive state is separate from component suppression state. The current graph filters on constraint state only.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag becomes input to the solver, which projects it onto the valid constraint space.

Joints describe allowed motion: Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear relation, Screw relation. Joints and some constraints can have limits (`angle_min/max`, `position_min/max`, `distance_min/max`).

## Assembly parameters

Assembly parameters can later drive constraints and joints for top-down control (`docs/parameter-model.md`):

```text
DistanceConstraint.distance = Assembly.bearing_distance
PrismaticJoint.position    = Assembly.slider_position
RevoluteJoint.angle        = Assembly.crank_angle
```

Assembly parameters already drive member part parameters through the MVP-4 `ParameterBinding` seed. Direct constraint-to-assembly-parameter binding is not implemented by the current concrete Distance record.

## Assembly features and collision

Assembly features and collision checks are later additions. They are not part of the component-instance, free-placement, constraint-record, or read-only graph blocks.

## Dependency-graph integration

Future resolved constraints should integrate into a dependency graph: when a referenced face/axis/hole/component changes, affected constraints re-solve.

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

The implemented `AssemblyConstraintGraph` is an assembly relationship-connectivity graph. It is not the part recompute `DependencyGraph` and is not yet a solve dependency graph.

## MVP scope (MVP 5)

Implemented:

- component instances owned by `AssemblyDocument`
- references from component instances to project-owned part documents
- finite free-placement transforms plus visibility, suppression, and grounding state
- explicit transform, visibility, suppression, and grounding update APIs
- solver-independent Mate, Concentric, and Distance relationship records
- semantic component targets using persistent tokens
- active/inactive constraint state
- type-specific Distance value validation
- assembly/project JSON roundtrip for component state and constraints
- project structure validation for constraint component targets
- read-only active-constraint graph
- deterministic graph nodes, edges, adjacency, and connected groups
- isolated component nodes and legal multi-edges
- headless inspection of component placement/state, stored constraints, and graph groups

Next MVP-5 steps:

- read-only semantic target geometry resolution for the supported generated-face family
- explicit assembly rigid-transform evaluation conventions
- constraint equation construction for supported Mate/Distance targets
- rigid-body solver producing transforms
- remaining-DOF display and under/fully/overconstrained analysis

Concentric solving remains blocked on a stable semantic axis-reference family. Then add: Insert, Angle, Tangent constraints; movable joints; limits; collision checks; flexible subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented: see `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs without a solver. Implemented: see `docs/component-instance-mvp5.md`.
3. Add semantic component reference targets and `AssemblyConstraint` records with Mate, Concentric, and Distance. Implemented: see `docs/assembly-constraint-model-intent-mvp5.md`.
4. Persist and inspect constraints without changing component transforms. Implemented: see `docs/assembly-constraint-model-intent-mvp5.md`.
5. Add a read-only active-constraint graph and connected-group queries without geometry resolution. Implemented: see `docs/assembly-constraint-graph-mvp5.md`.
6. Resolve supported generated-face assembly targets to component-local planar descriptors without solving. This is the next step.
7. Define assembly rigid-transform evaluation, build first supported constraint equations, and add a rigid-body solver producing transforms and remaining DOF.
8. Add solved-state JSON records only where they are not regenerable cache data; model intent remains primary.
9. Add semantic axis references and Concentric/Insert integration with hole axes / fastener patterns.
10. Add joints and limits, then motion via the solver.
11. Add subassemblies (rigid first, flexible later) and collision checks.

## Out of scope for the first versions

- geometric constraint solving before stable constraint records, semantic targets, graph structure, and supported target resolution exist
- joints, motion, and limits before the rigid solver is stable
- flexible subassemblies
- collision/interference analysis beyond a simple static check

## Role in the system

The assembly system connects part modeling, parameters, the hole wizard, the shaft wizard, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable constraints, not just visually stacked bodies.
