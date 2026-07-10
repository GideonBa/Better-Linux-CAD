# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The end goal is to store and recompute CAD model intent: parameters, sketches, features, semantic generated-face/edge/vertex references, projected sketch references, reference-driven sketch constraints, robust reference recovery metadata, dependencies, construction geometry, and later assembly relationships. OCCT is used as the geometry kernel, but OCCT shapes are treated as computed cache data, not as the primary model. The condensed target architecture is documented in `docs/architecture-summary.md`, and the explicit project goal is documented in `docs/project-goal.md`.

The current state is a deliberately small MVP-1 vertical slice plus executable MVP-2 seeds: derived workplanes, line-based closed profiles, composite closed profiles with inner holes, arc closed profiles, trim/extend metadata records, automatic single-region detection, explicit construction geometry, relation-driven construction geometry, semantic generated edge/vertex references, evaluated rectangular-extrude semantic references, deterministic construction-point and construction-line resolvers, projected sketch reference entities, first reference-driven sketch constraints, deterministic projected-reference helper evaluation, explicit reference status/remap records, sketch-origin override records, first-class reference-generated helper-line records, reference-generated helper-profile recompute, first sketch geometric constraints, first sketch driving dimensions, dimension-driven profile recompute, and the first chained construction-relation path. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` model, datum planes, derived workplanes, construction points, construction lines, construction planes, construction relations, semantic face/edge/vertex references, semantic recovery targets, reference status records, reference remap records, sketches, sketch origin overrides, line segments, arc segments, projected sketch points, projected sketch lines, sketch reference targets, sketch constraints, sketch geometric constraints, sketch driving dimensions, sketch trim/extend operations, reference-generated lines, rectangle profiles, circle profiles, closed profiles, arc closed profiles, composite closed profiles, feature-intent data models, a dependency graph, invalidation state, recompute planning, JSON model persistence for the older profile records, optional OCCT geometry execution, top/bottom/right/left/front/back workplane resolution, sketch-aware workplane resolution, construction-plane workplane resolution, bounded face validation, incremental derived-workplane recompute, construction-geometry invalidation, projected-reference invalidation, reference-generated helper-line invalidation, driving-dimension invalidation, chained relation validation, line-based closed-profile additive/subtractive extrudes, arc-profile additive/subtractive extrudes, reference-generated helper-profile additive/subtractive extrudes, dimension-driven closed-profile additive/subtractive extrudes, composite-profile additive/subtractive extrudes with holes, automatic-region additive/subtractive extrudes, and STEP export. There is no GUI yet.

The optional `blcad_geometry` target contains OCCT adapters for rectangle extrusion, circular cuts, line-based closed-profile prism geometry, arc-profile prism geometry, and faces with inner wires, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, a `WorkplaneResolver`, `SemanticReferenceEvaluator`, `ConstructionPointResolver`, `ConstructionLineResolver`, `SketchReferenceProjector`, `ReferenceDrivenSketchHelper`, `ReferenceGeneratedProfileResolver`, `DimensionDrivenProfileResolver`, `SketchRegionFinder`, full document recompute, incremental recompute, axis-directed through-all circular cuts, closed-profile through-all cuts, arc-profile through-all cuts, composite closed-profile extrudes/cuts with inner contours, explicit construction-plane resolution, offset construction-plane resolution, three-point construction-plane resolution, plane-parallel-through-point construction-plane resolution, sketch-origin override resolution through `resolve_for_sketch`, rectangular-extrude semantic generated edge/vertex evaluation, deterministic generated-reference point evaluation, deterministic construction-line evaluation, projected reference geometry evaluation, first projected-reference constraint evaluation, deterministic helper-line generation from two projected point constraints, reference-generated helper-line profile vertex resolution, driving-dimension profile vertex resolution, automatic simple-region detection from unordered local lines, direct additive/subtractive feature recompute from arc profiles, direct additive/subtractive feature recompute from reference-generated helper profiles, direct additive/subtractive feature recompute from dimension-driven helper profiles, direct additive/subtractive feature recompute from composite profiles with holes, direct additive/subtractive feature recompute from detected simple regions, and STEP export for the final shape. Model intent can be serialized to `.blcad.json` for the currently implemented JSON-backed records, loaded again, recomputed into a fresh `ShapeCache`, validated against derived face bounds, updated incrementally through derived-workplane, construction-geometry, projected-reference, reference-generated helper-line, driving-dimension, and recovery-record dependencies, and exported as STEP through a small headless command-line example.

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
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent, including sketch dimensions, generated-region selections, reference-generated helper lines, and reference recovery metadata
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/derived-workplane-mvp2-seed.md`: semantic generated-face workplanes and sketches on generated planar faces
- `docs/workplane-resolver-mvp2.md`: geometry-layer resolver for derived and construction workplanes, including sketch-origin overrides and projected sketch reference projection
- `docs/general-closed-sketch-profile-mvp.md`: implemented first line-based closed-profile MVP and remaining future profile work
- `docs/composite-closed-profile-holes-mvp.md`: composite closed profiles, inner contours, holes, and inner-wire geometry
- `docs/arc-and-trim-extend-sketch-profile-mvp.md`: first explicit arc segments, arc closed profiles, trim/extend metadata, and arc-profile recompute
- `docs/construction-geometry-mvp.md`: explicit construction geometry, relation-driven construction geometry, evaluated semantic generated edge/vertex refs, deterministic point/line resolvers, and chained relations
- `docs/projected-sketch-reference-geometry.md`: projected construction and semantic references, first reference-driven sketch constraints, and sketch origin behavior
- `docs/reference-recovery-mvp.md`: explicit lost-reference status, semantic remap intent, sketch-origin overrides, and recovery behavior
- `docs/reference-generated-profile-helpers-mvp.md`: first-class reference-generated helper-line records, JSON, invalidation, profile resolver, and feature recompute integration
- `docs/sketch-constraints-and-dimensions-mvp.md`: first explicit sketch constraints, driving dimensions, invalidation, and dimension-driven profile recompute
- `docs/automatic-profile-region-detection-mvp.md`: deterministic single-region detection, generated profile candidates, and automatic-region recompute
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/inventor-like-sketcher-and-feature-roadmap.md`: long-term Inventor-like sketcher and sketch-driven feature parity roadmap
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`: future block for 3D sketches, guide splines, sweeps, lofts, boundary surfaces, and closed-surface-to-solid workflows
- `docs/semantic-references.md`: canonical rule for semantic non-topological references
- `docs/parameter-model.md`, `docs/feature-system.md`, `docs/file-format.md`, `docs/user-interface.md`: long-term target model documents
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `DerivedWorkplane`, `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, `ConstructionRelation`, `SemanticFaceReference`, `SemanticEdgeReference`, `SemanticVertexReference`, `SemanticReferenceTarget`, `ReferenceStatusRecord`, `ReferenceRemapRecord`, `SketchOriginOverrideRecord`, `Sketch`, `LineSegment`, `ArcSegment`, `ProjectedSketchPoint`, `ProjectedSketchLine`, `ReferenceGeneratedLine`, `SketchReferenceTarget`, `SketchConstraint`, `SketchGeometricConstraint`, `SketchDrivingDimension`, `SketchTrimExtendOperation`, `RectangleProfile`, `CircleProfile`, `ClosedProfile`, `ArcClosedProfile`, and `CompositeClosedProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, construction-geometry, construction-relation, generated edge/vertex, projected-reference, reference-generated helper-line, sketch-dimension, recovery-record, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

