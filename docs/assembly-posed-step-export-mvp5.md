# Posed Assembly STEP Export MVP-5

Status: implemented deterministic flattened posed assembly STEP export over the canonical visible-active leaf boundary. Block 29 adds a separate structured XDE/STEP product consumer while retaining this compatibility path.

## Scope

`AssemblyStepExporter` is a derived geometry consumer. It does not own placement, relationship, motion, or exchange identity authority.

Current flattened pipeline:

```text
const Project
  -> validate complete assembly structure
  -> AssemblyHierarchyTraversal
  -> AssemblyLeafOccurrenceResolver
  -> visible-active flattened part leaves
  -> collect referenced PartDocumentId values
  -> AssemblyPartShapeDefinitionBuilder
  -> one recompute + one private ShapeCache per unique part
  -> copy each leaf's shared unposed final part shape definition
  -> pose through the exact leaf transform chain
  -> add posed shape to one OCCT compound
  -> StepExporter
  -> STEP file
```

All export geometry is derived and unpersisted.

## Canonical leaf boundary

`AssemblyStepExporter` consumes `AssemblyLeafOccurrenceResolver` rather than implementing hierarchy traversal or visibility/suppression policy.

One visible-active leaf descriptor supplies:

```text
assembly_document
subassembly_path
component_instance
referenced_part_document
transforms_inner_to_outer
```

Root component:

```text
[T_component]
```

Nested component:

```text
[T_component, T_inner_parent, ..., T_outer_parent]
```

The exporter follows the exact authored order.

## Shared transform semantics

Every `RigidTransform` level keeps:

```text
translation: millimeters
rotation: degrees
active right-handed fixed-axis rotation
X then Y then Z
then translation
```

Block 29 extracts the common internal conversion:

```text
to_occt_rigid_transform(RigidTransform)
to_occt_location(RigidTransform)
```

OCCT composition remains:

```text
translation * Rz * Ry * Rx
```

The flattened posed-leaf builder composes the exact canonical inner-to-outer leaf chain through this helper and performs one OCCT shape transformation per leaf.

The composed transform is derived and is never persisted or converted back into a BLCAD Euler triple.

## Shared part recompute and definition reuse

Block 29 extracts:

```text
AssemblyPartShapeDefinitionBuilder
```

The builder:

```text
sort PartDocumentId
-> deduplicate
-> resolve project-owned part
-> create one private ShapeCache
-> recompute exactly once
-> require a Block-53-compatible single Solid final shape
-> return one unposed TopoDS_Shape definition
```

`AssemblyPosedLeafShapeBuilder` now delegates to this helper.

Repeated component occurrences and repeated child assembly occurrences reuse the same unposed part definition before independent leaf posing.

Example:

```text
subassembly.left  -> child/component.plate -> part.plate
subassembly.right -> child/component.plate -> part.plate
```

causes:

```text
1 PartDocument recompute
1 private ShapeCache
1 unposed part shape definition
2 posed leaf shape copies
```

## Visibility and suppression

The exporter inherits the leaf resolver policy:

```text
visible subassembly path
&& active subassembly path
&& local component visible
&& local component active
```

A hidden or suppressed subassembly occurrence removes its complete descendant subtree.

A hidden or suppressed local component is absent from export.

The exporter does not mutate filtered records.

## Output contract

`AssemblyStepExporter` remains the flattened compatibility contract:

```text
all visible-active posed leaves
  -> one OCCT compound
  -> generic STEPControl writer
```

It intentionally does not preserve assembly/product references in the written STEP file.

Block 29 separately implements:

```text
AssemblyStructuredStepExporter
```

through `docs/assembly-structured-step-products-mvp5.md`.

The two consumers share leaf selection, unique part recompute, and rigid-transform conversion but have different exchange structure:

```text
AssemblyStepExporter
  -> flattened compound

AssemblyStructuredStepExporter
  -> XDE assembly/component references
  -> shared part product definitions
  -> structured STEP product graph
```

## Failure policy

Flattened export fails closed on:

- invalid project assembly structure or hierarchy cycles;
- missing project-owned child assembly references;
- unresolved visible-active leaf part documents;
- part recompute failures;
- recompute without a final shape;
- transform/shape posing failures;
- empty visible-active flattened output;
- OCCT compound/export transfer failures;
- STEP file-write failure.

No model mutation occurs because the export path is read-only.

## Headless orchestration

Flattened compatibility flow:

```bash
./build/dev-geometry/blcad_export_posed_assembly \
  examples/nested_subassembly.blcad.project.json \
  build/nested_subassembly.step
```

The CLI may solve/apply one supported root-local group before flattened export. That policy is outside `AssemblyStepExporter`.

Structured authored/current-pose flow:

```text
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
```

The structured command does not implicitly run a solver.

## Shared posed-geometry consumer boundary

Interference/clearance analysis and flattened STEP export reuse `AssemblyPosedLeafShapeBuilder`.

The shared boundary guarantees agreement on:

- project hierarchy validation;
- visible-active leaf selection;
- deterministic leaf identity/order;
- unique referenced-part recompute;
- exact transform-chain posing semantics.

Structured STEP reuses the same leaf resolver and part-definition builder but retains direct local component/assembly placements as product-reference locations instead of flattening the complete chain.

## Proven behavior

Existing flattened suites:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-step-export]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-nested-step-export]"
```

prove repeated parts, repeated child occurrences, hierarchy transform order, hidden/suppressed filtering, one recompute per unique part, solid/volume/bounds behavior, failure policy, and STEP re-import equivalence.

Block-29 structured coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
```

also compares structured STEP re-imported geometry against this flattened compatibility export for repeated root and repeated nested fixtures.

These re-import checks validate exported geometry only. They do not create persistent BLCAD Parts,
features, component instances, or subassemblies. That production import boundary is planned in
Blocks 95–101 of `docs/step-import-sequence-mvp7.md`.

## Persistence boundary

Persisted input authority remains:

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
AssemblyExchangeGraph records
part shape definitions and ShapeCache values
posed leaf shapes
composed OCCT posing transforms
XDE labels and assembly references
OCCT compounds
STEP entity identity
export summaries
```

Export never writes model transforms or relationship/joint state.

## Current handoff

Structured assembly/product STEP relationships are implemented in `docs/assembly-structured-step-products-mvp5.md` as Block 29.

The next technical step is Block 30: richer posed contact classification and swept-motion analysis over the frozen rooted occurrence identities and existing posed-leaf analysis boundary.
