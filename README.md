# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD stores CAD model intent in its own data structures and uses OCCT/Open CASCADE as computed geometry rather than as the primary model authority.

Detailed architecture, feature contracts, and implementation status live in `docs/`.

## Status

Implemented seeds now cover single-part parametric modeling, semantic references and richer sketch/profile workflows, a parametric bolt-circle pattern, assembly parameters/project ownership, deterministic local Mate/Distance/Concentric/Insert/Angle solving, local rank/remaining-DOF diagnostics, local Revolute motion, rigid nested assembly hierarchy, posed STEP export, interference/clearance analysis, document-scoped flexible child solving, occurrence-qualified cross-hierarchy geometric solving/fresh application/diagnostics, Project-level occurrence-qualified Revolute motion, deterministic structured STEP assembly/product export, complete rooted `Separated`/`Touching`/`Interfering` contact classification, and bounded sampled local/cross-hierarchy Revolute sweep analysis.

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

rooted contact and sampled motion analysis
  -> exact rooted component occurrence pair identity
  -> every visible-active unordered pair classified once
  -> positive common-solid volume first
  -> minimum distance for non-interfering pairs
  -> explicit touching tolerance
  -> Separated / Touching / Interfering
  -> root-local or Project cross-hierarchy Revolute dispatch
  -> 2..1001 inclusive requested-coordinate samples
  -> fresh source Project copy per sample
  -> existing motion solver + atomic applier
  -> complete rooted contact analysis per sample
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

Structured exchange adds a separate derived identity split:

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

`AssemblyPartShapeDefinitionBuilder` sorts/deduplicates referenced part ids, recomputes each unique exported `PartDocument` exactly once into one private `ShapeCache`, and exposes one unposed OCCT shape definition. `AssemblyPosedLeafShapeBuilder` and `AssemblyStructuredStepExporter` reuse this boundary.

The shared internal OCCT transform conversion preserves the established active fixed-axis X-then-Y-then-Z rotation followed by translation. The flattened posed-leaf path composes the canonical leaf chain through this helper. Structured STEP does not compose a second root-space chain: part components use the first/direct component transform from the exact leaf chain, while child assembly products use the direct hierarchy-derived `transform_from_parent` boundary.

`AssemblyStructuredStepExporter` creates one XDE part definition label per exported `PartDocumentId`, one assembly label per rooted exchange assembly occurrence, part component references to shared definitions, and exact parent-to-child assembly references. `STEPCAFControl_Writer` transfers the explicit root product graph. Exact nested component occurrence names remain BLCAD exchange-graph identity; STEPCAF does not guarantee every nested instance name verbatim in the Part 21 text.

## Rooted contact classification

`AssemblyContactAnalyzer` classifies every visible-active unordered posed component pair using the exact typed:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing rooted assembly occurrence path,
   local ComponentInstanceId)
```

One contact pair is stored in canonical typed occurrence order. It is not keyed by a slash-joined string or OCCT topology identity.

Classification is frozen as:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

Defaults are:

```text
touching_tolerance_mm = 1.0e-6
minimum_overlap_volume_mm3 = 1.0e-6
```

`AssemblyContactAnalysis::records` is a complete pair classification set. `Interfering` records carry overlap volume and no distance. `Touching`/`Separated` records carry measured finite minimum distance.

The historical `AssemblyInterferenceAnalyzer` and `AssemblyClearanceAnalyzer` remain unchanged compatibility query contracts.

## Bounded sampled Revolute sweep

`AssemblyRevoluteSweepAnalyzer` supports the two existing public Revolute motion query boundaries:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

The request selects one active Revolute joint, start/end `AngleDeg` coordinates inside its authored limits, and a sample count:

```text
2 <= sample_count <= 1001
```

Samples are linear, inclusive, and preserve caller direction:

```text
0 -> 90, count 3
  = [0, 45, 90]

90 -> 0, count 3
  = [90, 45, 0]
```

Every sample starts from a fresh copy of the same source Project. The existing scope-specific motion solver produces a result, the existing atomic applier applies it to the sample copy, and `AssemblyContactAnalyzer` classifies the resulting posed geometry. Samples do not accumulate preceding-sample numerical drift, and source transforms/authored joint coordinates remain unchanged.

This is deterministic discrete sampled motion analysis. It is **not continuous collision detection**; contact existing only between two requested sample coordinates can be missed.

Persistent Project-level cross-hierarchy fields remain:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Blocks 29 and 30 add no JSON fields. Exchange graphs, generated names, part shape definitions, XDE labels, contact records, sample Project copies, motion sample products, and sweep analyses remain derived and unpersisted.

## Planned general assembly target and joint expansion

With Block 30 implemented, Blocks 31-47 expand the current narrow generated plane/axis/seat target resolver toward an Inventor-/SolidWorks-like assembly target model in strict authority order:

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

Planned derived solver capabilities remain separate:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

Representative projections:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedCircularEdge -> Circle + Axis + Point(center)
DatumPlane -> Plane
DatumAxis -> Axis + Line
CircularFeatureSeat -> Frame + Axis + Plane
```

The numbered sequence is:

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

Current Revolute remains `Frame <-> Frame`. Axis-only Revolute is not enabled merely because two sources expose Axis capability: signed twist requires an oriented reference direction. Geometry may not invent an arbitrary world-axis reference.

Raw OCCT face/edge/vertex ids, topology traversal positions, XDE labels, or STEP entity ids will not become persistent assembly target identity.

Canonical roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

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
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-interference]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-clearance]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
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

Block 30 adds public library query APIs for complete contact classification and sampled Revolute sweep. It intentionally adds no CLI and no persistent analysis format.

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
- `docs/assembly-contact-swept-motion-mvp5.md`
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

Broader future roadmaps:

- `docs/assembly-general-geometric-target-roadmap.md`
- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 31 only** from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and capability projection.

Block 31 must separate selected semantic source kind from solver geometric capability while preserving all current semantic target strings and Mate/Distance/Angle/Concentric/Insert/Revolute numeric behavior. Do not add DatumAxis persistence, reference-geometry JSON, generic relationship families, or richer joint families in Block 31.