`ArcSegment` stores a first explicit circular arc through three sketch-local points. `ArcClosedProfile` stores ordered explicit line/arc curve IDs and validates deterministic end-to-start closure. `SketchTrimExtendOperation` stores explicit trim/extend metadata for line/arc endpoints, but it does not yet solve or rewrite curve geometry.

The optional geometry build resolves sketch workplanes before executing subtractive cuts and before executing line-based or arc-based closed-profile geometry. `ClosedProfileAdapter` can build OCCT wires from straight line edges and circular-arc edges and can use those wires for additive extrudes and subtractive through-all cuts.

Still not implemented: source-level JSON roundtrip for arc profile records, splines, exact curved self-intersection validation, tangent constraint solving, automatic fillets, trim/extend solving, mixed explicit-line/reference-generated-line closed profiles, full automatic multi-region selection, a full sketch constraint solver, over/under-constrained diagnosis, automatic profile topology from projected references without helper constraints, GUI reference-remapping dialogs, automatic topology matching after arbitrary edits, GUI sketch editing, arbitrary non-planar sketch geometry, arbitrary generated edge/vertex topology evaluation, analytic generated-surface construction references, revolve, revolve cut, 3D guide curves, loft, sweep, boundary surfaces, surface stitching, or closed-shell-to-solid conversion.

