# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD stores CAD model intent in its own data structures and uses OCCT/Open CASCADE as computed geometry rather than as the primary model authority.

Detailed architecture, feature contracts, and implementation status live in `docs/`.

## Status

Implemented seeds now cover single-part parametric modeling, semantic references and richer sketch/profile workflows, a parametric bolt-circle pattern, assembly parameters/project ownership, deterministic local Mate/Distance/Concentric/Insert/Angle solving, local rank/remaining-DOF diagnostics, local Revolute motion, rigid nested assembly hierarchy, posed STEP export, interference/clearance analysis, document-scoped flexible child solving, occurrence-qualified cross-hierarchy geometric solving/fresh application/diagnostics, Project-level occurrence-qualified Revolute motion, and deterministic structured STEP assembly/product export with repeated occurrence identity and shared part definitions.

There is no GUI yet. Current work remains focused on headless CAD-core and application contracts.

## Assembly architecture snapshot

```text
local component + geometric relationship intent
  -> deterministic local graph
  -> current semantic generated plane / axis / seat resolution
  -> shared residual/Jacobian/Gauss-Newton path
  -> exact modeled-input freshness
  -> atomic explicit application

local + Project-level Revolute intent
  -> local or occurrence-qualified .seat endpoints
  -> combined geometric/joint motion closure
  -> shared directed axis/seating/signed-twist mathematics
  -> authored-coordinate holding drives
  -> same authority variables + numeric engine
  -> atomic transforms + selected coordinate application

explicit root assembly + Project-owned child assemblies
  -> rigid SubassemblyInstance placement/state
  -> validated cycle-free hierarchy
  -> deterministic rooted occurrence traversal
  -> visible-active AssemblyLeafOccurrenceDescriptor values
  -> exact inner-to-outer authored transform chains
  -> posed geometry, analysis, and exchange consumers

structured assembly exchange
  -> assembly occurrence identity = exact rooted SubassemblyInstance path
  -> component occurrence identity = (containing rooted path, local ComponentInstanceId)
  -> part product definition identity = PartDocumentId
  -> root + every assembly path prefix required by one visible-active leaf
  -> deterministic lexicographic assembly/component/part order
  -> collision-free percent-encoded generated exchange names
  -> one recompute + one ShapeCache + one unposed shape definition per unique PartDocumentId
  -> direct local component and SubassemblyInstance boundary placements
  -> XDE assembly/component references
  -> STEPCAF structured AP214 transfer
```

The cross-hierarchy solve/motion architecture separates:

```text
semantic endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Repeated rooted occurrences can have different root-space geometry while sharing one child-document `ComponentInstance::transform()` authority. They therefore retain separate endpoint/occurrence context but one transform variable/proposal authority.

Structured exchange introduces a separate derived identity split:

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

part component occurrence
  = (containing assembly occurrence path, local ComponentInstanceId)

part product definition
  = referenced PartDocumentId
```

`AssemblyExchangeGraph` derives these values from `AssemblyHierarchyTraversal` and the already filtered `AssemblyLeafOccurrenceResolver`. It retains the root plus every assembly path prefix required by one visible-active leaf. Hidden/suppressed components and hidden/suppressed hierarchy branches therefore follow the same export policy as existing posed-leaf consumers.

Assembly occurrence order is lexicographic by exact path sequence. Component occurrence order is path then local component id. Part product definitions sort by `PartDocumentId`. None of these identities or orders depend on OCCT topology ids, XDE label tags, STEP entity ids, or insertion order.

Generated exchange names keep readable ordinary ids while percent-encoding authored id bytes outside `A-Z a-z 0-9 . _ -`. Literal `/` and `%` inside authored ids therefore cannot collide with path separators or percent escapes. The explicit `root` path spelling is reserved separately from a non-root occurrence whose authored id is literally `root`.

`AssemblyPartShapeDefinitionBuilder` sorts/deduplicates referenced part ids, recomputes each unique exported `PartDocument` exactly once into one private `ShapeCache`, and exposes one unposed OCCT shape definition. Both `AssemblyPosedLeafShapeBuilder` and `AssemblyStructuredStepExporter` reuse this boundary.

The shared internal OCCT transform conversion preserves the established active fixed-axis X-then-Y-then-Z rotation followed by translation. The flattened posed-leaf path composes the canonical leaf chain through this helper. Structured STEP does not compose a second root-space chain: part components use the first/direct component transform from the exact leaf chain, while child assembly products use the direct hierarchy-derived `transform_from_parent` boundary.

`AssemblyStructuredStepExporter` creates one XDE part definition label per exported `PartDocumentId`, one assembly label per rooted exchange assembly occurrence, part component references to shared definitions, and exact parent-to-child assembly references. `STEPCAFControl_Writer` transfers the explicit root product graph with names enabled. The existing `AssemblyStepExporter` remains the flattened-compound compatibility consumer.

