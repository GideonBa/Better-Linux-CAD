# Assembly System with Constraints

Status: target architecture with the first MVP-5 component-instance seed implemented. Constraint solving is still not implemented.

The assembly system composes parts into assemblies like Inventor or SolidWorks. The first implemented seed stores component instances that reference project-owned part documents. Later stages will describe spatial relationships through constraints and solve component positions from them.

## Core idea

A part document describes one part. An assembly holds instances of parts, so one part can appear many times without duplicating its geometry.

```text
PartDocument Screw_M6x25          # defines geometry once
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

The first seed stores free-placement transforms directly. Later solver work will make instance positions depend on constraints.

## Implemented component-instance seed

Detailed document: `docs/component-instance-mvp5.md`

Implemented now:

```text
ComponentInstance
  id
  name
  referenced_part_document
  visibility
  suppression_state
  grounding_state
  transform

RigidTransform
  translation_mm
  rotation_deg
```

Implemented states:

```text
ComponentVisibility: visible | hidden
ComponentSuppressionState: active | suppressed
ComponentGroundingState: free | grounded
```

The implemented seed validates that a component instance references an assembly member part and that project-level validation resolves that part to an owned project part document.

## Target component instances

Later versions should extend the current seed toward:

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

This is not implemented in the component-instance seed.

## Part features vs. assembly constraints

A part feature changes a part's geometry (extrude, cut, hole, fillet, chamfer). An assembly constraint does not change geometry; it describes the pose relationship between two components or between a component and an assembly reference.

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

```text
AssemblyConstraint
  id, name, constraint_type
  component_a, reference_a
  component_b, reference_b
  parameters
  active_state, solve_status
```

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

## Grounding

The current seed stores `ComponentGroundingState::Grounded` as model intent, but it does not enforce grounding behavior.

A later solver stage should warn if no component is grounded and should use grounded components as fixed references.

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

Assembly features and collision checks are later additions. They are not part of the component-instance seed.

## Dependency-graph integration

Future constraints should integrate into a dependency graph: when a referenced face/axis/hole/component changes, affected constraints re-solve.

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

## MVP scope (MVP 5)

Implemented first seed:

- component instances owned by `AssemblyDocument`
- references from component instances to project-owned part documents
- visibility, suppression, grounding state, and free-placement transform records
- JSON roundtrip through assembly/project JSON
- project validation for component instance references
- headless project component inspection

Next MVP-5 steps:

- explicit component placement/state update APIs
- semantic component references for constraint targets
- Mate, Concentric, and Distance constraint records
- a first constraint graph and rigid-body solver
- simple remaining-DOF display

Then add: Insert, Angle, Tangent constraints; movable joints; limits; collision checks; flexible subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding. Implemented seed: see `docs/component-instance-mvp5.md`.
2. Add explicit component placement/state update APIs without a solver.
3. Add `AssemblyConstraint` with Mate, Concentric, Distance on semantic references.
4. Add the constraint graph and a rigid-body solver producing transforms and remaining DOF.
5. Add JSON serialization and roundtrip tests for constraints and solved structure.
6. Add Insert constraints and integration with hole axes / fastener patterns.
7. Add joints and limits, then motion via the solver.
8. Add subassemblies (rigid first, flexible later) and collision checks.

## Out of scope for the first versions

- mate/concentric/distance constraints before stable component placement records.
- joints, motion, and limits before the rigid solver is stable.
- flexible subassemblies.
- collision/interference analysis beyond a simple static check.

## Role in the system

The assembly system connects part modeling, parameters, the hole wizard, the shaft wizard, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable constraints, not just visually stacked bodies.
