# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD owns CAD model intent in its own data structures and uses OCCT/Open CASCADE as a computed geometry and exchange kernel rather than as primary model authority.

Detailed architecture, exact feature contracts, persistence semantics, and implementation status live in `docs/`.

## Status

Implemented headless seeds now cover:

```text
single-part parametric modeling and feature history
semantic references and richer sketch/profile workflows
unit-aware parameter expressions and recompute planning
parametric bolt-circle patterns
assembly parameters, bindings, and Project ownership
local Mate / Distance / Angle / Concentric / Insert solving
local Jacobian-rank / remaining-DOF diagnostics
local Revolute motion
rigid nested assembly hierarchy
flattened posed STEP export
interference and clearance analysis
document-scoped flexible child solving
occurrence-qualified cross-hierarchy geometric solving
fresh atomic cross-hierarchy application and diagnostics
Project-level occurrence-qualified Revolute motion
structured XDE/STEP assembly/product export
rooted Separated / Touching / Interfering classification
bounded sampled local/cross-hierarchy Revolute sweep
typed assembly geometric target taxonomy and capability projection
assembly-selectable reference geometry intent and DatumAxis model
additive DatumAxis JSON and reference-spelling endpoint roundtrips
```

There is no GUI yet. Current work remains focused on CAD-core, Geometry query/execution, and headless application contracts.

## Authority model

BLCAD separates persistent semantic intent from derived Geometry execution products.

Persistent assembly endpoint identity remains:

```text
local
  (local ComponentInstanceId,
   semantic_reference)

cross-hierarchy
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

Persisted component transform authority remains:

```text
ComponentTransformAuthority =
  (assembly_document: DocumentId,
   local ComponentInstanceId)
```

Repeated rooted occurrences may have different root-space geometry while sharing one child-document component transform authority.

Structured exchange uses separate derived identities:

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

component occurrence
  = (containing rooted path,
     local ComponentInstanceId)

part product definition
  = PartDocumentId
```

Raw OCCT topology ids, XDE labels, and STEP entity ids are never persistent BLCAD model identity.

## Typed assembly geometric targets

Block 31 introduces a derived Geometry-layer split between semantic source classification and solver geometry capability.

Source kinds:

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

Capabilities:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

The canonical capability order is exactly the order above.

Representative projections are:

```text
GeneratedPlanarFace
  -> Plane

GeneratedCylindricalFace
  -> Axis
  -> Cylinder

GeneratedCircularEdge
  -> Axis
  -> Point(center)
  -> Circle

DatumPlane
  -> Plane

DatumAxis
  -> Axis
  -> Line

CircularFeatureSeat
  -> Plane
  -> Axis
  -> Frame
```

One `AssemblyResolvedGeometricTarget` retains:

```text
exact local or occurrence-qualified endpoint identity
resolved source kind
derived source model metadata
typed descriptor variant
canonical capability vector
component-local or root-assembly coordinate space
exact current transforms_inner_to_outer context
```

Consumers obtain typed geometry through:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projection validates the complete resolved target first and fails closed if the source does not expose the requested capability.

Current target strings remain unchanged:

```text
feature.<feature-id>.top
feature.<feature-id>.bottom
feature.<feature-id>.right
feature.<feature-id>.left
feature.<feature-id>.front
feature.<feature-id>.back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The six generated planar-face families resolve as `GeneratedPlanarFace -> Plane`. Plane preserves the existing independently oriented face-normal contract, so historical Bottom/side descriptors remain numerically unchanged.

The currently supported `.axis` producer remains the narrow single-CircleProfile subtractive-extrude feature. Block 31 classifies that derived producer as `GeneratedCylindricalFace`, derives its radius from the existing diameter parameter, and exposes `Axis + Cylinder`. The persisted `.axis` string remains unchanged.

`.seat` resolves as `CircularFeatureSeat -> Plane + Axis + Frame`. The Frame preserves historical seating X and semantic Axis Z, and deterministically derives `Y = Z × X` so the typed Frame is right-handed. Insert/Revolute residual-relevant axis, seating-normal, and signed-twist X-reference semantics remain unchanged.

Existing local and hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs remain compatibility adapters, but now obtain geometry through the typed capability projection boundary. Mate/Distance/Angle/Concentric/Insert/Revolute residual formulas and numeric execution remain unchanged.

Canonical Block-31 contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Reference geometry sources

Block 32 adds first-class `DatumAxis` PartDocument intent (`Explicit` and `FromConstructionLine` families) and freezes one canonical semantic-source spelling per reference family:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`, so reference spellings contain no `.` and can never collide with feature target spellings; ids containing `.`, `/`, and `%` stay unambiguous and parsing fails closed.

