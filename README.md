# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The end goal is to store and recompute CAD model intent: parameters, sketches, features, semantic generated-face references, dependencies, construction geometry, and later assembly relationships. OCCT is used as the geometry kernel, but OCCT shapes are treated as computed cache data, not as the primary model. The condensed target architecture is documented in `docs/architecture-summary.md`, and the explicit project goal is documented in `docs/project-goal.md`.

The current state is a deliberately small MVP-1 vertical slice plus the first executable seeds of MVP 2, the line-based closed-profile sketch block, and the first explicit construction-geometry block. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` model, datum planes, derived workplanes, construction points, construction lines, construction planes, sketches, line segments, rectangle profiles, circle profiles, closed profiles, feature-intent data models, a dependency graph, invalidation state, recompute planning, JSON model persistence, optional OCCT geometry execution, top/bottom/right/left/front/back workplane resolution, construction-plane workplane resolution, bounded face validation, incremental derived-workplane recompute, construction-geometry invalidation, line-based closed-profile additive/subtractive extrudes, and STEP export. There is no GUI yet.

The optional `blcad_geometry` target contains OCCT adapters for rectangle extrusion, circular cuts, and line-based closed-profile prism geometry, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, a `WorkplaneResolver`, full document recompute, incremental recompute, axis-directed through-all circular cuts, closed-profile through-all cuts, explicit construction-plane resolution, and STEP export for the final shape. Model intent can be serialized to `.blcad.json`, loaded again, recomputed into a fresh `ShapeCache`, validated against derived face bounds, updated incrementally through derived-workplane and construction-geometry dependencies, and exported as STEP through a small headless command-line example.

## Technical basis

- Language: C++20
- Build system: CMake with Ninja
- Geometry kernel: OCCT / Open CASCADE Technology
- Future GUI base: Qt 6
- Math helper library: Eigen
- File-format prototyping: nlohmann-json
- Logging and formatting: spdlog, fmt
- Tests: Catch2

## Project structure

- `docs/` contains architecture, MVP, and setup documents.
- `include/blcad/` contains the public headers of the core skeleton.
- `src/` contains the initial core and geometry implementation.
- `tests/` contains Catch2 tests for the core and geometry layer.
- `examples/` contains checked-in `.blcad.json` models and the headless export example.
- `cmake/` is reserved for later CMake helper modules.
- `assets/` is reserved for later UI and example resources.

## Documentation

- `docs/project-goal.md`: explicit long-term project goal
- `docs/development-setup.md`: local setup and standard commands
- `docs/architecture-summary.md`: architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: datum-plane and sketch data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: OCCT adapter for circular cuts
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: first additive recompute execution
- `docs/recompute-execution-mvp1-subtractive-cut.md`: subtractive recompute execution
- `docs/step-export-mvp1.md`: STEP export for the final shape
- `docs/document-recompute-mvp1.md`: full document recompute and reference-part pipeline
- `docs/parameter-update-mvp1.md`: parameter-value update and numeric incremental recompute
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/derived-workplane-mvp2-seed.md`: semantic generated-face workplanes and sketches on generated planar faces
- `docs/workplane-resolver-mvp2.md`: geometry-layer resolver for derived and construction workplanes
- `docs/bounded-workplane-validation-mvp2.md`: bounded circle and closed-profile validation on resolved workplanes
- `docs/incremental-derived-workplane-recompute-mvp2.md`: incremental recompute through derived-workplane dependencies
- `docs/bottom-workplane-mvp2.md`: bottom-face derived workplane for simple additive extrudes
- `docs/right-workplane-mvp2.md`: right-face derived workplane for simple additive extrudes
- `docs/left-workplane-mvp2.md`: left-face derived workplane for simple additive extrudes
- `docs/front-workplane-mvp2.md`: front-face derived workplane for simple additive extrudes
- `docs/back-workplane-mvp2.md`: back-face derived workplane for simple additive extrudes
- `docs/general-closed-sketch-profile-mvp.md`: implemented first line-based closed-profile MVP and remaining future profile work
- `docs/inventor-like-sketcher-and-feature-roadmap.md`: long-term Inventor-like sketcher and sketch-driven feature parity roadmap
- `docs/construction-geometry-mvp.md`: implemented first explicit construction-geometry block and next relation-driven construction-geometry steps
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`: future block for 3D sketches, guide splines, sweeps, lofts, boundary surfaces, and closed-surface-to-solid workflows
- `docs/semantic-references.md`: canonical rule for semantic (non-topological) references to faces, edges, axes, and points
- `docs/parameter-model.md`: target parameter scopes, expressions, cross-part flow, and top-down design
- `docs/feature-system.md`: target feature model, feature references, and the parametric bolt circle
- `docs/file-format.md`: target project and save file format
- `docs/fillet-chamfer-features.md`: target fillet and chamfer features
- `docs/pattern-and-mirror-features.md`: target linear/circular pattern and mirror features
- `docs/hole-wizard.md`: target hole wizard and semantic hole features
- `docs/shaft-wizard.md`: target shaft assistant (calculation plus geometry generation)
- `docs/assembly-system.md`: target assembly system with constraints, solver, joints, and motion
- `docs/engineering-modules.md`: target engineering modules (bolt, bearing, gear, material, standard parts)
- `docs/user-interface.md`: target UI architecture over the core
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `DerivedWorkplane`, `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, `Sketch`, `LineSegment`, `RectangleProfile`, `CircleProfile`, and `ClosedProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, construction-geometry, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

MVP 2 has a minimal semantic-face path for a simple `AdditiveExtrude`. `SemanticFaceReference` can point to `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back`. `DerivedWorkplane` can expose those semantic faces as sketch workplanes, and a sketch can use a derived workplane as its workplane reference. The dependency graph represents this as `feature.base_extrude -> workplane -> sketch -> cut_feature`.

The first construction-geometry path has explicit construction points, construction lines, and construction planes. Construction planes can be used as sketch workplanes. Optional `parameter_dependencies` on construction geometry create dependency graph edges so a parameter change marks the construction object, dependent sketches, and dependent features dirty. These dependencies are an invalidation hook only; expression-based coordinate evaluation is not implemented yet.

The optional geometry build resolves sketch workplanes before executing subtractive cuts and before executing line-based closed-profile geometry. `WorkplaneResolver` can resolve `datum.xy`, `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, `feature.base_extrude.back`, and explicit construction planes. Top and bottom cuts are vertical through-all cuts. Right and left face cuts are X-axis through-all cuts. Front and back face cuts are Y-axis through-all cuts. All currently supported generated-face workplanes carry rectangular bounds derived from the source rectangle sketch and extrude thickness. Construction planes are currently treated as unbounded explicit reference planes.

