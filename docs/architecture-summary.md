# Architecture Summary

Source: condensed from the current repository architecture documents.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model does not only store final BRep geometry, but the underlying design intent: parameters, sketches, features, dependencies, semantic references, construction geometry, 3D curves, surfaces, multi-body part records, body transforms, body booleans, and later assembly relationships. The explicit long-term goal is also recorded in `docs/project-goal.md`.

## Fundamental decision

- Do not fork FreeCAD.
- Use OCCT as the geometry kernel.
- Keep the custom CAD core above OCCT.
- Treat OCCT shapes as computed cache data, not as the primary CAD model.
- Dune 3D may serve as a technical reference for sketching and UI ideas, but not as the long-term foundation.

## Layers

- User interface
- Command system
- Parametric core
- Sketch and constraint layer
- Construction geometry and datum-relation layer
- Multi-body part layer
- 3D sketch and surfacing layer
- Assembly layer
- Engineering modules
- OCCT geometry kernel
- File and exchange formats

## Core objects

- `Document`
- `PartDocument`
- `AssemblyDocument`
- `Parameter`
- `Expression`
- `Sketch`
- `SketchEntityId`
- `LineSegment`
- `ClosedProfile`
- future sketch entities such as arcs, circles, ellipses, splines, polygons, slots, text, and projected geometry
- future `SketchConstraint`, `SketchDimension`, `SketchRegion`, and `ProfileRegion`
- `DatumPlane`
- `DerivedWorkplane`
- `SemanticFaceReference`
- `ConstructionPoint`
- `ConstructionLine` / `DatumAxis`
- `ConstructionPlane`
- future `ConstructionRelation`
- `BodyId`
- `Body`
- `BodyTransform`
- `BodyTransformStack`
- `BodyBooleanFeature`
- `SketchOwnership`
- `PathCurve`
- `PathSegmentReference`
- `ProfileSectionReference`
- `SketchPoint3D`
- `SketchCurve3D`
- `GuideCurve`
- `Feature`, including `RevolveFeature`, `RevolveCutFeature`, `PathExtrudeFeature`, `PathCutFeature`, `SweepFeature`, `LoftFeature`, `LoftCutFeature`, `SurfaceStitchFeature`, `FilletFeature`, and `ChamferFeature`
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraint`
- `Joint`

## MVP-1 implemented vertical slice

The MVP-1 code implements the first narrow vertical slice:

- `PartDocument` stores model intent for one part.
- Parameters are typed, named, and validated.
- Datum planes and simple sketches exist as data models.
- Rectangle and circle profiles are supported.
- `AdditiveExtrude` and `SubtractiveExtrude` exist as feature-intent data models.
- `DependencyGraph`, `InvalidationState`, and `RecomputePlan` derive recompute order from model references.
- The optional `blcad_geometry` target converts the model into OCCT geometry.
- `ShapeCache` stores computed feature shapes and the final shape.
- `StepExporter` writes the final shape to STEP.
- `PartDocument::set_parameter_value` enables numeric incremental recompute.
- JSON and `.blcad.json` helpers persist and restore model intent.
- `blcad_export_step` provides a headless file-to-STEP workflow.

## MVP-2 seed

The MVP-2 seed introduces semantic generated-face references and resolves them in the geometry layer:

- `SemanticFaceReference` can reference `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back`.
- `DerivedWorkplane` can expose those semantic faces as sketch workplanes.
- `Sketch` can reference either a standard datum plane or a derived workplane.
- The dependency graph connects source feature, derived workplane, sketch, and dependent cut feature.
- JSON serialization supports `derived_workplanes` with `top`, `bottom`, `right`, `left`, `front`, and `back` faces.
- `WorkplaneResolver` resolves `datum.xy`, `feature.base_extrude.top`, `feature.base_extrude.bottom`, `feature.base_extrude.right`, `feature.base_extrude.left`, `feature.base_extrude.front`, and `feature.base_extrude.back` into concrete workplane frames.
- `ResolvedWorkplane` carries rectangular bounds for the derived top, bottom, right, left, front, and back faces.
- Subtractive recompute maps circle-profile centers through the resolved workplane before executing the cut.
- The circular cut adapter supports axis-directed through-all cuts for the current principal-axis face cases.
- The geometry layer rejects circle profiles that exceed resolved workplane bounds.
- Incremental recompute follows the derived-workplane dependency path after source-dimension changes.
- `ShapeCache` removes stale dirty feature shapes before incremental recompute, so failed recompute does not leave old cut geometry behind.
- `examples/top_face_cut.blcad.json` demonstrates an off-center cut on the derived top-face workplane.
- `examples/bottom_face_cut.blcad.json` demonstrates an off-center cut on the derived bottom-face workplane.
- `examples/right_face_cut.blcad.json` demonstrates a side cut on the derived right-face workplane.
- `examples/left_face_cut.blcad.json` demonstrates a side cut on the derived left-face workplane.
- `examples/front_face_cut.blcad.json` demonstrates a side cut on the derived front-face workplane.
- `examples/back_face_cut.blcad.json` demonstrates a side cut on the derived back-face workplane.

This is still intentionally not a full topological-naming system. No raw OCCT face IDs are stored in `PartDocument`.

## Implemented line-based closed profiles

The first general closed-profile step is implemented:

- `SketchEntityId` identifies sketch entities.
- `LineSegment` stores ordered 2D endpoints in sketch-local coordinates.
- `ClosedProfile` references ordered line-segment IDs.
- The sketch model validates connected loops, duplicate line references, and self-intersections.
- JSON serialization stores `line_segments` and `closed_profiles` as model intent.
- The geometry layer converts closed-profile vertices into an OCCT polygon wire and face.
- `AdditiveExtrude` supports exactly one rectangle profile or exactly one closed profile.
- `SubtractiveExtrude` supports exactly one circle profile or exactly one closed profile.
- Closed-profile geometry uses resolved workplanes, so it works with the existing datum and semantic-face workplane paths.
- `examples/triangle_prism.blcad.json` demonstrates a non-rectangular additive prism.
- `examples/triangle_cut_plate.blcad.json` demonstrates a non-circular through-all cut.

Still intentionally missing: automatic multi-body inference, full automatic region detection for complex profiles, a full sketch constraint solver, and GUI sketch editing.

## Implemented explicit construction geometry

The first construction-geometry step is implemented:

- `ConstructionPointId`, `ConstructionLineId`, and `ConstructionPlaneId` identify construction geometry.
- `ConstructionPoint`, `ConstructionLine`, and `ConstructionPlane` are core model-intent objects.
- Construction geometry supports explicit numeric placement.
- Construction geometry can carry optional `parameter_dependencies` for invalidation.
- `PartDocument` stores construction points, construction lines, and construction planes.
- Construction geometry receives dependency graph nodes.
- Parameter dependencies create graph edges from parameters to construction geometry.
- A sketch can reference a construction plane as its workplane.
- A construction plane used as a sketch workplane creates a graph edge from construction plane to sketch.
- JSON serialization supports `construction_points`, `construction_lines`, and `construction_planes`.
- `WorkplaneResolver` resolves explicit construction planes into `ResolvedWorkplane` frames.
- `examples/construction_plane_prism.blcad.json` demonstrates a closed-profile sketch on a construction plane.

Still intentionally missing: expression-evaluated coordinate placement, surface-based construction geometry, GUI manipulators, and assembly-level construction geometry.

## Implemented MVP-3 parametric bolt circle

The first meaningful parametric feature test is implemented (`docs/bolt-circle-pattern-mvp3.md`):

- `Quantity` distinguishes `LengthMm` and dimensionless `Count` values.
- `ParameterType::Count` stores whole-number parameters such as a hole count.
- `CircularHolePattern` is a sketch-level model-intent record: center, angle offset, and radius/count/hole-diameter parameter references.
- `PartDocument` validates pattern parameter existence and types and creates parameter-to-sketch dependency edges.
- Subtractive recompute expands the pattern into sequential through-all circular cuts with per-hole bounds validation.
- Changing `bolt_count` or `bolt_circle_radius` drives incremental recompute of only the affected features.
- JSON persists count parameters (`"type": "count"`, `"unit": "1"`) and `circular_hole_patterns`.
- `examples/bolt_circle_plate.blcad.json` exports a six-hole plate to STEP headlessly.

Still intentionally missing: hole semantics (`docs/hole-wizard.md`), skip instances, and seed-feature patterns (`docs/pattern-and-mirror-features.md`).

## Implemented MVP-4 seed: shared assembly parameters

The first cross-part parametrization step is implemented (`docs/assembly-parameters-mvp4.md`):

- `ParameterScope` distinguishes `part` and `assembly`.
- `AssemblyDocument` owns assembly-scoped parameters, registers member parts by `DocumentId`, and stores explicit `ParameterBinding` records.
- `apply_bindings_to(PartDocument&)` pushes bound values through `PartDocument::set_parameter_value`, reusing part invalidation and recompute planning unchanged.
- Type agreement (length/count) is enforced when bindings are applied.
- Assembly documents serialize separately as `blcad.assembly_document.mvp4`.
- `examples/flange_assembly.blcad.json` binds the bolt-circle plate example; an end-to-end test drives two plates from one shared `bolt_count`.

Still intentionally missing: a project container with automatic propagation, component instances, constraints (MVP 5), and expressions over assembly parameters.

## Future multi-body part modeling, transforms, and path features

The target multi-body feature layer is documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

The intended capability is:

- one `PartDocument` can contain multiple `Body` records
- each body has stable `BodyId` identity independent from transient OCCT shapes
- features can create a new body, join into a target body, cut a target body, or intersect with a target body
- body-level transform features can translate, rotate, and scale bodies
- transform stacks preserve operation order
- transforming a body can optionally transform sketches and construction geometry owned by that body
- body boolean features add, subtract, or intersect semantic body references
- path curves can be composed from connected line, arc, spline, projected, construction, 3D-sketch, or semantic curve segments
- extrude and extruded cut can later follow a connected path curve
- loft can later use two or more profile sketches on arbitrarily oriented planes
- loft can later follow path or guide curves

Critical rule: body transforms and booleans are model intent. They must update through recompute and must not be represented as destructive raw BRep edits.

## Future Inventor-like sketcher and feature parity

The long-term sketcher roadmap is an Inventor-like parity target, not an immediate implementation target. It is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

The intended capability is:

- 2D sketch entities: points, lines, centerlines, rectangles, circles, arcs, ellipses, splines, polygons, slots, sketch fillets/chamfers, text, projected geometry, and included geometry
- sketch editing tools: trim, extend, split, break, offset, mirror, sketch patterns, move, copy, rotate, scale, stretch, and construction-geometry toggling
- sketch constraints: coincident, collinear, concentric, parallel, perpendicular, tangent, horizontal, vertical, equal, symmetric, midpoint, fix/ground, smooth, and point-on-curve relations
- sketch dimensions: linear, aligned, horizontal, vertical, angular, radius, diameter, arc length, point-to-line, point-to-point, driving, and driven/reference dimensions
- automatic region detection from unordered curves, including multiple regions, inner loops, islands, and profile-region selection
- feature families: richer extrude/cut, revolve, revolve cut, sweep, sweep cut, loft, loft cut, hole, thread, emboss, engrave, rib, web, shell, draft, patterns, mirrors, and surface workflows

This target depends on construction geometry, a stronger sketch entity model, profile-region detection, and eventually a constraint solver.

## Next construction geometry block

The next construction-geometry block is relation-driven datum geometry.

The intended capability is:

- `ConstructionRelation` model objects
- `PlaneOffsetFromPlane`
- `LineThroughTwoPoints`
- `PlaneThroughThreePoints`
- dependency graph edges from referenced construction objects to relation-driven construction objects
- JSON persistence for relation-driven construction definitions
- `WorkplaneResolver` support for relation-driven planes
- invalidation through point, line, plane, and parameter references

The detailed roadmap is in `docs/construction-geometry-mvp.md`.

## Future 3D sketching and surfacing

Advanced surfacing is a planned freeform-modeling layer, not yet implemented. It should introduce 3D sketch points, 3D lines, 3D splines, guide curves, sweeps, lofts, boundary surfaces, surface stitching, and closed-shell-to-solid conversion.

The intended capability is:

- connect points from different sketches on different planes with a 3D spline
- use those splines as guide curves for lofts, sweeps, or surface generation
- create sweeps along lines, polylines, arcs, splines, or reusable connected path curves
- create path-following extrudes and cuts along a connected path curve
- loft between two, three, or arbitrary many profile sketches
- loft between sketches on non-parallel or otherwise arbitrarily oriented planes
- use a middle profile as a smooth transition control without forcing a hard transition edge when smooth continuity is requested
- allow path/guide curves to control loft flow
- support continuity settings such as C0, G1, and G2 in later versions
- generate surfaces from arbitrary curves in space across several sketches
- stitch or knit several generated surfaces into a shell
- convert a closed stitched shell into a solid body
- support use cases such as turbine blades, propeller blades, wings, duct transitions, and pipe-to-pipe transitions

The detailed roadmap is in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

## Critical architecture topics

- Parameters must be first-class objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Sketches on generated faces require stable semantic references.
- User-created construction geometry should be model intent, not temporary UI state.
- Construction planes answer where sketches live; sketch profiles answer what shape sketches describe.
- Explicit construction geometry is now implemented; relation-driven construction geometry is the next datum-system block.
- A future `PartDocument` may contain multiple bodies; each body needs stable semantic identity and cache output.
- Body transforms must be feature-tree operations with dependency graph participation, not destructive geometry edits.
- If a transformed body owns sketches, sketch workplane frames should be transformable with the body while preserving sketch-local coordinates.
- Body booleans must reference semantic `BodyId` values and explicit tool/target body roles.
- Inventor-like sketching requires explicit sketch entities, constraints, dimensions, automatic profile detection, and stable region selection.
- 3D sketches and surfacing answer how spatial curves, multiple profile sections, guide curves, and surfaces form freeform geometry.
- Path curves provide reusable connected curve chains for sweep, path-extrude, path-cut, loft, and surfacing features.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target: adapters for rectangle extrusion, circular cut, line-based closed-profile extrusion/cut, a small ShapeCache, a WorkplaneResolver, recompute execution for `AdditiveExtrude` and `SubtractiveExtrude`, full document recompute, incremental recompute, bounds validation for the current top/bottom/right/left/front/back face cases, explicit construction-plane resolution, and STEP export of the final shape.
- The ShapeCache remains in the geometry layer; `PartDocument` remains OCCT-free and is computed into the cache through `execute_document` and `execute_plan`.
- JSON serialization stores model intent only; it does not serialize OCCT shapes or ShapeCache contents.
- Parameter values can be changed through `PartDocument::set_parameter_value`; a change marks dependents and drives incremental recompute.
- Derived workplanes and construction planes are resolved geometrically without turning raw OCCT faces into core model references.
- Line-based closed sketch profiles are implemented as a first planar general-profile path and remain separate from topological naming.
- Revolve, revolve cut, sweep, loft, path-following extrude, path-following cut, 3D sketch, surfacing, and surface-to-solid workflows are documented as future feature families and must remain semantic feature-tree operations, not raw BRep edits.
- Assembly parameters must later flow into parts in a controlled way.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- The assembly system will describe spatial relationships through constraints: a constraint graph and solver determine component positions and remaining degrees of freedom; joints later allow controlled motion.
- Edge and assembly references should remain semantic so they can survive model changes.
- Parameters have scopes (global, assembly, part, sketch, feature) and optional expressions; assembly parameters flow into parts for top-down design.
- Engineering assistants (hole wizard, shaft, bolt, bearing, gear) must size deterministically and traceably; AI is a later helper, never the sizing authority.
- The UI only operates the core and holds no CAD logic; it is built after the internal models work.
- The save file stores model intent; OCCT shapes are a regenerable cache and never the primary persisted data.

## Detailed target-architecture documents

The condensed points above are expanded in dedicated documents. Each is written as an incremental, testable block (goal, data model, dependency-graph integration, MVP scope, implementation sequence, out-of-scope):

- `docs/semantic-references.md` — the non-topological reference rule shared by all feature and assembly documents
- `docs/parameter-model.md` — scopes, expressions, cross-part flow, top-down design
- `docs/feature-system.md` — general feature model and sketch-driven feature families
- `docs/multi-body-transform-and-path-features-roadmap.md` — multi-body part files, body transforms, body booleans, path-following extrudes/cuts, and path/loft features
- `docs/inventor-like-sketcher-and-feature-roadmap.md` — Inventor-like 2D/3D sketcher, constraints, dimensions, profile detection, revolve, sweep, loft, and sketch-driven feature parity
- `docs/construction-geometry-mvp.md` — explicit construction geometry and next relation-driven construction geometry
- `docs/file-format.md` — project and save format
- `docs/fillet-chamfer-features.md` — fillets and chamfers
- `docs/pattern-and-mirror-features.md` — linear/circular patterns and mirror
- `docs/hole-wizard.md` — semantic hole features and the standards database
- `docs/shaft-wizard.md` — shaft calculation and geometry generation
- `docs/assembly-system.md` — constraints, solver, joints, motion
- `docs/engineering-modules.md` — bolt, bearing, gear, material, standard-parts modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md` — 3D sketches, guide curves, sweep, loft, boundary surfaces, surface stitching, and closed-shell-to-solid conversion
- `docs/user-interface.md` — UI architecture over the core
