# BLCAD

BLCAD is planned as an independent parametric CAD system for Linux. The end goal is to store and recompute CAD model intent: parameters, sketches, features, semantic generated-face/edge/vertex references, projected sketch references, reference-driven sketch constraints, dependencies, construction geometry, and later assembly relationships. OCCT is used as the geometry kernel, but OCCT shapes are treated as computed cache data, not as the primary model. The condensed target architecture is documented in `docs/architecture-summary.md`, and the explicit project goal is documented in `docs/project-goal.md`.

The current state is a deliberately small MVP-1 vertical slice plus executable MVP-2 seeds: derived workplanes, line-based closed profiles, explicit construction geometry, relation-driven construction geometry, semantic generated edge/vertex references, evaluated rectangular-extrude semantic references, deterministic construction-point and construction-line resolvers, projected sketch reference entities, first reference-driven sketch constraints, deterministic projected-reference helper evaluation, and the first chained construction-relation path. It contains base types for parameters, length quantities, error handling, a pure `PartDocument` model, datum planes, derived workplanes, construction points, construction lines, construction planes, construction relations, semantic face/edge/vertex references, sketches, line segments, projected sketch points, projected sketch lines, sketch reference targets, sketch constraints, rectangle profiles, circle profiles, closed profiles, feature-intent data models, a dependency graph, invalidation state, recompute planning, JSON model persistence, optional OCCT geometry execution, top/bottom/right/left/front/back workplane resolution, construction-plane workplane resolution, bounded face validation, incremental derived-workplane recompute, construction-geometry invalidation, projected-reference invalidation, chained relation validation, line-based closed-profile additive/subtractive extrudes, and STEP export. There is no GUI yet.

