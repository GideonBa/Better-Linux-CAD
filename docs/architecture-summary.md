# Architecture Summary

Source: condensed from the current repository architecture documents and implemented MVP status.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model does not only store final BRep geometry, but the underlying design intent: parameters, sketches, features, dependencies, semantic references, construction geometry, 3D curves, surfaces, multi-body part records, body transforms, body booleans, and assembly relationships. The explicit long-term goal is also recorded in `docs/project-goal.md`.

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

Current and planned core object families include:

- `Document`
- `PartDocument`
- `AssemblyDocument`
- `Project`
- `Parameter`
- `Expression`
- `Sketch`
- `SketchEntityId`
- `LineSegment`
- `ClosedProfile`
- sketch entities such as arcs, circles, splines, projected geometry, and later ellipses, polygons, slots, and text
- `SketchConstraint`, `SketchDimension`, `SketchRegion`, and `ProfileRegion` families
- `DatumPlane`
- `DerivedWorkplane`
- `SemanticFaceReference`
- `ConstructionPoint`
- `ConstructionLine` / `DatumAxis`
- `ConstructionPlane`
- future `ConstructionRelation`
- future `BodyId`, `Body`, `BodyTransform`, `BodyTransformStack`, and `BodyBooleanFeature`
- future `SketchOwnership`
- future `PathCurve`, `PathSegmentReference`, and `ProfileSectionReference`
- future `SketchPoint3D`, `SketchCurve3D`, and `GuideCurve`
- feature families including extrude/cut and future revolve, sweep, loft, surface, fillet, and chamfer features
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- future `AssemblyConstraint`
- future `Joint`

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

## MVP-2 semantic reference and workplane path

The MVP-2 workplane seed introduces semantic generated-face references and resolves them in the geometry layer:

- `SemanticFaceReference` identifies generated faces such as `feature.base_extrude.top` and the other principal faces.
- `DerivedWorkplane` exposes semantic generated faces as sketch workplanes.
- `Sketch` can reference datum, derived, and implemented construction workplanes.
- The dependency graph connects source features, workplanes, sketches, and dependent features.
- JSON persists the implemented semantic workplane intent.
- `WorkplaneResolver` maps supported semantic faces and explicit construction planes to concrete frames.
- Geometry recompute maps sketch-local profile coordinates through resolved workplanes.
- Incremental recompute follows the semantic workplane dependency path after source-dimension changes.
- `ShapeCache` removes stale dirty feature shapes before incremental recompute, so failed recompute does not leave old geometry behind.

This remains intentionally different from storing raw OCCT face IDs in `PartDocument`.

## Implemented richer sketch-driven profile path

The current sketch/profile blocks include:

- stable sketch entity ids
- line segments and ordered closed profiles
- connected-loop and self-intersection validation
- construction geometry and projected reference geometry
- reference recovery helpers
- arcs, trim/extend support, splines, and tangent-continuity records
- first sketch constraints and driving dimensions
- automatic profile-region detection
- composite closed profiles with holes
- sketch-plane-relative extrude direction
- JSON persistence for the implemented model-intent records
- geometry execution for the implemented line/composite profile paths

The sketch-repair chain additionally provides diagnostics, deterministic repair suggestions, safe repair commands, transaction capture, undo stacks, and read-only presentation snapshots. Presentation helpers are intentionally frozen until a real CLI or GUI consumer exists.

## Implemented explicit construction geometry

The first construction-geometry step is implemented:

- `ConstructionPointId`, `ConstructionLineId`, and `ConstructionPlaneId` identify construction geometry.
- `ConstructionPoint`, `ConstructionLine`, and `ConstructionPlane` are core model-intent objects.
- Construction geometry supports explicit numeric placement.
- Construction geometry can carry optional parameter dependencies for invalidation.
- `PartDocument` stores construction points, lines, and planes.
- Construction geometry receives dependency-graph nodes and parameter dependency edges.
- A sketch can reference a construction plane as its workplane.
- JSON serialization supports construction points, lines, and planes.
- `WorkplaneResolver` resolves explicit construction planes into concrete workplane frames.

