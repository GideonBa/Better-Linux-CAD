# Architecture Summary

Source: condensed from the current repository architecture documents and implemented MVP status.

## Goal

BLCAD is intended to become an independent parametric CAD system for Linux. The model stores design intent rather than only final BRep geometry: parameters, sketches, features, dependencies, semantic references, construction geometry, assembly structure, and later multi-body, path, surfacing, and engineering intent.

The explicit long-term goal is recorded in `docs/project-goal.md`.

## Fundamental decision

- Do not fork FreeCAD.
- Use OCCT as the geometry kernel.
- Keep the custom CAD core above OCCT.
- Treat OCCT shapes as computed cache data, not primary CAD model intent.
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
- `ConstructionLine`
- `ConstructionPlane`
- `ConstructionRelation`
- future `BodyId`, `Body`, `BodyTransform`, `BodyTransformStack`, and `BodyBooleanFeature`
- future `SketchOwnership`
- future `PathCurve`, `PathSegmentReference`, and `ProfileSectionReference`
- future `SketchPoint3D`, `SketchCurve3D`, and `GuideCurve`
- implemented extrude/cut feature paths and future revolve, sweep, loft, surface, fillet, and chamfer families
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
- `AssemblySpacePlanarDescriptor`
- `AssemblyTransformEvaluator`
- `AssemblySpaceConstraintTargetDescriptor`
- `PlanarMateResidualDescriptor`
- `PlanarDistanceResidualDescriptor`
- `AssemblyConstraintEquationDescriptor`
- `AssemblyConstraintEquationBuilder`
- future rigid-body assembly solver and `Joint`

## MVP-1 implemented vertical slice

The MVP-1 code implements the first narrow vertical slice:

- `PartDocument` stores model intent for one part.
- Parameters are typed, named, and validated.
- Datum planes and simple sketches exist as data models.
- Rectangle and circle profiles are supported.
- `AdditiveExtrude` and `SubtractiveExtrude` exist as feature-intent data models.
- `DependencyGraph`, `InvalidationState`, and `RecomputePlan` derive recompute order.
- The optional `blcad_geometry` target converts model intent into OCCT geometry.
- `ShapeCache` stores computed feature shapes and the final shape.
- `StepExporter` writes the final shape to STEP.
- `PartDocument::set_parameter_value` enables numeric incremental recompute.
- JSON and `.blcad.json` helpers persist and restore model intent.
- `blcad_export_step` provides a headless file-to-STEP workflow.

## MVP-2 semantic reference and workplane path

The implemented workplane/reference path introduces semantic generated-face references:

- `SemanticFaceReference` identifies generated faces such as `feature.base_extrude.top`.
- `DerivedWorkplane` exposes semantic generated faces as sketch workplanes.
- `Sketch` references implemented datum, derived, and construction workplanes.
- The dependency graph connects source features, workplanes, sketches, and dependent features.
- JSON persists implemented semantic workplane intent.
- `WorkplaneResolver` maps supported semantic faces and construction planes to concrete frames.
- `WorkplaneResolver::resolve_generated_face` exposes the same generated-face frame path to other geometry consumers.
- Geometry recompute maps sketch-local profile coordinates through resolved workplanes.
- Incremental recompute follows semantic workplane and construction dependency paths.
- `ShapeCache` removes stale dirty feature shapes before incremental recompute.

Persistent model intent remains deliberately different from raw OCCT topology ids.

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

`docs/construction-geometry-mvp.md` is the canonical detailed status document.

Implemented capabilities include:

- `ConstructionPointId`, `ConstructionLineId`, `ConstructionPlaneId`, and `ConstructionRelationId`
- explicit and relation-driven construction points, lines, and planes
- `PlaneOffsetFromPlane`, `PlaneThroughThreePoints`, and `PlaneParallelToPlaneThroughPoint`
- `LineThroughTwoPoints`, line-parallel-through-point, and generated-edge-parallel line relations
- generated-vertex and generated-edge construction-point relations
- semantic generated edge and vertex references without raw OCCT topology ids
- `SemanticReferenceEvaluator`, `ConstructionPointResolver`, and `ConstructionLineResolver`
- dependency-graph integration and chained construction relations
- JSON roundtrip for implemented construction geometry
- `WorkplaneResolver` support for implemented construction-plane families