The optional `blcad_geometry` target contains OCCT adapters for rectangle extrusion, circular cuts, and line-based closed-profile prism geometry, a small `ShapeCache`, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, a `WorkplaneResolver`, `SemanticReferenceEvaluator`, `ConstructionPointResolver`, `ConstructionLineResolver`, `SketchReferenceProjector`, `ReferenceDrivenSketchHelper`, full document recompute, incremental recompute, axis-directed through-all circular cuts, closed-profile through-all cuts, explicit construction-plane resolution, offset construction-plane resolution, three-point construction-plane resolution, plane-parallel-through-point construction-plane resolution, rectangular-extrude semantic generated edge/vertex evaluation, deterministic generated-reference point evaluation, deterministic construction-line evaluation, projected reference geometry evaluation, first projected-reference constraint evaluation, deterministic helper-line generation from two projected point constraints, and STEP export for the final shape. Model intent can be serialized to `.blcad.json`, loaded again, recomputed into a fresh `ShapeCache`, validated against derived face bounds, updated incrementally through derived-workplane, construction-geometry, and projected-reference dependencies, and exported as STEP through a small headless command-line example.

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
- `docs/json-serialization-mvp1.md`: JSON serialization of model intent
- `docs/json-file-workflow-mvp1.md`: `.blcad.json` file workflow and headless export example
- `docs/derived-workplane-mvp2-seed.md`: semantic generated-face workplanes and sketches on generated planar faces
- `docs/workplane-resolver-mvp2.md`: geometry-layer resolver for derived and construction workplanes, including projected sketch reference projection
- `docs/general-closed-sketch-profile-mvp.md`: implemented first line-based closed-profile MVP and remaining future profile work
- `docs/construction-geometry-mvp.md`: explicit construction geometry, relation-driven construction geometry, evaluated semantic generated edge/vertex refs, deterministic point/line resolvers, and chained relations
- `docs/projected-sketch-reference-geometry.md`: projected construction and semantic references, first reference-driven sketch constraints, sketch origin behavior, and planned lost-reference recovery
- `docs/mvp-plan.md`: MVP sequence
- `docs/mvp-1-specification.md`: detailed MVP-1 specification
- `docs/inventor-like-sketcher-and-feature-roadmap.md`: long-term Inventor-like sketcher and sketch-driven feature parity roadmap
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`: future block for 3D sketches, guide splines, sweeps, lofts, boundary surfaces, and closed-surface-to-solid workflows
- `docs/semantic-references.md`: canonical rule for semantic non-topological references
- `docs/parameter-model.md`, `docs/feature-system.md`, `docs/file-format.md`, `docs/user-interface.md`: long-term target model documents
- `docs/decisions/`: architecture decision records

## Current technical state

The current core skeleton covers `Quantity`, typed IDs, `Error`, `Result`, `Parameter`, `PartDocument`, `DatumPlane`, `DerivedWorkplane`, `ConstructionPoint`, `ConstructionLine`, `ConstructionPlane`, `ConstructionRelation`, `SemanticFaceReference`, `SemanticEdgeReference`, `SemanticVertexReference`, `Sketch`, `LineSegment`, `ProjectedSketchPoint`, `ProjectedSketchLine`, `SketchReferenceTarget`, `SketchConstraint`, `RectangleProfile`, `CircleProfile`, and `ClosedProfile`, as well as `Feature`, `AdditiveExtrude`, `SubtractiveExtrude`, `DependencyGraph`, `InvalidationState`, and `RecomputePlan`. `PartDocument` validates workplane, profile, construction-geometry, construction-relation, generated edge/vertex, projected-reference, and feature references, creates graph nodes and graph edges from them, marks affected nodes after a parameter change, and derives an ordered recompute plan from that state.

MVP 2 has a minimal semantic-face path for a simple `AdditiveExtrude`. `SemanticFaceReference` can point to `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back`. `DerivedWorkplane` can expose those semantic faces as sketch workplanes, and a sketch can use a derived workplane as its workplane reference. The dependency graph represents this as `feature.base_extrude -> workplane -> sketch -> cut_feature`.

A sketch's local origin is currently the origin of its resolved workplane frame. For a datum sketch this is the datum-plane origin. For a generated-face sketch this is the deterministic face-local origin reconstructed by `WorkplaneResolver`; therefore a sketch on a generated face moves when the source feature dimensions change. If the referenced face/edge/vertex disappears after topology changes, interactive reference repair is not implemented yet. The planned behavior is explicit lost-reference reporting plus user-controlled remapping, not silent reassignment.

The construction-geometry path now has explicit construction points, explicit construction lines, explicit construction planes, relation-driven construction definitions, semantic generated edge/vertex references, evaluated rectangular-extrude edge/vertex references, relation-driven generated-reference construction points, deterministic construction-point evaluation, deterministic construction-line evaluation, and chained relation definitions. Implemented relation types include `PlaneOffsetFromPlane`, `LineThroughTwoPoints`, `PlaneThroughThreePoints`, `PointOnPlane`, `PointOnLine`, `PointOnGeneratedEdge`, `PointOnGeneratedVertex`, `LineOnPlane`, `PlaneParallelToPlaneThroughPoint`, `LineParallelToLineThroughPoint`, and `LineParallelToGeneratedEdgeThroughPoint`. Construction planes can be used as sketch workplanes. Generated edge and vertex references remain model intent and are evaluated only for the controlled rectangular additive extrude topology.

Projected sketch references are represented as sketch-local model intent. `ProjectedSketchPoint` can reference a `ConstructionPointId` or a `SemanticVertexReference`; `ProjectedSketchLine` can reference a `ConstructionLineId` or a `SemanticEdgeReference`. The JSON layer stores them in `projected_points` and `projected_lines`. The geometry layer can resolve those references into sketch-local 2D point coordinates or point-plus-direction helper lines when the resolved 3D reference lies on the sketch workplane.

Reference-driven sketch constraints are implemented as the first non-solver associative sketch target path. `SketchReferenceTarget` can identify line segments, line endpoints, projected points, and projected lines. `SketchConstraint` currently supports `coincident_to_projected_point`, `parallel_to_projected_line`, and `collinear_with_projected_line`. The JSON layer stores these in sketch-level `constraints`. The geometry layer can resolve the first constraints and can create a deterministic helper `LineSegment` from two projected point endpoint constraints when both endpoints resolve cleanly.

The optional geometry build resolves sketch workplanes before executing subtractive cuts and before executing line-based closed-profile geometry. `WorkplaneResolver` can resolve `datum.xy`, `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, `feature.base_extrude.back`, explicit construction planes, offset construction-plane relations, three-point construction-plane relations, and plane-parallel-through-point construction-plane relations. `SemanticReferenceEvaluator` can resolve semantic generated edges and vertices of simple rectangular additive extrudes. `ConstructionPointResolver` can resolve explicit points, generated vertices, and deterministic generated-edge midpoints. `ConstructionLineResolver` can resolve explicit lines, two-point lines, line-parallel-through-point lines, and generated-edge-parallel-through-point lines. `SketchReferenceProjector` can project resolved construction points, construction lines, semantic vertices, and semantic edges into sketch-local reference geometry with explicit out-of-plane validation. `ReferenceDrivenSketchHelper` can evaluate the first projected-reference helper constraints.

