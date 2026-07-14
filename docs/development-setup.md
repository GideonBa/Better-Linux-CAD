# Development Setup

This document covers local BLCAD development, build/test workflows, formatting, and current headless tools. Feature status and sequencing live in `docs/mvp-plan.md`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT/Open CASCADE from Ubuntu packages for optional Geometry targets
- Qt 6 packages for later GUI work
- nlohmann-json
- Catch2

## Install dependencies

See `docs/dependencies-ubuntu-24.04.md` for package notes.

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git clang-format clang-tidy \
  libocct-foundation-dev libocct-modeling-data-dev libocct-modeling-algorithms-dev \
  libocct-data-exchange-dev libocct-visualization-dev libocct-ocaf-dev libocct-draw-dev \
  qt6-base-dev libeigen3-dev libtbb-dev nlohmann-json3-dev libfmt-dev libspdlog-dev catch2 \
  doxygen graphviz
```

The current assembly numeric engine uses project-owned dynamic containers plus deterministic dense elimination routines. Eigen is not currently a BLCAD target dependency.

## Configure, build, and test

Complete Core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Complete Geometry workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

Build directories:

```text
build/dev
build/dev-geometry
build/release
```

## Focused current assembly tests

Core identity/model/connectivity:

```bash
./build/dev/blcad_core_tests "[core][datum-axis]"
./build/dev/blcad_core_tests "[core][datum-axis-json]"
./build/dev/blcad_core_tests "[core][assembly-reference-target-intent]"
./build/dev/blcad_core_tests "[core][assembly-reference-target-json]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev/blcad_core_tests "[core][assembly-constraint]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
./build/dev/blcad_core_tests "[core][assembly-joint]"
./build/dev/blcad_core_tests "[core][assembly-joint-graph]"
./build/dev/blcad_core_tests "[core][assembly-joint-coordinate-model]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint-coordinate-model]"
./build/dev/blcad_core_tests "[core][assembly-joint-coordinate-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint-coordinate-json]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-vector-joint-drive]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-vector-joint-drive]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-vector-joint-drive-application]"
./build/dev/blcad_core_tests "[core][subassembly-instance]"
./build/dev/blcad_core_tests "[core][assembly-hierarchy]"
./build/dev/blcad_core_tests "[core][assembly-leaf-occurrence]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-joint-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-motion-graph]"
./build/dev/blcad_core_tests "[core][assembly-exchange-graph]"
```

Geometry target/equation/solve/motion/freshness:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-geometric-target-taxonomy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-reference-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-revolute]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-motion]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

The Block-36 generated-topology target resolution tag:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
```

