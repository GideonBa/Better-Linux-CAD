# Architecture Summary

Source: condensed from the current repository architecture documents and implemented MVP status.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, and later multi-body, path, surfacing, and engineering intent. The explicit long-term goal is recorded in `docs/project-goal.md`.

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
- future `Expression`
- `Sketch`
- `SketchEntityId`
- line, arc, spline, projected, and profile-region sketch records
- `SketchConstraint` and `SketchDimension` families
- `DatumPlane`
- `DerivedWorkplane`
- `SemanticFaceReference`, `SemanticEdgeReference`, and `SemanticVertexReference`
- `ConstructionPoint`
- `ConstructionLine` / datum-axis intent
- `ConstructionPlane`
- `ConstructionRelation`
- future `BodyId`, `Body`, `BodyTransform`, `BodyTransformStack`, and `BodyBooleanFeature`
- future `SketchOwnership`
- future `PathCurve`, `PathSegmentReference`, and `ProfileSectionReference`
- future `SketchPoint3D`, `SketchCurve3D`, and `GuideCurve`
- feature families including implemented extrude/cut paths and future revolve, sweep, loft, surface, fillet, and chamfer features
- `FeatureReference`
- `DependencyGraph`
- `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraintTarget`
- `AssemblyConstraint`
- `AssemblyConstraintGraphEdge`
- `AssemblyConstraintGraph`
- `ComponentLocalPlanarDescriptor`
- `ResolvedAssemblyConstraintTarget`
- `AssemblyConstraintTargetResolver`
- future assembly transform evaluator, rigid-body assembly solver, and `Joint`

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

The implemented workplane/reference path introduces semantic generated-face references and resolves them in the geometry layer:

- `SemanticFaceReference` identifies generated faces such as `feature.base_extrude.top` and the other supported principal faces.
- `DerivedWorkplane` exposes semantic generated faces as sketch workplanes.
- `Sketch` can reference datum, derived, and implemented construction workplanes.
- The dependency graph connects source features, workplanes, sketches, and dependent features.
- JSON persists implemented semantic workplane intent.
- `WorkplaneResolver` maps supported semantic faces and construction planes to concrete frames.
- `WorkplaneResolver::resolve_generated_face` exposes the same generated-face frame path directly to other geometry-layer consumers.
- Geometry recompute maps sketch-local profile coordinates through resolved workplanes.
- Incremental recompute follows semantic workplane and construction dependency paths after source changes.
- `ShapeCache` removes stale dirty feature shapes before incremental recompute.

Persistent model intent remains deliberately different from raw OCCT face IDs.

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
- JSON persistence for implemented model-intent records
- geometry execution for implemented line/composite profile paths

The sketch-repair chain additionally provides diagnostics, deterministic repair suggestions, safe repair commands, transaction capture, undo stacks, and read-only presentation snapshots. Presentation helpers are frozen until a real CLI or GUI consumer exists.

## Implemented construction geometry and datum relations

The current headless core/geometry path implements explicit and relation-driven construction geometry; `docs/construction-geometry-mvp.md` is the canonical detailed status document.

Implemented capabilities include:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- explicit and relation-driven construction points, lines, and planes
- `PlaneOffsetFromPlane`, `PlaneThroughThreePoints`, and `PlaneParallelToPlaneThroughPoint`
- `LineThroughTwoPoints`, line-parallel-through-point relations, and generated-edge-parallel line relations
- generated-vertex and generated-edge construction-point relations
- semantic generated edge and vertex references without raw OCCT topology ids
- `SemanticReferenceEvaluator`, `ConstructionPointResolver`, and `ConstructionLineResolver`
- dependency-graph integration and chained construction relations
- JSON roundtrip for explicit, relation-driven, chained, and semantic-reference construction geometry
- `WorkplaneResolver` support for implemented construction-plane families

