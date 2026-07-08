# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The condensed target architecture is documented in `docs/architecture-summary.md`.

The current state is a deliberately small MVP-1 vertical slice plus the first executable seed of MVP 2. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` model, datum planes, derived workplanes, sketches, feature-intent data models, a dependency graph, invalidation state, recompute planning, JSON model persistence, optional OCCT geometry execution, top/bottom workplane resolution, bounded face validation, incremental derived-workplane recompute, and STEP export. There is no GUI yet.

The optional `blcad_geometry` target contains OCCT adapters for rectangle extrusion and circular cuts, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, a `WorkplaneResolver`, full document recompute, incremental recompute, and STEP export for the final shape. Model intent can be serialized to `.blcad.json`, loaded again, recomputed into a fresh `ShapeCache`, validated against derived face bounds, updated incrementally through derived-workplane dependencies, and exported as STEP through a small headless command-line example.

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

- `docs/development-setup.md`: local setup and standard commands
- `docs/architecture-summary.md`: architecture overview
- `docs/core-mvp1-skeleton.md`: current core skeleton
- `docs/sketch-mvp1-data-model.md`: datum-plane and sketch data model
- `docs/feature-mvp1-data-model.md`: feature data model
- `docs/dependency-graph-mvp1-data-model.md`: dependency graph data model
- `docs/invalidation-mvp1-data-model.md`: invalidation-state data model
- `docs/recompute-plan-mvp1-data-model.md`: recompute-plan data model
- `docs/geometry-adapter-mvp1-rectangle-extrusion.md`: first OCCT adapter
- `docs/geometry-adapter-mvp1-circular-cut.md`: OCCT adapter for the centered cut
- `docs/shape-cache-mvp1-data-model.md`: ShapeCache data model
- `docs/recompute-execution-mvp1-additive-extrude.md`: first additive recompute execution
- `docs/recompute-execution-mvp1-subtractive-cut.md`: subtractive recompute execution
- `docs/step-export-mvp1.md`: STEP export for the final shape
- `docs/document-recompute-mvp1.md`: full document recompute and reference-part pipeline
- `docs/parameter-update-mvp1.md`: parameter-value update and numeric incremental recompute
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/derived-workplane-mvp2-seed.md`: semantic top-face workplanes and sketches on generated planar faces
- `docs/workplane-resolver-mvp2.md`: geometry-layer resolver for derived top/bottom workplanes
- `docs/bounded-workplane-validation-mvp2.md`: bounded circle validation on derived workplanes
- `docs/incremental-derived-workplane-recompute-mvp2.md`: incremental recompute through derived-workplane dependencies
- `docs/bottom-workplane-mvp2.md`: bottom-face derived workplane for simple additive extrudes
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `DerivedWorkplane`, `Sketch`, `RectangleProfile`, and `CircleProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

MVP 2 has started with a minimal semantic-face path. `SemanticFaceReference` can point to `feature.base_extrude.top` and `feature.base_extrude.bottom`. `DerivedWorkplane` can expose those semantic faces as sketch workplanes, and a sketch can use a derived workplane as its workplane reference. The dependency graph represents this as `feature.base_extrude -> workplane -> sketch -> cut_feature`.

The optional geometry build resolves sketch workplanes before executing subtractive cuts. `WorkplaneResolver` can resolve `datum.xy`, `feature.base_extrude.top`, and `feature.base_extrude.bottom`. For the top face of a simple additive rectangle extrusion, it resolves origin `z = thickness` and normal `+Z`. For the bottom face, it resolves origin `z = 0` and normal `-Z`. Both carry rectangular bounds derived from the source rectangle sketch.

Incremental recompute follows derived-workplane dependency paths. Updating `part.width`, `part.height`, or `part.thickness` can mark the base feature, derived workplane, dependent sketch, and cut feature as affected. `GeometryRecomputeExecutor::execute_plan` skips non-feature nodes while preserving their dependency-ordering role, removes stale cached feature shapes before recomputing dirty features, and can surface bounded-workplane validation errors after source-dimension changes.

The persistence path is file-based: `serialize_part_document_to_json` and `deserialize_part_document_from_json` handle in-memory model-intent JSON, while `write_part_document_json_file` and `read_part_document_json_file` handle `.blcad.json` files. The checked-in `examples/reference_plate.blcad.json`, `examples/top_face_cut.blcad.json`, and `examples/bottom_face_cut.blcad.json` models can be loaded by `blcad_export_step`, recomputed through the geometry layer, validated, and exported as STEP.

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

Export the top-face derived-workplane model:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
```

Export the bottom-face derived-workplane model:

```bash
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
```

Depending on the local CMake preset output directory, the executable path may differ. The command shape is:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Next technical step

The next technical step should add the first side-face semantic reference, but only one side and only for the simple additive rectangle extrude.

1. Add one side-face enum value, preferably `SemanticFace::Right`, for the `+X` face of a simple `AdditiveExtrude`.
2. Allow `DerivedWorkplane` to reference `feature.base_extrude.right`.
3. Resolve the right-face workplane with a clear origin, axes, normal, and rectangular bounds.
4. Use local side-face coordinates consistently, for example width along local Y and height along local Z.
5. Add JSON roundtrip coverage for the right-face derived workplane.
6. Add geometry tests that place a sketch on the right face and execute a through-all circular cut.
7. Keep support limited to top, bottom, and right faces of a simple `AdditiveExtrude`.
8. Do not add all side faces yet.
9. Do not build a full topological naming system yet.
10. Do not build a GUI yet.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.
