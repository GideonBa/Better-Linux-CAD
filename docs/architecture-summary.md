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

Current and planned object families include:

- `PartDocument`, `AssemblyDocument`, and `Project`
- `Parameter` and future `Expression`
- `Sketch`, sketch entities, constraints, dimensions, and profile-region records
- `DatumPlane`, `DerivedWorkplane`, and construction geometry/relation records
- semantic face, edge, and vertex references
- future `BodyId`, `Body`, body transform/boolean records, and sketch ownership
- future path, 3D-sketch, guide-curve, loft, and surfacing records
- implemented extrude/cut feature paths and future richer feature families
- `DependencyGraph` and `ShapeCache`
- `ComponentInstance`
- `AssemblyConstraintTarget` and `AssemblyConstraint`
- `AssemblyConstraintGraphEdge` and `AssemblyConstraintGraph`
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
- `AssemblySolveState`
- `AssemblyRigidBodySolverOptions`
- `AssemblySolveResidualSummary`
- `AssemblySolveComponentSnapshot`
- `ProposedComponentTransform`
- `AssemblySolveResult`
- `AssemblyRigidBodySolver`
- `AssemblySolveResultApplier`
- future assembly DOF diagnostics and `Joint`

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
- JSON helpers persist and restore model intent.
- `blcad_export_step` provides a headless file-to-STEP workflow.

## MVP-2 semantic reference and richer sketch path

The implemented path includes:

- semantic generated-face references
- derived workplanes and implemented construction workplanes
- dependency edges from source geometry through workplanes to sketches/features
- `WorkplaneResolver` and direct generated-face resolution
- sketch-local profile mapping through resolved frames
- incremental invalidation and stale shape-cache removal
- stable sketch entity ids
- line, arc, spline, composite, projected, and detected profile-region records
- first sketch constraints and driving dimensions
- sketch-plane-relative extrude direction
- diagnostics, deterministic repair suggestions, safe repair commands, transactions, undo, and presentation snapshots

Persistent model intent remains deliberately different from raw OCCT topology ids.

## Implemented construction geometry and datum relations

`docs/construction-geometry-mvp.md` is canonical.

Implemented capabilities include explicit and relation-driven construction points, lines, and planes; chained construction relations; generated edge/vertex semantic references; geometry-layer resolvers; dependency integration; JSON roundtrip; and construction-plane support in `WorkplaneResolver`.

Some relation types remain validated intent without geometric solving. Expression-evaluated coordinates, arbitrary topology families, and GUI manipulators remain later work.

## Implemented MVP-3 parametric bolt circle

The first meaningful parametric feature test is implemented in `docs/bolt-circle-pattern-mvp3.md`:

- length and dimensionless count quantities
- count parameters
- circular hole pattern intent
- parameter-to-sketch dependency edges
- subtractive recompute expansion into through-all cuts
- incremental count/radius recompute
- JSON persistence
- checked-in headless STEP example

Hole-wizard semantics, partial/skipped patterns, and arbitrary seed-feature patterns remain later work.

## Implemented MVP-4 assembly/project path

The cross-part path is implemented through `docs/assembly-parameters-mvp4.md` and `docs/project-container-mvp4.md`:

- assembly-scoped parameters
- member-part registration and `ParameterBinding`
- binding propagation through normal part invalidation
- one `Project` owning an assembly and embedded part documents
- assembly-member validation
- project-level parameter propagation and per-part recompute plans
- assembly/project JSON model intent
- headless project parameter-update, recompute, and per-part STEP export

Assembly-level geometry/export and manifest-based external part references remain deferred.

## Implemented MVP-5 component instances and placement

`docs/component-instance-mvp5.md` is canonical.

- component identity is independent from part-document identity
- repeated occurrences may reference one project-owned part
- visibility, suppression, grounding, and finite `RigidTransform` placement are persisted
- explicit placement/state update APIs preserve component identity and part intent
- storage-level direct transform updates infer no constraints and do not run the solver
- assembly/project JSON roundtrip preserves placement/state

The persisted `RigidTransform` has an explicit geometry-evaluation convention in the transform-evaluator block.

## Implemented MVP-5 assembly constraint intent

`docs/assembly-constraint-model-intent-mvp5.md` is canonical.

- dedicated `AssemblyConstraintId`
- semantic component targets
- Mate, Concentric, and Distance record types
- active/inactive state
- Distance-only positive length value
- unique id and existing component-target validation
- no transform mutation during record creation/loading
- backward-compatible additive JSON persistence
- project structure validation
- headless constraint inspection

No solver cache or DOF state is persisted by the record layer.

## Implemented MVP-5 active-constraint graph

