# BLCAD

BLCAD is an independent parametric CAD system for Linux in active development. BLCAD stores CAD model intent in its own data structures and uses OCCT/Open CASCADE as computed geometry rather than as the primary model authority.

Detailed architecture, feature contracts, and implementation status live in `docs/`.

## Status

Implemented seeds now cover single-part parametric modeling, semantic references and richer sketch/profile workflows, a parametric bolt-circle pattern, assembly parameters/project ownership, deterministic local Mate/Distance/Concentric/Insert/Angle solving, local rank/remaining-DOF diagnostics, local Revolute motion, rigid nested assembly hierarchy, posed STEP export, interference/clearance analysis, document-scoped flexible child solving, occurrence-qualified cross-hierarchy geometric intent/JSON/solving/fresh application/diagnostics, and Project-level occurrence-qualified Revolute motion through combined local/cross geometric/joint connectivity.

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
  -> exact semantic PartDocument freshness
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
  -> Project-owned persistent relationships + additive JSON
  -> exact path/reached-component Core validation
  -> endpoint occurrences map to ComponentTransformAuthority
  -> deterministic relationship-to-authority incidence and solve groups
  -> one six-variable block per unique free active authority
  -> document-local local relationships + root-space cross relationships
  -> shared residual/Jacobian/Gauss-Newton path
  -> complete authority/relationship/path-boundary/PartDocument freshness
  -> atomic direct-transform authority application
  -> shared matrix-rank and remaining-DOF diagnostics

Project-level cross-hierarchy Revolute motion
  -> persistent AssemblyHierarchyJoint + cross_hierarchy_joints[]
  -> exact occurrence-qualified .seat endpoints
  -> joint endpoint -> ComponentTransformAuthority mapping
  -> combined motion closure over local geometry, local joints,
     Project cross geometry, and Project cross joints
  -> same root-space axis/seating/signed-twist Revolute mathematics
  -> authored-coordinate holding drives for non-selected active joints
  -> one six-variable block + at most one proposal per unique free authority
  -> same finite differences and Gauss-Newton engine
  -> shared Block-27 authority/boundary/PartDocument freshness
  -> atomic authority transforms + selected Project joint coordinate
```

The cross-hierarchy architecture separates three identities:

```text
geometric/motion endpoint
  = (occurrence_path, local ComponentInstanceId, semantic_reference)

geometric component occurrence
  = (occurrence_path, local ComponentInstanceId)

persisted transform authority
  = (assembly_document: DocumentId, local ComponentInstanceId)
```

Repeated occurrences of one child assembly may have different root-space geometry while sharing one child-document `ComponentInstance::transform()` authority. They do not become independent numeric variables until occurrence-local internal pose overrides exist.

`AssemblyCrossHierarchyConstraintGraph` derives active geometric relationship-to-authority incidence. `AssemblyCrossHierarchyMotionGraph` separately derives combined local/cross geometric/joint connectivity. The motion relationship-kind order is local geometry, local joint, Project cross geometry, then Project cross joint. Visibility does not filter participation; inactive relationships and suppressed local components or cross-hierarchy paths/components do.

A Project-level `AssemblyHierarchyJoint` may connect two different rooted endpoints that map to the same transform authority. The graph retains two endpoint mappings but one unique relationship-to-authority incidence. Numeric motion therefore uses one six-variable block and at most one proposal for that shared authority.

`AssemblyHierarchyRevoluteJointEquationBuilder` resolves exact occurrence-qualified `.seat` endpoints into root-assembly space and calls the same shared Revolute residual constructor as the local joint path. Directed axis alignment, axis offset, seating, and signed periodic twist semantics are therefore identical after target resolution.

`AssemblyCrossHierarchyJointMotionSolver` selects one active Project-level Revolute joint, finds its exact combined motion group, drives the selected joint to the requested in-range coordinate, and holds every other active local or Project-level Revolute joint in that group at its authored coordinate. All geometric relationships remain active residuals. The same authority variable order, central finite differences, damping, backtracking, and dense solve are reused.

`AssemblyCrossHierarchyJointMotionResultApplier` applies only fresh converged results. It protects complete authority inputs, all four relationship record families, exact current motion-group participation, every persistent hierarchy boundary on participating cross paths, exact canonical PartDocument model intent, and selected source/request coordinate semantics. Application updates direct component transforms and only the selected Project-level joint coordinate on one Project copy.

Semantic target-producing model freshness uses the exact result of:

```text
serialize_part_document_to_json(part)
```

as a derived, unpersisted `AssemblySemanticTargetPartSnapshot` payload. The current payload must compare byte-for-byte equal at application. This conservative contract is shared by ordinary local solve results, flexible-child application, local Revolute motion, cross-hierarchy geometric application, and cross-hierarchy Revolute motion.

Persistent Project-level cross-hierarchy fields are now:

```text
cross_hierarchy_constraints[]
cross_hierarchy_joints[]
```

Graphs, endpoint mappings, holding drives, root-space targets, residuals, Jacobians, solve results, freshness snapshots, proposals, and diagnostics remain derived and unpersisted.

See `docs/assembly-cross-hierarchy-revolute-motion-mvp5.md` and `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md` for the current assembly handoff.

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

## Headless tools

```text
blcad_export_step <input.blcad.json> <output.step>
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
blcad_inspect_project_components <input.blcad.project.json>
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
blcad_move_joint <input.blcad.project.json> <joint-id> <angle-deg> <output.blcad.project.json>
blcad_analyze_assembly <input.blcad.project.json> [clearance-threshold-mm]
```

`blcad_move_joint` remains the local root-assembly joint seed. Block 28 adds the library/API contract for Project-level cross-hierarchy Revolute motion; a dedicated CLI command is deferred until a consumer requires it.

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
- `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`

Broader future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Implement **Block 29 only** from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: component geometry instancing and structured STEP assembly products. Define stable derived exchange occurrence/product identities, preserve repeated assembly occurrences without duplicating persistent PartDocument intent, reuse canonical posed-leaf transform chains, and emit structured STEP assembly/product relationships instead of only one flattened compound.

Occurrence-local child pose overrides, whole-subassembly solve variables, and swept-motion contact analysis remain deferred.
