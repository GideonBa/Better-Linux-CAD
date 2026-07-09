# Assembly System with Constraints

Status: target architecture. Not implemented yet. MVP 5 in `docs/mvp-plan.md` starts the minimal version.

The assembly system composes parts into assemblies like Inventor or SolidWorks: it describes spatial relationships between parts through constraints, then solves component positions from them. It has its own data model for component instances, references, degrees of freedom, constraints, a solver, motion, subassemblies, and assembly parameters.

## Core idea

A part document describes one part. An assembly holds instances of parts, so one part can appear many times without duplicating its geometry.

```text
PartDocument Screw_M6x25          # defines geometry once
AssemblyDocument HousingAssembly
  ComponentInstance Screw_01 -> Screw_M6x25
  ComponentInstance Screw_02 -> Screw_M6x25
```

Each instance has a position determined by constraints, not only a free transform.

## Component instances

```text
ComponentInstance
  id, name
  referenced_document
  transform
  fixed_state          # grounded/fixed vs. free
  visibility, suppression_state, color_override
  constraints
  exposed_references
  degrees_of_freedom   # computed by solver
```

State display: Fixed, Underdefined, Fully constrained, Overconstrained, Suppressed.

## Degrees of freedom

A rigid body has 6 DOF (translate x/y/z, rotate x/y/z). Constraints reduce them. Face-on-face removes translation along the normal and some rotations; axis-on-axis leaves translation along and rotation about the axis. The system shows remaining DOF per component.

## Part features vs. assembly constraints

A part feature changes a part's geometry (extrude, cut, hole, fillet, chamfer). An assembly constraint does not change geometry; it describes the pose relationship between two components (or a component and an assembly reference).

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

Constraints work on **semantic references** (`docs/semantic-references.md`): `CoverPlate.bottom_mounting_face`, `Screw.main_axis`, `BasePlate.HoleFeature_01.axis` — never `Face17`. Hole features expose their axis so a screw binds to the correct bore, not a random cylindrical face.

```text
AssemblyConstraint
  id, name, constraint_type
  component_a, reference_a
  component_b, reference_b
  parameters           # offset, distance, angle, direction
  active_state, solve_status

InsertConstraint
  component_a = Screw_01, axis_a = Screw_01.main_axis, face_a = Screw_01.under_head_face
  component_b = BasePlate, axis_b = BasePlate.HoleFeature_01.axis, face_b = BasePlate.top_mounting_face
  axial_offset = 0 mm, rotation_free = true
```

## Solver and constraint graph

The solver computes component transforms from all active constraints over rigid bodies, DOF, transforms, and nonlinear geometric relations.

```text
AssemblySolvePipeline
  read component instances -> read active constraints -> identify fixed components
  -> build constraint graph -> solve component transforms
  -> detect underdefined -> detect overconstrained
  -> update assembly transforms -> update viewport
```

A separate **constraint graph** (components = nodes, constraints = edges) reveals connected groups; independent groups can be solved separately.

## Grounding

Every assembly needs at least one grounded/fixed component or coordinate system, else the whole assembly floats. Warn if no component is grounded.

## Motion, joints, and limits

Underdefined components can be dragged only within allowed DOF: a drag is input to the solver, which projects it onto the valid constraint space; connected components update; the viewport updates. Never edit the transform freely.

Joints describe allowed motion (not only a fixed pose): Revolute, Prismatic, Cylindrical, Planar, Ball, Rigid, Gear relation, Screw relation. Joints and some constraints can have limits (`angle_min/max`, `position_min/max`, `distance_min/max`) for realistic mechanisms.

## Assembly parameters

Assembly parameters can drive constraints and joints for top-down control (`docs/parameter-model.md`):

```text
DistanceConstraint.distance = Assembly.bearing_distance
PrismaticJoint.position    = Assembly.slider_position
RevoluteJoint.angle        = Assembly.crank_angle
```

## Assembly features and collision

Assembly features (e.g. a hole through several mounted plates) act in assembly context and may or may not modify the source parts — the distinction must be explicit. Collision detection (static, during motion, clearance, minimum distances, interference) is a later addition; an MVP needs only a simple static check.

## Dependency-graph integration

Constraints integrate into the dependency graph: when a referenced face/axis/hole/component changes, affected constraints re-solve.

```text
BasePlate.HoleFeature_M6.axis -> InsertConstraint_Screw_01 -> Screw_01.transform
Assembly.bolt_circle_radius -> HoleFeature_BoltCircle -> hole axes -> InsertConstraints -> screw transforms
```

Works with the hole wizard and pattern system: changing `bolt_count` regenerates hole axes, screw instances (`FastenerPattern`, `docs/pattern-and-mirror-features.md`), and their insert constraints together.

## MVP scope (MVP 5)

- assembly document, component instances, one grounded component.
- free placement of components.
- Mate (planar), Concentric (cylindrical axes), Distance constraints.
- simple remaining-DOF display.
- simple rigid-body solver.
- save/load of the assembly structure.

Then add: Insert, Angle, Tangent constraints; movable joints; limits; collision checks; flexible subassemblies.

## Proposed implementation sequence

1. Add `AssemblyDocument`, `ComponentInstance`, and grounding.
2. Add `AssemblyConstraint` with Mate, Concentric, Distance on semantic references.
3. Add the constraint graph and a rigid-body solver producing transforms and remaining DOF.
4. Add JSON serialization and roundtrip tests for the assembly structure.
5. Add Insert constraints and integration with hole axes / fastener patterns.
6. Add joints and limits, then motion via the solver.
7. Add subassemblies (rigid first, flexible later) and collision checks.

## Out of scope for the first versions

- joints, motion, and limits before the rigid solver is stable.
- flexible subassemblies.
- collision/interference analysis beyond a simple static check.

## Role in the system

The assembly system connects part modeling, parameters, the hole wizard, the shaft wizard, standard parts, patterns, and motion analysis. A subassembly is itself an assembly used as a component. The goal is assemblies defined by traceable constraints, not just visually stacked bodies.
