# Rigid Subassembly Hierarchy and Nested Posed Export MVP-5

Status: implemented the first hierarchical assembly structure and recursive posed-geometry consumer. Child assemblies are project-owned documents referenced by rigid subassembly occurrences. Hierarchy traversal, flattened leaf occurrences, and posed export geometry remain derived.

## Scope

This seed adds rigid hierarchy only.

```text
Project
  explicit root AssemblyDocument
  child AssemblyDocument records
  PartDocument records
```

A child assembly occurrence is rigid at its parent boundary. Its internal component transforms are already-authored pose. The seed does not introduce flexible-subassembly solver coordinates, parent-child constraint targets, or joints across an assembly boundary.

## Project ownership

`Project::assembly()` remains the one explicit root assembly.

Project-owned child assemblies are stored separately:

```text
Project::child_assembly_documents()
```

Assembly document ids are unique across the root and child assembly collection. A child document is not implicitly part of the rooted occurrence tree merely because it is owned by the project; it enters the rooted tree only through a `SubassemblyInstance` reference.

Project validation still validates owned but currently unreferenced child assemblies. In particular, an invalid cycle hidden in an unreferenced child-document graph is rejected rather than becoming latent project state.

## Persistent rigid subassembly occurrence

Stable identity:

```text
SubassemblyInstanceId
```

Persistent record:

```text
SubassemblyInstance
  id
  name
  referenced_assembly_document
  visibility
  suppression_state
  RigidTransform
    translation_mm
    rotation_deg
```

There is deliberately no grounding field and no subassembly solve variable in this seed.

The record reuses the existing component visibility, suppression, and rigid-transform semantics. Transform values must be finite.

`AssemblyDocument` stores:

```text
component_instances[]
subassembly_instances[]
assembly_constraints[]
assembly_joints[]
```

Part component occurrences and child assembly occurrences are separate record families.

Adding or loading a `SubassemblyInstance` never mutates the referenced child assembly and never moves any child component.

## Reference and cycle validation

`AssemblyDocument::add_subassembly_instance` rejects:

- empty occurrence identity through record construction;
- duplicate `SubassemblyInstanceId` within one containing assembly;
- direct reference to the containing assembly document.

`Project` owns the cross-document validation boundary.

Every subassembly occurrence must reference a project-owned child `AssemblyDocument`. References to the project root assembly are rejected, preserving one explicit root identity.

The hierarchy document graph must be acyclic. `Project::validate_assembly_hierarchy` performs deterministic depth-first validation over the root and all owned child assembly documents. Re-entering a document on the active DFS path is a direct or indirect cycle and fails validation.

The validation graph is regenerated. Parent links and cycle-state caches are not persisted.

## Additive JSON form

The historical project marker remains:

```text
blcad.project.mvp4
version 1
```

The root assembly remains the existing field:

```text
assembly
```

Project-owned child assemblies are the optional additive collection:

```text
assemblies[]
```

The historical assembly marker also remains:

```text
blcad.assembly_document.mvp4
version 1
```

Rigid child occurrences are the optional additive collection:

```text
subassembly_instances[]
```

Representative occurrence:

```json
{
  "id": "subassembly.gearbox.left",
  "name": "Left gearbox",
  "referenced_assembly_document": "assembly.gearbox",
  "visibility": "visible",
  "suppression_state": "active",
  "transform": {
    "translation_mm": {"x": 100.0, "y": 0.0, "z": 0.0},
    "rotation_deg": {"x": 0.0, "y": 0.0, "z": 90.0}
  }
}
```

Files without `assemblies` load with zero child assembly documents. Assembly records without `subassembly_instances` load with zero child occurrences.

## Deterministic hierarchy traversal

`AssemblyHierarchyTraversal` is a read-only derived pre-order DFS of the rooted occurrence tree.

The root is always the first descriptor. Within each assembly occurrence, child `SubassemblyInstance` records are visited in lexicographic id order.

A referenced child `AssemblyDocument` may occur more than once. Repeated rigid occurrences remain distinct traversal descriptors even though they reference the same child document.

Each `AssemblyHierarchyOccurrenceDescriptor` contains derived data:

```text
assembly_document
parent_assembly_document
via_subassembly_instance
occurrence_path
parent_transforms_inner_to_outer
visible_path
active_path
```

`occurrence_path` is root-to-current occurrence identity.

For:

```text
root --subassembly.outer--> child --subassembly.inner--> grandchild
```

the grandchild path is:

```text
[subassembly.outer, subassembly.inner]
```

Its parent transform chain is stored in evaluation order:

```text
[T_inner, T_outer]
```

No traversal order, parent descriptor, occurrence path cache, or transform chain is persisted.

## Hierarchical rigid-transform semantics

`AssemblyHierarchyTransformEvaluator` evaluates an authored transform chain exactly in inner-to-outer order.

Every individual `RigidTransform` retains the established convention:

```text
translation_mm in millimeters
rotation_deg in degrees
active right-handed fixed-axis rotation
apply X, then Y, then Z
then translate
```

For a leaf component transform `Tc`, an inner child occurrence `Ti`, and an outer occurrence `To`, a point is evaluated as:

```text
To(Ti(Tc(p)))
```

The transform chain is therefore:

```text
[Tc, Ti, To]
```

Points rotate and translate at each level. Vectors rotate only at each level.

The implementation deliberately does not multiply the hierarchy into an arbitrary rotation and convert it back to persisted X/Y/Z Euler angles. Such a conversion would add branch, representation, and gimbal semantics that are not part of the authored model. Exact authored levels are evaluated sequentially instead.

## Flattened leaf occurrence boundary

`AssemblyLeafOccurrenceResolver` is the canonical read-only flattening boundary between assembly hierarchy intent and geometry consumers.