Some relation types such as `PointOnPlane`, `PointOnLine`, and `LineOnPlane` are validated model-intent records but are not geometrically solved. Expression-evaluated coordinates, arbitrary topology families, and GUI manipulators remain later work.

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

## Implemented MVP-4 assembly and project path

The cross-part and project-container path is implemented through `docs/assembly-parameters-mvp4.md` and `docs/project-container-mvp4.md`:

- `ParameterScope` distinguishes part and assembly scope.
- `AssemblyDocument` owns assembly-scoped parameters, member-part ids, and `ParameterBinding` records.
- `apply_bindings_to(PartDocument&)` pushes bound values through the normal part update/invalidation path.
- `Project` owns one `AssemblyDocument` and embedded `PartDocument` objects.
- Project validation resolves assembly members to project-owned parts.
- Project-level assembly parameter updates propagate bindings and produce per-part recompute plans.
- Assembly/project JSON persist model intent through the historical compatibility markers `blcad.assembly_document.mvp4` and `blcad.project.mvp4`.
- `blcad_export_project` provides a headless project-level parameter-update, recompute, and per-part STEP export path.

Assembly-level geometry/export and external/manifest part references remain deferred.

## Implemented MVP-5 component instances and free placement

The occurrence and free-placement blocks are implemented (`docs/component-instance-mvp5.md`):

- `ComponentInstanceId` identifies assembly occurrences independently from part-document identity.
- `ComponentInstance` stores identity, name, referenced part document, visibility, suppression state, grounding state, and `RigidTransform`.
- Multiple component instances can reference one project-owned `PartDocument` without duplicating the owned part model intent.
- Free-placement translation and rotation components must be finite.
- Assembly and project validation check component references.
- Copy-style component updates preserve identity and referenced part intent.
- `AssemblyDocument` exposes explicit transform, visibility, suppression, and grounding update APIs.
- The public component update entry points share one internal lookup/replacement path.
- Direct transform updates infer no constraints and run no solver or DOF analysis.
- Grounding, visibility, and suppression remain stored model intent until future assembly consumers enforce their semantics.
- Assembly/project JSON roundtrip preserves component placement/state.

Assembly-level geometry instancing is not implemented.

## Implemented MVP-5 assembly constraint model intent

The persistent assembly relationship layer is implemented (`docs/assembly-constraint-model-intent-mvp5.md`):

- `AssemblyConstraintId` is a dedicated typed id.
- `AssemblyConstraintTarget` combines an existing `ComponentInstanceId` with a persistent non-empty semantic reference token.
- `AssemblyConstraintType` is limited to Mate, Concentric, and Distance.
- `AssemblyConstraint` stores stable id/name, two targets, active/inactive state, and a Distance-only length quantity.
- Distance requires a length; Mate and Concentric reject distance values.
- `AssemblyDocument` validates unique constraint ids and existing component target ids.
- Constraint creation and JSON loading do not mutate component `RigidTransform` values.
- Semantic target tokens remain opaque and unresolved at the record layer.
- Additive `assembly_constraints` JSON remains backward compatible with old files that omit the field.
- Project structure validation includes constraint component targets.
- Multiple constrained component occurrences may still share one owned `PartDocument` model.
- `blcad_inspect_project_components` exposes stored constraint type, state, semantic targets, and optional distance.

No solved transform, DOF state, or graph cache is persisted by this block.

## Implemented MVP-5 read-only assembly constraint graph

The first derived assembly relationship graph is implemented (`docs/assembly-constraint-graph-mvp5.md`):

