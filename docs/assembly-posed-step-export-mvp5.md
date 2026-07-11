# Posed Assembly STEP Export MVP-5

Status: implemented deterministic posed assembly STEP export over the canonical visible-active flattened leaf boundary, including root components and nested rigid child assembly occurrences.

## Scope

`AssemblyStepExporter` is a derived geometry consumer. It does not own assembly placement or solve relationships.

Current pipeline:

```text
const Project
  -> validate complete assembly structure and cycles
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible-active flattened part leaves
  -> collect unique referenced leaf PartDocument ids
  -> sort referenced part ids lexicographically
  -> recompute each referenced part exactly once into one ShapeCache
  -> copy each leaf's final part shape
  -> pose the shape through the leaf's exact transform chain
  -> add posed shape to one OCCT compound
  -> existing StepExporter
  -> STEP file
```

All export geometry is derived and unpersisted.

## Canonical leaf boundary

`AssemblyStepExporter` consumes `AssemblyLeafOccurrenceResolver` rather than implementing its own hierarchy traversal or visibility/suppression policy.

One visible-active leaf descriptor supplies:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

For a root component:

```text
transforms_inner_to_outer = [T_component]
```

For a nested component:

```text
transforms_inner_to_outer = [T_component, T_inner_parent, ..., T_outer_parent]
```

The exporter follows the exact authored transform order.

## Transform semantics

Every `RigidTransform` level keeps the established convention:

```text
translation: millimeters
rotation: degrees
active right-handed fixed-axis rotation
X then Y then Z
then translation
```

The hierarchy chain is evaluated inner-to-outer.

The internal posed-leaf geometry builder composes the authored chain into one OCCT rigid transform for shape posing. This composed geometry transform is derived and is never written back as model intent or converted into a persisted Euler triple.

## Part recompute and cache reuse

The exporter first collects all referenced visible-active leaf part ids.

Each unique referenced `PartDocument` is recomputed exactly once per export into one derived `ShapeCache`.

Repeated component occurrences and repeated child assembly occurrences may reuse that cache.

Each leaf still receives an independent shape copy before posing.

Example:

```text
subassembly.left  -> child -> part.plate
subassembly.right -> child -> part.plate
```

Export recomputes `part.plate` once, then poses two independent copies through the two leaf transform chains.

## Visibility and suppression

The exporter inherits the leaf resolver policy:

```text
visible subassembly path
&& active subassembly path
&& local component visible
&& local component active
```

A hidden or suppressed subassembly occurrence removes its complete descendant subtree.

A hidden or suppressed local component is also absent from export.

The exporter does not mutate filtered records.

## Output contract

The current exchange result is one geometric OCCT compound written through the existing STEP writer.

The export is not yet a structured STEP product hierarchy with named assembly/component products.

Solid identity in the STEP file is exchange/geometry output, not persistent BLCAD model identity.

## Failure policy

Export fails closed on:

- invalid project assembly structure or hierarchy cycles;
- missing project-owned child assembly references;
- unresolved visible-active leaf part documents;
- part recompute failures;
- recompute without a final shape;
- transform/shape posing failures;
- empty visible-active flattened output;
- OCCT compound/export transfer failures;
- STEP file-write failure.

No partial model mutation occurs because the export path is read-only.

## Headless orchestration

`blcad_export_posed_assembly` loads project JSON and exports the current posed model.

The CLI may optionally solve/apply one supported root local geometric group before export when the loaded root assembly contains such a group. That orchestration is outside `AssemblyStepExporter` itself.

### Root local solve example

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/posed_assembly.blcad.project.json \
  build/posed_assembly.step
```

The example solves/applies one planar Mate and exports two root component occurrences.

### Applied Revolute motion example

```bash
./build/dev-geometry/blcad_move_joint \
  examples/revolute_joint.blcad.project.json \
  joint.revolute \
  45 \
  build/revolute_joint_45.blcad.project.json

./build/dev-geometry/blcad_export_posed_assembly \
  build/revolute_joint_45.blcad.project.json \
  build/revolute_joint_45.step
```

The exporter consumes the already-applied component/joint pose.

### Nested rigid subassembly example

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/nested_subassembly.blcad.project.json \
  build/nested_subassembly.step
```

The example contains two rigid root occurrences of one child assembly and one further child level. The flattened leaves share one referenced part recompute cache but receive distinct complete transform chains.

## Shared posed-geometry consumer boundary

Later posed interference and clearance analysis reuse the same internal `AssemblyPosedLeafShapeBuilder` boundary.

That shared boundary guarantees export and analysis agree on:

- project hierarchy validation;
- visible-active leaf selection;
- deterministic leaf identity/order;
- unique referenced-part recompute;
- transform-chain posing semantics.

Export does not maintain a second posed-assembly interpretation.

## Proven behavior

`tests/geometry/assembly_step_exporter_tests.cpp` covers root-only export:

- differently posed occurrences of one recomputed part;
- one cache per referenced part and reuse across occurrences;
- compound solid count and total volume;
- rotation/translation bounding-box behavior;
- hidden/suppressed component exclusion;
- repeated STEP geometric equivalence after re-import;
- unresolved member and missing-final-shape failures.

`tests/geometry/assembly_nested_step_exporter_tests.cpp` covers hierarchy export:

- repeated child assembly occurrences;
- two hierarchy levels;
- cache reuse across nested leaves;
- exact transform-order bounding-box union;
- solid count and volume;
- hidden/suppressed subtree exclusion;
- local child component filtering;
- empty flattened output rejection;
- unresolved nested leaf part rejection;
- repeated STEP geometric equivalence after re-import.

Related interference/clearance tests prove the shared posed-leaf geometry boundary remains deterministic for additional consumers.

## Persistence boundary

Persisted input authority includes:

```text
PartDocument model intent
ComponentInstance state and direct transform
SubassemblyInstance state and boundary transform
project-owned assembly hierarchy references
```

Derived and unpersisted:

```text
hierarchy traversal descriptors
occurrence paths
parent transform chains
flattened leaves
per-export ShapeCache values
copied posed leaf shapes
composed OCCT posing transforms
OCCT compound
STEP entity identity
```

Export never writes model transforms or relationship state.

## Current boundaries

Still deferred:

- structured STEP product hierarchy with named assembly/component occurrence products;
- geometry instancing instead of transformed shape copies;
- persistent exchange caches;
- BOM/product-structure exchange semantics.

Implemented later in separate blocks:

- posed interference and clearance analysis over the same leaf boundary;
- document-scoped flexible child solving;
- read-only occurrence-qualified cross-hierarchy target/residual semantics.

Persistent project-level cross-hierarchy constraints and solving remain sequenced in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Cross-hierarchy solving changes component model intent only through explicit result application; export then naturally consumes the updated transforms through normal leaf regeneration.
