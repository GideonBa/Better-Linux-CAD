# Hole Wizard

Status: target architecture. Not implemented yet.

The hole wizard creates holes as semantic, parametric, standards-oriented features, not as a bare circle plus cut. A `HoleFeature` carries hole type, standard, diameter, depth, thread, counterbore/countersink, fit, fastener type, and manufacturing intent. This makes later drawings, BOMs, fastener placement, collision checks, and manufacturing notes possible.

## Why more than a cut

```text
Weak:
  Sketch001: circle diameter = 6.6 mm
  Cut001: depth = through_all

Strong:
  HoleFeature
    type = clearance_hole
    screw_size = M6
    fit = normal
    diameter = derived_from_standard_table
    depth = through_all
    placement = circular_pattern
    count = Assembly.bolt_count
    radius = Assembly.bolt_circle_radius
```

The strong model knows the hole is an M6 clearance hole, so that meaning survives into drawings and downstream modules.

## Hole types

Simple, through, blind, threaded, countersunk, counterbored (cylindrical), stepped, fitted/reamed (tolerance), slot, and center hole. Each is a semantic variant of `HoleFeature`.

## Data model

```text
HoleFeature
  id, name
  target_body
  placement_plane          # semantic face/datum reference
  placement_points
  hole_type
  standard_family, size, diameter
  depth_mode, depth_value
  thread_data
  counterbore_data, countersink_data
  tolerance_data
  pattern_data
  manufacturing_data
  generated_shape          # cache
```

Examples:

```text
HoleFeature M6_Clearance_Holes
  placement_plane = BasePlate.top_mounting_face
  hole_type = clearance_hole, screw_size = M6, fit = normal
  diameter = StandardTable.clearance_hole(M6, normal)
  depth_mode = through_all
  placement = circular_pattern
  pattern_radius = Assembly.bolt_circle_radius
  pattern_count = Assembly.bolt_count

HoleFeature M8_Threaded_Hole
  placement_plane = MountingBlock.top_face
  hole_type = threaded_hole, thread_size = M8, thread_pitch = standard_coarse
  thread_depth = 18 mm, drill_depth = 22 mm
  core_hole_diameter = StandardTable.tap_drill(M8, coarse)
  depth_mode = blind
```

## Placement

Single point, multiple free points, points from a sketch, rectangular pattern, circular pattern, along a construction line, concentric to an existing axis, or relative to edges/center lines/datum planes. Placement must be parametric and dimensionable, not only absolute coordinates. The placement face is a semantic reference (`docs/semantic-references.md`).

## End conditions

Blind, Through All, Up To Next, Up To Face, Up To Body, Symmetric, and Through Assembly (multiple bodies). A hole that affects several parts of an assembly must be distinguished from one that changes a single part (assembly hole feature, below).

## Assembly hole features

A hole defined in an assembly can create coordinated holes across parts, e.g. clearance holes in a cover and threaded holes in a base block from one bolt circle:

```text
AssemblyHoleFeature Housing_Bolt_Holes
  placement = circular_pattern
  radius = Assembly.bolt_circle_radius, count = Assembly.bolt_count, screw_size = M6
  affected_parts:
    CoverPlate: clearance_hole, through_all
    BaseBlock:  threaded_hole, thread_size = M6, thread_depth = 14 mm
```

This is central to top-down design: the bolt circle is defined once, several parts depend on it. See `docs/parameter-model.md` and `docs/assembly-system.md`.

## Standards database

Many dimensions (clearance holes, tap-drill sizes, thread sizes, counterbore/countersink dimensions, fit tolerances) come from a versioned standards database, not free numbers. Every auto-chosen dimension must have a source (licensed, manufacturer, or user data). Standards data must not be copied uncontrolled from the internet; the system owns the database structure but sources concrete values from licensed/manufacturer/user tables.

## Threads as semantic features

Threads should default to a cosmetic/semantic definition used in drawings, exports, and manufacturing info; real modeled thread geometry is optional (for 3D printing, rendering, or special simulation) because it creates many faces and slows large assemblies.

```text
ThreadData
  type = internal_thread, thread_system = metric
  nominal_size = M8, pitch = 1.25 mm, tolerance_class = 6H, thread_depth = 18 mm
  cosmetic_representation = true, modeled_thread_geometry = false
```

## Validation

Minimum edge distances, minimum hole-to-hole distances, sufficient thread depth, sufficient material for countersinks, collisions with other features, screw length vs. part thickness, countersink vs. head, hole fully on the selected face, sensible tolerance on fitted holes. Warnings are shown but do not always block; the user keeps control.

## Dependency-graph integration

A `HoleFeature` depends on the placement face, hole parameters, standards data, assembly parameters, and target body.

```text
Assembly.bolt_count -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
Assembly.bolt_circle_radius -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
BasePlate.top_mounting_face -> HoleWizard_M6_ClearancePattern -> BasePlate.Shape
```

## MVP scope and sequence

1. Add a `HoleFeature` intent model for a single clearance hole on a semantic face, `through_all`.
2. Add depth modes (blind, through all).
3. Add circular and rectangular placement patterns driven by parameters.
4. Add the standards-database structure with sourced entries and derive diameters from it.
5. Add threaded holes with cosmetic thread data.
6. Add validation warnings.
7. Later: assembly hole features, fastener generation, counterbore/countersink, slots, fitted holes.

## Role in the system

The hole wizard binds part modeling, assembly modeling, the standards/fastener library, and engineering assistants. It creates `HoleFeature`s (and optional `FastenerPattern`s, see `docs/pattern-and-mirror-features.md`) and updates the dependency graph, enabling later drawing derivation, BOMs, and screw placement.

## Out of scope for the first version

- automatic fastener/washer instances.
- drawing-note generation.
- through-assembly holes before the assembly document exists.