- `AssemblyConstraintGraph` is built read-only from an `AssemblyDocument`.
- Every `ComponentInstanceId` becomes a node, including isolated components.
- Every active `AssemblyConstraintId` becomes one distinct graph edge.
- Inactive constraints do not participate in connectivity.
- `AssemblyConstraintGraphEdge` preserves the constraint id and target-A/target-B component endpoints.
- Multiple constraints between the same component pair remain distinct legal multi-edges.
- Active edge endpoints are defensively revalidated during graph construction.
- Nodes are ordered lexicographically by component id.
- Edges and adjacency results are ordered lexicographically by constraint id.
- `adjacent_constraints` provides deterministic incident-edge queries.
- `connected_components` provides deterministic independent component groups and includes isolated nodes.
- Graph construction and queries do not mutate transforms, grounding, constraint state, or part model intent.
- `blcad_inspect_project_components` prints graph node/active-edge counts and deterministic connected groups.

The graph is regenerable derived data and is not serialized. It is distinct from the part recompute `DependencyGraph` and is not yet a solver graph.

## Implemented MVP-5 read-only assembly target resolution

The first semantic assembly target geometry bridge is implemented (`docs/assembly-constraint-target-resolution-mvp5.md`):

- `AssemblyConstraintTargetResolver` is a read-only optional geometry-layer consumer of persistent target intent.
- Target component ids resolve through the project assembly to a concrete `ComponentInstance`.
- Component part references resolve to project-owned `PartDocument` objects.
- The supported assembly target family is `feature.<feature-id>.{top,bottom,right,left,front,back}`.
- Supported targets become `SemanticFaceReference` values and reuse `WorkplaneResolver::resolve_generated_face`.
- `ComponentLocalPlanarDescriptor` carries origin, x-axis, y-axis, and normal in component-local coordinates.
- `ResolvedAssemblyConstraintTarget` preserves component identity, referenced part identity, source feature, semantic face, local plane, and the separate persisted `RigidTransform`.
- Malformed and unsupported target families fail explicitly.
- Semantic axes and generated edge/vertex assembly targets remain unsupported.
- Resolution does not mutate component transforms, constraints, part model intent, or geometry cache ownership.
- Resolved target descriptors are regenerable derived geometry data and are not serialized.

The target resolver does not interpret `rotation_deg` or produce assembly-space target geometry. This preserves a clean boundary before an explicit rotation convention exists.

## Next assembly block

The next core-CAD MVP block is explicit assembly rigid-transform evaluation.

The first evaluator should:

- consume the existing persisted `RigidTransform`
- document the exact angle and rotation-order convention for `rotation_deg`
- evaluate component-local points into assembly space by rotation followed by translation
- evaluate vectors, basis axes, and normals by rotation only
- evaluate `ComponentLocalPlanarDescriptor` into a deterministic assembly-space planar frame
- prove the rotation order with combined-axis tests rather than relying on an implicit Euler convention
- remain read-only and avoid persisted transform-evaluation cache data

Constraint equation construction, rigid-body solving, enforced grounding, remaining-DOF computation, and overconstraint analysis remain later work. The detailed sequence is maintained in `docs/mvp-plan.md` and `docs/assembly-system.md`.

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

The intended capability includes richer 2D entities and editing tools, more complete geometric constraints and dimensions, robust region selection, 3D sketches, and broader sketch-driven feature families such as revolve, sweep, loft, hole, thread, shell, pattern, mirror, and surfacing workflows.

This target depends on construction geometry, stable semantic references, the existing sketch entity/profile work, and stronger constraint solving where required.

## Future 3D sketching and surfacing

Advanced surfacing is a planned freeform-modeling layer, not yet implemented. It should introduce 3D sketch points, 3D lines, 3D splines, guide curves, sweeps, lofts, boundary surfaces, surface stitching, and closed-shell-to-solid conversion.

The intended capability includes connecting geometry across differently oriented sketches, reusable spatial guide/path curves, path-following features, arbitrary multi-section lofts, and surface workflows without reducing the model to opaque imported BRep edits.

## Critical architecture topics