The persistence path is file-based for currently JSON-backed records: `serialize_part_document_to_json` and `deserialize_part_document_from_json` handle in-memory model-intent JSON, while `write_part_document_json_file` and `read_part_document_json_file` handle `.blcad.json` files. The checked-in example models can be loaded by `blcad_export_step`, recomputed through the geometry layer, validated, and exported as STEP.

## Headless examples

Run core configure, build, and tests in one command:

```bash
cmake --workflow --preset dev-build-test
```

Run geometry configure, build, and tests in one command:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Build the geometry preset manually:

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

Load the generated-reference and projected-reference example models:

```bash
./build/dev-geometry/blcad_export_step examples/generated_semantic_references.blcad.json build/generated_semantic_references.step
./build/dev-geometry/blcad_export_step examples/projected_sketch_references.blcad.json build/projected_sketch_references.step
```

Depending on the local CMake preset output directory, the executable path may differ. The command shape is:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Next technical step

The next technical step should complete source-level JSON roundtrip for the arc and trim/extend records and then add the first exact curved-contour validation seed.

1. Extend `part_document_json.cpp` to serialize and deserialize `arc_segments`, `trim_extend_operations`, and `arc_closed_profiles`.
2. Add JSON roundtrip tests for one line/arc profile and one trim/extend metadata record.
3. Add a curved self-intersection check for simple line/arc profiles.
4. Add a generated `.blcad.json` example for an arc-profile additive extrude.
5. Keep splines, tangent constraint solving, automatic fillets, GUI trim/extend solving, 3D sketches, sweep, loft, surface stitching, and closed-shell-to-solid conversion deferred.

The completed arc and trim/extend sketch profile seed is documented in `docs/arc-and-trim-extend-sketch-profile-mvp.md`.

The completed composite closed profile and holes block is documented in `docs/composite-closed-profile-holes-mvp.md`.

The completed automatic profile region detection block is documented in `docs/automatic-profile-region-detection-mvp.md`.

The completed sketch constraints and dimensions block is documented in `docs/sketch-constraints-and-dimensions-mvp.md`.

The completed reference-generated profile helper block is documented in `docs/reference-generated-profile-helpers-mvp.md`.

The completed robust reference recovery block is documented in `docs/reference-recovery-mvp.md`.

The completed projected sketch reference and first reference-driven constraint block is documented in `docs/projected-sketch-reference-geometry.md`.

The completed construction-geometry and semantic-reference block is documented in `docs/construction-geometry-mvp.md`.

The completed first line-based closed-profile block is documented in `docs/general-closed-sketch-profile-mvp.md`.

The long-term Inventor-like sketcher and sketch-driven feature parity roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

A separate later block for 3D sketches, guide splines, sweep, loft, boundary surfaces, surface stitching, and closed-surface-to-solid conversion is documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.