Block 33 serializes DatumAxis intent through the additive optional `datum_axes` part-document array and proves `ref:` endpoint spellings roundtrip byte-for-byte; loading validates ownership/family rules and resolves no geometry. Geometry resolution of `ref:` sources remains Block 34.

Canonical Blocks-32/33 contract: `docs/assembly-reference-geometry-intent-mvp5.md`.

## Assembly solving and motion

The current assembly execution chain remains:

```text
persistent relationship/joint intent
  -> deterministic local/cross connectivity
  -> ComponentTransformAuthority variable ownership
  -> typed semantic target resolution and capability projection
  -> local or exact rooted target geometry
  -> existing relationship/joint equation builders
  -> shared residual flattening
  -> shared central finite differences
  -> shared damped Gauss-Newton engine
  -> complete modeled-input freshness
  -> atomic explicit application
```

Every unique free active transform authority contributes exactly six direct component-transform variables:

```text
tx_mm
ty_mm
tz_mm
rx_deg
ry_deg
rz_deg
```

`SubassemblyInstance::transform()` remains rigid authored hierarchy-boundary intent and is not a component solve variable.

## Posed analysis and exchange

`AssemblyLeafOccurrenceResolver` remains the canonical visible-active hierarchy-to-part-leaf boundary.

`AssemblyPartShapeDefinitionBuilder` recomputes each unique referenced `PartDocumentId` once into one private `ShapeCache` and exposes one shared unposed shape definition.

Flattened posed geometry, structured STEP export, interference/clearance, and rooted contact analysis reuse these boundaries.

`AssemblyContactAnalyzer` evaluates every visible-active unordered rooted component occurrence pair once and classifies:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

`AssemblyRevoluteSweepAnalyzer` samples one selected existing root-local or Project-level cross-hierarchy Revolute interval at `2..1001` inclusive coordinates. Every sample starts from a fresh source Project copy, reuses the existing motion solver and atomic applier, then runs complete contact classification.

The sweep is deterministic discrete sampling. It is not continuous collision detection.

## Persistence

Current Project-level cross-hierarchy JSON fields remain:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Blocks 29–31 add no JSON fields.

Do not persist:

```text
hierarchy traversal
transform authority mappings
resolved target source kinds
geometric target descriptors
capability vectors
capability projections
residuals / Jacobians / solve results
freshness snapshots / proposals / diagnostics
exchange graphs / XDE labels / STEP entities
contact records
sweep sample Projects or sweep analyses
```

`docs/file-format.md` is canonical for actual save-format semantics.

## Build and test

Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Focused current assembly tests include:

```bash
./build/dev/blcad_core_tests "[core][assembly-hierarchy]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-motion-graph]"
./build/dev/blcad_core_tests "[core][assembly-exchange-graph]"

./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-revolute]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
```

The exact source/test registration in `CMakeLists.txt` remains authoritative.

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

The typed geometric target/capability layer is currently a public library query contract and adds no dedicated CLI or persistent analysis format.

## Repository structure

- `include/blcad/`: public headers
- `src/`: Core and optional Geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` / `.blcad.project.json` examples and headless flows
- `docs/`: architecture, implemented contracts, sequences, and future roadmaps

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/file-format.md`

Current assembly handoff:

- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`
- `docs/assembly-structured-step-products-mvp5.md`
- `docs/assembly-contact-swept-motion-mvp5.md`
- `docs/assembly-geometric-target-taxonomy-mvp5.md`
- `docs/assembly-reference-geometry-intent-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`

Broader roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 34 only** from `docs/assembly-general-geometric-target-roadmap.md`: datum, axis, line, and point target resolution.

Block 34 must resolve `ref:` semantic sources into the Block-31 taxonomy (`DatumPlane -> Plane`, `DatumAxis -> Axis + Line`, `ConstructionLine -> Line`, `ConstructionPoint -> Point`), reusing existing workplane/construction execution, component-local and exact rooted transform chains, and canonical PartDocument snapshot freshness.

Do not add stable generated topology identity in Block 34. That remains Block 35.
