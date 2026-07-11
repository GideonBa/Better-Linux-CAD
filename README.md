# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD stores CAD model intent in its own data structures and uses OCCT/Open CASCADE as computed geometry rather than as the primary model authority.

Detailed architecture, feature contracts, and implementation status live in `docs/`.

## Status

Implemented seeds now cover single-part parametric modeling, semantic references and richer sketch/profile workflows, a parametric bolt-circle pattern, assembly parameters/project ownership, deterministic local Mate/Distance/Concentric/Insert/Angle solving, rank/remaining-DOF diagnostics, first Revolute joint motion, rigid nested assembly hierarchy, posed STEP export, interference/clearance analysis, document-scoped flexible child solving, and occurrence-qualified read-only cross-hierarchy target/residual semantics.

There is no GUI yet. Current work remains focused on headless CAD-core and application contracts.

## Assembly architecture snapshot

```text
local component + geometric relationship intent
  -> deterministic local graph
  -> semantic face/axis/seat resolution
  -> canonical residual descriptors
  -> shared numeric residual/Jacobian path
  -> damped Gauss-Newton solve
  -> complete stale snapshots + transform proposals
  -> atomic explicit application

persistent Revolute joint/limit/coordinate intent
  -> deterministic local joint graph
  -> combined local relationship closure
  -> directed axis/seating/twist drive residuals
  -> same numeric solve engine
  -> atomic transform + selected coordinate application

explicit root assembly + project-owned child assemblies
  -> rigid SubassemblyInstance placement/state
  -> validated cycle-free hierarchy
  -> deterministic rooted occurrence traversal
  -> visible-active leaf flattening
  -> exact inner-to-outer authored transform chains
  -> nested posed STEP and posed analysis consumers

document-scoped flexible child solving
  -> exact active child occurrence selection
  -> child AssemblyDocument as temporary local solve root
  -> existing local solver/application contract reused
  -> successful application updates child component transforms only
  -> repeated child occurrences share the child document's internal pose

read-only cross-hierarchy relationship semantics
  -> endpoint = (occurrence_path, local component id, semantic reference)
  -> exact rooted occurrence resolution
  -> existing local semantic target resolver reused
  -> exact component + parent chain evaluation into root space
  -> canonical Mate/Distance/Angle/Concentric/Insert residuals reused
```

The current cross-hierarchy planning explicitly separates three identities:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

This matters for repeated occurrences of one child assembly. Two occurrences may have different root-space geometry while sharing one child-document `ComponentInstance::transform()` authority. They must not become independent numeric transform variables until occurrence-local internal pose overrides exist.

Future cross-hierarchy solve connectivity is planned as relationship-to-transform-authority incidence. Local constraints remain one relationship per containing `AssemblyDocument`; project-level cross-hierarchy relationships retain exact occurrence-qualified endpoints.

See `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` for the corrected implementation sequence.

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
./build/dev/blcad_core_tests "[core][semantic-axis]"
./build/dev/blcad_core_tests "[core][semantic-seat]"
./build/dev/blcad_core_tests "[core][assembly-insert]"
./build/dev/blcad_core_tests "[core][assembly-joint]"
./build/dev/blcad_core_tests "[core][assembly-joint-graph]"
./build/dev/blcad_core_tests "[core][subassembly-instance]"
./build/dev/blcad_core_tests "[core][assembly-hierarchy]"
./build/dev/blcad_core_tests "[core][assembly-leaf-occurrence]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-joint]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-hierarchy-transform]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-nested-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-interference]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-clearance]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy]"
```

## Headless tools

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
blcad_move_joint <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>
blcad_analyze_assembly <input.blcad.project.json> [clearance-threshold-mm]
```

Examples:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/bolt_circle_plate.blcad.json build/bolt_circle_plate.step
./build/dev/blcad_inspect_project_components examples/component_instances.blcad.project.json
./build/dev-geometry/blcad_export_posed_assembly examples/posed_assembly.blcad.project.json build/posed_assembly.step
./build/dev-geometry/blcad_move_joint examples/revolute_joint.blcad.project.json joint.revolute 45 build/revolute_joint_45.blcad.project.json
./build/dev-geometry/blcad_export_posed_assembly build/revolute_joint_45.blcad.project.json build/revolute_joint_45.step
./build/dev-geometry/blcad_export_posed_assembly examples/nested_subassembly.blcad.project.json build/nested_subassembly.step
./build/dev-geometry/blcad_analyze_assembly examples/posed_assembly.blcad.project.json 15
```

## Repository structure

- `include/blcad/`: public headers
- `src/`: core and optional geometry implementation
- `tests/`: Catch2 tests
- `examples/`: `.blcad.json` / `.blcad.project.json` examples and headless flows
- `docs/`: architecture, implemented MVP contracts, implementation sequences, and future roadmaps

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/file-format.md`

Current assembly handoff:

- `docs/assembly-flexible-subassembly-solving-mvp5.md`
- `docs/assembly-cross-hierarchy-relationship-semantics-mvp5.md`
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

Broader future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **block 23 only** from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: extract the frozen occurrence-qualified endpoint value contract into the Core layer and add persistent project-owned cross-hierarchy geometric constraint intent.

JSON, connectivity, numeric variables, solving, snapshots, proposals, diagnostics, and application are explicitly later blocks.
