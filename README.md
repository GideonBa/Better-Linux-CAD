# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD stores CAD model intent in its own data structures and uses OCCT/Open CASCADE as computed geometry rather than as the primary model authority.

Detailed architecture, feature contracts, and implementation status live in `docs/`.

## Status

Implemented seeds now cover single-part parametric modeling, semantic references and richer sketch/profile workflows, a parametric bolt-circle pattern, assembly parameters/project ownership, deterministic local Mate/Distance/Concentric/Insert/Angle solving, local rank/remaining-DOF diagnostics, local Revolute joint motion, rigid nested assembly hierarchy, posed STEP export, interference/clearance analysis, document-scoped flexible child solving, occurrence-qualified cross-hierarchy geometric intent/JSON/target semantics, deterministic relationship-to-transform-authority solve groups, authority-scoped mixed local/root-space numeric solving, complete cross-hierarchy modeled-input freshness, atomic authority-qualified application, and cross-hierarchy Jacobian-rank/remaining-DOF diagnostics.

There is no GUI yet. Current work remains focused on headless CAD-core and application contracts.

## Assembly architecture snapshot

```text
local component + geometric relationship intent
  -> deterministic local graph
  -> semantic face/axis/seat resolution
  -> canonical residual descriptors
  -> shared numeric residual/Jacobian path
  -> damped Gauss-Newton solve
  -> component + exact PartDocument model-intent snapshots
  -> atomic explicit application

persistent local Revolute joint/limit/coordinate intent
  -> deterministic local joint graph
  -> combined local relationship closure
  -> directed axis/seating/twist drive residuals
  -> same numeric solve engine
  -> same exact semantic PartDocument freshness
  -> atomic transform + selected coordinate application

explicit root assembly + Project-owned child assemblies
  -> rigid SubassemblyInstance placement/state
  -> validated cycle-free hierarchy
  -> deterministic rooted occurrence traversal
  -> visible-active leaf flattening
  -> exact inner-to-outer authored transform chains
  -> nested posed STEP and posed analysis consumers

document-scoped flexible child solving
  -> exact active child occurrence selection
  -> child AssemblyDocument as temporary local solve root
  -> ordinary local solver/application contract reused
  -> inherited exact PartDocument model-intent freshness
  -> successful application updates child component transforms only
  -> repeated child occurrences share the child document's internal pose

cross-hierarchy geometric intent and solving
  -> exact occurrence-qualified endpoint identity
  -> Project-owned persistent relationship records + additive JSON
  -> exact path/reached-component Core validation
  -> endpoint occurrence paths map to ComponentTransformAuthority
  -> deterministic relationship-to-authority incidence and solve groups
  -> one six-variable block per unique free active authority
  -> local relationships evaluated once in document-local space
  -> cross relationships evaluated through exact root-space parent chains
  -> shared five-family residual flattening
  -> shared central finite-difference Jacobian
  -> existing damped Gauss-Newton engine
  -> complete authority/relationship/path-boundary/PartDocument snapshots
  -> atomic direct-transform authority application
  -> shared matrix-rank and remaining-DOF diagnostics
```

The cross-hierarchy architecture separates three identities:

```text
geometric endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Repeated occurrences of one child assembly may have different root-space geometry while sharing one child-document `ComponentInstance::transform()` authority. They do not become independent numeric variables until occurrence-local internal pose overrides exist.

`AssemblyCrossHierarchyConstraintGraph` derives active relationship-to-authority incidence. Local constraints appear once per containing `AssemblyDocument`. Cross-hierarchy target A/B paths remain distinct endpoint mappings even when both map to one transform authority. Visibility does not filter solve participation; inactive relationships, suppressed local target components, and suppressed cross-hierarchy paths/components do.

`AssemblyCrossHierarchyConstraintSolver` consumes one exact current solve group. It applies each absolute candidate authority transform once to a Project copy, evaluates local relationships in document-local space and Project-level relationships in root-assembly space, and reuses the same residual flattening, central finite differences, damping, backtracking, and dense-solve machinery as ordinary assembly solving.

`AssemblyCrossHierarchySolveResultApplier` applies only fresh converged results. It protects exact authority inputs, complete local/cross relationship records, exact active solve-group participation, every persistent `SubassemblyInstance` boundary on participating cross paths, and exact canonical model intent for every PartDocument referenced by participating authorities.

Semantic target-producing model freshness uses the exact result of:

```text
serialize_part_document_to_json(part)
```

as a derived, unpersisted `AssemblySemanticTargetPartSnapshot` payload. The current payload must compare byte-for-byte equal at application. This conservative contract is shared by ordinary local solve results, flexible-child application, local Revolute motion, and cross-hierarchy application.

`AssemblyCrossHierarchySolveDiagnosticsAnalyzer` uses the exact Block-26 free-authority proposal order and the same authority read/apply, mixed residual, and finite-difference functions as the solver:

```text
variable_count = 6 * unique_free_active_transform_authority_count
constrained_dof = rank(J)
remaining_dof = variable_count - rank(J)
```

Repeated rooted occurrences sharing one free authority contribute six variables, not twelve.

`cross_hierarchy_constraints[]` remains the only cross-hierarchy geometric Project JSON field. Solve results, freshness snapshots, Jacobians, rank products, and diagnostics are derived and unpersisted.

See `docs/assembly-cross-hierarchy-constraint-intent-mvp5.md`, `docs/assembly-cross-hierarchy-constraint-json-mvp5.md`, `docs/assembly-cross-hierarchy-incidence-groups-mvp5.md`, `docs/assembly-cross-hierarchy-numeric-solver-mvp5.md`, `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`, and `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

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
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-intent]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-json]"
./build/dev/blcad_core_tests "[core][assembly-cross-hierarchy-graph]"
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
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-application]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-cross-hierarchy-diagnostics]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
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
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

Broader future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 28 only** from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: persistent occurrence-qualified Project-level cross-hierarchy Revolute joint intent and additive JSON, joint-to-`ComponentTransformAuthority` incidence, combined geometric/joint motion connectivity across assembly documents, root-space nested Revolute drive evaluation through exact parent chains, shared authority-scoped numeric solving, and complete Block-27-style fresh atomic transform-plus-coordinate application.

Occurrence-local child pose overrides, whole-subassembly transform variables, component geometry instancing, and swept-motion contact analysis remain deferred.