Still intentionally missing from this datum block: expression-evaluated coordinate placement, surface-based construction geometry, GUI manipulators, and relation-driven construction geometry.

## Implemented MVP-3 parametric bolt circle

The first meaningful parametric feature test is implemented (`docs/bolt-circle-pattern-mvp3.md`):

- `Quantity` distinguishes length and dimensionless count values.
- `ParameterType::Count` stores whole-number parameters such as a hole count.
- `CircularHolePattern` stores center, angle offset, and radius/count/hole-diameter parameter references.
- `PartDocument` validates pattern parameter existence and types and creates parameter-to-sketch dependency edges.
- Subtractive recompute expands the pattern into sequential through-all circular cuts with per-hole bounds validation.
- Changing `bolt_count` or `bolt_circle_radius` drives incremental recompute of affected features.
- JSON persists count parameters and circular hole patterns.
- `examples/bolt_circle_plate.blcad.json` exports a six-hole plate to STEP headlessly.

Still intentionally missing: hole-wizard semantics, skipped/partial pattern instances, and arbitrary seed-feature patterns.

## Implemented MVP-4 shared assembly parameters

The first cross-part parametrization step is implemented (`docs/assembly-parameters-mvp4.md`):

- `ParameterScope` distinguishes part and assembly scope.
- `AssemblyDocument` owns assembly-scoped parameters, registers member parts by `DocumentId`, and stores explicit `ParameterBinding` records.
- `apply_bindings_to(PartDocument&)` pushes bound values through `PartDocument::set_parameter_value`, reusing part invalidation and recompute planning unchanged.
- Type agreement is enforced when bindings are applied.
- Assembly documents serialize as `blcad.assembly_document.mvp4`.
- The flange assembly path demonstrates one assembly parameter driving member-part parameters.

## Implemented MVP-4 project container

The project container is implemented (`docs/project-container-mvp4.md`):

- `Project` owns one `AssemblyDocument` and embedded `PartDocument` objects.
- Assembly member ids are validated against project-owned parts.
- Project-level assembly parameter updates propagate bindings into member parts.
- Affected parts produce per-part recompute plans.
- Project JSON persists embedded assembly and part model intent as `blcad.project.mvp4`.
- `blcad_export_project` provides a headless project-level parameter-update, recompute, and per-part STEP export path.

The project container still deliberately avoids assembly-level geometry/export and external/manifest part references.

## Implemented MVP-5 component instances and free placement

The first assembly-structure blocks are implemented (`docs/component-instance-mvp5.md`):

- `ComponentInstanceId` identifies assembly occurrences independently from part-document identity.
- `ComponentInstance` stores stable identity, display name, referenced part document, visibility, suppression state, grounding state, and `RigidTransform`.
- Multiple component instances can reference one project-owned `PartDocument` without duplicating part model intent.
- `AssemblyDocument` validates that component instances reference registered member parts.
- `Project` validates that component instance references resolve to project-owned parts.
- `ComponentInstance::with_*` operations preserve identity and referenced part intent while replacing one placement/state field.
- `AssemblyDocument` exposes explicit transform, visibility, suppression, and grounding update APIs for existing component instances.
- A transform update is a direct free-placement edit; no constraint is inferred and no solver or DOF analysis runs.
- Grounding is stored model intent only and does not yet prevent an explicit transform update.
- Visibility and suppression are stored state only until future assembly consumers define their behavior.
- Assembly/project JSON roundtrip preserves current component placement/state values.
- `blcad_inspect_project_components` exposes persisted component reference and placement/state fields headlessly.

The next assembly block is solver-independent constraint model intent on semantic component targets. Constraint graph construction and rigid-body solving remain later work.

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
- path curves can be composed from connected semantic curve sources
- extrude and cut can later follow connected path curves
- loft can later use two or more profile sketches on arbitrarily oriented planes
- loft can later follow path or guide curves