Persistent Project-level cross-hierarchy fields remain:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Block 29 adds no JSON field. Exchange graphs, generated names, part shape definitions, XDE labels, product/reference relationships, STEP entities, and export summaries remain derived and unpersisted.

## Planned general assembly target and joint expansion

The current assembly target resolver is intentionally narrower than an Inventor-/SolidWorks-like selector. It currently resolves generated planar feature faces plus the narrow circular-feature `.axis` and `.seat` families.

After the current Block-30 contact/sweep work, Blocks 31-47 expand the headless model in strict authority order:

```text
semantic source identity
  -> typed geometric descriptors and capabilities
  -> explicit target compatibility
  -> persistent generic relationship intent
  -> shared relationship equation/numeric integration
  -> joint target compatibility and oriented Frame semantics
  -> general multi-coordinate joint state
  -> coordinate JSON compatibility
  -> vector joint drive execution
  -> one richer joint family per block
```

Planned semantic source kinds include:

```text
GeneratedPlanarFace
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
CircularFeatureSeat
```

Planned derived solver capabilities are separate:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

For example:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedCircularEdge -> Circle + Axis + Point(center)
DatumPlane -> Plane
DatumAxis -> Axis + Line
CircularFeatureSeat -> Frame + Axis + Plane
```

This lets equation builders consume geometry capabilities rather than feature-specific source kinds. A cylindrical face, circular edge, DatumAxis, and current generated `.axis` can eventually reach the same `Axis <-> Axis` Concentric semantics.

The sequence is deliberately split:

```text
31 target taxonomy/capability projection
32 reference geometry Core intent
33 reference geometry serialization
34 reference target resolution
35 generated topology semantic identity/recovery
36 generated topology target resolution
37 explicit compatibility matrix
38 Coincident/Parallel/Perpendicular persistent intent + JSON
39 generic relationship equations + existing solver/diagnostics
40 joint compatibility + oriented Frame contract
41 general coordinate/limit Core model
42 coordinate JSON + legacy Revolute compatibility
43 vector drives + holding/freshness/atomic apply
44 Prismatic
45 Cylindrical
46 Planar
47 Ball/Spherical
```

The first planned generic geometric relationship families are:

```text
Coincident
Parallel
Perpendicular
```

The first richer joint families are planned one family per block after the general multi-coordinate drive boundary exists:

```text
Prismatic
Cylindrical
Planar
Ball/Spherical
```

Current Revolute remains `Frame <-> Frame`. Axis-only Revolute is not enabled merely because two sources expose Axis capability: signed twist requires an oriented reference direction. Geometry may not invent an arbitrary world-axis reference.

Raw OCCT face/edge/vertex ids, topology traversal positions, XDE labels, or STEP entity ids will not become persistent assembly target identity.

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

See `docs/assembly-structured-step-products-mvp5.md` and `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` for the current assembly handoff.

## Technical basis

- C++20
- CMake + Ninja
- OCCT / Open CASCADE Technology
- nlohmann-json
- Catch2
- Qt 6 planned for a future GUI layer

## Build and test

Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Manual geometry build:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
```

Focused current assembly tests:

```bash
./build/dev/blcad_core_tests "[core][assembly-joint]"
./build/dev/blcad_core_tests "[core][assembly-hierarchy]"
./build/dev/blcad_core_tests "[core][assembly-leaf-occurrence]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-motion-graph]"
./build/dev/blcad_core_tests "[core][assembly-exchange-graph]"

./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-revolute]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-motion]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-nested-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

## Headless tools

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
blcad_move_joint <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>
blcad_analyze_assembly <input.blcad.project.json> [clearance-threshold-mm]
```

`blcad_export_posed_assembly` remains the solved-root/local flattened compatibility flow. `blcad_export_structured_assembly` exports the current authored/persisted Project pose as a structured assembly/product STEP graph and does not implicitly run any solver.

## Repository structure

- `include/blcad/`: public headers
- `src/`: Core and optional Geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` / `.blcad.project.json` examples and headless flows
- `docs/`: architecture, implemented MVP contracts, sequences, and future roadmaps

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/file-format.md`

Current assembly handoff:

- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-revolute-joint-motion-mvp5.md`
- `docs/assembly-flexible-subassembly-solving-mvp5.md`
- `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`
- `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`
- `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`
- `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`
- `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

Broader future roadmaps:

- `docs/assembly-general-geometric-target-roadmap.md`
- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 30 only** from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: richer posed contact classification and swept-motion analysis over the now-frozen rooted exchange/occurrence identities.

Block 30 must extend the existing posed leaf/interference analysis boundary. Occurrence-local child pose overrides, whole-subassembly solve variables, richer joint families, and a general physics engine remain deferred.

After Block 30, implement **Block 31 only** from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and capability projection while preserving all current target strings and relationship behavior.
