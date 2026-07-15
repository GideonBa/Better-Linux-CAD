# Structured Assembly Exchange Graph and STEP Products MVP-5

Status: implemented as Block 29 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for derived assembly/component exchange identity, deterministic structured exchange graph construction, shared part-shape definition recompute, and XDE-backed structured STEP assembly export.

## Scope

Implemented:

- exact rooted assembly occurrence exchange identity;
- exact rooted component occurrence exchange identity;
- shared part product definition identity by `PartDocumentId`;
- deterministic visible-active structured exchange graph derivation;
- collision-free deterministic generated exchange names;
- one part recompute and one private `ShapeCache` per unique exported `PartDocumentId`;
- one shared OCCT rigid-transform conversion contract;
- XDE assembly/product labels and component references;
- nested and repeated assembly occurrence products;
- repeated component occurrences referencing shared part product definitions;
- AP214 STEPCAF transfer with names for exported product labels and writer-supported occurrence labels;
- source Project immutability;
- retained flattened-compound `AssemblyStepExporter` compatibility path;
- `blcad_export_structured_assembly` headless consumer.

Not implemented:

- new persistent exchange/product records;
- occurrence-local flexible child pose overrides;
- whole-`SubassemblyInstance` solve variables;
- richer joint families or multi-turn motion;
- collision response or contact solving;
- swept-motion contact analysis.

## Derived exchange identities

### Assembly occurrence

```text
AssemblyExchangeAssemblyOccurrenceIdentity
  = exact rooted SubassemblyInstance occurrence path
```

The explicit root assembly occurrence is:

```text
[]
```

Repeated occurrences of one child `AssemblyDocument` remain different values:

```text
[subassembly.left]
[subassembly.right]
```

The `AssemblyDocumentId` alone is not an assembly occurrence identity.

### Component occurrence

```text
AssemblyExchangeComponentOccurrenceIdentity
  = (containing assembly occurrence path,
     local ComponentInstanceId)
```

Thus:

```text
([subassembly.left],  component.shaft)
([subassembly.right], component.shaft)
```

are different exchange occurrences even when both read the same shared child-document component record and transform authority.

### Part product definition

```text
AssemblyExchangePartProductDefinitionIdentity
  = referenced PartDocumentId
```

Repeated component occurrences may reference the same part definition:

```text
(root, component.a) -> part.plate
(root, component.b) -> part.plate

one part product definition:
  part.plate
```

No `PartDocument` model intent is copied per occurrence.

## Persistence boundary

All three exchange identities and all graph records are derived.

BLCAD persists existing model intent:

```text
AssemblyDocument records
SubassemblyInstance references/state/transforms
ComponentInstance references/state/transforms
PartDocument model intent
constraints and joints
```

BLCAD does not persist:

```text
AssemblyExchangeGraph
exchange occurrence records
exchange parent links
exchange generated names
OCCT XDE documents
TDF_Label values
XCAF component references
STEP entity ids
part shape definition caches
structured export summaries
```

Block 29 adds no JSON field and does not change the historical project schema/version marker.

## Export inclusion policy

`AssemblyExchangeGraph` does not implement a second visibility/suppression traversal.

It first obtains:

```text
AssemblyLeafOccurrenceResolver::resolve(project)
```

The canonical leaf resolver already excludes:

```text
hidden component occurrences
suppressed component occurrences
components below hidden SubassemblyInstance paths
components below suppressed SubassemblyInstance paths
```

The structured graph then retains:

```text
explicit root assembly occurrence
+
every rooted assembly path prefix required by one exported leaf
```

Example:

```text
exported leaf path = [outer, inner]
```

retains:

```text
[]
[outer]
[outer, inner]
```

A hidden/suppressed empty branch with no exported leaf does not create a structured export assembly occurrence.

This keeps structured STEP inclusion identical to posed-leaf consumers.

## Deterministic ordering

Assembly occurrence order is lexicographic by exact `SubassemblyInstanceId` path sequence.

The empty root path sorts first.

Example:

```text
[]
[left]
[left, inner]
[right]
[right, inner]
```

Component occurrence order is:

```text
containing assembly occurrence path
then local ComponentInstanceId
```

Part product definitions sort lexicographically by `PartDocumentId`.

No order depends on insertion order, OCCT topology order, TDF label tags, or STEP entity numbers.

## Collision-free generated exchange names

Generated names are presentation/exchange metadata, not identity authority.

Prefixes:

```text
blcad:assembly-occurrence:
blcad:component-occurrence:
blcad:part-definition:
```

The root path text is:

```text
root
```

