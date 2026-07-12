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
./build/dev/blcad_core_tests "[core][component-instance]"
./build/dev/blcad_core_tests "[core][assembly-constraint]"
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
./build/dev/blcad_core_tests "[core][assembly-joint]"
./build/dev/blcad_core_tests "[core][assembly-joint-graph]"
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
- additive DatumAxis JSON, historical-file compatibility, and byte-for-byte endpoint spelling roundtrips;
- local Mate/Distance/Angle/Concentric/Insert intent and solving;
- exact local and occurrence-qualified endpoint identity;
- typed target source classification and capability projection;
- Plane/Axis/Line/Point/Circle/Cylinder/Frame descriptor validation;
- canonical capability order and unsupported-projection failure;
- current generated planar-face target migration to `GeneratedPlanarFace -> Plane`;
- current `.axis` producer migration to `GeneratedCylindricalFace -> Axis + Cylinder`;
- current `.seat` migration to `CircularFeatureSeat -> Plane + Axis + Frame`;
- local typed descriptor geometry equal to historical target geometry;
- hierarchy/root-space typed descriptor geometry equal to historical target geometry;
- exact component-plus-parent transform context on hierarchy targets;
- existing legacy target resolver APIs adapting from typed projections;
- canonical active fixed-axis X-then-Y-then-Z rigid transforms;
- shared residual flattening, central finite differences, and damped Gauss-Newton solving;
- exact PartDocument semantic-target freshness and atomic application;
- local and cross-hierarchy rank diagnostics;
- local and Project-level occurrence-qualified Revolute motion;
- Project-owned child assemblies and exact rooted hierarchy chains;
- visible-active leaf flattening and repeated child occurrence semantics;
- flattened and structured STEP exchange;
- historical interference/clearance analysis;
- complete rooted `Separated`/`Touching`/`Interfering` classification;
- bounded local/cross-hierarchy Revolute sampling from fresh source Project copies;
- document-scoped flexible child solving;
- deterministic relationship/joint-to-`ComponentTransformAuthority` incidence;
- complete authority/relationship/path/PartDocument freshness;
- rooted assembly/component exchange identity and shared part definitions.

When adding a feature block, register sources/tests in `CMakeLists.txt` and document exact scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Public versus private Geometry boundaries

Private Geometry headers remain private to `src/geometry`.

The Geometry test target has a private include path to `src/geometry` so focused integration tests may verify internal residual/Jacobian ordering and shared helper boundaries without promoting execution internals to public BLCAD API.

Production consumers access target resolution/projection, solving, diagnostics, analysis, and export through public Geometry APIs.

Block 31 public target API lives in:

```text
include/blcad/geometry/assembly_geometric_target.hpp
```

Block 32 public Core reference-geometry APIs live in:

```text
include/blcad/core/datum_axis.hpp
include/blcad/core/assembly_reference_target.hpp
```

Current typed resolvers are exposed through:

```text
AssemblyConstraintTargetResolver::resolve_geometric
AssemblyHierarchyConstraintTargetResolver::resolve_geometric
```

Legacy `resolve`, `resolve_axis`, and `resolve_insert` methods remain compatibility APIs and now adapt from typed projections.

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

The typed target/capability layer is a public library query contract and intentionally adds no CLI or persistent result format.

## Documentation entry points

- `README.md`: short status and current next step
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed implemented architecture
- `docs/file-format.md`: persistent save-format authority
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: numbered assembly sequence
- `docs/assembly-geometric-target-taxonomy-mvp5.md`: Block-31 implemented target taxonomy/projection contract
- `docs/assembly-reference-geometry-intent-mvp5.md`: Blocks-32/33 implemented reference-geometry intent/grammar/JSON contract
- `docs/assembly-general-geometric-target-roadmap.md`: planned Blocks 34–47
- `docs/project-goal.md`: long-term direction

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For Block 32 production/test files:

```bash
clang-format -i \
  include/blcad/core/datum_axis.hpp \
  include/blcad/core/assembly_reference_target.hpp \
  src/core/datum_axis.cpp \
  src/core/assembly_reference_target.cpp \
  tests/core/datum_axis_tests.cpp \
  tests/core/assembly_reference_target_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Blocks 23–33 are implemented.

Block 31 freezes:

```text
semantic source classification
  != geometric capability

resolved target
  = exact endpoint identity
    + source kind
    + source metadata
    + typed descriptor
    + canonical capability vector
    + coordinate space
    + exact transform context

supported projection path
  = project_plane / project_axis / project_line
    / project_point / project_circle / project_cylinder / project_frame
```

Block 32 freezes:

```text
DatumAxis
  = Explicit (finite origin + unit direction + parameter dependencies)
    | FromConstructionLine (owned ConstructionLineId identity)

reference semantic source spelling
  = ref:<family>:<encoded-id>
    with uppercase %HH escaping outside [A-Za-z0-9_-]

valid reference spellings are dot-free
  -> provably disjoint from <feature-id>.<role> spellings
```

The immediate next step is Block 34: Geometry resolution of `ref:` reference sources into the Block-31 taxonomy. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