`docs/assembly-constraint-graph-mvp5.md` is canonical.

- every component is a node
- every active constraint is a distinct edge
- inactive constraints do not affect connectivity
- legal multi-edges
- deterministic node, edge, adjacency, and connected-group order
- read-only graph construction and queries
- derived graph data is not serialized

The exact connected groups are now the rigid-body solver partition boundary.

## Implemented MVP-5 semantic target resolution

`docs/assembly-constraint-target-resolution-mvp5.md` is canonical.

- target components resolve through the project assembly
- referenced parts resolve to project-owned `PartDocument` objects
- generated planar face targets support top/bottom/right/left/front/back
- generated-face geometry reuses `WorkplaneResolver::resolve_generated_face`
- local planar descriptors contain origin, basis axes, and normal
- persisted component transforms remain separate
- malformed and unsupported target families fail explicitly
- resolved descriptors are read-only and unpersisted

Semantic axes and generated edge/vertex assembly target families remain unsupported.

## Implemented MVP-5 rigid-transform evaluation

`docs/assembly-rigid-transform-evaluation-mvp5.md` is canonical.

- assembly-space planar descriptors are distinct from component-local descriptors
- translation is millimeters
- rotation is degrees
- rotations are active right-handed fixed-axis rotations applied X, then Y, then Z
- column-vector composition is `Rz * Ry * Rx`
- points are rotated and translated
- vectors, axes, and normals are rotated only
- magnitude and frame orthogonality are preserved within floating-point tolerance
- evaluation is deterministic, read-only, and unpersisted

## Implemented MVP-5 planar Mate/Distance residual construction

`docs/assembly-planar-constraint-equations-mvp5.md` is canonical.

The path is:

```text
AssemblyConstraint
  -> AssemblyConstraintTargetResolver
  -> AssemblyTransformEvaluator
  -> assembly-space target A/B planes
  -> AssemblyConstraintEquationBuilder
  -> AssemblyConstraintEquationDescriptor
```

Canonical Mate residuals:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Canonical Distance residuals:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

The signed Distance convention is target-order dependent. The builder preserves constraint and target identity, supports active planar Mate/Distance only, rejects Concentric until semantic axes exist, remains read-only, and does not persist residual descriptors.

## Implemented MVP-5 first rigid-body assembly solver

`docs/assembly-rigid-body-solver-mvp5.md` is canonical.

The solver path is:

```text
AssemblyConstraintGraph connected group
  -> exact deterministic group validation
  -> grounded fixed / free variable partition
  -> deterministic residual flattening
  -> central finite-difference Jacobian
  -> damped Gauss-Newton iteration
  -> proposed component transforms on a private Project copy
  -> AssemblySolveResult
  -> explicit AssemblySolveResultApplier
```

Participation policy:

- at least one grounded component is required
- every grounded component is fixed
- multiple grounded components are allowed
- all-grounded satisfied groups converge without proposals
- all-grounded inconsistent groups return `FixedGeometryInconsistent`
- suppressed components are explicitly rejected by the first solver seed
- visibility does not affect solve participation

Variable ordering is lexicographic by free component id. Each free component contributes `tx,ty,tz,rx_deg,ry_deg,rz_deg`, directly preserving persisted `RigidTransform` coordinate semantics.

Residuals are ordered by graph constraint id. Each Mate or Distance contributes its three orientation components followed by its length component divided by the default `1 mm` scale.

The numeric seed uses central finite differences, damped normal equations, partial-pivot Gaussian elimination, deterministic backtracking line search, and damping escalation.

Solve states are:

```text
Converged
MaximumIterationsReached
FixedGeometryInconsistent
NumericalFailure
```

The solver never mutates the source project. Every solve result snapshots all group component transforms plus grounding/suppression input state and returns free-component proposals separately.

`AssemblySolveResultApplier` accepts only converged results, rejects stale solve inputs including moved grounded anchors, prevalidates proposals, applies them to a project copy, and replaces the caller project only after all updates succeed.

Solver results, proposed transforms, Jacobians, and residual summaries remain derived and unpersisted.

## Next assembly block

The next core-CAD MVP block is read-only solve diagnostics and remaining-degree-of-freedom analysis.

The first diagnostics layer should:

- reuse the solver's deterministic connected-group, constraint, residual, and variable ordering
- expose or reuse a read-only numeric Jacobian evaluation path
- define a canonical Jacobian-rank tolerance
- compute variable count, local Jacobian rank, constrained DOF, and remaining DOF
- distinguish underconstrained and locally fully constrained groups
- preserve fixed-geometry and non-convergence diagnostics
- define an initial overconstraint/inconsistency boundary without equating every redundant residual with semantic overconstraint
- remain read-only and avoid persisted DOF cache state
- keep Concentric out of scope until semantic axis targets and Concentric residuals exist