Some relation types remain validated model-intent records without geometric solving. Expression-evaluated coordinates, arbitrary topology families, and GUI manipulators remain later work.

## Implemented MVP-3 parametric bolt circle

The first meaningful parametric feature test is implemented in `docs/bolt-circle-pattern-mvp3.md`:

- `Quantity` distinguishes length and dimensionless count values.
- `ParameterType::Count` stores whole-number values such as a hole count.
- `CircularHolePattern` stores center, angle offset, and radius/count/hole-diameter parameter references.
- `PartDocument` validates pattern parameters and creates dependency edges.
- Subtractive recompute expands the pattern into sequential through-all circular cuts.
- Changing count or radius drives incremental recompute.
- JSON persists count parameters and circular hole patterns.
- `examples/bolt_circle_plate.blcad.json` exports a six-hole plate headlessly.

Hole-wizard semantics, skipped/partial pattern instances, and arbitrary seed-feature patterns remain later work.

## Implemented MVP-4 assembly and project path

The cross-part path is implemented through `docs/assembly-parameters-mvp4.md` and `docs/project-container-mvp4.md`:

- `ParameterScope` distinguishes part and assembly scope.
- `AssemblyDocument` owns assembly parameters, member-part ids, and `ParameterBinding` records.
- `apply_bindings_to(PartDocument&)` pushes bound values through normal part invalidation.
- `Project` owns one `AssemblyDocument` and embedded `PartDocument` objects.
- Project validation resolves assembly members to project-owned parts.
- Assembly parameter updates propagate bindings and produce per-part recompute plans.
- Assembly/project JSON persist model intent through the historical compatibility markers `blcad.assembly_document.mvp4` and `blcad.project.mvp4`.
- `blcad_export_project` provides a headless project parameter-update, recompute, and per-part STEP export path.

Assembly-level geometry/export and external manifest part references remain deferred.

## Implemented MVP-5 component instances and free placement

`docs/component-instance-mvp5.md` is canonical.

- `ComponentInstanceId` identifies occurrences independently from part-document identity.
- `ComponentInstance` stores identity, name, referenced part, visibility, suppression, grounding, and `RigidTransform`.
- Multiple instances may reference one project-owned `PartDocument`.
- Free-placement transform components must be finite.
- Assembly and project validation check component references.
- Copy-style updates preserve identity and referenced part intent.
- `AssemblyDocument` exposes transform and component-state update APIs.
- Direct transform updates infer no constraints and run no solver or DOF analysis.
- Assembly/project JSON roundtrip preserves component placement/state.

The persisted `RigidTransform` has an explicit geometry-evaluation convention in the transform-evaluator block.

## Implemented MVP-5 assembly constraint model intent

`docs/assembly-constraint-model-intent-mvp5.md` is canonical.

- `AssemblyConstraintId` is a dedicated typed id.
- `AssemblyConstraintTarget` combines a component id with a persistent semantic reference token.
- `AssemblyConstraintType` is limited to Mate, Concentric, and Distance.
- `AssemblyConstraint` stores id/name, target A/B, active/inactive state, and a Distance-only length quantity.
- Distance requires a length; Mate and Concentric reject distance values.
- `AssemblyDocument` validates unique ids and existing component targets.
- Constraint creation and JSON loading do not mutate component transforms.
- Semantic target tokens remain opaque at the record layer.
- Additive `assembly_constraints` JSON remains backward compatible.
- Project structure validation includes constraint component targets.
- `blcad_inspect_project_components` exposes stored constraint intent.

No solved transform or DOF state is persisted by this block.

## Implemented MVP-5 read-only assembly constraint graph

`docs/assembly-constraint-graph-mvp5.md` is canonical.

- every component becomes a node, including isolated occurrences
- every active constraint becomes one distinct edge
- inactive constraints do not participate in connectivity
- multiple constraints between one component pair remain legal multi-edges
- active edge endpoints are defensively revalidated
- nodes, edges, adjacency, and connected groups are deterministic
- graph construction and queries do not mutate assembly or part model intent
- the headless inspector prints graph-group summaries

The graph is regenerable derived data and is not serialized.

## Implemented MVP-5 semantic assembly target resolution

`docs/assembly-constraint-target-resolution-mvp5.md` is canonical.

