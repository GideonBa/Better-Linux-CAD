# Project Goal

BLCAD is intended to become an independent parametric CAD system for Linux.

The long-term goal is not a thin STEP exporter and not a wrapper around final BRep geometry. BLCAD owns model intent and uses OCCT/Open CASCADE as the computed geometry and exchange implementation layer.

Persistent or explicitly authored model intent already includes substantial parts of:

- typed parameters and unit-aware expressions;
- sketches, construction geometry, and constraints;
- parametric features and feature history;
- semantic generated geometry references;
- canonical generated-topology producer-role identities;
- dependency, invalidation, and recompute intent;
- assembly parameters and bindings;
- part component occurrences and direct rigid placement;
- local geometric assembly constraints, including persistent Coincident/Parallel/Perpendicular intent;
- local Revolute joint limits and coordinates;
- Project-owned child assemblies and rigid subassembly occurrences;
- Project-level occurrence-qualified geometric constraints, including persistent Coincident/Parallel/Perpendicular intent;
- Project-level occurrence-qualified Revolute joints.

Derived data such as OCCT shapes, generated-topology producer classification/recovery query results, hierarchy traversal, transform-authority mappings, typed target source classifications/descriptors/capabilities, residuals/Jacobians, solve/motion results, freshness snapshots, rooted exchange records, posed shapes, contact classifications, sampled motion products, XDE labels, and STEP products/entities remains regenerable query/execution output rather than primary model authority.

## Direction

The project grows through controlled headless vertical slices. Historical phase order:

1. single-part parametric model;
2. stable recompute graph;
3. generated-face workplanes and semantic references;
4. broader sketch/profile/feature support;
5. sketch constraints and diagnostics;
6. assembly relationships, motion, hierarchy, posed geometry, analysis, structured exchange, and general target architecture;
7. broad multi-body Part Construction;
8. STEP Part/Assembly import as Reference or EditableBody;
9. GUI/application workflows;
10. engineering modules.

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

The completed Assembly phase has implemented:

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
assembly-selectable reference geometry intent + JSON + Geometry resolution
stable producer-driven generated topology identity and read-only recovery
generated topology target resolution and deterministic target compatibility
persistent local/Project-level Coincident, Parallel, and Perpendicular intent + JSON
```

Blocks 48–92 establish Body identity, body-scoped recompute/inspection, Body Booleans, associative
BodyTransform/SketchOwnership behavior, reusable Part-feature semantic input references, and
persistent richer Extrude/Cut intent plus Geometry, Revolve/RevolveCut intent plus Geometry, and
general Linear/Circular Pattern intent plus Geometry, persistent plus executed MirrorFeature
Geometry, persistent plus executed Fillet/Chamfer/Shell/Draft Geometry, and model-space 3D Sketch
Core through persistent Arc/Spline/Helix/Guide intent, strict JSON, deterministic transient OCCT
conversion, reusable connected PathCurve intent, executed Sweep/SweepCut/SweepSurface through
spatial paths, twist, and guide control, path-following Extrude/Extruded Cut, and persistent plus
executed path/guide-controlled multi-section Loft Geometry through verified G1/C1, and the first
persistent Surface-feature family plus executed Boundary/Fill, Trim/Extend, Stitch/Knit/Sew shell,
and Closed-shell-to-solid Surface Geometry. The current next technical step is Block 93 multi-body
STEP export and deterministic body naming.
STEP Part and
structured Assembly import follows that sequence in Blocks 95–101.

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

The semantic-reference string may represent a legacy feature role, a `ref:` reference-geometry source, or a canonical `topo:` generated-topology producer-role identity.

### Persisted transform authority

```text
(assembly_document: DocumentId,
 local ComponentInstanceId)
```

### Generated topology semantic source identity

```text
source feature producer
+ generated topology family
+ named semantic producer role
+ exact source profile identity where profile-derived
```

The first Block-35 producer matrices are:

```text
RectangularAdditiveExtrude
  -> 12 named linear edges, expected cardinality 1 each
  -> 8 named vertices, expected cardinality 1 each

SingleCircleSubtractiveExtrude
  -> cylindrical wall, expected cardinality 1
  -> source_rim, expected cardinality 1
  -> opposite_rim, expected cardinality 1
