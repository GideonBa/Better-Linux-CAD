# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. BLCAD owns model intent and uses OCCT/Open CASCADE as the computed geometry kernel.

Persistent or explicitly authored model intent already includes substantial parts of:

- typed parameters and unit-aware expressions;
- sketches, construction geometry, and constraints;
- parametric features and feature history;
- semantic generated geometry references;
- dependency, invalidation, and recompute intent;
- assembly parameters and bindings;
- part component occurrences and direct rigid placement;
- local geometric assembly constraints;
- Revolute joint limits and coordinates;
- project-owned child assemblies and rigid subassembly occurrences.

Derived data such as OCCT shapes, solved residuals, Jacobians, hierarchy traversal descriptors, posed leaf shapes, interference records, and STEP compounds remains regenerable cache/query output rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices. The historical phase order was:

1. single-part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader sketch/profile/feature support;
5. sketch constraints and diagnostics;
6. assembly relationships, motion, hierarchy, and posed geometry;
7. GUI/application workflows;
8. engineering modules.

Phases 1-6 now have implemented seeds. The current work is still centered on the CAD core and headless application boundary rather than GUI breadth.

The development rule is:

```text
model intent
  -> serialization compatibility
  -> deterministic derived semantics
  -> geometry/numeric execution
  -> explicit application
  -> diagnostics/consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology support is added. When one feature crosses several authority boundaries, it is split into separate implementation blocks instead of being delivered as one large solver or format rewrite.

## Current technical phase

The active assembly phase has already implemented local geometric solving, first joint motion, rigid hierarchy traversal, nested posed export, interference/clearance analysis, and document-scoped flexible child solving.

The current next direction is cross-hierarchy geometric constraint solving. The key architecture distinction is:

```text
geometric occurrence identity
!=
persisted transform authority identity
```

For example, two rooted occurrences of the same child assembly may expose two geometrically distinct shaft endpoints because their parent transforms differ, while both endpoints still read and eventually mutate one shared child-document `ComponentInstance::transform()` record.

The canonical implementation sequence for this phase is `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

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
- technical drawings and bills of materials;
- later CAM or FEM coupling.

The aim is a modern engineering-oriented CAD system for Linux that combines classic parametric CAD concepts with explicit semantic model intent, deterministic headless behavior, and strong technical assistants.

## Non-goals for the current phase

The current phase should not attempt to deliver:

- a production GUI before the core command/application contracts stabilize;
- a full general topological naming solution;
- arbitrary raw OCCT face/edge picking as persistent identity;
- a general-purpose symbolic or physics constraint engine;
- occurrence-local flexible child poses before a second persistent pose authority is deliberately designed;
- whole-subassembly rigid solve variables before grounding and application semantics are defined;
- cross-hierarchy joints before geometric cross-hierarchy solve connectivity is stable;
- full contact dynamics, collision response, or rigid-body physics;
- production engineering assistants before underlying model intent is reliable.

These are sequencing boundaries, not permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/architecture-summary.md` summarizes implemented architecture.

Feature-specific MVP documents are canonical for exact contracts, persistence boundaries, mathematical semantics, failure policies, and focused proofs.

`docs/file-format.md` is canonical for currently implemented serialization semantics. Planned JSON shapes do not become save-format authority until their serialization block is implemented and documented there.
