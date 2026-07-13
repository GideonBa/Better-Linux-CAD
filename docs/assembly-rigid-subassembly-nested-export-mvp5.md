# Rigid Subassembly Hierarchy and Nested Posed Export MVP-5

Status: implemented explicit root plus Project-owned child assembly definitions, persistent rigid child occurrences, cycle-free deterministic rooted traversal, exact authored transform chains, visible-active leaf flattening, nested flattened posed STEP export, and the later Block-29 structured STEP occurrence/product consumer.

## Project ownership

`Project` owns:

```text
one explicit root AssemblyDocument
Project-owned child AssemblyDocument records
Project-owned PartDocument records
```

A child `AssemblyDocument` is a shared model definition. It becomes a rooted occurrence only when a containing assembly stores a `SubassemblyInstance` referencing it.

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

The transform is the authored rigid boundary from the containing assembly occurrence to the referenced child assembly definition.

A `SubassemblyInstance` has no grounding field and is not currently a numeric rigid-body variable.

Adding/loading an occurrence never mutates child component transforms.

## Separate local collections

`AssemblyDocument` keeps direct part components and child occurrences separately:

```text
component_instances[]
subassembly_instances[]
assembly_constraints[]
assembly_joints[]
```

Local geometric constraints and local joints address direct local part components in the containing assembly document.

Project-level occurrence-qualified relationships/joints are separate Project collections.

## Hierarchy validity

`Project` validates the complete owned assembly-document reference graph.

Validation rejects:

- duplicate child assembly document ids;
- collision with the explicit root assembly id;
- duplicate subassembly occurrence ids within one containing assembly;
- direct self-reference;
- references to the explicit root as a child;
- missing child assembly references;
- direct or indirect cycles.

Owned but currently unreferenced child graphs are validated as well.

The reference graph and DFS validation state are derived/unpersisted.

## Deterministic rooted traversal

`AssemblyHierarchyTraversal::build(project)` validates Project structure and derives rooted occurrence descriptors.

Traversal order:

```text
root first
pre-order DFS
child SubassemblyInstance records lexicographically by id
```

Repeated occurrences of one child document remain separate traversal entries.

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

Root:

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

At descent, the current direct `SubassemblyInstance::transform()` is prepended. Therefore a non-root descriptor's first parent transform is exactly its direct boundary from the containing occurrence.

Paths are ordered identity sequences, not sets.

## Hierarchical transform evaluation

Every authored `RigidTransform` retains:

```text
translation: millimeters
rotation: degrees
active right-handed fixed-axis
X then Y then Z
then translation
```

`AssemblyHierarchyTransformEvaluator` applies exact inner-to-outer chains.

For:

```text
chain = [Tc, Ti, To]
```

point evaluation is:

```text
To(Ti(Tc(p)))
```

Vectors/normals/directions ignore translation.

The hierarchy is not collapsed into a persisted composed transform or recomputed Euler representation.

## Canonical leaf flattening

`AssemblyLeafOccurrenceResolver` converts the rooted hierarchy into visible-active part-component leaves.

One `AssemblyLeafOccurrenceDescriptor` stores:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Nested component:

```text
transforms_inner_to_outer =
  [T_component, T_inner_parent, ..., T_outer_parent]
```

Leaf order is deterministic:

```text
hierarchy pre-order
then local ComponentInstanceId order
```

Leaf descriptors are derived/unpersisted.

## Visibility and suppression

Path state is accumulated through ancestor occurrences:

```text
visible_path = every ancestor occurrence visible
active_path  = every ancestor occurrence active
```

A hidden/suppressed subassembly occurrence removes its complete descendant subtree from leaf flattening.

Within a visible-active assembly occurrence, local component visibility/suppression also applies.

A leaf exists only when:

```text
visible_path
&& active_path
&& component.visibility == visible
&& component.suppression_state == active
```

No filtered record is mutated.

## Flattened posed STEP compatibility path

`AssemblyStepExporter` consumes canonical leaves.

Current pipeline:

```text
Project
-> AssemblyLeafOccurrenceResolver
-> visible-active leaves
-> AssemblyPartShapeDefinitionBuilder
-> one recompute / one ShapeCache / one unposed shape per unique PartDocumentId
-> pose each leaf through its exact complete chain
-> one OCCT compound
-> StepExporter
```

Repeated child occurrences reuse shared part definitions while retaining independent leaf transform chains and posed copies.

The root-only export is the degenerate one-root hierarchy.

Canonical details: `docs/assembly-posed-step-export-mvp5.md`.

## Shared child-document transform authority