```

Pattern result-vector position is not persistent semantic identity. Pattern-generated subelements remain unavailable until stable per-instance model identity exists.

Raw OCCT hash/traversal/map identity, XDE label tags, STEP entity ids, and memory addresses are never promoted to semantic target identity.

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

Block 31 establishes the generic assembly target Geometry boundary.

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

Current `.top/.bottom/.right/.left/.front/.back`, `.axis`, and `.seat` strings remain unchanged. Existing Mate/Distance/Angle/Concentric/Insert/Revolute consumers receive legacy descriptor shapes adapted from the typed projection boundary.

Blocks 32–34 established assembly-selectable reference geometry Core intent, serialization, and Geometry resolution: first-class DatumAxis intent, the unambiguous `ref:` semantic source grammar, additive `datum_axes` JSON, byte-for-byte endpoint spelling roundtrips, and derived resolution of DatumPlane/DatumAxis/ConstructionLine/ConstructionPoint into Plane/Axis/Line/Point capabilities.

Block 35 establishes stable generated topology identity and recovery before Geometry topology lookup. Canonical `topo:` spellings encode exact semantic producer identities for cylindrical wall, rectangular linear-edge/vertex roles, and circular source/opposite rim roles. Producer role matrices publish expected cardinality and unsupported/ambiguous/patterned sources fail closed. Recovery is read-only and never writes raw kernel topology ids.

Block 36 resolves the supported Block-35 semantic producers into Cylinder/Axis, Line, Circle/Axis/center Point, and Point capabilities, computed analytically from validated model intent for both component-local and exact rooted transform semantics. Block 37 adds deterministic relationship/target compatibility selection. Block 38 adds persistent local/Project-level relationship intent. Blocks 39–47 complete generic equations and the Assembly joint families through Spherical. Blocks 48–92 add stable Body identity, body-scoped recompute/inspection, Body Booleans, associative Body transforms, reusable Part-feature semantic input references, richer Extrude/Cut extent/taper/thin intent plus Geometry, persistent plus executed Revolve/RevolveCut, general Pattern Core intent plus Geometry, persistent plus executed MirrorFeature Geometry, persistent plus executed Fillet/Chamfer/Shell/Draft Geometry, persistent model-space 3D Sketch Geometry, reusable connected PathCurve Core/JSON intent, executed Sweep/SweepCut/SweepSurface through spatial paths, twist, and guide control, path-following Extrude/Extruded Cut, and persistent ordered Loft/LoftCut/LoftSurface intent. Block 85 Two-section Loft Geometry on arbitrary planes is implemented. Block 86 Multi-section Loft is implemented. Block 87 Guided and continuity-controlled Loft is implemented. Block 88 Surface feature Core intent and JSON is implemented. Block 89 Boundary and Fill Surface Geometry is implemented. Block 90 Trim and Extend Surface Geometry is implemented. Block 91 Stitch/Knit/Sew shell Geometry is implemented. Block 92 Closed shell to solid conversion is implemented. The next authority step is Block 93 multi-body STEP export and deterministic body naming.

Canonical sequence: `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

Canonical target/joint roadmap: `docs/assembly-general-geometric-target-roadmap.md`.

Canonical generated-topology identity contract: `docs/assembly-generated-topology-reference-mvp5.md`.

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
- structured assembly exchange, STEP import/export, and STL export; STEP import is concretely
  sequenced in Blocks 95–101 as immutable Reference Parts or EditableBody base features;
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
- generic relationship or joint equations before persistent intent and target capability compatibility exist;
- richer joint families before typed coordinate slots and vector-drive semantics exist;
- a general-purpose physics constraint engine;
- full contact dynamics, collision response, friction, or rigid-body simulation;
- continuous collision detection unless a continuous algorithm is implemented and proved;
- production engineering assistants before underlying model intent is reliable.

These are sequencing boundaries, not permanent product exclusions.

## Documentation authority

`docs/mvp-plan.md` is the implementation-sequence source of truth.

`docs/step-import-sequence-mvp7.md` is the canonical post-Block-94 plan for STEP Part and
structured Assembly import.

`docs/architecture-summary.md` summarizes implemented architecture.

Feature-specific MVP documents are canonical for exact contracts, persistence boundaries, mathematical semantics, failure policies, and focused proofs.

`docs/assembly-geometric-target-taxonomy-mvp5.md` is canonical for the Block-31 source taxonomy, typed descriptors, capability projection, and current target-family migration.

`docs/assembly-reference-geometry-intent-mvp5.md` is canonical for Blocks 32–34 reference-geometry intent, JSON, and resolution handoff.

`docs/assembly-generated-topology-reference-mvp5.md` is canonical for Block-35 producer-driven generated topology identity and recovery.

`docs/assembly-generic-relationship-intent-mvp5.md` is canonical for Block-38 persistent Coincident/Parallel/Perpendicular intent and JSON.

`docs/assembly-generic-relationship-equations-mvp5.md` is canonical for Block-39 generic relationship compatibility, equations, shared solve integration, freshness/application reuse, and rank diagnostics.

`docs/assembly-joint-target-compatibility-mvp5.md` is canonical for Block-40 joint compatibility and oriented Frame semantics.

`docs/assembly-general-geometric-target-roadmap.md` is canonical for implemented Blocks 31–43 and planned Blocks 44–47.

`docs/file-format.md` is canonical for implemented serialization. Derived target source kinds/descriptors/capabilities/projections, producer matrices/classification, and recovery query results are not save-format fields.
