# Rigid Subassembly Hierarchy and Nested Posed Export MVP-5

Status: implemented one explicit root assembly plus project-owned child assembly documents, persistent rigid child occurrences, cycle-free deterministic rooted hierarchy traversal, exact authored transform chains, visible-active leaf flattening, and nested posed STEP export.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
project-owned child AssemblyDocument records
project-owned PartDocument records
```

`Project::assembly()` remains the explicit root assembly API.

A project-owned child assembly document is a model definition. It becomes a rooted occurrence only when a containing assembly stores a `SubassemblyInstance` that references it.

The same child document may be referenced by multiple `SubassemblyInstance` records.

## Persistent rigid child occurrence

`SubassemblyInstance` stores:

```text
SubassemblyInstanceId id
name
referenced_assembly_document
visibility
suppression_state
RigidTransform
  translation_mm
  rotation_deg
```

The transform is the authored rigid boundary from the containing assembly occurrence to the referenced child assembly document.

A `SubassemblyInstance` currently has no grounding field and is not itself a numeric rigid-body variable.

Adding or loading an occurrence never mutates child component transforms.

## Separate assembly collections

`AssemblyDocument` keeps direct part components and child assembly occurrences as separate model-intent collections:

```text
component_instances[]
subassembly_instances[]
assembly_constraints[]
assembly_joints[]
```

Local geometric constraints and local joints continue to address direct local part components in the containing assembly document.

The later read-only cross-hierarchy query layer is separate from these local collections.

## Hierarchy validity

`Project` validates the complete owned assembly-document reference graph.

Validation rejects:

- duplicate child assembly document ids;
- collision with the explicit root assembly id;
- duplicate subassembly occurrence ids within one containing assembly;
- direct self-reference;
- references to the explicit root as a child;
- references to missing child assembly documents;
- direct or indirect assembly-reference cycles.

Owned but currently unreferenced child graphs are validated as well, so latent cycles cannot enter project state.

The document reference graph and DFS validation state are derived and unpersisted.

## Deterministic rooted traversal

`AssemblyHierarchyTraversal::build(project)` validates project structure and derives rooted occurrence descriptors.

Traversal order:

```text
root first
pre-order DFS
child SubassemblyInstance records lexicographically by id
```

Repeated occurrences of one child assembly document remain separate traversal entries.

Each `AssemblyHierarchyOccurrenceDescriptor` contains:

```text
assembly_document
parent_assembly_document
via_subassembly_instance
occurrence_path
parent_transforms_inner_to_outer
visible_path
active_path
```

The root descriptor uses:

```text
parent_assembly_document = none
via_subassembly_instance = none
occurrence_path = []
parent_transforms_inner_to_outer = []
```

For:

```text
root --outer--> child --inner--> grandchild
```

the grandchild descriptor contains:

```text
occurrence_path = [outer, inner]
parent_transforms_inner_to_outer = [T_inner, T_outer]
```

Paths are ordered identity sequences, not sets.

## Hierarchical transform evaluation

Every authored `RigidTransform` level retains the existing convention:

```text
translation: millimeters
rotation: degrees
active right-handed fixed-axis
X then Y then Z
then translation
```

`AssemblyHierarchyTransformEvaluator` applies transform chains exactly inner-to-outer.

For component transform `Tc`, inner parent `Ti`, and outer parent `To`:

```text
chain = [Tc, Ti, To]
point = To(Ti(Tc(p)))
```

Vectors, normals, and directions ignore translation at every level.

The hierarchy is not collapsed into a persisted composed transform or converted back into a recomputed Euler representation.

## Canonical leaf flattening

`AssemblyLeafOccurrenceResolver` converts the rooted hierarchy into visible-active part-component leaves for posed geometry consumers.

One `AssemblyLeafOccurrenceDescriptor` stores:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

For a nested part component:

```text
transforms_inner_to_outer =
  [T_component, T_inner_parent, ..., T_outer_parent]
```

Leaf order is deterministic:

```text
hierarchy pre-order
then local ComponentInstanceId order
```

Flattened descriptors are derived and unpersisted.

## Visibility and suppression

Path state is accumulated through ancestor subassembly occurrences:

```text
visible_path = every ancestor occurrence is visible
active_path  = every ancestor occurrence is active
```

A hidden or suppressed subassembly occurrence removes its complete descendant subtree from leaf flattening.

Within an otherwise visible-active assembly occurrence, local component visibility and suppression still apply.

A posed leaf exists only when:

```text
visible_path
&& active_path
&& component.visibility == visible
&& component.suppression_state == active
```

No filtered record is mutated.

## Nested posed STEP export

`AssemblyStepExporter` consumes `AssemblyLeafOccurrenceResolver`.

The hierarchy export pipeline is:

```text
const Project
  -> validate structure and hierarchy
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible-active leaves
  -> unique referenced part ids
  -> one ShapeCache per referenced part
  -> copy each leaf shape
  -> apply every authored leaf transform inner-to-outer
  -> one OCCT compound
  -> existing StepExporter