Posed geometry, analysis, and exchange:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-nested-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-interference]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-clearance]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
```

The exact source/test registration in `CMakeLists.txt` remains authoritative.

## Current assembly architecture under test

Current suites cover:

- persistent component occurrence state and direct placement;
- first-class DatumAxis intent, ownership, dependency, and invalidation;
- canonical `ref:` reference-target spellings, roundtrips, and fail-closed parsing;
- additive DatumAxis JSON, historical-file compatibility, and byte-for-byte `ref:` endpoint spelling roundtrips;
- Geometry resolution of DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint into Plane/Axis/Line/Point capabilities;
- canonical `topo:` generated-topology spellings with strict uppercase `%HH` encoding;
- producer-driven generated topology identity for cylindrical faces, linear edges, circular edges, and vertices;
- finite role matrices and expected cardinality-one contracts;
- `RectangularAdditiveExtrude` edge/vertex semantic roles;
- `SingleCircleSubtractiveExtrude` wall/source-rim/opposite-rim semantic roles with exact source ProfileId;
- feature insertion order independence of generated-topology identity;
- fail-closed unsupported/ambiguous producer handling;
- explicit rejection of patterned subelements until stable per-instance identity exists;
- read-only generated-topology recovery and source PartDocument immutability;
- byte-for-byte `topo:` assembly endpoint JSON roundtrips;
- local Mate/Distance/Angle/Concentric/Insert intent and solving;
- persistent local and Project-level Coincident/Parallel/Perpendicular intent and lowercase JSON roundtrips;
- generic relationship target order/state/id-scope preservation and scalar rejection;
- Block-39 Coincident/Parallel/Perpendicular compatibility, residuals, graph participation, local/cross solving, freshness/application, and Jacobian-rank diagnostics;
- non-planar Line/Axis Angle execution through shared directional equations;
- exact local and occurrence-qualified endpoint identity;
- typed target source classification and capability projection;
- Plane/Axis/Line/Point/Circle/Cylinder/Frame descriptor validation;
- canonical capability order and unsupported-projection failure;
- current generated planar-face migration to `GeneratedPlanarFace -> Plane`;
- current `.axis` migration to `GeneratedCylindricalFace -> Axis + Cylinder`;
- current `.seat` migration to `CircularFeatureSeat -> Plane + Axis + Frame`;
- local and hierarchy target transform semantics;
- shared residual flattening, central finite differences, and damped Gauss-Newton solving;
- exact PartDocument semantic-target freshness and atomic application;
- local and cross-hierarchy rank diagnostics;
- local and Project-level occurrence-qualified Revolute motion;
- Project-owned child assemblies and exact rooted hierarchy chains;
- visible-active leaf flattening and repeated child occurrence semantics;
- flattened and structured STEP exchange;
- interference, clearance, rooted contact classification, and bounded Revolute sampling;
- document-scoped flexible child solving;
- deterministic relationship/joint-to-`ComponentTransformAuthority` incidence;
- complete authority/relationship/path/PartDocument freshness;
- rooted assembly/component exchange identity and shared part definitions.

When adding a feature block, register sources/tests in `CMakeLists.txt` and document exact scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Public versus private Geometry boundaries

Private Geometry headers remain private to `src/geometry`.

The Geometry test target has a private include path to `src/geometry` so focused integration tests may verify internal residual/Jacobian ordering and shared helper boundaries without promoting execution internals to public BLCAD API.

Production consumers access target resolution/projection, solving, diagnostics, analysis, and export through public Geometry APIs.

Block-31 public target API:

```text
include/blcad/geometry/assembly_geometric_target.hpp
```

Block-32 public Core reference-geometry APIs:

```text
include/blcad/core/datum_axis.hpp
include/blcad/core/assembly_reference_target.hpp
```

Block-35 public Core generated-topology identity API:

```text
include/blcad/core/generated_topology_reference.hpp
include/blcad/core/reference_recovery.hpp
```

Current typed resolvers are exposed through:

```text
AssemblyConstraintTargetResolver::resolve_geometric
AssemblyHierarchyConstraintTargetResolver::resolve_geometric
```

Legacy `resolve`, `resolve_axis`, and `resolve_insert` methods remain compatibility APIs and adapt from typed projections.

Block 36 adds the `topo:` branch to these resolvers: canonical generated-topology producer identity is parsed before the legacy grammar and resolved into typed descriptors from validated model intent.

## Headless tools

Core inspection:

```bash
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
```

Part export:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
```

Current command shapes:

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
blcad_move_joint <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>
blcad_analyze_assembly <input.blcad.project.json> [clearance-threshold-mm]
```

The typed target/capability and generated-topology identity layers are public library contracts and intentionally add no CLI or persistent result format.

## Documentation entry points

- `README.md`: short repository entry point
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed implemented architecture
- `docs/file-format.md`: persistent save-format authority
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: numbered assembly sequence
- `docs/assembly-geometric-target-taxonomy-mvp5.md`: Block-31 target taxonomy/projection contract
- `docs/assembly-reference-geometry-intent-mvp5.md`: Blocks 32–34 reference-geometry contract
- `docs/assembly-generated-topology-reference-mvp5.md`: Block-35 generated-topology identity/recovery contract
- `docs/assembly-generic-relationship-intent-mvp5.md`: Block-38 generic relationship intent/JSON contract
- `docs/assembly-generic-relationship-equations-mvp5.md`: Block-39 generic relationship equation/solve contract
- `docs/assembly-joint-target-compatibility-mvp5.md`: Block-40 joint compatibility/oriented Frame contract
- `docs/assembly-joint-coordinate-model-mvp5.md`: Block-41 family-defined typed joint coordinates and Revolute compatibility adapter
- `docs/assembly-joint-coordinate-json-mvp5.md`: Block-42 additive slot JSON and historical Revolute compatibility
- `docs/assembly-vector-joint-drive-mvp5.md`: Block-43 role-addressed drives, holding, freshness, and atomic application
- `docs/assembly-general-geometric-target-roadmap.md`: implemented Blocks 31–43 and planned Blocks 44–47
- `docs/project-goal.md`: long-term direction

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For Blocks 39–40 production/test files:

```bash
clang-format -i \
  include/blcad/geometry/assembly_generic_relationship_equation_builder.hpp \
  include/blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp \
  src/core/assembly_constraint_graph_participation.hpp \
  src/geometry/assembly_generic_relationship_equation_builder.cpp \
  src/geometry/assembly_target_compatibility.cpp \
  src/geometry/assembly_constraint_numeric_system.hpp \
  src/geometry/assembly_constraint_numeric_system.cpp \
  src/geometry/assembly_hierarchy_constraint_equation_builder.cpp \
  tests/geometry/assembly_generic_relationship_equation_tests.cpp \
  include/blcad/geometry/assembly_joint_target_compatibility.hpp \
  include/blcad/geometry/assembly_revolute_joint_equation_builder.hpp \
  include/blcad/geometry/assembly_hierarchy_revolute_joint_equation_builder.hpp \
  src/geometry/assembly_joint_target_compatibility.cpp \
  src/geometry/assembly_revolute_joint_equation_builder.cpp \
  src/geometry/assembly_hierarchy_revolute_joint_equation_builder.cpp \
  tests/geometry/assembly_joint_target_compatibility_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Blocks 23–43 are implemented.