Critical rule: body transforms and booleans are model intent. They must update through recompute and must not be represented as destructive raw BRep edits.

## Future Inventor-like sketcher and feature parity

The long-term sketcher roadmap is an Inventor-like parity target, not an immediate implementation target. It is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

The intended capability includes richer 2D entities and editing tools, complete geometric constraints and dimensions, robust region selection, 3D sketches, and broader sketch-driven feature families such as revolve, sweep, loft, hole, thread, shell, pattern, mirror, and surfacing workflows.

This target depends on construction geometry, stable semantic references, the existing sketch entity/profile work, and eventually stronger constraint solving.

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

The intended capability includes connecting geometry across differently oriented sketches, reusable spatial guide/path curves, path-following features, arbitrary multi-section lofts, continuity controls, freeform surface generation, shell stitching, and conversion of closed shells into solid bodies.

The detailed roadmap is in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

## Critical architecture topics

- Parameters are first-class model-intent objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Persistent geometry references must be semantic rather than raw OCCT topology identifiers.
- User-created construction geometry is model intent, not temporary UI state.
- Construction planes answer where sketches live; sketch profiles answer what shape sketches describe.
- Explicit construction geometry is implemented; relation-driven construction geometry is the next datum-system block.
- A future `PartDocument` may contain multiple bodies; each body needs stable semantic identity and cache output.
- Body transforms must be feature-tree operations with dependency graph participation, not destructive geometry edits.
- Body booleans must reference semantic `BodyId` values and explicit tool/target body roles.
- Path curves provide reusable connected semantic curve chains for path-driven feature and surfacing families.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target; `PartDocument` remains OCCT-free.
- JSON serialization stores model intent only; it does not serialize OCCT shapes or `ShapeCache` contents as the source of truth.
- Parameter changes use the normal document update/invalidation path and drive incremental recompute.
- Assembly parameters already flow into member parts through explicit `ParameterBinding` records.
- Component instances reference part model intent without duplicating `PartDocument` objects.
- Component placement/state updates are explicit free-placement edits until assembly constraints and a solver exist.
- Future assembly constraints must use semantic component targets and must remain model intent distinct from solver/cache output.
- The assembly system will eventually describe spatial relationships through constraints; a constraint graph and solver determine component positions and remaining degrees of freedom.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- Engineering assistants must size deterministically and traceably; AI is a later helper, never the sizing authority.
- The UI only operates the core and holds no CAD logic; it is built after the internal models work.
- The save file stores model intent; computed geometry and future solved cache data remain regenerable.

## Detailed target-architecture documents

The condensed points above are expanded in dedicated documents. Each is written as an incremental, testable block:

- `docs/semantic-references.md` — the non-topological reference rule shared by feature and assembly documents
- `docs/parameter-model.md` — scopes, expressions, cross-part flow, top-down design
- `docs/feature-system.md` — general feature model and sketch-driven feature families
- `docs/multi-body-transform-and-path-features-roadmap.md` — multi-body part files, body transforms, body booleans, path-following extrudes/cuts, and path/loft features
- `docs/inventor-like-sketcher-and-feature-roadmap.md` — Inventor-like 2D/3D sketcher, constraints, dimensions, profile detection, and sketch-driven feature parity
- `docs/construction-geometry-mvp.md` — explicit construction geometry and next relation-driven construction geometry
- `docs/file-format.md` — project and save format
- `docs/fillet-chamfer-features.md` — fillets and chamfers
- `docs/pattern-and-mirror-features.md` — linear/circular patterns and mirror
- `docs/hole-wizard.md` — semantic hole features and the standards database
- `docs/shaft-wizard.md` — shaft calculation and geometry generation
- `docs/assembly-system.md` — component instances, constraints, solver, joints, and motion
- `docs/engineering-modules.md` — bolt, bearing, gear, material, and standard-parts modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md` — 3D sketches, guide curves, sweep, loft, boundary surfaces, surface stitching, and closed-shell-to-solid conversion
- `docs/user-interface.md` — UI architecture over the core