Line-based closed sketch profiles are implemented as the first general-profile step. A sketch can store `LineSegment` entities, build a `ClosedProfile` from ordered line-segment IDs, validate that the chain is connected and non-self-intersecting, serialize that intent to JSON, restore it from JSON, convert it to an OCCT wire and face in the geometry layer, and use it as one additive extrude profile or one subtractive through-all cut profile.

Still not implemented: arcs, splines, trimmed curves, automatic region detection, multiple contours in one feature, inner holes in one profile, a full sketch constraint solver, automatic profile topology from projected references without helper constraints, interactive lost-reference repair/remapping, GUI sketch editing, arbitrary non-planar sketch geometry, arbitrary generated edge/vertex topology evaluation, analytic generated-surface construction references, revolve, revolve cut, 3D guide curves, loft, sweep, boundary surfaces, surface stitching, or closed-shell-to-solid conversion.

The persistence path is file-based: `serialize_part_document_to_json` and `deserialize_part_document_from_json` handle in-memory model-intent JSON, while `write_part_document_json_file` and `read_part_document_json_file` handle `.blcad.json` files. The checked-in example models can be loaded by `blcad_export_step`, recomputed through the geometry layer, validated, and exported as STEP.

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

The next technical step should make sketch placement and projected references robust when a source face, edge, or vertex changes or disappears after feature edits.

1. Add explicit lost-reference status records for semantic face, edge, and vertex references.
2. Add stable remap records that can move a sketch or projected reference from one valid semantic source to another without silently changing model intent.
3. Add optional sketch-origin override metadata for workplane-based sketches while keeping the default origin equal to the resolved workplane origin.
4. Add tests for generated-face sketches moving correctly when source dimensions change.
5. Add tests for source topology removal producing explicit lost-reference errors instead of arbitrary reassignment.
6. Add JSON roundtrip tests for lost-reference/remap metadata.
7. Keep GUI remapping dialogs, full solver integration, automatic region detection, 3D sketches, sweep, loft, surface stitching, and closed-shell-to-solid conversion deferred.

The completed projected sketch reference and first reference-driven constraint block is documented in `docs/projected-sketch-reference-geometry.md`.

The completed construction-geometry and semantic-reference block is documented in `docs/construction-geometry-mvp.md`.

The completed first line-based closed-profile block is documented in `docs/general-closed-sketch-profile-mvp.md`.

The long-term Inventor-like sketcher and sketch-driven feature parity roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

A separate later block for 3D sketches, guide splines, sweep, loft, boundary surfaces, surface stitching, and closed-surface-to-solid conversion is documented in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

The detailed MVP-1 specification is in `docs/mvp-1-specification.md`.
