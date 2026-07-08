# Core Skeleton and MVP-2 Seed

Status: implemented core skeleton for MVP-1 data models, recompute planning, JSON model-intent serialization, `.blcad.json` file helpers, semantic top/bottom/right/left face workplanes, geometry-layer workplane resolution, bounded face validation, axis-directed cuts, and incremental recompute through derived-workplane dependencies.

The core remains free of OCCT and Qt. Geometry is handled only in the optional `blcad_geometry` target. JSON serialization and `.blcad.json` file helpers stay in the core because they store model intent rather than computed shapes.

## Goal

The skeleton makes the first architecture decisions executable:

- typed IDs
- validated length quantities
- `Error` and `Result`
- `Parameter`
- `PartDocument`
- standard datum planes
- semantic generated-face references
- derived workplanes
- sketches and simple profiles
- additive and subtractive feature intent
- dependency graph
- invalidation state
- recompute plan
- model-intent JSON serialization
- file-level `.blcad.json` workflow
- optional computed `ShapeCache`
- geometry-layer workplane resolution

## Current core scope

`PartDocument` currently stores:

- document identity
- parameters
- datum planes
- derived workplanes
- sketches
- features
- dependency graph
- invalidation state

`PartDocument` validates references, creates graph nodes and edges, marks affected nodes after parameter changes, and derives an ordered recompute plan from dirty nodes.

The core model intentionally does not contain OCCT geometry. `ShapeCache` remains in the geometry layer.

## Current semantic faces

The current semantic face references support:

```text
feature.base_extrude.top
feature.base_extrude.bottom
feature.base_extrude.right
feature.base_extrude.left
```

A derived workplane exposes a generated face as a sketch workplane:

```text
workplane.base_left -> feature.base_extrude.left
```

The dependency graph records paths such as:

```text
feature.base_extrude -> workplane.base_left -> sketch.left_hole -> feature.left_hole_cut
```

No raw OCCT face IDs are stored in the core.

## Geometry workplane resolution

`WorkplaneResolver` lives in the geometry layer. It resolves:

- `datum.xy`
- `workplane.base_top`
- `workplane.base_bottom`
- `workplane.base_right`
- `workplane.base_left`

For side faces, local frames are explicit and tested. The right face uses:

```text
origin = (rectangle_center.x + width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, 1, 0)
y_axis = (0, 0, 1)
normal = (1, 0, 0)
```

The left face uses:

```text
origin = (rectangle_center.x - width / 2, rectangle_center.y, thickness / 2)
x_axis = (0, -1, 0)
y_axis = (0, 0, 1)
normal = (-1, 0, 0)
```

All currently supported generated-face workplanes carry rectangular local bounds.

## Optional geometry layer

The geometry layer is intentionally outside the core. It uses OCCT behind adapter boundaries and keeps `PartDocument` free of OCCT headers and linking.

Current geometry capabilities:

- `RectangleExtrusionAdapter` creates a centered rectangular OCCT solid.
- `CircularCutAdapter` cuts centered and principal-axis through-all circular holes.
- `ShapeCache` stores feature shapes, removes stale dirty feature shapes, and tracks the current final shape.
- `WorkplaneResolver` resolves selected semantic faces of a simple additive rectangle extrude.
- `GeometryRecomputeExecutor` executes additive rectangle extrudes and subtractive circular through-all cuts.
- `execute_document` recomputes a full document in topological order.
- `execute_plan` recomputes affected feature nodes from an incremental plan and skips non-feature nodes.
- Bounded validation rejects out-of-bounds circle profiles before OCCT cutting.
- `StepExporter` writes the final shape as STEP.
- `blcad_export_step` wires `.blcad.json` input to recompute and STEP export.

## JSON and file workflow

The JSON contains model intent only:

- schema marker `blcad.part_document.mvp1`
- version `1`
- document ID and name
- length parameters
- datum planes
- derived workplanes
- sketches and profiles
- additive and subtractive extrude features

The JSON deliberately does not contain OCCT shapes, ShapeCache contents, STEP data, or GUI state.

## Reference models

The checked-in models are:

```text
examples/reference_plate.blcad.json
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
```

All can be exported with:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Deliberate limitation

This skeleton still does not implement:

- GUI
- assemblies
- general sketch constraints
- general constraint solver
- front/back derived workplanes
- arbitrary planar faces
- ShapeCache serialization
- command-line argument parsing beyond the minimal example

This order is intentional. The core first needs safe data types, validation, dependency tracking, recompute planning, serialization, file workflow, a narrow geometry pipeline, semantic face references, workplane resolution, bounded validation, and incremental recompute before larger CAD subsystems are added.

## Standard commands

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Optional geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```
