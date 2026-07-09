# MVP Plan

## MVP 1: Single-part modeling

Goal: one single parametric part with a headless file-based export path.

Detailed document: `docs/mvp-1-specification.md`

Implemented scope:

- part document
- parameters with units
- simple 2D sketch on a standard plane
- rectangle and circle profiles
- simple dimensions through parameters
- additive extrude
- subtractive through-all cut
- dependency graph
- invalidation state
- recompute plan
- numeric parameter update
- optional OCCT geometry execution
- ShapeCache as computed result
- STEP export
- JSON serialization of model intent
- `.blcad.json` file read/write helpers
- checked-in reference model under `examples/`
- headless JSON-to-STEP command-line example

Example: rectangular plate with width, height, thickness, and a centered hole.

Current state: MVP 1 has the required core data models, dependency graph, invalidation state, recompute plan, numeric parameter updates, JSON serialization of model intent, `.blcad.json` file workflow, optional OCCT geometry execution, centered cut, full document recompute, and STEP export. The checked-in `examples/reference_plate.blcad.json` model can be loaded by `blcad_export_step`, recomputed into a fresh `ShapeCache`, and exported as STEP.

## MVP 2: Sketch on planar face

Goal: place sketches on existing generated planar faces while preserving semantic model intent instead of storing raw OCCT face IDs.

Implemented seed:

- semantic reference for the top face of `feature.base_extrude`
- semantic reference for the bottom face of `feature.base_extrude`
- semantic reference for the right `+X` face of `feature.base_extrude`
- semantic reference for the left `-X` face of `feature.base_extrude`
- semantic reference for the front `+Y` face of `feature.base_extrude`
- semantic reference for the back `-Y` face of `feature.base_extrude`
- `DerivedWorkplane` from those semantic faces
- sketches can reference derived workplanes
- dependency graph includes paths such as `feature.base_extrude -> workplane.base_back -> sketch.back_hole`
- JSON serialization supports `derived_workplanes` with `top`, `bottom`, `right`, `left`, `front`, and `back` faces
- checked-in `examples/top_face_cut.blcad.json`
- checked-in `examples/bottom_face_cut.blcad.json`
- checked-in `examples/right_face_cut.blcad.json`
- checked-in `examples/left_face_cut.blcad.json`
- checked-in `examples/front_face_cut.blcad.json`
- checked-in `examples/back_face_cut.blcad.json`
- geometry-layer `WorkplaneResolver`
- top-face frame resolved from source rectangle sketch and thickness parameter
- bottom-face frame resolved from source rectangle sketch at `z = 0` with negative normal
- right-face frame resolved at `x = width / 2` with normal `+X`
- left-face frame resolved at `x = -width / 2` with normal `-X`
- front-face frame resolved at `y = height / 2` with normal `+Y`
- back-face frame resolved at `y = -height / 2` with normal `-Y`
- subtractive recompute evaluates circle centers through the resolved workplane
- axis-directed through-all circular cuts
- rectangular bounds on resolved top, bottom, right, left, front, and back workplanes
- near-edge valid hole test and out-of-bounds invalid hole test
- incremental recompute through derived-workplane dependencies
- stale dirty feature shapes are removed before incremental recompute
- shrinking the source rectangle can invalidate an existing top-face hole

Next narrow step:

- do not add arbitrary generated-face topology immediately
- keep the controlled six-face seed stable while moving to the next sketch-modeling block
- start the general closed sketch profile MVP documented below

## Future MVP: General closed sketch profiles

Goal: support arbitrary closed sketch profiles instead of only rectangle and circle profile primitives.

Detailed document: `docs/general-closed-sketch-profile-mvp.md`

Explicitly not implemented yet:

- free line chains
- polylines
- arcs
- splines
- connected sketch entities
- general closed sketch loops
- closed wires
- multiple contours in one sketch
- inner holes in the same sketch profile
- profile selection from several closed regions
- validation of closed loops and self-intersections
- general OCCT face creation from arbitrary sketch wires
- additive extrude from arbitrary `TopoDS_Wire` / `TopoDS_Face`
- subtractive extrude from arbitrary `TopoDS_Wire` / `TopoDS_Face`

Proposed first implementation sequence:

- add `LineSegment`
- add `SketchLoop` / `ClosedProfile`
- validate closed and connected loops
- reject self-intersections initially
- create an OCCT `TopoDS_Wire` and `TopoDS_Face` in the geometry layer
- support additive extrude for one closed profile
- support subtractive through-all extrude for one closed profile
- add JSON roundtrip and geometry tests for non-rectangular profiles

This block comes after the controlled semantic-face workplane sequence, because arbitrary profiles still need reliable workplane placement, bounds handling, JSON persistence, recompute ordering, and STEP export.

## Future MVP: Construction geometry and relation-driven datum system

Goal: support user-created construction planes, construction lines/axes, and construction points that can be placed freely in 3D and can also be defined through geometric relationships.

Detailed document: `docs/construction-geometry-mvp.md`

Explicitly not implemented yet:

- user-created construction planes beyond the current fixed `datum.xy` and semantic-face workplanes
- user-created construction lines / datum axes
- user-created construction points / datum points
- arbitrary numeric 3D placement of construction geometry
- sketches on user-created construction planes
- relation-driven placement such as parallel, orthogonal, angle, offset, point-on-line, point-on-plane, line-through-two-points, and plane-through-three-points
- relation-driven invalidation through the dependency graph
- JSON persistence of construction geometry and construction relations
- references from construction geometry to generated semantic faces, edges, vertices, or analytic surfaces

Proposed first implementation sequence:

- add `ConstructionPoint`, `ConstructionLine`, and `ConstructionPlane` IDs and core model types
- add explicit placement definitions for point, line, and plane
- add `PartDocument` storage, validation, and dependency graph nodes
- allow sketches to reference construction planes as workplanes
- add JSON roundtrip tests
- resolve construction planes into `ResolvedWorkplane` frames in the geometry layer
- add recompute tests for a feature driven by a sketch on a construction plane
- add relation models for offset plane, line through two points, plane through three points, parallel, orthogonal, and angle-based construction

This block answers where a sketch can live. The general closed sketch profile block answers what shape a sketch can describe. They should remain separate, testable increments.

## MVP 3: Parametric bolt circle

Goal: first meaningful parametric feature test.

Detailed documents: `docs/feature-system.md`, `docs/hole-wizard.md`, `docs/pattern-and-mirror-features.md`

- bolt circle radius as parameter
- hole count as parameter
- hole diameter as parameter
- automatic recompute on change

## MVP 4: Simple assembly parameters

Goal: cross-part parametrization.

Detailed documents: `docs/parameter-model.md`, `docs/file-format.md`

- assembly document
- two part documents in one assembly
- shared assembly parameter
- both parts use the same bolt-circle parameter

## MVP 5: Simple assembly constraints

Goal: place parts through constraints.

Detailed document: `docs/assembly-system.md`

- component instances
- axis-to-axis
- face-to-face
- simple screw placement

## Later target-architecture blocks

These are documented as incremental, testable blocks and are sequenced after the numbered MVPs above:

- `docs/fillet-chamfer-features.md`: fillets and chamfers as parametric features
- `docs/pattern-and-mirror-features.md`: linear/circular patterns and mirror
- `docs/hole-wizard.md`: semantic hole features and the standards database
- `docs/shaft-wizard.md`: shaft assistant (calculation plus geometry generation)
- `docs/assembly-system.md`: full constraint solver, joints, motion, subassemblies
- `docs/engineering-modules.md`: bolt, bearing, gear, material, and standard-parts modules
- `docs/user-interface.md`: UI over the core

The cross-cutting rules they all rely on are in `docs/semantic-references.md` and `docs/parameter-model.md`.
