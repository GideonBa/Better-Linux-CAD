# Document-Scoped Flexible Subassembly Local Solving MVP-5

[Blocks 192–193](inventor-parity-sequence.md) extend the authority with the independent
occurrence API described below. The historical document-local/shared-child sections retain
their existing behavior. Numbered acceptance and delivery status live in [the MVP plan](mvp-plan.md).

Status: implemented.

This document is canonical for document-scoped flexible-child solving. The implementation solves one shared child `AssemblyDocument` in its own local assembly space; it does not introduce occurrence-local internal pose overrides.

## Independent rooted occurrence authority — 192–193

`AssemblyOccurrencePose` persists an exact root-to-child `SubassemblyInstanceId` path,
expected child `DocumentId`, explicit `Rigid`/`Flexible` policy, sparse component-local rigid
transforms and role-addressed local joint coordinates. A missing record uses the historical
shared definition pose. A rigid record cannot contain overrides. No composed world transform,
solved residual or geometry cache is stored. Stable IDs survive label changes; changing a path
or referenced child requires explicit repair/removal. Duplicate component/joint roles, nonfinite
transforms, orphan records and moved grounded components reject before publication.

`Project::effective_component_transform` is the common local-pose authority used by leaf
resolution and hierarchy geometric targets. Viewport, physical properties, contact, measurement
and flat/structured STEP consume those same rooted leaves/targets. Physical participation keeps
hidden active components; suppression excludes them. Parent occurrence boundary transforms remain
independent authored definition placement and compose in their existing inner-to-outer order.
Nested flexible children use their own full rooted paths, so repeated parents do not alias them.

`ComponentTransformAuthority`, local geometric relationships and local joint identities carry an
optional rooted path. Empty retains shared definition authority; a flexible path identifies one
independent component variable. Cross-hierarchy graphs and numeric solve/application read and write
these identities through the same effective pose boundary. Local constraints/joints evaluate in a
disposable child-root view with the selected occurrence's transforms and joint coordinates.
Grounding, semantic source freshness, stale proposal rejection and atomic application are retained.
Named child Assembly states supply their actual component selections and suppression to hierarchy
queries and scoped relationships. Direct queries reject suppressed occurrence/component targets.
State/pose Projects conservatively snapshot complete canonical Project context for cross-hierarchy
solve/motion application; changing a selected state, dependency or unrelated authored Project field
invalidates that pending extended result. Legacy Projects retain their focused historical checks.

`AssemblyOccurrenceSolver::solve/move/apply` exposes independent local solving and typed joint
motion. Applying stores only the selected occurrence's transforms/coordinate values on a Project
candidate. The complete canonical Project snapshot plus ordinary local solver/source checks guards
application; an unrelated authored Project change conservatively invalidates a pending local
occurrence result. Shared child JSON and other occurrence overrides remain unchanged. The earlier
`AssemblyFlexibleSubassemblySolver` API continues to update the shared child definition explicitly.

With directed references or coordinated machining, every solver first consumes a normalized
complete Project. Ordinary and shared-child results retain the complete extended authority context,
including equal-geometry source rebinding and position/view catalogs; Motion embeds the same
ordinary result. The shared-child wrapper additionally snapshots its parent Project. A plain
legacy Project keeps the original selective Part/component freshness boundary. Independent,
shared-child, root and cross-hierarchy appliers validate a disposable complete output before
publication; a locally valid free pose that makes a downstream bore miss its Part is rejected with
all prior definitions and poses retained. Nested evaluator-internal applies use the current candidate
without recursive normalization. The Geometry regressions are `[assembly-context-freshness]`,
`[assembly-hole-normalization]` and `[legacy-flex-normalization]` in the existing adaptive and
Assembly Hole test sources; execution and numbered gates remain MVP-owned.

Project JSON selects necessary version 3 only while occurrence-pose records exist. Each record
uses strict version 1 dispatch and exact fields; older envelopes cannot carry the extension. Removing
all records restores the historical envelope when no other extension requires it. Joint values use
explicit `mm`/`deg` units and typed role validation against the child joint's existing limits.