## Future multi-body part modeling, transforms, and path features

The target multi-body layer is documented in `docs/multi-body-transform-and-path-features-roadmap.md`. Stable body identity, feature-output body roles, ordered transform stacks, semantic booleans, path curves, path-following feature families, and multi-section loft inputs are future work.

Body transforms and booleans are model intent and must update through recompute rather than destructive raw BRep edits.

## Future Inventor-like sketcher and feature parity

The long-term sketcher roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md` and includes richer 2D editing, more complete constraints/dimensions, robust region selection, 3D sketches, and broader sketch-driven feature families.

## Future 3D sketching and surfacing

`docs/advanced-surfacing-and-3d-sketch-mvp.md` plans spatial sketch entities, guide/path curves, sweeps, arbitrary multi-section lofts, boundary surfaces, stitching, and closed-shell-to-solid workflows.

## Critical architecture topics

- Parameters are first-class model-intent objects.
- Features store rules, not only result geometry.
- Recompute runs through a dependency graph.
- Persistent geometry references are semantic rather than raw OCCT topology identifiers.
- User-created construction geometry is model intent, not temporary UI state.
- OCCT shapes are cache data, not the primary model.
- The optional OCCT path lives in `blcad_geometry`; `PartDocument` remains OCCT-free.
- JSON serialization stores model intent rather than geometry cache output.
- Assembly parameters flow into member parts through explicit bindings.
- Component instances reference part intent without duplicating owned parts.
- Storage-level placement/state edits remain explicit and separate from solve policy.
- Assembly constraints are model intent distinct from graph, geometry, residual, Jacobian, and solver output.
- `AssemblyConstraintGraph` owns deterministic active relationship connectivity.
- `AssemblyConstraintTargetResolver` owns supported semantic target lookup and local frame construction.
- `AssemblyTransformEvaluator` owns the exact local-to-assembly coordinate convention.
- `AssemblyConstraintEquationBuilder` owns canonical planar Mate/Distance residual conventions.
- Target order is semantically observable for signed Distance residuals.
- `AssemblyRigidBodySolver` owns fixed/variable participation, residual flattening, numeric solve policy, and proposed transforms.
- `AssemblySolveResultApplier` owns the explicit fresh-result atomic transform mutation boundary.
- Graph, target, frame, residual, Jacobian, and solve-result data remain regenerable and unpersisted.
- Remaining-DOF analysis must define rank tolerance explicitly before claiming constraint state.
- Fillets and chamfers are parametric features with semantic edge references.
- Engineering assistants must size deterministically and traceably; AI is a later helper, never the sizing authority.
- The UI operates the core and must not own CAD logic.

## Detailed target-architecture documents

- `docs/semantic-references.md` — semantic reference rule
- `docs/parameter-model.md` — scopes, expressions, cross-part flow, top-down design
- `docs/feature-system.md` — general feature model and sketch-driven feature families
- `docs/multi-body-transform-and-path-features-roadmap.md` — multi-body, transform, boolean, path, and loft roadmap
- `docs/inventor-like-sketcher-and-feature-roadmap.md` — Inventor-like sketcher and feature parity roadmap
- `docs/construction-geometry-mvp.md` — implemented construction geometry and remaining limitations
- `docs/file-format.md` — project and save format
- `docs/fillet-chamfer-features.md` — fillets and chamfers
- `docs/pattern-and-mirror-features.md` — patterns and mirror
- `docs/hole-wizard.md` — semantic hole features and standards database
- `docs/shaft-wizard.md` — shaft calculation and geometry generation
- `docs/assembly-system.md` — assembly records, graph, target geometry, transforms, residuals, solver, DOF, joints, and motion
- `docs/assembly-constraint-model-intent-mvp5.md` — persistent constraint record layer
- `docs/assembly-constraint-graph-mvp5.md` — active-constraint connectivity graph
- `docs/assembly-constraint-target-resolution-mvp5.md` — generated-face target resolution
- `docs/assembly-rigid-transform-evaluation-mvp5.md` — transform convention and evaluator
- `docs/assembly-planar-constraint-equations-mvp5.md` — planar Mate/Distance residual construction
- `docs/assembly-rigid-body-solver-mvp5.md` — first deterministic rigid-body solver and result application boundary
- `docs/engineering-modules.md` — engineering assistants and standards-backed modules
- `docs/advanced-surfacing-and-3d-sketch-mvp.md` — 3D sketches and surfacing roadmap
- `docs/user-interface.md` — UI architecture over the core
