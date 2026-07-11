# Posed Assembly STEP Export MVP-5

Status: implemented the derived end-to-end assembly geometry export boundary. The original flat occurrence seed now consumes the rigid assembly hierarchy and exports deterministic visible-active flattened part leaves as one OCCT compound and one STEP file.

The canonical hierarchy extension is documented in `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

## Export boundary

`AssemblyStepExporter` is a geometry-layer service. It consumes a `const Project&` and never changes project, assembly, component, subassembly, joint, or part model intent.

The current build path is:

```text
Project
  -> validate complete root/child assembly structure and hierarchy
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> deterministic visible-active flattened part leaves
  -> collect unique referenced leaf part documents
  -> sort part ids lexicographically
  -> recompute each referenced part once into one ShapeCache per part document
  -> for each flattened leaf in deterministic order
       copy the final part shape
       apply every authored transform inner-to-outer
       add the posed shape to one OCCT compound
  -> GeometryShape
  -> existing StepExporter
  -> STEP file
```

Repeated component occurrences and repeated rigid occurrences of one child assembly reuse the same recomputed part cache when they reference the same `PartDocument`. Every leaf receives an independent transformed shape copy before compound composition.

The root-only case remains the degenerate hierarchy with one root traversal entry and an empty parent transform chain. A root component therefore retains the original flat export behavior.

## Leaf identity and deterministic order

The exporter does not implement hierarchy traversal itself. It consumes `AssemblyLeafOccurrenceResolver`.

Each derived leaf contains:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Leaf order is determined by:

```text
root-first hierarchy pre-order
subassembly occurrence id order
local component id order
```

Repeated child assembly occurrences remain separate leaves because their subassembly paths and parent transforms differ.

No leaf descriptor is persistent model intent.

## Transform contract

Assembly export does not define another placement convention. Every authored transform level uses the exact existing `RigidTransform` semantics:

```text
rotation units: degrees
rotation type: active right-handed fixed-axis
application order: X, then Y, then Z
translation units: millimeters
per-level order: rotate X -> rotate Y -> rotate Z -> translate
```

For one component transform `Tc`, an inner subassembly transform `Ti`, and an outer transform `To`, the shape is evaluated as:

```text
To(Ti(Tc(shape)))
```

The transform chain is:

```text
[Tc, Ti, To]
```

The implementation applies each authored level through four explicit OCCT transforms in sequence. It does not multiply an arbitrary hierarchy rotation and convert it back to Euler angles.

This keeps nested export aligned with `AssemblyTransformEvaluator` and `AssemblyHierarchyTransformEvaluator` semantics.

## Visibility and suppression

A flattened leaf exists only when:

```text
every ancestor SubassemblyInstance is Visible
every ancestor SubassemblyInstance is Active
component.visibility == Visible
component.suppression_state == Active
```

A hidden or suppressed subassembly occurrence excludes its complete descendant subtree.

Local child component visibility and suppression still apply inside an otherwise visible active subassembly occurrence.

Filtered records remain persistent model intent and are never mutated by export.

## Part recompute/cache reuse

The exporter collects referenced part ids only from visible-active flattened leaves.

Part ids are deduplicated and sorted lexicographically. Each referenced `PartDocument` is recomputed exactly once into one per-export `ShapeCache`.

Repeated leaves that reference one part share the recomputed final part shape as the source. They still receive separate transformed shape copies.

`ShapeCache` values are derived and scoped to one export operation.

## Failure policy

The exporter fails closed before writing usable output when:

- project member/component structure is invalid;
- a subassembly reference does not resolve to a project-owned child assembly;
- a child occurrence references the root assembly;
- the assembly hierarchy contains a direct or indirect cycle;
- a flattened leaf part does not resolve to an owned `PartDocument`;
- a referenced leaf part recompute fails;
- a referenced part recomputes without a final shape;
- an OCCT rigid transform fails or produces an empty shape;
- no visible active flattened leaf remains;
- the existing STEP writer rejects transfer or file output.

Errors from project validation, recompute, and the existing STEP writer are preserved. Assembly-specific geometry/export failures continue to use `geometry.assembly_step_exporter`.

## Derived-data rule

The following are derived and unpersisted:

- assembly hierarchy traversal descriptors;
- occurrence paths;
- parent transform chains;
- flattened leaf occurrence descriptors;
- per-export `ShapeCache` instances;
- recomputed part final shapes;
- transformed leaf shape copies;
- the posed OCCT compound;
- `AssemblyStepExportSummary`;
- STEP entity identity.

No hierarchy traversal cache, composed transform, assembly geometry cache, STEP entity identity, or transformed BRep is written into `Project`, `AssemblyDocument`, `SubassemblyInstance`, or `ComponentInstance`.

## Repeat-export contract

Determinism is defined at the BLCAD hierarchy/leaf and resulting geometry boundary, not as byte-identical STEP serialization from independent OCCT writer sessions.

For one unchanged project, repeated exports must rebuild the same ordered flattened leaf set and re-import as geometrically equivalent STEP shapes.

Focused coverage compares:

```text
solid count
volume
axis-aligned bounding box
```

This avoids treating STEP entity numbers or writer-session ordering as BLCAD identity. `STEPControl_Writer` remains the exchange-format boundary.

## Headless end-to-end example

`blcad_export_posed_assembly` first inspects the root active geometric constraint graph. If one connected multi-component root constraint group exists, the example solves and explicitly applies that group before export. If no such group exists, current persisted component/subassembly poses are exported directly.

```text
read_project_json_file
  -> AssemblyConstraintGraph::build on project root assembly
  -> optional connected multi-component geometric group
       -> AssemblyRigidBodySolver::solve
       -> AssemblySolveResultApplier::apply
  -> AssemblyStepExporter::write_step
       -> hierarchy traversal
       -> flattened leaves
       -> nested posed compound