Each authored ID segment is encoded byte-for-byte from UTF-8. ASCII:

```text
A-Z a-z 0-9 . _ -
```

remains unchanged. Every other byte becomes uppercase:

```text
%HH
```

Thus:

```text
SubassemblyInstanceId("a/b") -> a%2Fb
ComponentInstanceId("component%root") -> component%25root
PartDocumentId("part/a%") -> part%2Fa%25
```

Path separators remain literal `/` between encoded segments, so:

```text
["a/b"] -> a%2Fb
["a", "b"] -> a/b
```

cannot collide.

The explicit root sentinel is reserved. A non-root one-segment path whose encoded text is exactly `root` is written as:

```text
%72oot
```

Percent itself is encoded for authored IDs, so this reserved spelling remains collision-free.

Representative names:

```text
blcad:assembly-occurrence:root
blcad:assembly-occurrence:subassembly.left/subassembly.inner

blcad:component-occurrence:root/component.plate.a
blcad:component-occurrence:subassembly.left/component.shaft

blcad:part-definition:part.plate
```

## Exchange graph records

One `AssemblyExchangeAssemblyOccurrence` stores:

```text
identity
assembly_document
optional parent identity
optional via SubassemblyInstanceId
direct transform_from_parent
product_name
```

For a non-root occurrence, `transform_from_parent` is taken from:

```text
AssemblyHierarchyOccurrenceDescriptor
  .parent_transforms_inner_to_outer.front()
```

The hierarchy traversal prepends the current direct `SubassemblyInstance::transform()` at every descent. Therefore the first parent-chain transform is exactly the current occurrence boundary.

The root uses identity transform and has no parent/via occurrence.

One `AssemblyExchangeComponentOccurrence` stores:

```text
identity
containing AssemblyDocumentId
part product definition identity
complete transforms_inner_to_outer
occurrence_name
```

The complete transform chain is copied verbatim from `AssemblyLeafOccurrenceDescriptor`.

The exchange graph never recomposes or decomposes Euler rotations.

## Shared OCCT rigid-transform conversion

Block 29 extracts one internal conversion:

```text
to_occt_rigid_transform(RigidTransform)
to_occt_location(RigidTransform)
```

It preserves established BLCAD transform semantics:

```text
active fixed-axis X
then Y
then Z
then translation
```

For OCCT multiplication:

```text
translation * Rz * Ry * Rx
```

The existing posed-leaf builder now uses the same conversion helper.

The structured exporter uses only direct local placements:

```text
part component placement
  = component occurrence transforms_inner_to_outer.front()

child assembly occurrence placement
  = exchange assembly occurrence transform_from_parent
```

Nested placement composition is represented by the XDE assembly/product reference graph. The structured exporter does not compose a root-space transform chain itself.

## Shared part shape definitions

`AssemblyPartShapeDefinitionBuilder` is the single Block-29 part-definition recompute boundary.

Input:

```text
Project
requested referenced PartDocumentId values
```

Processing:

```text
sort PartDocumentId
-> deduplicate
-> resolve project-owned PartDocument
-> create one private ShapeCache
-> GeometryRecomputeExecutor::execute_document
-> require a Block-53-compatible single Solid final shape
-> copy unposed TopoDS_Shape definition
```

Output definitions remain sorted by `PartDocumentId`.

The existing `AssemblyPosedLeafShapeBuilder` now delegates to this helper. Structured STEP uses the same helper.

Therefore repeated component occurrences of one part cause:

```text
one PartDocument recompute
one private ShapeCache
one unposed TopoDS_Shape definition
```

not one recompute per occurrence.

## Structured XDE/STEP model

Public consumer:

```text
AssemblyStructuredStepExporter
```

The exporter:

1. builds `AssemblyExchangeGraph`;
2. rejects an empty exported component set;
3. builds the unique part shape definitions;
4. creates an XDE document and shape tool;
5. adds one part definition label per part product definition;
6. creates one assembly label per rooted assembly occurrence;
7. adds each part component occurrence as a reference to its shared part definition with its direct local component placement;
8. adds each non-root assembly occurrence as a reference from its exact parent occurrence with its direct `SubassemblyInstance` boundary placement;
9. updates XDE assembly compounds;
10. transfers the explicit root assembly label through `STEPCAFControl_Writer` with name mode enabled;
11. writes AP214 STEP and verifies a non-empty output file.

The root product graph therefore distinguishes:

```text
root assembly occurrence
nested assembly occurrence products
repeated child assembly occurrences
repeated component occurrences
shared part product definitions
```