- `AssemblyConstraintTargetResolver` consumes persistent target intent read-only.
- Target component ids resolve through the project assembly.
- Component part references resolve to project-owned `PartDocument` objects.
- The supported family is `feature.<feature-id>.{top,bottom,right,left,front,back}`.
- Supported targets reuse `WorkplaneResolver::resolve_generated_face`.
- `ComponentLocalPlanarDescriptor` carries local origin, basis axes, and normal.
- `ResolvedAssemblyConstraintTarget` preserves component, part, feature, face, local plane, and separate persisted transform.
- Malformed and unsupported target families fail explicitly.
- Semantic axes and generated edge/vertex assembly targets remain unsupported.
- Resolution does not mutate component transforms, constraints, part intent, or geometry cache ownership.
- Resolved target descriptors are not serialized.

Target resolution owns semantic lookup and local frame construction only.

## Implemented MVP-5 rigid-transform evaluation

`docs/assembly-rigid-transform-evaluation-mvp5.md` is canonical.

- `AssemblySpacePlanarDescriptor` represents a planar frame in assembly coordinates.
- `AssemblyTransformEvaluator` evaluates points, vectors, and local planar descriptors.
- translation is interpreted in millimeters
- rotation is interpreted in degrees
- positive rotations follow the right-hand rule
- active fixed-axis rotations are applied X, then Y, then Z
- column-vector composition is `Rz * Ry * Rx`
- points are rotated and translated
- vectors, axes, and normals are rotated without translation
- rigid rotation preserves vector magnitude and frame orthogonality within tolerance
- repeated evaluation is deterministic
- transform evaluation is read-only and unpersisted

The assembly-space target path is:

```text
AssemblyConstraintTarget
  -> AssemblyConstraintTargetResolver
  -> ComponentLocalPlanarDescriptor + RigidTransform
  -> AssemblyTransformEvaluator
  -> AssemblySpacePlanarDescriptor
```

## Implemented MVP-5 planar constraint residual construction

`docs/assembly-planar-constraint-equations-mvp5.md` is canonical.

`AssemblyConstraintEquationBuilder` completes the first geometric constraint-evaluation path:

```text
AssemblyConstraint
  -> target A/B resolution
  -> target A/B assembly-space transform evaluation
  -> Mate or Distance residual construction
  -> AssemblyConstraintEquationDescriptor
```

Implemented descriptor families:

- `AssemblySpaceConstraintTargetDescriptor`
- `PlanarMateResidualDescriptor`
- `PlanarDistanceResidualDescriptor`
- `AssemblyConstraintEquationDescriptor`
- `AssemblyConstraintEquationBuilder`

Canonical Mate residuals:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Canonical planar Distance residuals:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

The signed Distance convention is target-order dependent and uses target A's normal. The builder preserves constraint id/type, target component ids, semantic tokens, and target A/B order.

The builder:

- supports active Mate and Distance only
- reuses target resolution and transform evaluation
- rejects inactive constraints before geometry resolution
- rejects Concentric before semantic-axis resolution
- resolves target A before target B for deterministic error precedence
- propagates target-resolution failures
- remains read-only
- does not solve or mutate transforms
- does not persist residual descriptors

Residual Jacobians, weighting, convergence tolerances, and solve policy remain future solver concerns.

## Next assembly block

The next core-CAD MVP block is a first rigid-body assembly solver seed.

The first solver should:

- consume one deterministic connected component group from `AssemblyConstraintGraph`
- define grounded components as fixed references for the first seed
- define behavior for groups with zero or multiple grounded components
- consume supported active Mate/Distance residuals from `AssemblyConstraintEquationBuilder`
- define the variable rigid-transform representation explicitly
- preserve persisted `RigidTransform` semantics at the API boundary
- flatten residuals in deterministic constraint-id/component order
- define orientation-versus-length residual weighting
- define numeric tolerances, maximum iterations, convergence, and failure behavior
- return proposed transforms before mutating model intent
- expose a separate explicit successful-result application boundary
- leave project state unchanged on failed/non-converged solves
- keep Concentric out of scope until semantic axis targets and Concentric residuals exist

Remaining-DOF computation and under/fully/overconstrained analysis follow the first stable solver path.

## Future multi-body part modeling, transforms, and path features

The target multi-body feature layer is documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