```

The optional solve is headless-example orchestration, not part of `AssemblyStepExporter`.

### Root geometric solve example

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/posed_assembly.blcad.project.json \
  build/posed_assembly.step
```

The example solves and applies one planar Mate before exporting two root component occurrences.

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

The export consumes the already-applied pose without requiring a geometric root solve group.

### Nested rigid subassembly example

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/nested_subassembly.blcad.project.json \
  build/nested_subassembly.step
```

The project contains two rigid root occurrences of one child assembly. The child contains one rigid grandchild occurrence, and the grandchild contains one plate component. The two flattened leaves share one referenced part recompute cache and receive distinct complete transform chains.

## Proven behavior

`tests/geometry/assembly_step_exporter_tests.cpp` preserves root-only coverage:

- two differently posed root instances of one recomputed part;
- one `ShapeCache` per referenced part document and reuse across occurrences;
- compound solid count and total volume;
- Z-rotation-then-translation bounding-box correctness;
- hidden/suppressed root component exclusion;
- repeated STEP geometric equivalence after re-import;
- unresolved member failure;
- recompute without a final shape failure.

`tests/geometry/assembly_nested_step_exporter_tests.cpp` adds hierarchy coverage:

- repeated rigid occurrences of one child assembly;
- two hierarchy levels;
- one recomputed part cache reused by two nested leaves;
- exact transform-order bounding-box union `x=[65,210]`, `y=[-10,35]`, `z=[0,2]` for the checked geometry;
- two solids and `800 mm^3` total volume;
- hidden subassembly subtree exclusion;
- suppressed subassembly subtree exclusion;
- local child component visibility filtering;
- empty visible-active flattened output rejection;
- unresolved nested leaf part rejection;
- repeated STEP exports re-importing with equal solid count, volume, and bounding box.

## Still not implemented

- structured STEP product hierarchy with named assembly/component occurrence products;
- component/leaf geometry instancing instead of transformed shape copies;
- flexible subassembly solver variables;
- constraints or joints across assembly-document boundaries;
- collision/interference analysis;
- contact response or physics.

The next assembly consumer is a read-only posed interference-analysis seed over the same deterministic flattened leaf boundary.
