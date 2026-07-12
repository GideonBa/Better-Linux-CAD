# Development Setup

This document covers local BLCAD development, build/test workflows, formatting, and current headless tools. Feature status and sequencing live in `docs/mvp-plan.md`.

## Target environment

- Ubuntu 24.04
- C++20
- CMake 3.28 or newer
- Ninja
- OCCT from Ubuntu packages for optional geometry targets
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

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

Equivalent commands:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Complete geometry-enabled workflow:

```bash
cmake --workflow --preset dev-geometry-build-test
```

Equivalent commands:

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

Geometry solving/motion/freshness:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-target]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
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

At a high level the current suites cover:

- persistent component occurrence state and direct placement;
- local Mate/Distance/Concentric/Insert/Angle intent and solving;
- semantic planar face, `.axis`, and `.seat` target resolution;
- canonical active fixed-axis X-then-Y-then-Z rigid transforms;
- shared residual flattening, central finite differences, and damped Gauss-Newton solving;
- exact semantic PartDocument freshness and atomic result application;
- local rank/remaining-DOF diagnostics;
- local Revolute motion and authored-coordinate holding drives;
- Project-owned child assemblies, cycle-free rooted hierarchy, and exact parent transform chains;
- visible-active leaf flattening and repeated child occurrence semantics;
- flattened posed STEP export;
- historical interference and clearance analysis over shared posed leaves;
- complete typed rooted `Separated`/`Touching`/`Interfering` pair classification;
- exact rooted contact pair ordering independent from component insertion order;
- explicit touching and overlap-volume tolerances;
- bounded inclusive local Revolute sampling;
- bounded inclusive Project-level cross-hierarchy Revolute sampling;
- one fresh source Project copy per sampled coordinate;
- existing motion solver/applier reuse before each sample contact analysis;
- source Project transform and authored joint-coordinate immutability during sweep queries;
- document-scoped flexible child solving;
- persistent occurrence-qualified Project-level geometric constraints and Revolute joints;
- deterministic relationship/joint-to-`ComponentTransformAuthority` incidence;
- mixed document-local/root-space cross-hierarchy numeric solving;
- complete authority/relationship/path/PartDocument freshness;
- cross-hierarchy rank diagnostics;
- Project-level occurrence-qualified Revolute motion;
- derived rooted assembly/component exchange identity;
- shared part product definitions by `PartDocumentId`;
- XDE/STEP structured assembly/component references;
- geometric equivalence between structured and flattened STEP exports for repeated/nested fixtures.

When adding a feature block, register sources/tests in `CMakeLists.txt` and document the exact scope in the feature-specific canonical document plus `docs/mvp-plan.md`.

## Private implementation test access

Private geometry headers remain private to `src/geometry`.

The geometry test target has a private include path to `src/geometry` so focused integration tests may verify internal residual/Jacobian order and shared geometry helper boundaries without promoting execution internals to public BLCAD API.

Production consumers access solving, diagnostics, analysis, and export through public geometry-layer APIs.

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

`blcad_export_project` updates one assembly parameter, propagates bindings, recomputes affected parts, and exports per-part STEP files.

`blcad_export_posed_assembly` is the solved-root/local flattened compound flow.

`blcad_export_structured_assembly` exports the current authored/persisted Project pose through structured XDE/STEP assembly/product relationships and does not invoke solving implicitly.

`blcad_move_joint` remains the local root-assembly joint CLI seed. Project-level cross-hierarchy Revolute motion has a public library/API contract but no dedicated CLI.

Block 30 adds public library APIs for complete rooted contact classification and sampled local/cross-hierarchy Revolute analysis. It intentionally adds no CLI or persistent analysis format.

## Documentation entry points

- `README.md`: short status and current next step
- `docs/mvp-plan.md`: implementation sequence
- `docs/architecture-summary.md`: condensed implemented architecture
- `docs/file-format.md`: persistent save-format authority
- `docs/assembly-posed-step-export-mvp5.md`: flattened posed STEP compatibility contract
- `docs/assembly-structured-step-products-mvp5.md`: Block-29 exchange identity and structured STEP contract
- `docs/assembly-contact-swept-motion-mvp5.md`: Block-30 rooted contact and sampled Revolute sweep contract
- `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md`: Project-level occurrence-qualified Revolute motion
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: current numbered sequence and Block-31 handoff
- `docs/assembly-general-geometric-target-roadmap.md`: planned Blocks 31-47 target/relationship/joint sequence
- `docs/project-goal.md`: long-term direction

## Formatting

Formatting is configured by `.editorconfig` and `.clang-format`.

For Block 30 production/test files:

```bash
clang-format -i \
  include/blcad/geometry/assembly_contact_analyzer.hpp \
  include/blcad/geometry/assembly_revolute_sweep_analyzer.hpp \
  src/geometry/assembly_contact_analyzer.cpp \
  src/geometry/assembly_revolute_sweep_analyzer.cpp \
  tests/geometry/assembly_contact_analyzer_tests.cpp \
  tests/geometry/assembly_revolute_sweep_analyzer_tests.cpp
```

## Clean generated files

```bash
rm -rf build/
```

## Current assembly development boundary

Blocks 23-30 of the current cross-hierarchy sequence are implemented.

Block 30 freezes:

```text
contact pair identity
  = canonical ordered pair of exact rooted
    AssemblyExchangeComponentOccurrenceIdentity values

static classification
  = Interfering by strict positive-volume threshold
    else Touching by inclusive distance tolerance
    else Separated

sampled Revolute sweep
  = 2..1001 inclusive requested-coordinate samples
    over RootAssemblyLocal or ProjectCrossHierarchy motion
    with one fresh source Project copy per sample
```

The immediate next block is Block 31: typed geometric target taxonomy and capability projection. Exact sequencing is maintained in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` and detailed target planning in `docs/assembly-general-geometric-target-roadmap.md`.