- Parameters are first-class model-intent objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Persistent geometry references must be semantic rather than raw OCCT topology identifiers.
- User-created explicit and relation-driven construction geometry is model intent, not temporary UI state.
- Construction planes answer where sketches live; sketch profiles answer what shape sketches describe.
- Relation-driven construction geometry and chained construction relations are implemented for the documented deterministic families.
- A future `PartDocument` may contain multiple bodies; each body needs stable semantic identity and cache output.
- Body transforms must be feature-tree operations with dependency graph participation, not destructive geometry edits.
- Body booleans must reference semantic `BodyId` values and explicit tool/target body roles.
- Path curves provide reusable connected semantic curve chains for path-driven feature and surfacing families.
- OCCT shapes are a cache, not the primary model.
- The OCCT path lives in an optional `blcad_geometry` target; `PartDocument` remains OCCT-free.
- JSON serialization stores model intent only; it does not serialize OCCT shapes or `ShapeCache` contents as the source of truth.
- Parameter changes use the normal document update/invalidation path and drive incremental recompute.
- Assembly parameters flow into member parts through explicit `ParameterBinding` records.
- Component instances reference part model intent without duplicating owned `PartDocument` objects.
- Component placement/state updates remain explicit free-placement edits until a solver produces derived placement.
- Assembly constraints use semantic component targets and remain model intent distinct from solver/cache output.
- `AssemblyConstraintGraph` connectivity is regenerated from persistent records rather than stored as a second source of truth.
- Active/inactive constraint state controls graph participation; component suppression does not yet alter graph connectivity.
- `AssemblyConstraintTargetResolver` reuses existing semantic generated-face geometry and remains distinct from solving.
- Generated-face target descriptors are component-local; the persisted `RigidTransform` remains separate until explicit assembly-space evaluation is defined.
- A future assembly solver will consume connected groups and assembly-space resolved targets to determine component positions and remaining degrees of freedom.
- Fillets and chamfers are their own parametric features with semantic edge references, not only late BRep corrections.
- Engineering assistants must size deterministically and traceably; AI is a later helper, never the sizing authority.
- The UI only operates the core and holds no CAD logic; it is built after the internal models work.
- The save file stores model intent; computed geometry, graph connectivity, target-resolution results, and future solved cache data remain regenerable.

## Detailed target-architecture documents

The condensed points above are expanded in dedicated documents. Each is written as an incremental, testable block:

- `docs/semantic-references.md` — the non-topological reference rule shared by feature and assembly documents
- `docs/parameter-model.md` — scopes, expressions, cross-part flow, top-down design
- `docs/feature-system.md` — general feature model and sketch-driven feature families
- `docs/multi-body-transform-and-path-features-roadmap.md` — multi-body part files, body transforms, body booleans, path-following extrudes/cuts, and path/loft features
- `docs/inventor-like-sketcher-and-feature-roadmap.md` — Inventor-like 2D/3D sketcher, constraints, dimensions, profile detection, and sketch-driven feature parity
- `docs/construction-geometry-mvp.md` — implemented explicit/relation-driven construction geometry and remaining datum limitations
- `docs/file-format.md` — project and save format
- `docs/fillet-chamfer-features.md` — fillets and chamfers
- `docs/pattern-and-mirror-features.md` — linear/circular patterns and mirror
- `docs/hole-wizard.md` — semantic hole features and the standards database
- `docs/shaft-wizard.md` — shaft calculation and geometry generation
- `docs/assembly-system.md` — component instances, persistent constraint intent, graph connectivity, target resolution, transform evaluation, solver, joints, and motion
- `docs/assembly-constraint-model-intent-mvp5.md` — implemented Mate/Concentric/Distance record layer
- `docs/assembly-constraint-graph-mvp5.md` — implemented read-only active-constraint connectivity graph
- `docs/assembly-constraint-target-resolution-mvp5.md` — implemented read-only generated-face assembly target resolution
- `docs/engineering-modules.md` — bolt, bearing, gear, material, and standard-parts modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md` — 3D sketches, guide curves, sweep, loft, boundary surfaces, surface stitching, and closed-shell-to-solid conversion
- `docs/user-interface.md` — UI architecture over the core