Block 35 freezes:

```text
generated topology identity
  = semantic feature producer
    + semantic family
    + named producer role
    + exact source ProfileId where profile-derived

canonical spelling
  = topo:<family>:<encoded producer ids>:<role>
    with uppercase %HH escaping outside [A-Za-z0-9_-]

supported first producers
  = RectangularAdditiveExtrude
      12 linear edges + 8 vertices, cardinality 1 each
    SingleCircleSubtractiveExtrude
      wall + source_rim + opposite_rim, cardinality 1 each

pattern result-vector index != persistent instance identity

recovery
  = read-only validation of current producer/profile/role intent
  -> Resolved | Lost
```

Block 36 resolves those identities into typed Geometry descriptors. Block 37 freezes deterministic target compatibility selection. Block 38 adds persistent local/Project-level Coincident, Parallel, and Perpendicular intent plus lowercase JSON spellings. Block 39 adds their capability-driven equations, enables shared graph participation and numeric execution, and closes non-planar Line/Axis Angle execution.

Focused Block-35 tests:

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
```

Focused Blocks 36–40 tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generated-topology-target-resolution]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-target-compatibility]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-intent]"
./build/dev/blcad_core_tests "[core][assembly-generic-relationship-json]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-cross-hierarchy]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-generic-relationships-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-joint-target-compatibility]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-joint-target-compatibility]"
```

Blocks 44–47 Prismatic, Cylindrical, Planar, and passive Spherical are implemented. Block 48 Body identity and deterministic `PartDocument` ownership is also implemented; its focused Core proof is:

```bash
./build/dev/blcad_core_tests "[core][part-body]"
```

Block 49 Body JSON and structural validation is implemented. Its focused Core proof is:

```bash
./build/dev/blcad_core_tests "[core][part-body-json]"
```

Block 50 feature output-body and operation-mode Core intent is implemented. Its focused Core proof is:

```bash
./build/dev/blcad_core_tests "[core][feature-body-operation]"
```

Block 51 feature Body-operation persistence and graph semantics are implemented. Focused proofs are:

```bash
./build/dev/blcad_core_tests "[core][feature-body-operation-json]"
./build/dev/blcad_core_tests "[core][part-body-dependency]"
```