Line-based closed sketch profiles are implemented as the first general-profile step. A sketch can store `LineSegment` entities, build a `ClosedProfile` from ordered line-segment IDs, validate that the chain is connected and non-self-intersecting, serialize that intent to JSON, restore it from JSON, convert it to an OCCT wire and face in the geometry layer, and use it as one additive extrude profile or one subtractive through-all cut profile.

Still not implemented: arcs, splines, trimmed curves, automatic region detection, multiple contours in one feature, inner holes in one profile, a full sketch constraint solver, relation-driven construction geometry, GUI sketch editing, arbitrary non-planar sketch geometry, revolve, revolve cut, 3D guide curves, loft, sweep, boundary surfaces, surface stitching, or closed-shell-to-solid conversion.

The long-term sketcher and feature parity target is explicitly documented as a separate roadmap. It covers Inventor-like 2D sketch entities, sketch editing operations, constraints, dimensions, automatic profile and region detection, 3D sketching, revolve/revolve cut, sweep, loft, surfacing, and surface-to-solid conversion. This roadmap is tracked in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

Advanced 3D sketching and surfacing are explicitly documented as a separate future block. The system should later support 3D sketch points, 3D lines, 3D splines, guide curves connecting points from sketches on different planes, sweeps along lines or splines, lofts between two or more sketches, smooth multi-section lofts, boundary surfaces, surface stitching, and conversion from a closed stitched shell into a solid. This future block is tracked in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

