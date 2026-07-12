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
reference geometry target resolution into Plane/Axis/Line/Point capabilities
stable producer-driven generated topology identity and read-only recovery
canonical topo: semantic endpoint spellings for supported generated topology
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

Semantic-reference strings currently include legacy feature-role targets, canonical `ref:` reference-geometry targets, and canonical `topo:` generated-topology producer-role targets.

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

Raw OCCT topology ids, traversal indices, topology hashes/map positions, XDE labels, STEP entity ids, and memory addresses are never persistent BLCAD model identity.

## Typed assembly geometric targets

The derived Geometry target layer separates semantic source classification from geometric capability.

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

Representative projections are:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Axis + Cylinder
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Axis + Point(center) + Circle
GeneratedVertex          -> Point
DatumPlane               -> Plane
DatumAxis                -> Axis + Line
ConstructionLine         -> Line
ConstructionPoint        -> Point
CircularFeatureSeat      -> Plane + Axis + Frame
```

One `AssemblyResolvedGeometricTarget` retains exact endpoint identity, resolved source kind, derived source model metadata, typed descriptor variant, canonical capability vector, coordinate space, and exact transform context.

Consumers obtain geometry through:

```text
project_plane
project_axis
project_line
project_point
project_circle
project_cylinder
project_frame
```

Every projection validates the complete resolved target and fails closed if the source does not expose the requested capability.

Current legacy target strings remain unchanged:

```text
<feature-id>.top
<feature-id>.bottom
<feature-id>.right
<feature-id>.left
<feature-id>.front
<feature-id>.back
<feature-id>.axis
<feature-id>.seat
```

The six generated planar-face families resolve as `GeneratedPlanarFace -> Plane`. The current narrow `.axis` path resolves as `GeneratedCylindricalFace -> Axis + Cylinder`. `.seat` resolves as `CircularFeatureSeat -> Plane + Axis + Frame` and preserves Insert/Revolute orientation and signed-twist semantics.

Existing local and hierarchy `resolve`, `resolve_axis`, and `resolve_insert` APIs remain compatibility adapters. Mate/Distance/Angle/Concentric/Insert/Revolute residual formulas and numeric execution remain unchanged.

Canonical target contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

## Reference geometry sources

First-class `DatumAxis` PartDocument intent supports `Explicit` and `FromConstructionLine` families.

Canonical reference semantic-source spellings are:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`. Valid reference spellings contain no `.` and remain disjoint from legacy feature-role spellings.

DatumAxis intent serializes through additive optional `datum_axes`. Existing endpoint JSON shapes are unchanged and `ref:` strings roundtrip byte-for-byte.

Geometry resolution maps:

```text
DatumPlane        -> Plane
DatumAxis         -> Axis + Line
ConstructionLine  -> Line
ConstructionPoint -> Point
```

Local results remain component-local. Hierarchy results use the exact rooted transform chain. Canonical PartDocument snapshots remain freshness authority.

Canonical reference geometry contract: `docs/assembly-reference-geometry-intent-mvp5.md`.

## Stable generated topology identity and recovery

Block 35 establishes Core semantic identity before generated topology lookup.

Canonical generated-topology endpoint spellings are:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:source_rim
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:opposite_rim
topo:vertex:<encoded-feature-id>:<role>
```

The `topo:` grammar uses the same strict uppercase `%HH` canonical encoding discipline. Valid spellings are dot-free, preserve arbitrary typed-id bytes, and remain distinct from both `ref:` and legacy feature-role strings.

The first supported producer matrices are:

```text
RectangularAdditiveExtrude
  -> 12 named linear-edge roles, expected cardinality 1 each
  -> 8 named vertex roles, expected cardinality 1 each

SingleCircleSubtractiveExtrude
  -> cylindrical wall, expected cardinality 1
  -> source_rim, expected cardinality 1
  -> opposite_rim, expected cardinality 1
```

The single-circle producer retains exact source `CircleProfileId`. Unsupported producers, mixed/multiple-circle ambiguity, wrong profile ids, and family/role mismatches fail closed.

`CircularHolePattern` subelements remain unavailable until stable per-instance semantic identity exists. Transient expanded-hole vector position is not persistent identity.

`ReferenceRecoveryEvaluator` performs read-only generated-topology recovery from current PartDocument producer intent. It reports `Resolved` or `Lost` and mutates no source model. Recovery never writes raw OCCT topology identity.

Block 35 adds no generated-topology Geometry target resolver branch. That remains Block 36.

Canonical contract: `docs/assembly-generated-topology-reference-mvp5.md`.

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

Every unique free active transform authority contributes six direct component-transform variables:

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

`AssemblyContactAnalyzer` classifies every visible-active unordered rooted component occurrence pair as:

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

Block 35 adds no JSON field. It adds canonical `topo:` semantic-reference strings carried by the existing endpoint string field.

Do not persist:

```text
hierarchy traversal
transform authority mappings
generated topology producer role matrices/classification/recovery query results
resolved target source kinds
geometric target descriptors
capability vectors
capability projections
resolved OCCT topology handles
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

Focused Block-35 tests:

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
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

The typed geometric target/capability and generated-topology identity layers are public library contracts and add no dedicated CLI or persistent analysis format.

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
- `docs/assembly-generated-topology-reference-mvp5.md`
- `docs/assembly-general-geometric-target-roadmap.md`

Broader roadmaps:

- `docs/part-construction-sequence-mvp6.md`
- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 36 only** from `docs/assembly-general-geometric-target-roadmap.md`: generated face/edge/vertex Geometry target resolution.

Block 36 must parse and validate canonical Block-35 `topo:` identities, identify the exact current generated subelement for each supported producer role, prove topology type and expected cardinality, and resolve:

```text
GeneratedPlanarFace      -> Plane
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

Do not add target compatibility rules in Block 36. That remains Block 37.
