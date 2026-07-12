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

Derived data such as OCCT shapes, residuals/Jacobians, hierarchy traversal descriptors, rooted exchange occurrence records, posed leaf shapes, contact classifications, sample Project copies, sampled motion records, XDE labels, and STEP products/entities remains regenerable query/execution output rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices. Historical phase order:

1. single-part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader sketch/profile/feature support;
5. sketch constraints and diagnostics;
6. assembly relationships, motion, hierarchy, posed geometry, analysis, and structured exchange;
7. GUI/application workflows;
8. engineering modules.

Phases 1-6 now have implemented seeds. Current work remains centered on CAD-core and headless application contracts rather than GUI breadth.

Development rule:

```text
model intent
  -> serialization compatibility
  -> stable semantic identity
  -> deterministic derived connectivity/query semantics
  -> geometry/numeric execution
  -> complete freshness / explicit application when mutation is required
  -> diagnostics, analysis, or exchange consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology support is added. When one feature crosses several authority boundaries, it is split into sequenced implementation blocks instead of becoming one large solver, hierarchy, format, or motion rewrite.

## Current technical phase

The active assembly phase has implemented:

```text
local five-family geometric solving + diagnostics
local Revolute motion
rigid nested assembly hierarchy
flattened posed STEP export
interference + clearance compatibility analysis
document-scoped flexible child solving
cross-hierarchy five-family geometric solving + fresh application + diagnostics
Project-level occurrence-qualified Revolute motion
structured assembly/product STEP export
complete rooted Separated / Touching / Interfering contact classification
bounded sampled local/cross-hierarchy Revolute sweep analysis
```

Four different identity questions are deliberately separated.

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

### Structured exchange component occurrence identity

```text
(containing rooted assembly path,
 local ComponentInstanceId)
```

### Static contact pair identity

```text
canonical ordered pair of exact rooted
component exchange occurrence identities
```

Two rooted occurrences of the same child assembly can expose two geometrically, exchange-, and contact-distinct component occurrences because their parent boundaries differ while still reading and mutating one shared child-document `ComponentInstance::transform()` authority.

This prevents both numeric variable duplication and occurrence collapse in exchange/analysis consumers.

Block 29 emits structured XDE/STEP assembly/product relationships from derived rooted exchange identities while reusing one recomputed part definition per unique `PartDocumentId`.

Block 30 classifies every visible-active rooted component pair as `Separated`, `Touching`, or `Interfering` and can sample one supported local or Project-level Revolute coordinate interval. Every sample starts from a fresh source Project copy and reuses existing motion solver/application boundaries before posed contact analysis.

This sweep is deliberately discrete sampled analysis rather than continuous collision detection.

The current next direction is general assembly geometric target expressiveness: typed source taxonomy and capability projection are Block 31, followed by reference geometry, stable semantic generated topology targets, explicit compatibility, generic relationships, multi-coordinate joint infrastructure, and richer joint families through Block 47.

Canonical implementation sequence: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Canonical target/joint roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

## Long-term scope

The system should eventually cover:

- parametric part modeling;
- robust sketches with constraints;
- sketches on datum planes and generated geometry;
- stable semantic references and reference recovery;
- feature history and richer feature families;
- assembly modeling with geometric constraints and joints;
- surface/edge/vertex/plane/axis/line/point assembly target selection through semantic identity;
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

- a production GUI before core command/application contracts stabilize;
- arbitrary raw OCCT face/edge/vertex selection as persistent identity;
- a second transform or occurrence-local pose authority without an explicit persistence/application design;
- whole-subassembly rigid solve variables before grounding and application semantics are defined;
- generic relationship families before target capability compatibility exists;
- richer joint families before typed coordinate slots and vector drive semantics exist;
- a general-purpose physics constraint engine;
- full contact dynamics, collision response, friction, or rigid-body simulation;
- continuous collision detection unless a continuous algorithm is actually implemented and proved;
- production engineering assistants before underlying model intent is reliable.

These are sequencing boundaries, not permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/architecture-summary.md` summarizes implemented architecture.

Feature-specific MVP documents are canonical for exact contracts, persistence boundaries, mathematical semantics, failure policies, and focused proofs.

`docs/assembly-contact-swept-motion-mvp5.md` is canonical for Block-30 contact and sampled motion semantics.

`docs/assembly-general-geometric-target-roadmap.md` is canonical for planned Blocks 31-47.

`docs/file-format.md` is canonical for currently implemented serialization semantics. Derived exchange identities, XDE labels, contact records, and sampled sweep products are not save-format fields merely because they are exported or queried.
