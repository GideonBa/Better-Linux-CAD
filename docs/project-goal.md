# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. BLCAD owns model intent and uses OCCT/Open CASCADE as the computed geometry kernel and exchange implementation layer.

Persistent or explicitly authored model intent already includes substantial parts of:

- typed parameters and unit-aware expressions;
- sketches, construction geometry, and constraints;
- parametric features and feature history;
- semantic generated geometry references;
- dependency, invalidation, and recompute intent;
- assembly parameters and bindings;
- part component occurrences and direct rigid placement;
- local geometric assembly constraints;
- local Revolute joint limits and coordinates;
- Project-owned child assemblies and rigid subassembly occurrences;
- Project-level occurrence-qualified geometric constraints;
- Project-level occurrence-qualified Revolute joints.

Derived data such as OCCT shapes, residuals/Jacobians, hierarchy traversal descriptors, rooted exchange occurrence records, posed leaf shapes, XDE labels, STEP products/entities, and analysis records remains regenerable query/execution output rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices. Historical phase order:

1. single-part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader sketch/profile/feature support;
5. sketch constraints and diagnostics;
6. assembly relationships, motion, hierarchy, posed geometry, and structured exchange;
7. GUI/application workflows;
8. engineering modules.

Phases 1-6 now have implemented seeds. Current work is still centered on the CAD core and headless application boundary rather than GUI breadth.

Development rule:

```text
model intent
  -> serialization compatibility
  -> deterministic derived identity/connectivity
  -> geometry/numeric execution
  -> complete freshness / explicit application when mutation is required
  -> diagnostics, analysis, or exchange consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology support is added. When one feature crosses several authority boundaries, it is split into sequenced implementation blocks instead of becoming one large solver, hierarchy, or format rewrite.

## Current technical phase

The active assembly phase has implemented:

```text
local five-family geometric solving + diagnostics
local Revolute motion
rigid nested assembly hierarchy
flattened posed STEP export
interference + clearance analysis
document-scoped flexible child solving
cross-hierarchy five-family geometric solving + fresh application + diagnostics
Project-level occurrence-qualified Revolute motion
structured assembly/product STEP export
```

Three different identity questions are now deliberately separated.

### Semantic/geometric endpoint identity

```text
(occurrence_path,
 local ComponentInstanceId,
 semantic_reference)
```

### Persisted transform authority identity

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

### Structured exchange occurrence/product identity

```text
assembly occurrence
  = exact rooted SubassemblyInstance path

part component occurrence
  = (containing rooted assembly path,
     local ComponentInstanceId)

part product definition
  = PartDocumentId
```

For example, two rooted occurrences of the same child assembly can expose two geometrically and exchange-distinct shaft occurrences because their parent boundaries differ, while both still read and mutate one shared child-document `ComponentInstance::transform()` authority.

This distinction prevents both numeric variable duplication and exchange occurrence collapse.

Block 29 now emits structured XDE/STEP assembly/product relationships from those derived rooted exchange identities while reusing one recomputed part definition per unique `PartDocumentId`.

The current next direction is richer posed contact classification and bounded deterministic swept-Revolute analysis over exact rooted component occurrence identities. The canonical implementation sequence is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

## Long-term scope

The system should eventually cover:

- parametric part modeling;
- robust sketches with constraints;
- sketches on datum planes and generated geometry;
- stable semantic references and reference recovery;
- feature history and richer feature families;
- assembly modeling with geometric constraints and joints;
- cross-hierarchy assembly solving and nested motion;
- top-down design with assembly/cross-part parameter relationships;
- engineering assistants for bolts, holes, shafts, bearings, and gears;
- standard-parts libraries and a material database;
- structured assembly exchange, STEP import/export, and STL export;
- richer contact/interference/motion analysis;
- technical drawings and bills of materials;
- later CAM or FEM coupling.

The aim is a modern engineering-oriented CAD system for Linux that combines classic parametric CAD concepts with explicit semantic model intent, deterministic headless behavior, and strong technical assistants.

## Non-goals for the current phase

The current phase should not attempt to deliver:

- a production GUI before the core command/application contracts stabilize;
- a full general topological naming solution;
- arbitrary raw OCCT face/edge picking as persistent identity;
- a second transform or occurrence-local pose authority without an explicit persistence/application design;
- whole-subassembly rigid solve variables before grounding and application semantics are defined;
- richer joint families or multi-turn coordinates inside the contact-analysis block;
- a general-purpose physics constraint engine;
- full contact dynamics, collision response, or rigid-body simulation;
- continuous collision detection unless a continuous algorithm is actually implemented and proved;
- production engineering assistants before underlying model intent is reliable.

These are sequencing boundaries, not permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/architecture-summary.md` summarizes implemented architecture.

Feature-specific MVP documents are canonical for exact contracts, persistence boundaries, mathematical semantics, failure policies, and focused proofs.

`docs/file-format.md` is canonical for currently implemented serialization semantics. Derived exchange identities, XDE labels, and STEP product relationships are not save-format fields merely because they are exported.
