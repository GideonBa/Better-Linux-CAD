# Assembly System with Constraints

Status: target architecture with component instances and explicit free-placement/state updates implemented. Assembly constraint records and constraint solving are still not implemented.

The assembly system composes parts into assemblies like Inventor or SolidWorks. The implemented MVP-5 blocks store component instances that reference project-owned part documents and allow direct placement/state edits. Later stages will describe spatial relationships through constraints and solve component positions from them.

## Core idea

A part document defines part model intent once. An assembly holds occurrences of parts, so one owned `PartDocument` can be referenced by several component instances.

```text
PartDocument Screw_M6x25
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

The current core stores assembly occurrence intent only; assembly-level geometry instancing is not implemented yet.

The current implementation stores and updates free-placement transforms directly. Later solver work will make instance positions depend on constraints.

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

Field-by-field record definitions and JSON examples are intentionally not duplicated here; `docs/component-instance-mvp5.md` is the source of truth for the implemented block.

## Target component instances

Later versions should extend the current records toward:

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

DOF analysis is not implemented in the current free-placement block.

## Part features vs. assembly constraints

A part feature changes a part's geometry (extrude, cut, hole, fillet, chamfer). An assembly constraint does not change part geometry; it describes the pose relationship between two component instances or between a component and an assembly reference.

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

Constraints should work on semantic references (`docs/semantic-references.md`): `CoverPlate.bottom_mounting_face`, `Screw.main_axis`, `BasePlate.HoleFeature_01.axis` — never raw OCCT face names such as `Face17`.

The next model-intent seed should start with Mate, Concentric, and Distance only:

```text
AssemblyConstraint
  id, name, constraint_type
  target_a
    component_instance
    semantic_reference
  target_b
    component_instance
    semantic_reference
  active_state
  distance_parameter       # Distance only
```

The record layer should validate persistent intent without resolving geometry or moving components.

## Solver and constraint graph

The future solver computes component transforms from active constraints over rigid bodies, DOF, transforms, and nonlinear geometric relations.

```text
AssemblySolvePipeline
  read component instances -> read active constraints -> identify fixed components
  -> build constraint graph -> solve component transforms
  -> detect underdefined -> detect overconstrained
  -> update assembly transforms -> update viewport
```

A separate constraint graph should reveal connected component groups; independent groups can be solved separately.

Neither the constraint graph nor solver is implemented yet.

## Grounding

The current implementation stores and updates `ComponentGroundingState::Grounded` as model intent, but it does not enforce grounding behavior.

A component marked `grounded` can still receive an explicit free-placement transform update. This is intentional for the no-solver MVP boundary.

A later solver stage should warn if no component is grounded and should use grounded components as fixed references.

## Visibility and suppression

Visibility and suppression are persisted and explicitly editable component states.

They do not yet affect assembly geometry execution, export, collision analysis, or solver participation because those assembly consumers are not implemented. Later consumers must define those semantics explicitly instead of inferring them inside the storage API.

## Motion, joints, and limits

Underdefined components should eventually be draggable only within allowed DOF. A drag becomes input to the solver, which projects it onto the valid constraint space.

Joints describe allowed motion: Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear relation, Screw relation. Joints and some constraints can have limits (`angle_min/max`, `position_min/max`, `distance_min/max`).

## Assembly parameters

Assembly parameters can drive constraints and joints for top-down control (`docs/parameter-model.md`):

```text
DistanceConstraint.distance = Assembly.bearing_distance
PrismaticJoint.position    = Assembly.slider_position
RevoluteJoint.angle        = Assembly.crank_angle
```

Assembly parameters already drive member part parameters through the MVP-4 `ParameterBinding` seed.

## Assembly features and collision

Assembly features and collision checks are later additions. They are not part of the component-instance or free-placement update blocks.

## Dependency-graph integration

Future constraints should integrate into a dependency graph: when a referenced face/axis/hole/component changes, affected constraints re-solve.

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

The next constraint-record seed should not create these solver dependency edges yet. It should first make the relationship intent persistent and validated.

## MVP scope (MVP 5)

Implemented:

- component instances owned by `AssemblyDocument`
- references from component instances to project-owned part documents
- finite free-placement transforms plus visibility, suppression, and grounding state
- explicit transform, visibility, suppression, and grounding update APIs
- update validation for empty or missing component instance ids and non-finite transforms
- placement/state updates that preserve instance identity and part-document references
- JSON roundtrip through assembly/project JSON for updated placement/state
- project validation for component instance references after updates
- headless project component inspection of persisted placement/state

Next MVP-5 steps:

- semantic component reference targets for assembly relationships
- solver-independent `AssemblyConstraint` model-intent records for Mate, Concentric, and Distance
- assembly/project JSON roundtrip and inspection for those constraint records
- a first constraint graph and rigid-body solver
- simple remaining-DOF display

Then add: Insert, Angle, Tangent constraints; movable joints; limits; collision checks; flexible subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented: see `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs without a solver. Implemented: see `docs/component-instance-mvp5.md`.
3. Add semantic component reference targets and `AssemblyConstraint` records with Mate, Concentric, and Distance. This is the next step.
4. Persist and inspect constraints without changing component transforms.
5. Add the constraint graph and a rigid-body solver producing transforms and remaining DOF.
6. Add solved-state JSON records only where they are not regenerable cache data; model intent remains primary.
7. Add Insert constraints and integration with hole axes / fastener patterns.
8. Add joints and limits, then motion via the solver.
9. Add subassemblies (rigid first, flexible later) and collision checks.

## Out of scope for the first versions

- geometric constraint solving before stable constraint model-intent records and semantic targets exist
- joints, motion, and limits before the rigid solver is stable
- flexible subassemblies
- collision/interference analysis beyond a simple static check

## Role in the system

The assembly system connects part modeling, parameters, the hole wizard, the shaft wizard, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable constraints, not just visually stacked bodies.
