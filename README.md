# BLCAD

BLCAD is a planned independent parametric CAD system for Linux. The project stores CAD model intent in BLCAD data structures and uses OCCT/Open CASCADE as computed geometry, not as the primary model.

Detailed architecture and feature status live in `docs/`. This README is intentionally short.

## Status

Current state: MVP-1 core skeleton, staged MVP-2 sketch/workplane/profile/recompute/reference blocks, the MVP-3 parametric bolt circle, the MVP-4 assembly/project container path, and MVP-5 assembly infrastructure through deterministic Mate/Distance/Concentric/Insert/Angle rigid-body solving, local Jacobian-rank/remaining-DOF diagnostics, suppressed-component solve filtering, posed assembly STEP export, the first persistent Revolute joint/limit motion path, rigid subassembly hierarchy with nested posed-leaf export, read-only posed interference and clearance analysis over the shared posed-leaf boundary, and document-scoped flexible child-assembly solving through the existing rigid-body solver.

The implemented assembly path now includes:

```text
component and Mate/Distance/Concentric/Insert/Angle model intent
  -> deterministic active-constraint graph
  -> generated planar face, circular-cut axis, and circular-cut seat target resolution
  -> explicit local-to-assembly rigid-transform evaluation
  -> planar, axis-line, and composite Insert residual construction
  -> shared numeric residual/Jacobian system
  -> shared damped Gauss-Newton rigid-body solve engine on Project copies
  -> explicit atomic converged-result application
  -> read-only local Jacobian-rank and remaining-DOF diagnostics
  -> suppressed-component filtering over the active solve subgroup

persistent Revolute joint/limit/coordinate intent
  -> deterministic active-joint graph
  -> existing semantic seat axis + oriented frame resolution
  -> transitive combined constraint/joint relationship group
  -> directed-axis + seating + signed periodic twist drive residuals
  -> the same shared numeric residual/Jacobian and solve engine
  -> complete component and driven-joint stale snapshots
  -> atomic transform + selected joint-coordinate application

explicit root assembly + project-owned child assemblies
  -> rigid SubassemblyInstance placement/state intent
  -> cycle-free deterministic assembly hierarchy traversal
  -> visible-active AssemblyLeafOccurrenceDescriptor flattening
  -> exact component/parent transform chains in inner-to-outer order

active rooted child occurrence selection
  -> referenced child AssemblyDocument becomes an isolated local solve root
  -> existing local constraint graph + rigid-body solver/application path reused unchanged
  -> result carries occurrence path + child document identity + normal component snapshots/proposals
  -> atomic application updates child component transforms only; rigid occurrence boundary placement stays unchanged

posed assembly export
  -> one recomputed ShapeCache per referenced leaf part document
  -> repeated root/child assembly occurrences reuse recomputed part caches
  -> visible active leaf shape posing with X-then-Y-then-Z semantics at every authored level
  -> one derived OCCT compound and STEP export
```

Concentric, Insert, Angle, and transient Revolute motion drives reuse the shared numeric system. Regular one-free-body Concentric and Insert relationships are proven as rank `4/6` and `5/6`; the scalar cosine Angle seed is rank `1/6` away from its documented extremal degeneracies. A driven one-free-body Revolute query contributes nine residual components and is proven as local rank `6/6` because the requested joint coordinate temporarily constrains the joint's one motion DOF.

Suppressed components contribute no solve variables. Geometric constraints and non-selected joint drives touching them vanish from the active numeric subgroup while full component snapshots still protect result application from stale suppression changes. A selected Revolute joint cannot be driven through a suppressed endpoint.

`AssemblyHierarchyTraversal` derives the rooted rigid occurrence tree in deterministic pre-order. `AssemblyLeafOccurrenceResolver` removes hidden/suppressed subtrees and local hidden/suppressed components, then exposes stable leaf identity/path plus authored transform chains. These descriptors are derived and unpersisted.

`AssemblyFlexibleSubassemblySolver` selects an exact active non-root occurrence path, constructs a temporary child-as-root project view, and reuses the existing local rigid-body solver. Its applier reconstructs the current local view, reuses normal stale-result validation, and atomically writes proposed direct component transforms back to the referenced child document. Repeated occurrences of one child document therefore share the same internal solved pose while retaining independent rigid boundary transforms.

`AssemblyStepExporter` recomputes each referenced visible-active leaf part once per export, reuses that cache across repeated component and subassembly occurrences, applies every authored rigid transform to independent shape copies in exact inner-to-outer order, composes one OCCT compound, and delegates final file writing to the existing STEP writer. Export geometry remains derived and unpersisted.

There is no GUI yet.

For the complete implementation sequence, see `docs/mvp-plan.md`.

## Technical basis

- C++20
- CMake + Ninja
- OCCT / Open CASCADE Technology
- nlohmann-json
- Catch2
- Qt 6 planned for the future GUI

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
- `examples/`: `.blcad.json` / `.blcad.project.json` examples and headless examples
- `docs/`: architecture, implemented MVP blocks, and roadmaps

## Key documents

Start here:

- `docs/project-goal.md`
- `docs/architecture-summary.md`
- `docs/mvp-plan.md`
- `docs/development-setup.md`
- `docs/file-format.md`

Implemented assembly blocks:

- `docs/assembly-parameters-mvp4.md`
- `docs/project-container-mvp4.md`
- `docs/component-instance-mvp5.md`
- `docs/assembly-constraint-model-intent-mvp5.md`
- `docs/assembly-constraint-graph-mvp5.md`
- `docs/assembly-constraint-target-resolution-mvp5.md`
- `docs/assembly-rigid-transform-evaluation-mvp5.md`
- `docs/assembly-planar-constraint-equations-mvp5.md`
- `docs/assembly-rigid-body-solver-mvp5.md`
- `docs/assembly-solve-diagnostics-mvp5.md`
- `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`
- `docs/assembly-concentric-numeric-solver-dof-mvp5.md`
- `docs/assembly-insert-intent-composite-residuals-mvp5.md`
- `docs/assembly-insert-numeric-solver-dof-mvp5.md`
- `docs/assembly-angle-constraint-mvp5.md`
- `docs/assembly-suppressed-component-solving-mvp5.md`
- `docs/assembly-posed-step-export-mvp5.md`
- `docs/assembly-revolute-joint-motion-mvp5.md`
- `docs/assembly-rigid-subassembly-nested-export-mvp5.md`
- `docs/assembly-interference-analysis-mvp5.md`
- `docs/assembly-flexible-subassembly-solving-mvp5.md`

Broader implemented sketch/profile documents remain listed from `docs/mvp-plan.md` and `docs/architecture-summary.md`.

Future roadmaps:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

## Next technical step

Document-scoped flexible subassembly solving is implemented (`docs/assembly-flexible-subassembly-solving-mvp5.md`). The next assembly block is cross-hierarchy relationship semantics: define stable endpoint identity for a component reached through a subassembly occurrence path before constraints or joints are allowed to cross assembly-document boundaries. See `docs/mvp-plan.md`.