Repeated child occurrences are different rooted geometric occurrences but reference one shared child `AssemblyDocument` model definition.

For a child component:

```text
ComponentTransformAuthority =
  (AssemblyDocumentId,
   local ComponentInstanceId)
```

Example:

```text
([left],  component.shaft) -> (assembly.gearbox, component.shaft)
([right], component.shaft) -> (assembly.gearbox, component.shaft)
```

The rooted paths differ and produce different parent chains. The direct child component transform remains stored once in `assembly.gearbox`.

Document-scoped flexible solving, cross-hierarchy geometric solving, and cross-hierarchy motion preserve this shared-authority rule.

## Block-29 structured exchange occurrence identity

Structured exchange deliberately uses a different derived identity from transform authority.

Assembly occurrence:

```text
exact rooted SubassemblyInstance occurrence path
```

Component occurrence:

```text
(containing rooted assembly path,
 local ComponentInstanceId)
```

Part product definition:

```text
referenced PartDocumentId
```

Thus:

```text
([left],  component.shaft)
([right], component.shaft)
```

are two structured exchange component occurrences while both may still map to one solver transform authority:

```text
(assembly.gearbox, component.shaft)
```

This distinction is intentional.

`AssemblyExchangeGraph` retains the root plus every assembly path prefix required by one visible-active leaf. It inherits leaf filtering and exact transform chains rather than implementing a second hierarchy interpretation.

## Structured STEP assembly/product export

`AssemblyStructuredStepExporter` consumes the derived exchange graph.

It creates:

```text
one XDE part definition label per PartDocumentId
one XDE assembly label per rooted assembly occurrence
part component references to shared part definitions
parent -> child assembly references for non-root occurrences
```

Part component placement is the direct first transform from the canonical leaf chain.

Child assembly placement is the hierarchy-derived direct `transform_from_parent` boundary.

Nested placement composition is represented by the XDE assembly graph instead of precomposing every leaf into root space.

Repeated child occurrences remain separate assembly product occurrences. Repeated parts share one part product definition.

Canonical details: `docs/assembly-structured-step-products-mvp5.md`.

## Failure policy

Hierarchy/flattened/structured consumers fail closed on their documented boundaries, including invalid references/cycles, unresolved parts, recompute failures, missing final shapes, malformed derived transform context, empty visible-active output, OCCT/XDE transfer failures, and STEP write failures.

All hierarchy/export consumers are read-only with respect to persistent Project model intent.

## Persistence boundary

Persisted model intent:

```text
explicit root assembly identity
Project-owned child AssemblyDocument records
SubassemblyInstance identity and referenced child id
subassembly visibility/suppression/boundary transform
child ComponentInstance state and direct transform
```

Derived/unpersisted:

```text
assembly document reference graph
cycle-validation traversal state
hierarchy occurrence descriptors
occurrence paths
parent transform chains
leaf occurrence descriptors
flattened leaf order
AssemblyExchangeGraph identities/records/names
part shape definitions and ShapeCache values
posed leaf shape copies
composed OCCT transforms
XDE documents/labels/component references
OCCT compounds
STEP product/entity identity
```

Hierarchy occurrence state and direct authored transforms are model intent. Traversal, composition, exchange occurrence products, and STEP entities are not second sources of truth.

## Focused coverage

Hierarchy/leaf tests prove cycle validation, deterministic traversal, repeated child occurrences, exact path/parent-chain semantics, and visible-active filtering.

Flattened geometry tests:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-nested-step-export]"
```

prove repeated child occurrences, nested transform order, shared part recompute, filtering, solid/volume/bounds behavior, and STEP re-import equivalence.

Structured exchange tests:

```bash
./build/dev/blcad_core_tests "[core][assembly-exchange-graph]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
```

prove rooted assembly/component exchange identity, shared part definitions, repeated/nested occurrence products, exact transform-chain reuse, collision-free names, XDE/STEP assembly usage relationships, and geometric equivalence with the flattened compatibility export.

The reader used for geometric equivalence is test-only export verification. It is not a BLCAD STEP
Part/Assembly import path. Reference and EditableBody import plus structured hierarchy conversion
are planned in Blocks 95–101 of `docs/step-import-sequence-mvp7.md`.

## Current handoff

Block 29 structured STEP assembly/product relationships are implemented.

The next technical step is Block 30: richer posed contact classification and bounded deterministic swept-Revolute analysis over exact rooted component occurrence identities.

Occurrence-local internal pose overrides, whole-subassembly solve variables, richer joint families, and a general physics engine remain deferred.