Focused proof lives in [Core occurrence tests](../tests/core/assembly_occurrence_pose_tests.cpp)
and [Geometry occurrence tests](../tests/geometry/assembly_occurrence_pose_tests.cpp), tagged
`[occurrence-pose]`. The physical fixture drives one shared hollow-cylinder child to two independent
prismatic extensions under rotated/translated parents. It checks immutable child JSON, exact reopen,
stale rollback, actual material points, volume/centroid, contact, hidden physical participation,
flat and structured STEP reimport, and separate cross-hierarchy numeric authorities. Native authoring,
named positional representations and context controls remain the separate Block 198 gate.

## Scope

Public adapters:

```text
AssemblyFlexibleSubassemblySolver
AssemblyFlexibleSubassemblySolveResultApplier
```

A caller selects one exact active non-root subassembly occurrence path.

The selected path identifies the referenced Project-owned child `AssemblyDocument` whose internal component transforms are persistent solve authority.

## Exact occurrence selection

The solve API accepts:

```text
occurrence_path = [SubassemblyInstanceId, ...]
```

The path is an exact root-to-current rooted occurrence sequence.

The empty root path, missing paths, and suppressed paths are rejected.

Visibility is not the flexible-solve participation boundary; suppression is.

The selected `SubassemblyInstance` boundary identifies the child document. Its rigid boundary transform is not a numeric variable.

## Child-as-local-root solve view

After exact occurrence selection, the solver creates:

```text
selected child AssemblyDocument -> temporary root assembly
all Project-owned PartDocument records -> copied into temporary Project
```

The ordinary `AssemblyRigidBodySolver` solves the requested local connected component.

This reuses:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
local equation builders
shared residual flattening
shared central finite differences
shared numeric solve engine
AssemblySolveResult snapshots/proposals
```

There is no flexible-child-specific optimizer or residual implementation.

## Result contract

`AssemblyFlexibleSubassemblySolveResult` stores:

```text
selected occurrence_path
selected child assembly_document identity
ordinary local AssemblySolveResult
```

`converged()` delegates to the local result.

The wrapper preserves the hierarchy identity needed to reject application when the selected occurrence no longer addresses the same child document.

## Explicit application

`AssemblyFlexibleSubassemblySolveResultApplier`:

1. re-resolves the exact current occurrence path;
2. requires the same child `AssemblyDocument` identity;
3. rebuilds a fresh child-as-local-root Project view;
4. delegates complete local solve-result freshness to `AssemblySolveResultApplier`;
5. applies the validated local proposals on the temporary view;
6. creates a source Project copy;
7. writes the solved direct child component transforms into the addressed Project-owned child document;
8. replaces the source Project only after every write succeeds.

The selected and ancestor `SubassemblyInstance::transform()` values remain unchanged.

No composed hierarchy transform is written back.

## Inherited semantic target PartDocument freshness

Block 27 extends every ordinary `AssemblySolveResult` with exact canonical semantic target PartDocument snapshots.

The flexible-child result embeds that ordinary result unchanged.

Therefore flexible-child application now also requires every PartDocument referenced by the complete selected local component group to reproduce the exact canonical:

```text
serialize_part_document_to_json(part)
```

payload captured at solve time.

A participating PartDocument parameter, formula, workplane, sketch/profile, reference-recovery/remap, or feature-history model-intent edit invalidates the result before any child transform is written back.

This uses the same local `AssemblySolveResultApplier` freshness boundary rather than a flexible-child-specific revision mechanism.

The snapshot is conservative and derived/unpersisted.

## Repeated child occurrence semantics

Suppose:

```text
subassembly.left  -> assembly.gearbox
subassembly.right -> assembly.gearbox

assembly.gearbox
  component.shaft
```

There is one persistent direct child-component transform authority:

```text
(assembly.gearbox, component.shaft)
```

A successful flexible-child solve updates that direct transform once.

Both rooted occurrences observe the updated internal pose through their independent rigid boundaries:

```text
left:
  [T_shaft_updated, T_left]