```

Repeated occurrences of one child assembly document reuse recomputed part caches while retaining independent leaf transform chains and shape copies.

The root-only export path is the degenerate one-root hierarchy.

## Shared child-document model authority

Repeated child occurrences are different rooted geometric occurrences, but they reference one shared child `AssemblyDocument` model definition.

For a child component:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId, local ComponentInstanceId)
```

Example:

```text
([left],  component.shaft) -> (assembly.gearbox, component.shaft)
([right], component.shaft) -> (assembly.gearbox, component.shaft)
```

The rooted occurrence paths differ and therefore produce different parent transform chains. The direct local component transform is still stored once in `assembly.gearbox`.

This ownership rule is later consumed by document-scoped flexible child solving and is a critical constraint on future cross-hierarchy numeric variable identity.

## Failure policy

Hierarchy/export fails closed on:

- missing child assembly references;
- root-as-child references;
- direct or indirect cycles;
- unresolved member or leaf parts;
- failed part recompute;
- recompute without a final shape;
- rigid-transform/shape posing failures;
- empty visible-active leaf output;
- STEP transfer or file-write failure.

Validation and geometry/export errors preserve their existing error categories.

## Persistence boundary

Persisted model intent:

```text
explicit root assembly identity
project-owned child AssemblyDocument records
SubassemblyInstance identity and referenced child id
subassembly visibility and suppression
subassembly rigid boundary transform
child ComponentInstance state and direct transform
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
flattened leaf order
composed/evaluated hierarchy geometry
per-consumer ShapeCache values
posed leaf shape copies
OCCT compounds
STEP entity identity
```

Hierarchy occurrence state and direct authored transforms are model intent. Composed transforms are not a second source of truth.

## Focused coverage

Core coverage proves:

- `SubassemblyInstance` identity/reference/finite-transform validation;
- explicit transform/visibility/suppression edits;
- duplicate occurrence rejection;
- direct self-reference rejection;
- additive assembly/project JSON roundtrip;
- backward compatibility without child assembly collections;
- assembly id uniqueness including the root id;
- missing-child and child-to-root rejection;
- direct/indirect cycle rejection;
- deterministic insertion-order-independent traversal;
- repeated child occurrences remaining distinct;
- exact path and parent transform-chain semantics;
- visible-active leaf flattening and deterministic filtering/order.

Geometry coverage proves:

- hierarchical point/vector evaluation;
- non-commutative transform order;
- two-level nested posed export;
- repeated occurrences of one child assembly;
- one recomputed part cache reused by nested leaves;
- exact STEP bounding-box union, solid count, and volume;
- hidden/suppressed subtree filtering;
- local child component filtering;
- empty output and unresolved nested leaf failures;
- repeated STEP geometric equivalence after re-import.

## Headless example

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/nested_subassembly.blcad.project.json \
  build/nested_subassembly.step
```

The checked-in project contains two rigid occurrences of one child assembly. That child contains one rigid grandchild occurrence, and the grandchild contains one plate component.

Both root occurrences flatten to distinct posed plate leaves while sharing one referenced part recompute cache.

## Downstream blocks now implemented

The following work was implemented after this hierarchy seed:

- posed interference and clearance analysis over the same flattened leaf boundary;
- document-scoped flexible child solving through exact active occurrence selection;
- occurrence-qualified read-only cross-hierarchy target/residual semantics.

The flexible solver preserves this document's shared-child ownership rule: applying a child-local solve changes the child document's direct component transform, so every rooted occurrence of that child observes the same internal pose through its own boundary transform.

## Still deferred

- occurrence-local internal component pose overrides;
- grounding or solving a complete `SubassemblyInstance` boundary as one rigid variable;
- persistent project-level cross-hierarchy geometric constraints and solve/application integration;
- cross-hierarchy joints and nested motion propagation;
- structured STEP product hierarchy and named occurrence products;
- component geometry instancing;
- contact response or rigid-body physics.

The current cross-hierarchy implementation sequence is documented in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.