It returns `AssemblyLeafOccurrenceDescriptor` values containing:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

For each visible active component leaf, the transform chain begins with the component transform and then appends the containing assembly occurrence's parent chain.

Example:

```text
component Tc in grandchild
inner occurrence Ti
outer occurrence To

transforms_inner_to_outer = [Tc, Ti, To]
```

The resolver preserves repeated child occurrences as separate leaves because their `subassembly_path` and parent transform levels differ.

Ordering is deterministic:

1. hierarchy pre-order DFS;
2. child subassembly occurrences lexicographically by `SubassemblyInstanceId`;
3. local part components lexicographically by `ComponentInstanceId`.

Flattened descriptors are regenerable and unpersisted.

## Visibility and suppression

A hidden or suppressed `SubassemblyInstance` excludes its complete descendant subtree from leaf flattening and posed export.

Path state is accumulated through the hierarchy:

```text
visible_path = every ancestor occurrence is Visible
active_path  = every ancestor occurrence is Active
```

Within an otherwise visible active assembly occurrence, each local `ComponentInstance` still applies its own existing visibility and suppression state.

Therefore a leaf is present only when:

```text
visible_path
&& active_path
&& component.visibility == Visible
&& component.suppression_state == Active
```

No filtered record is mutated.

## Nested posed STEP export

`AssemblyStepExporter` now consumes `AssemblyLeafOccurrenceResolver` rather than iterating only root components.

The derived pipeline is:

```text
const Project
  -> validate complete assembly structure and hierarchy
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible active flattened leaves
  -> collect unique referenced leaf part ids
  -> sort part ids lexicographically
  -> recompute each referenced PartDocument exactly once into one ShapeCache
  -> for each leaf in deterministic order
       copy final part shape
       apply every authored transform inner-to-outer
       add posed shape to OCCT compound
  -> existing StepExporter
  -> STEP file
```

Repeated rigid occurrences of one child assembly reuse the same recomputed part cache. Each leaf still receives an independent transformed shape copy.

The existing root-only path is a degenerate hierarchy with exactly one root descriptor and no parent transforms. A root component therefore keeps the original one-level export semantics.

## Failure policy

The hierarchy/export path fails closed on:

- missing project-owned child assembly references;
- root assembly references through a child occurrence;
- direct or indirect assembly-reference cycles;
- unresolved member or leaf part documents;
- part recompute failures;
- recompute without a final shape;
- OCCT rigid-transform failures or empty transformed shapes;
- no visible active flattened leaves;
- STEP transfer or file-write failure.

Validation errors remain validation errors. Geometry and export errors preserve the existing assembly STEP exporter boundaries.

## Persistence boundary

Persisted model intent:

```text
explicit root assembly identity
project-owned child assembly documents
SubassemblyInstance identity
referenced child assembly id
subassembly visibility
subassembly suppression
subassembly RigidTransform
child ComponentInstance authored transforms and states
```

Derived and unpersisted:

```text
assembly document reference graph
cycle-validation traversal state
parent links
hierarchy traversal order
AssemblyHierarchyOccurrenceDescriptor values
occurrence paths
parent transform chains
AssemblyLeafOccurrenceDescriptor values
flattened leaf ordering
composed/evaluated hierarchy points or vectors
per-export ShapeCache values
transformed leaf shape copies
OCCT compound
STEP entity identity
```

Hierarchy transforms and occurrence state are model intent. A composed transform is not a second source of truth.

## Focused coverage

Core coverage proves:

- `SubassemblyInstance` identity/name/reference/finite-transform validation;
- explicit transform, visibility, and suppression edits;
- duplicate occurrence id rejection;
- direct self-reference rejection;
- additive assembly JSON roundtrip;
- additive project JSON roundtrip with an explicit root plus owned child assemblies;
- backward compatibility without either additive collection;
- child assembly id uniqueness including the root id;
- missing-child and child-to-root reference rejection;
- direct/indirect cycle rejection;
- deterministic hierarchy traversal independent of insertion order;
- repeated child assembly occurrences remaining distinct;
- exact path and inner-to-outer parent transform-chain semantics;
- canonical visible-active leaf flattening;
- deterministic leaf ordering and subtree/component filtering.

Geometry coverage proves:

- hierarchical point evaluation through three authored transforms;
- vector evaluation ignoring translation at every level;
- non-commutative transform ordering;
- two-level nested posed export;
- repeated occurrences of one child assembly;
- one recomputed part cache reused by two nested leaves;
- exact transformed STEP bounding-box union;
- solid count and volume;
- hidden/suppressed complete subtree exclusion;
- local child component visibility filtering;
- empty flattened output rejection;
- unresolved nested leaf part rejection;
- repeated STEP exports re-importing with equivalent solid count, volume, and bounding box.

## Headless example

Checked-in project:

```text
examples/nested_subassembly.blcad.project.json
```

Command:

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/nested_subassembly.blcad.project.json \
  build/nested_subassembly.step
```

The project contains two rigid occurrences of one child assembly. That child contains one rigid grandchild occurrence, and the grandchild contains one plate component. Both root occurrences therefore flatten to distinct posed plate leaves while sharing one referenced part recompute cache.

## Explicitly deferred

- flexible subassembly solver variables;
- grounding semantics for whole subassembly occurrences;
- geometric constraints across assembly-document boundaries;
- joints across assembly-document boundaries;
- motion propagation through flexible child assemblies;
- collision/contact response or physics;
- structured STEP product hierarchy and named occurrence products;
- geometry instancing instead of transformed shape copies.

The next block can now consume the stable flattened posed-leaf identity/transform boundary without reimplementing hierarchy traversal.