right:
  [T_shaft_updated, T_right]
```

This is document-scoped flexible solving, not:

```text
([left],  component.shaft) -> independent internal transform
([right], component.shaft) -> independent internal transform
```

Occurrence-local internal pose overrides remain deferred.

## Relationship to cross-hierarchy transform authority

Blocks 25-27 formalize the same shared-child ownership rule through:

```text
ComponentTransformAuthority =
  (assembly_document,
   local ComponentInstanceId)
```

Block 25 uses it for solve connectivity.

Block 26 uses it for numeric variable ownership and proposal identity.

Block 27 uses it for complete authority freshness and atomic application.

Repeated child endpoints can retain distinct occurrence paths and parent chains while sharing one candidate direct transform, one six-variable block, one authority snapshot, and one transform proposal.

## Leaf flattening and posed consumers

`AssemblyLeafOccurrenceResolver` remains the canonical hierarchy-to-leaf boundary for posed geometry consumers.

After successful application, repeated child occurrences retain distinct occurrence paths and parent transform chains while reading the updated shared child component transform.

Posed STEP export and posed analysis therefore observe the updated internal pose without hierarchy cache persistence.

## Persistence boundary

Flexible-child solving introduces no JSON field.

Persistent changes after explicit successful application are direct child `ComponentInstance::transform()` values already owned by the child `AssemblyDocument`.

Derived and unpersisted data includes:

```text
selected occurrence resolution
child-as-local-root Project view
ordinary local numeric solve state
local component snapshots
AssemblySemanticTargetPartSnapshot values
canonical PartDocument freshness payloads
local transform proposals
wrapper result context
```

No flexible/rigid mode flag, occurrence-local transform override, composed transform cache, or solver coordinate is persisted.

## Failure policy

The flexible solve/apply path fails closed on:

- invalid Project structure;
- empty/root occurrence path;
- missing occurrence path;
- suppressed occurrence path;
- selected child identity mismatch at application;
- ordinary local solver failures;
- non-converged results;
- stale local component referenced-part/transform/grounding/suppression inputs;
- changed participating PartDocument canonical model intent;
- duplicate or invalid ordinary proposals;
- invalid proposed direct transforms.

The source Project remains unchanged on failed application.

## Focused coverage

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-flexible-subassembly]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-semantic-freshness]"
```

The flexible-child suite proves deterministic child-local solving, source immutability before apply, atomic child-transform application, unchanged subassembly boundary transforms, leaf-resolver visibility of the applied child pose, repeated child occurrences sharing one solved pose while retaining distinct paths, invalid/suppressed path rejection, and stale child-transform rejection.

The shared semantic freshness suite proves that ordinary local results now reject changed participating PartDocument model intent. Flexible-child application inherits that same applier contract.

## Cross-hierarchy follow-up status

The geometric hierarchy sequence is complete through Block 27:

```text
Block 22  read-only occurrence-qualified target/residual semantics
Block 23  persistent Core endpoint + Project-level relationship intent
Block 24  additive Project JSON + exact endpoint structure validation
Block 25  relationship-to-ComponentTransformAuthority incidence + solve groups
Block 26  authority-scoped mixed local/root-space numeric solving
Block 27  complete modeled-input freshness + atomic authority application + diagnostics
```

Canonical Block-27 details are in `docs/assembly-cross-hierarchy-application-diagnostics-mvp5.md`.

## Explicitly deferred

- occurrence-local internal child pose overrides;
- whole-subassembly grounding and solve variables;
- `SubassemblyInstance` transform proposals;
- component geometry instancing;
- structured STEP product hierarchy;
- collision/contact response or physics.

## Current handoff

Next is Block 28 from `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: persistent occurrence-qualified cross-hierarchy joint intent and nested motion propagation through the frozen endpoint/occurrence/transform-authority identities and the shared numeric/fresh-application boundaries.