Block 56 BodyTransform/SketchOwnership intent can be verified with `./build/blcad_core_tests "[core][body-transform],[core][sketch-ownership]"`; Block 57 Geometry execution with `./build/blcad_geometry_tests "[geometry][body-transform]"`; Block 58 reusable Part feature inputs with `./build/blcad_core_tests "[core][part-feature-input-reference]"`; Block 59 richer Extrude/Cut intent/JSON with `./build/blcad_core_tests "[core][extrude-extent]"`; Block 60 Geometry with `./build/blcad_geometry_tests "[geometry][extrude-extent]"`; Block 61 Revolve/RevolveCut intent/JSON with `./build/blcad_core_tests "[core][revolve-feature]"`; Block 62 Geometry with `./build/blcad_geometry_tests "[geometry][revolve-feature]"`; Block 63 Pattern Core/JSON with `./build/blcad_core_tests "[core][part-pattern]"`; Block 64 Linear Pattern Geometry with `./build/blcad_geometry_tests "[geometry][linear-pattern]"`; Block 65 Circular Pattern Geometry with `./build/blcad_geometry_tests "[geometry][circular-pattern]"`; Block 66 MirrorFeature Core/JSON with `./build/blcad_core_tests "[core][mirror-feature]"`; Block 67 Geometry with `./build/blcad_geometry_tests "[geometry][mirror-feature]"`; Block 68 Edge Treatment Core/JSON with `./build/blcad_core_tests "[core][edge-treatment]"`; Block 69 Fillet Geometry with `./build/blcad_geometry_tests "[geometry][fillet-feature]"`; Block 70 Chamfer Geometry with `./build/blcad_geometry_tests "[geometry][chamfer-feature]"`; Block 71 ShellFeature Core/JSON with `./build/blcad_core_tests "[core][shell-feature]"`; Block 72 ShellFeature Geometry with `./build/blcad_geometry_tests "[geometry][shell-feature]"`; Block 73 DraftFeature Core/JSON with `./build/blcad_core_tests "[core][draft-feature]"`; Block 74 DraftFeature Geometry with `./build/blcad_geometry_tests "[geometry][draft-feature]"`; Block 75 Basic 3D Sketch Core with `./build/blcad_core_tests "[core][sketch-3d]"`; Block 76 richer 3D curve intent with `./build/dev-geometry/blcad_core_tests "[core][sketch-3d-curves]"`; Block 77 JSON with `./build/dev-geometry/blcad_core_tests "[core][sketch-3d-json]"`; and Block 78 Geometry conversion with `./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-3d]"`. The current contracts additionally include `docs/part-pattern-core-mvp6.md`, `docs/part-linear-pattern-geometry-mvp6.md`, `docs/part-circular-pattern-geometry-mvp6.md`, `docs/part-mirror-intent-mvp6.md`, `docs/part-mirror-geometry-mvp6.md`, `docs/part-edge-treatment-intent-mvp6.md`, `docs/part-fillet-geometry-mvp6.md`, `docs/part-chamfer-geometry-mvp6.md`, `docs/part-shell-intent-mvp6.md`, `docs/part-shell-geometry-mvp6.md`, `docs/part-draft-intent-mvp6.md`, `docs/part-draft-geometry-mvp6.md`, `docs/part-sketch-3d-core-mvp6.md`, `docs/part-sketch-3d-curves-core-mvp6.md`, `docs/part-sketch-3d-json-mvp6.md`, `docs/part-sketch-3d-geometry-mvp6.md`, `docs/part-path-curve-core-mvp6.md`, `docs/part-sweep-intent-mvp6.md`, `docs/part-sweep-geometry-mvp6.md`, `docs/part-sweep-3d-geometry-mvp6.md`, `docs/part-path-extrude-geometry-mvp6.md`, `docs/part-loft-intent-mvp6.md`, `docs/part-loft-geometry-mvp6.md`, `docs/part-multi-section-loft-geometry-mvp6.md`, `docs/part-guided-loft-geometry-mvp6.md`, `docs/part-surface-feature-intent-mvp6.md`, `docs/part-boundary-fill-surface-geometry-mvp6.md`, `docs/part-trim-extend-surface-geometry-mvp6.md`, `docs/part-surface-stitch-geometry-mvp6.md`, `docs/part-closed-shell-to-solid-geometry-mvp6.md`, and `docs/part-multi-body-step-export-mvp6.md`. Block 79 PathCurve Core/JSON can be verified with `./build/dev-geometry/blcad_core_tests "[core][path-curve]"`; Block 80 Sweep Core/JSON with `./build/dev-geometry/blcad_core_tests "[core][sweep-feature]"`; Block 81 Sweep Geometry with `./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"`; Block 82 spatial/twist/guide Sweep with `./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-3d]"`; and Block 83 Path-following Extrude/Extruded Cut with `./build/blcad_core_tests "[core][path-extrude]"` plus `./build/blcad_geometry_tests "[geometry][path-extrude]"`; Block 84 Loft Core/JSON with `./build/blcad_core_tests "[core][loft-feature]"`; Block 85 Two-section Loft Geometry with `./build/blcad_geometry_tests "[geometry][loft-feature]"`; Block 86 Multi-section Loft with `./build/blcad_geometry_tests "[geometry][multi-section-loft]"`; Block 87 Guided Loft with `./build/blcad_geometry_tests "[geometry][guided-loft]"`; Block 88 Surface Core/JSON with `./build/blcad_core_tests "[core][surface-feature]"`; Block 89 Boundary/Fill Geometry with `./build/blcad_geometry_tests "[geometry][surface-boundary-fill]"`; Block 90 Trim/Extend Geometry with `./build/blcad_geometry_tests "[geometry][surface-trim-extend]"`; Block 91 Stitch/Knit/Sew shell Geometry with `./build/blcad_geometry_tests "[geometry][surface-stitch]"`; Block 92 Closed shell to solid conversion with `./build/blcad_geometry_tests "[geometry][surface-to-solid]"`; and Block 93 multi-body STEP export with `./build/blcad_geometry_tests "[geometry][multi-body-step-export]"`. The immediate next step is Block 94 integrated Part Construction MVP acceptance. Block 43 role-addressed drives, authored holding, complete coordinate-slot freshness, and atomic application remain authoritative for local and Project-level driven motion; Spherical participates passively and rejects selection. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and `docs/part-construction-sequence-mvp6.md`.

After the Blocks 48–94 Part Construction sequence, Blocks 95–101 plan STEP import as either an
immutable Assembly-ready Reference Part or an EditableBody `ImportedBodyFeature` with downstream
BLCAD features. Canonical sequencing: `docs/step-import-sequence-mvp7.md`.