Incremental recompute follows derived-workplane and construction-geometry dependency paths. Updating a parameter that drives a base feature, derived workplane path, or construction-geometry dependency can mark the affected workplane/construction object, dependent sketch, and feature as dirty. `GeometryRecomputeExecutor::execute_plan` skips non-feature nodes while preserving their dependency-ordering role, removes stale cached feature shapes before recomputing dirty features, and can surface bounded-workplane validation errors after source-dimension changes.

The persistence path is file-based: `serialize_part_document_to_json` and `deserialize_part_document_from_json` handle in-memory model-intent JSON, while `write_part_document_json_file` and `read_part_document_json_file` handle `.blcad.json` files. The checked-in `examples/reference_plate.blcad.json`, `examples/top_face_cut.blcad.json`, `examples/bottom_face_cut.blcad.json`, `examples/right_face_cut.blcad.json`, `examples/left_face_cut.blcad.json`, `examples/front_face_cut.blcad.json`, `examples/back_face_cut.blcad.json`, `examples/triangle_prism.blcad.json`, `examples/triangle_cut_plate.blcad.json`, and `examples/construction_plane_prism.blcad.json` models can be loaded by `blcad_export_step`, recomputed through the geometry layer, validated, and exported as STEP.

## Headless examples

Build the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
```

Export the MVP-1 reference model:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
```

Export the derived-workplane models:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
./build/dev-geometry/blcad_export_step examples/back_face_cut.blcad.json build/back_face_cut.step
```

Export the closed-profile and construction-plane models:

```bash
./build/dev-geometry/blcad_export_step examples/triangle_prism.blcad.json build/triangle_prism.step
./build/dev-geometry/blcad_export_step examples/triangle_cut_plate.blcad.json build/triangle_cut_plate.step
./build/dev-geometry/blcad_export_step examples/construction_plane_prism.blcad.json build/construction_plane_prism.step
```

Depending on the local CMake preset output directory, the executable path may differ. The command shape is:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Next technical step

The next technical step should start relation-driven construction geometry while keeping the explicit construction-geometry path stable.

1. Add a `ConstructionRelation` model.
2. Add a first `PlaneOffsetFromPlane` relation-driven construction plane.
3. Add a first `LineThroughTwoPoints` relation-driven construction line.
4. Add a first `PlaneThroughThreePoints` relation-driven construction plane with collinearity validation.
5. Add dependency graph edges from referenced construction objects to relation-driven construction objects.
6. Add JSON serialization and roundtrip tests for relation-driven construction geometry.
7. Extend `WorkplaneResolver` so relation-driven construction planes resolve into `ResolvedWorkplane` frames.
8. Add invalidation tests proving that changing referenced points, lines, planes, or parameters marks dependent construction geometry, sketches, and features dirty.
9. Keep explicit construction geometry as the stable fallback path.
10. Do not add GUI manipulators yet.
11. Do not add 3D sketch splines, sweep, loft, surface stitching, or closed-shell-to-solid conversion yet.
12. Do not add generated semantic edge, vertex, or analytic-surface construction references until the relation path is stable.

The completed first explicit construction-geometry block and the next relation-driven construction step are documented in `docs/construction-geometry-mvp.md`.

The completed first line-based closed-profile block is documented in `docs/general-closed-sketch-profile-mvp.md`.

The long-term Inventor-like sketcher and sketch-driven feature parity roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

A separate later block for 3D sketches, guide splines, sweep, loft, boundary surfaces, surface stitching, and closed-shell-to-solid conversion is documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.