The product/reference graph is derived from BLCAD identity. It is never derived from raw OCCT topology ids.

## STEPCAF component-instance name boundary

BLCAD assigns deterministic `TDataStd_Name` values to XDE part-definition labels, assembly occurrence product labels, and component reference labels before transfer.

The exact BLCAD component occurrence name remains authoritative in `AssemblyExchangeGraph`:

```text
blcad:component-occurrence:<exact rooted path>/<local component id>
```

However, OCCT's `STEPCAFControl_Writer` documents a current writer limitation: names and validation properties are supported for top-level shapes only. Nested XDE component-reference label names are therefore not guaranteed to appear verbatim in the written STEP Part 21 text.

Consequences:

```text
AssemblyExchangeGraph occurrence_name
  -> exact deterministic BLCAD derived name

XDE component label TDataStd_Name
  -> assigned before transfer

written STEP nested component instance name
  -> writer-dependent; not a Block-29 identity guarantee
```

Structured STEP correctness for nested components is instead proven through:

```text
exact AssemblyExchangeGraph occurrence identities/names
nested assembly product names
shared part-definition name
complete STEP NEXT_ASSEMBLY_USAGE_OCCURRENCE relationships
STEP re-imported posed geometry equivalence
```

Do not treat raw STEP text presence of every nested component occurrence name as BLCAD model or exchange identity authority.

## Compatibility flattened export

`AssemblyStepExporter` remains available.

Its `build_posed_shape()` and `write_step()` continue producing one flattened posed OCCT compound.

That path now reuses:

```text
AssemblyPartShapeDefinitionBuilder
shared OCCT rigid-transform conversion
canonical AssemblyLeafOccurrenceResolver transform chains
```

This keeps the flattened compatibility consumer and structured product consumer on the same part recompute and transform conventions.

## Headless consumer

```text
blcad_export_structured_assembly <input.blcad.project.json> <output.step>
```

The command reads and validates current persisted Project intent and exports that authored/current pose.

It does not implicitly run local solving, cross-hierarchy solving, or joint motion. Solver policy remains separate from exchange policy.

## Failure policy

Structured export fails closed on:

- empty output path;
- invalid Project/hierarchy structure;
- no visible-active exported component occurrence;
- exchange path that cannot resolve back to the canonical hierarchy;
- malformed derived non-root boundary context;
- malformed derived component transform-chain context;
- missing referenced project-owned PartDocument;
- part recompute failure;
- missing final part shape;
- missing exchange-to-XDE label mapping;
- XDE part/assembly/component label creation failure;
- STEPCAF transfer failure;
- STEP write failure;
- unreadable or empty written file;
- OCCT exceptions.

The source Project is read-only and remains unchanged on every success or failure path.

## Focused coverage

Core:

```bash
./build/dev/blcad_core_tests "[core][assembly-exchange-graph]"
```

Geometry:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-structured-step-export]"
```

The suites prove:

- explicit root exchange occurrence;
- repeated root component occurrences;
- one shared part definition for repeated part use;
- repeated child assembly occurrences remain distinct rooted paths;
- nested hierarchy parent relationships;
- deterministic assembly/component/part ordering;
- exact leaf transform-chain reuse;
- direct hierarchy boundary extraction;
- hidden/suppressed export filtering inherited from the leaf resolver;
- source Project model-intent immutability;
- collision-free percent-encoded generated names including slash, percent, and reserved-root cases;
- exact nested component occurrence names at the derived `AssemblyExchangeGraph` layer;
- structured STEP text contains exported assembly product names, shared part-definition names, writer-supported top-level component names, and assembly usage relationships;
- nested STEP usage relationships remain complete without assuming unsupported verbatim nested component-reference names;
- structured STEP can be read back through `STEPControl_Reader`;
- structured and flattened exporters produce equivalent posed solid count, volume, and bounds for repeated root and nested assembly fixtures;
- empty output path and empty exported assembly fail closed.

The `STEPControl_Reader` use above is export verification only; it is not a user-facing STEP
importer and creates no BLCAD Part/Assembly intent. Production Reference, EditableBody, and
structured Assembly import is planned separately in Blocks 132–138 of`docs/step-import-sequence-mvp10.md`.

## Next technical step

Implement Block 30 only: richer posed contact classification and swept-motion analysis over the now-frozen rooted exchange/occurrence identities.

Block 30 should extend the existing posed leaf/interference analysis boundary rather than introducing a physics engine or mutating solver state.

Occurrence-local child pose overrides, whole-subassembly solve variables, richer joint families, and general dynamic simulation remain deferred.