The intended capability includes stable body identity, feature-output body roles, ordered body transform stacks, semantic body booleans, path curves, path-following feature families, and multi-section loft inputs.

Body transforms and booleans are model intent. They must update through recompute and must not be represented as destructive raw BRep edits.

## Future Inventor-like sketcher and feature parity

The long-term sketcher roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

The intended capability includes richer 2D entities and editing tools, more complete geometric constraints and dimensions, robust region selection, 3D sketches, and broader sketch-driven feature families such as revolve, sweep, loft, hole, thread, shell, pattern, mirror, and surfacing workflows.

## Future 3D sketching and surfacing

Advanced surfacing is planned in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

The intended capability includes spatial sketch entities, reusable guide/path curves, sweeps, arbitrary multi-section lofts, boundary surfaces, stitching, and closed-shell-to-solid workflows without reducing the model to opaque imported BRep edits.

## Critical architecture topics

- Parameters are first-class model-intent objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Persistent geometry references are semantic rather than raw OCCT topology identifiers.
- User-created construction geometry is model intent, not temporary UI state.
- Construction planes answer where sketches live; profiles answer what shape sketches describe.
- OCCT shapes are cache data, not the primary model.
- The optional OCCT path lives in `blcad_geometry`; `PartDocument` remains OCCT-free.
- JSON serialization stores model intent rather than geometry cache output.
- Assembly parameters flow into member parts through explicit `ParameterBinding` records.
- Component instances reference part model intent without duplicating owned parts.
- Component placement/state edits remain explicit until a solver produces proposed placement.
- Assembly constraints are semantic model intent distinct from graph, geometry, residual, and solver output.
- `AssemblyConstraintGraph` connectivity is regenerated from persistent records.
- `AssemblyConstraintTargetResolver` owns supported semantic target lookup and local frame construction.
- `AssemblyTransformEvaluator` owns the exact local-to-assembly coordinate convention.
- `AssemblyConstraintEquationBuilder` owns the first canonical planar Mate/Distance residual conventions.
- Target order is semantically observable for signed Distance residuals and must be preserved by later solver code.
- Graph connectivity, resolved targets, assembly-space frames, and residual descriptors are regenerable and unpersisted.
- A future solver must define grounding participation, residual weighting, convergence, and transform application explicitly.
- Fillets and chamfers are parametric features with semantic edge references, not only late BRep corrections.
- Engineering assistants must size deterministically and traceably; AI is a later helper, never the sizing authority.
- The UI operates the core and must not own CAD logic.

## Detailed target-architecture documents

- `docs/semantic-references.md` — semantic reference rule shared by feature and assembly documents
- `docs/parameter-model.md` — scopes, expressions, cross-part flow, top-down design
- `docs/feature-system.md` — general feature model and sketch-driven feature families
- `docs/multi-body-transform-and-path-features-roadmap.md` — multi-body, transform, boolean, path, and loft roadmap
- `docs/inventor-like-sketcher-and-feature-roadmap.md` — Inventor-like 2D/3D sketcher and feature parity roadmap
- `docs/construction-geometry-mvp.md` — implemented construction geometry and remaining limitations
- `docs/file-format.md` — project and save format
- `docs/fillet-chamfer-features.md` — fillets and chamfers
- `docs/pattern-and-mirror-features.md` — patterns and mirror
- `docs/hole-wizard.md` — semantic hole features and standards database
- `docs/shaft-wizard.md` — shaft calculation and geometry generation
- `docs/assembly-system.md` — assembly records, graph, target geometry, transforms, residuals, solver, DOF, joints, and motion
- `docs/assembly-constraint-model-intent-mvp5.md` — persistent Mate/Concentric/Distance record layer
- `docs/assembly-constraint-graph-mvp5.md` — active-constraint connectivity graph
- `docs/assembly-constraint-target-resolution-mvp5.md` — generated-face assembly target resolution
- `docs/assembly-rigid-transform-evaluation-mvp5.md` — rigid-transform convention and evaluator
- `docs/assembly-planar-constraint-equations-mvp5.md` — planar Mate/Distance residual construction
- `docs/engineering-modules.md` — engineering assistants and standards-backed modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md` — 3D sketches and surfacing roadmap
- `docs/user-interface.md` — UI architecture over the core
