# Engineering Modules

Status: target architecture. Not implemented yet. These are a long-term differentiator, built after the parametric core, features, and assemblies.

Engineering modules combine calculation, standards logic, and geometry generation. They are the main distinction from FreeCAD or simple modelers: the user should not have to assemble every detail by hand from tables, standards, and catalogs. Each module produces a normal parametric feature in the feature tree and can use parameters.

## Planned modules

| Module | Function |
|--------|----------|
| Bolt assistant | choose bolt size, clearance hole, countersink, nut, washer, engagement depth, safety factor. |
| Bolt-circle generator | parametric bolt circles: radius, count, angle offset, hole diameter, optional screw instances. |
| Shaft assistant | shafts with steps, keyways, bearing seats, and diameter sizing (`docs/shaft-wizard.md`). |
| Bearing assistant | select and place bearings by diameter, load, speed, and envelope. |
| Gear generator | simple gears from module, tooth count, pressure angle, width. |
| Material database | materials with density, yield, tensile strength, Young's modulus. |
| Standard-parts library | reusable standard parts: screws, nuts, washers, profiles, bearings. |

## Bolt assistant example

```text
Input:
  connection_type = bolted_plate_connection
  plate_1 = steel, 8 mm ; plate_2 = steel, 12 mm
  transverse_load = 2 kN ; safety_factor = 2

Output:
  recommended_bolt = M8
  clearance_hole_diameter = 9.0 mm
  minimum_edge_distance = 18 mm
  recommended_bolt_length = 25 mm
  generated_geometry = hole_pattern + bolts + washers
```

## Shared principles

- **Deterministic sizing, no AI**: bearing reactions, minimum diameters, bearing life, keys, retaining rings, and standard geometry must be deterministic, traceable, and checkable. AI may later assist with explanation, start values, catalog search, onboarding, and warning summaries — never with safety-relevant results. (Same rule as `docs/shaft-wizard.md`.)
- **Data with provenance**: material and standard/catalog data live in controlled databases that flag each value as normative, manufacturer, or user-defined. Standards data must not be copied uncontrolled; the system owns the structure but sources concrete values from licensed/manufacturer/user tables (see the standards database in `docs/hole-wizard.md`).
- **Integrated, not bolted on**: every module emits normal features/components, updates the dependency graph, and stays parametric and editable. Every automatic decision must be visible, justifiable, and editable — the system proposes a traceable, standards-oriented construction; the user keeps control.
- **Reuse across modules**: hole wizard, bolt assistant, bearing assistant, and standard-parts library share the same standards/material databases and the same fastener/bearing catalog.

## Relationship to other blocks

- Materials and standard parts become project-level `resources` in the file format (`docs/file-format.md`).
- Bolt circles combine the hole wizard (`docs/hole-wizard.md`), circular patterns (`docs/pattern-and-mirror-features.md`), and assembly insert constraints (`docs/assembly-system.md`).
- The gear generator and involute splines are later, more complex modules.

## Proposed implementation sequence

1. Add the material database as project-level resource data with provenance flags.
2. Add the standard-parts library structure (screws/nuts/washers) with sourced entries.
3. Add the bolt assistant driving the hole wizard and fastener placement.
4. Add the bearing assistant feeding the shaft wizard's bearing selection.
5. Add the gear generator (simple involute gears) as an isolated module.

## Out of scope for the first versions

- concrete normative standard values shipped in the repository (only the database structure).
- FEM/CAM coupling.
- gear/spline exact tooth geometry before the simpler modules are stable.
