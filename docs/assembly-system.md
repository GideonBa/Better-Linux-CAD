# Assembly System with Constraints

Status: component instances, explicit free-placement/state updates, and solver-independent Mate/Concentric/Distance constraint records are implemented. Constraint graph construction and rigid-body solving are not implemented yet.

The assembly system composes parts into assemblies like Inventor or SolidWorks. The implemented MVP-5 blocks store component occurrences, direct placement/state intent, and persistent semantic relationship records. Later stages will resolve semantic target geometry and solve component positions from those relationships.

## Core idea

A part document defines part model intent once. An assembly holds occurrences of parts, so one owned `PartDocument` can be referenced by several component instances.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

The current core stores assembly occurrence and relationship intent only; assembly-level geometry instancing is not implemented yet.

Free-placement transforms are still stored and updated directly. Constraints are now persistent records but do not yet solve or replace those transforms.

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

DOF analysis is not implemented. The current constraint records do not claim any DOF effect.

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

The implemented seed is limited to Mate, Concentric, and Distance. The other rows remain target architecture.

Constraints use semantic references (`docs/semantic-references.md`): `CoverPlate.bottom_mounting_face`, `Screw.main_axis`, `BasePlate.HoleFeature_01.axis` — never raw OCCT face names such as `Face17`.

The current record layer validates persistent target tokens without resolving geometry or moving components.

## Next block: read-only constraint graph

The next assembly step should derive component connectivity from the stable persistent records before a solver is introduced.

Target seed:

```text
AssemblyConstraintGraph
  nodes = ComponentInstanceId
  edges = active AssemblyConstraintId

adjacent_constraints(component)
connected_components()
```

Graph rules:

- every component instance is a node, including isolated instances
- active constraints create edges between target A and target B component instances
- inactive constraints create no graph edge
- multiple different constraints between the same component pair remain distinct legal edges
- adjacency and connected-group results must be deterministic
- graph construction is read-only and must not modify transforms, grounding, constraint state, or part model intent
- semantic reference tokens stay on the constraint records; the first graph does not resolve them

## Solver and constraint graph

After the read-only graph seed, the future solver computes component transforms from active constraints over rigid bodies, resolved geometry, DOF, transforms, and nonlinear geometric relations.

```text
AssemblySolvePipeline
  read component instances -> read active constraints -> identify fixed components
  -> build constraint graph -> resolve semantic target geometry
  -> solve component transforms
  -> detect underdefined -> detect overconstrained
  -> update assembly transforms -> update viewport
```

Independent connected groups identified by the graph can later be solved separately.

Neither rigid-body solving nor semantic target geometry resolution is implemented yet.

## Grounding

The current implementation stores and updates `ComponentGroundingState::Grounded` as model intent, but it does not enforce grounding behavior.

A component marked `grounded` can still receive an explicit free-placement transform update. Adding or loading constraints also does not enforce grounding. This is intentional for the no-solver MVP boundary.

A later solver stage should warn if no component is grounded and should use grounded components as fixed references.

## Visibility and suppression

Visibility and suppression are persisted and explicitly editable component states.

They do not yet affect assembly geometry execution, export, collision analysis, or solver participation because those assembly consumers are not implemented. Later consumers must define those semantics explicitly instead of inferring them inside the storage API.

Constraint active/inactive state is separate from component suppression state.

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

Assembly features and collision checks are later additions. They are not part of the component-instance, free-placement, or constraint-record blocks.

## Dependency-graph integration

Future resolved constraints should integrate into a dependency graph: when a referenced face/axis/hole/component changes, affected constraints re-solve.

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

The current constraint-record seed creates no solver dependency edges. The next read-only connectivity graph is an assembly relationship graph, not the part recompute `DependencyGraph` and not yet a solve graph.

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
- headless inspection of component placement/state and stored constraints

Next MVP-5 steps:

- read-only constraint graph with active edges, deterministic adjacency, and connected groups
- semantic target geometry resolution
- rigid-body solver producing transforms
- remaining-DOF display and under/fully/overconstrained analysis

Then add: Insert, Angle, Tangent constraints; movable joints; limits; collision checks; flexible subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented: see `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs without a solver. Implemented: see `docs/component-instance-mvp5.md`.
3. Add semantic component reference targets and `AssemblyConstraint` records with Mate, Concentric, and Distance. Implemented: see `docs/assembly-constraint-model-intent-mvp5.md`.
4. Persist and inspect constraints without changing component transforms. Implemented: see `docs/assembly-constraint-model-intent-mvp5.md`.
5. Add a read-only active-constraint graph and connected-group queries without geometry resolution. This is the next step.
6. Resolve supported semantic target geometry and add a first rigid-body solver producing transforms and remaining DOF.
7. Add solved-state JSON records only where they are not regenerable cache data; model intent remains primary.
8. Add Insert constraints and integration with hole axes / fastener patterns.
9. Add joints and limits, then motion via the solver.
10. Add subassemblies (rigid first, flexible later) and collision checks.

## Out of scope for the first versions

- geometric constraint solving before stable constraint records, semantic targets, and graph structure exist
- joints, motion, and limits before the rigid solver is stable
- flexible subassemblies
- collision/interference analysis beyond a simple static check

## Role in the system

The assembly system connects part modeling, parameters, the hole wizard, the shaft wizard, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable constraints, not just visually stacked bodies.
