# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. BLCAD owns model intent and uses OCCT/Open CASCADE as the computed geometry and exchange implementation layer.

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

Derived data such as OCCT shapes, hierarchy traversal, transform-authority mappings, typed target source classifications/descriptors/capabilities, residuals/Jacobians, solve/motion results, freshness snapshots, rooted exchange records, posed shapes, contact classifications, sampled motion products, XDE labels, and STEP products/entities remains regenerable query/execution output rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices. Historical phase order:

1. single-part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader sketch/profile/feature support;
5. sketch constraints and diagnostics;
6. assembly relationships, motion, hierarchy, posed geometry, analysis, structured exchange, and general target architecture;
7. GUI/application workflows;
8. engineering modules.

Phases 1–6 have implemented seeds. Current work remains centered on CAD-core and headless application contracts rather than GUI breadth.

Development rule:

```text
model intent
  -> serialization compatibility
  -> stable semantic source identity
  -> deterministic derived connectivity/query semantics
  -> typed geometry and capability projection
  -> geometry/numeric execution
  -> complete freshness / explicit application when mutation is required
  -> diagnostics, analysis, or exchange consumers
```

A feature should be narrow enough to prove through focused headless tests before UI or broad topology support is added. When one feature crosses several authority boundaries, it is split into sequenced implementation blocks instead of becoming one large solver, hierarchy, format, target, or motion rewrite.

## Current technical phase

The active assembly phase has implemented:

```text
local five-family geometric solving + diagnostics
local Revolute motion
rigid nested assembly hierarchy
flattened posed STEP export
interference + clearance compatibility analysis
document-scoped flexible child solving
cross-hierarchy five-family solving + fresh application + diagnostics
Project-level occurrence-qualified Revolute motion
structured assembly/product STEP export
complete rooted Separated / Touching / Interfering classification
bounded sampled local/cross-hierarchy Revolute sweep
typed assembly geometric target taxonomy and capability projection
```

Several identity/authority questions are deliberately separated.

### Semantic endpoint identity

```text
local
  (local ComponentInstanceId,
   semantic_reference)

cross-hierarchy
  (occurrence_path,
   local ComponentInstanceId,
   semantic_reference)
```

### Persisted transform authority

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

### Resolved source classification and capability

```text
semantic endpoint
  -> derived AssemblyGeometricTargetSourceKind
  -> typed descriptor
  -> canonical capability set
```

Source kind is not persistent endpoint identity and capability is not source identity.

### Structured exchange component occurrence

```text
(containing rooted assembly path,
 local ComponentInstanceId)
```

### Static contact pair identity

```text
canonical ordered pair of exact rooted
component exchange occurrence identities
```

Two rooted occurrences of the same child assembly can be geometrically, exchange-, and contact-distinct because their parent boundaries differ while still reading and mutating one shared child-document `ComponentInstance::transform()` authority.

This prevents numeric variable duplication and occurrence collapse.

## General target architecture progress

Block 31 establishes the first generic assembly target Geometry boundary.

Source kinds:

```text
GeneratedPlanarFace
GeneratedCylindricalFace
GeneratedLinearEdge
GeneratedCircularEdge
GeneratedVertex
DatumPlane
DatumAxis
ConstructionLine
ConstructionPoint
CircularFeatureSeat
```

Capabilities:

```text
Plane
Axis
Line
Point
Circle
Cylinder
Frame
```

One resolved target retains exact endpoint identity, source classification, derived source metadata, one typed descriptor variant, canonical capabilities, coordinate space, and exact current transform context.

Consumers project geometry explicitly through `project_plane`, `project_axis`, `project_line`, `project_point`, `project_circle`, `project_cylinder`, or `project_frame`.

Current `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` strings remain unchanged. Existing Mate/Distance/Angle/Concentric/Insert/Revolute consumers now receive legacy descriptor shapes adapted from the typed projection boundary, so target expressiveness can grow without duplicating equation or numeric machinery.

Canonical implemented target contract: `docs/assembly-geometric-target-taxonomy-mvp5.md`.

Blocks 32–33 established assembly-selectable reference geometry Core intent and serialization: first-class DatumAxis PartDocument intent, the unambiguous `ref:` semantic source grammar, and additive `datum_axes` JSON with byte-for-byte endpoint spelling roundtrips. The next authority step is Block 34: Geometry resolution of `ref:` sources into Block-31 capabilities.

Canonical sequence: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

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
- a second transform or occurrence-local pose authority without explicit persistence/application design;
- whole-subassembly rigid solve variables before grounding and application semantics exist;
- generic relationship families before target capability compatibility exists;
- richer joint families before typed coordinate slots and vector-drive semantics exist;
- a general-purpose physics constraint engine;
- full contact dynamics, collision response, friction, or rigid-body simulation;
- continuous collision detection unless a continuous algorithm is implemented and proved;
- production engineering assistants before underlying model intent is reliable.

These are sequencing boundaries, not permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/architecture-summary.md` summarizes implemented architecture.

Feature-specific MVP documents are canonical for exact contracts, persistence boundaries, mathematical semantics, failure policies, and focused proofs.

`docs/assembly-geometric-target-taxonomy-mvp5.md` is canonical for implemented Block-31 source taxonomy, typed descriptors, capability projection, and current target-family migration.

`docs/assembly-general-geometric-target-roadmap.md` is canonical for planned Blocks 32–47.

`docs/file-format.md` is canonical for implemented serialization. Derived target source kinds/descriptors/capabilities/projections are not save-format fields.
