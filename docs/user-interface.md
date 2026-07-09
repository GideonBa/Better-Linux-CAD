# User Interface Architecture

Status: target architecture. Not implemented yet. There is no GUI; the current pipeline is headless. The UI comes after the internal models work.

The UI is deliberately not built like FreeCAD. The goal is a modern, consistent, reduced interface with a clear separation between model, parameters, features, and assembly. Crucially, the UI only operates the core; it must not contain CAD logic (last of the core principles in `docs/architecture-summary.md`).

## Main areas

| UI area | Purpose |
|---------|---------|
| 3D viewport | display and interaction with parts, sketches, faces, edges, axes, assemblies. |
| Feature tree | chronological/logical feature list of a part. |
| Assembly tree | parts, instances, and constraints of an assembly. |
| Parameter window | table of parameters: name, value, unit, formula, scope, usage sites. |
| Property panel | edit the currently selected object/feature. |
| Command palette | quick access to Sketch, Extrude, Cut, Fillet, Hole, Pattern, Assembly Constraint. |
| Engineering panel | access technical assistants (bolt, bearing, shaft). |

## Parameter window

Long-term important but not the first priority: the internal parameter model must work first, then the UI is laid over it as an editor. Target shape:

```text
Assembly Parameters
Name                     Value  Unit  Formula          Used by
bolt_circle_radius       50     mm    -                3 parts
bolt_count               8      -     -                3 parts
bolt_clearance_diameter  6.6    mm    from bolt_size   2 parts
plate_thickness          8      mm    -                BasePlate
gasket_thickness         2      mm    -                Gasket
```

The "Used by" column comes from parameter usage tracking (`docs/parameter-model.md`).

## Feature dialogs

Feature-creation dialogs (hole wizard, fillet/chamfer, pattern/mirror, assembly constraints) share a consistent structure and are specified in their own documents:

- hole wizard dialog: `docs/hole-wizard.md`.
- fillet/chamfer dialog: `docs/fillet-chamfer-features.md`.
- pattern/mirror dialog: `docs/pattern-and-mirror-features.md`.
- assembly-constraint dialog with reference selection, suggested constraints, preview, remaining-DOF display, and validation: `docs/assembly-system.md`.

Constraint suggestions are deterministic from geometry types (plane+plane → Mate/Flush/Distance/Angle; cylinder+cylinder → Concentric/Tangent; screw+hole → Insert) and need no AI.

## Recompute feedback

After a change the UI updates the viewport, feature tree, and parameter window from the recompute result. Errors are surfaced, not swallowed. See the recompute process in `docs/architecture-summary.md` and `docs/recompute-plan-mvp1-data-model.md`.

## Proposed implementation sequence

1. Keep everything headless until the core, features, and serialization are stable.
2. Add a read-only viewport that renders the recomputed final shape.
3. Add a feature tree bound to the part's feature list.
4. Add a parameter table bound to the parameter model, then make it editable and drive recompute.
5. Add a property panel for the selected feature.
6. Add feature-creation dialogs one module at a time.
7. Add the assembly tree and constraint dialog once the assembly system exists.

## Out of scope for the first versions

- any UI before the headless model/recompute path is reliable.
- putting CAD logic in the UI layer.
- a full command/undo-redo system before the command layer is designed (see the command system layer in `docs/architecture-summary.md`).
