# Architecture Summary

This document summarizes implemented BLCAD architecture and the current technical direction.
Feature-specific documents remain canonical for exact formulas, save-format spellings,
validation/failure policy, ordering, and focused proof.

## Goal and authority model

BLCAD is an independent parametric CAD system for Linux. BLCAD owns semantic model intent; OCCT/Open
CASCADE is the computed geometry and data-exchange kernel.

Fundamental decisions are:

- persist parametric and semantic intent rather than final BRep authority;
- persist stable semantic references rather than raw OCCT topology ids;
- use stable shared planar point identity rather than coordinate equality for Sketch connectivity;
- solve planar constraints and driving dimensions over Core topology identity, never screen/OCCT objects;
- keep Core model intent below Geometry query/execution types;
- separate source identity, geometric capability, hierarchy occurrence, transform, and exchange identity;
- keep viewport pixels, hover, grid, hit stacks, snaps, sampled curves, control polygons, formatted
  dimension text, glyph layout, and font rasterization transient;
- keep solver variables, residuals, Jacobians, rank, DOF, conflict diagnostics, measured display values,
  curvature samples, resolved text, glyph strokes, and fallback font choices derived;
- use explicit migration or versioned sidecar persistence when a new identity model is required;
- include every persistent sidecar owned by an editing session in dirty-state, Save/Open, and exact
  document history;
- require validated complete candidates before solve/recompute/edit products mutate persistent intent.

A lower authority layer must not depend on a higher execution or presentation layer for persistent
identity.

## Layer direction

```text
Qt user interface / commands / transient interaction
  -> GUI headless controllers and complete candidate transactions
  -> GuiDocumentSession document + constraint/dimension sidecar history authority
  -> Core planar Sketch topology + edit/annotation/constraint/dimension intent
  -> Core deterministic planar constraint and dimension solve composition
  -> Core Part/Assembly persistent model intent
  -> Geometry semantic resolution / curve evaluation / glyph layout
  -> Geometry equation / recompute execution
  -> freshness validation + atomic application
  -> posed geometry / analysis / exchange consumers
  -> OCCT geometry + XDE data-exchange kernel
```

The optional GUI is a client of Core and Geometry. Qt types stay out of `blcad_core`; widgets never
own BRep, constraint mathematics, dimension measurements, units, parameter expressions, transform
authority, recompute, persistent Sketch topology, target identity, spline representation, or text
persistence.

## PartDocument and Part construction authority

`PartDocument` is persistent parametric definition authority. Implemented Part intent includes typed
quantities/parameters and formulas, datum/derived workplanes, construction geometry, planar Sketch and
profile records, projected/reference-driven Sketch geometry, historical constraints/dimensions, Body
identity, feature history, multi-body result operations, broad solid features, Sketch3D/PathCurve,
Sweep, path Extrude, Loft, Surface features, closed-shell conversion, and multi-body STEP export intent.

Stable `BodyId` and persistent result intent let recompute publish deterministic Body-scoped products.
Core does not persist `TopoDS_Shape`. `ShapeCache` and OCCT shapes are derived and consumers requiring
fresh results use explicit recompute/freshness boundaries.

## Shared planar Sketch topology

Historical planar `Sketch` records embed `Point2` coordinates in line, arc, spline, and profile-center
records. They remain load-compatible and continue as the Geometry/recompute compatibility carrier.

Block 108 adds the topology identity used by solver/direct manipulation:

```text
SketchTopology
  SketchId
  points[]       SketchTopologyPoint
  entities[]     SketchTopologyEntity
  dependencies[] SketchTopologyDependency
```

Global point/entity collections are lexicographic id order. Defining point roles and profile
dependencies preserve semantic order. Two points may occupy equal coordinates while retaining
different `SketchPointId` values.

Historical migration derives sharing only from explicit ordered profile connectivity. Editable
topology uses dependency-safe Add/Move/Replace/Remove and complete before/after snapshots. Canonical
persistence is `blcad.sketch_topology.mvp8`, version 1. Any state that historical Sketch JSON cannot
represent fails closed at the materialization bridge.

Canonical contract: `docs/sketch-shared-topology-mvp8.md`.

## Deterministic planar solver

`SketchConstraintSolver` consumes `SketchTopology` plus a canonical `SketchConstraintSystem`. Stable
constraint ids and non-reference `SketchPointId` order define variables/residuals. Supported families
include Coincident, Fixed, Horizontal, Vertical, Parallel, Perpendicular, Collinear, Equal, Tangent,
Concentric, Midpoint, Symmetric, PointOnObject, linear distances, Radial, Diameter, and Angular.

The solver normalizes by characteristic length, computes a central-difference Jacobian, uses
deterministic damped Gauss-Newton execution, derives local DOF from final Jacobian rank, and publishes
stable redundancy/conflict diagnostics. `SketchSolveResult` is derived and never an opaque save cache.

Block 115 maps typed dimensions onto this existing system. Line length becomes aligned endpoint
distance; radius, diameter, and angle use their corresponding families; arc length uses deterministic
iterative equivalent-radius calibration followed by actual measured-length validation. No parallel Qt
or Geometry variational solver exists.

Canonical contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Interactive Sketch architecture through Block 115

### Workspace and plane interaction

Blocks 106–107 add the contextual Sketch workspace, command lifecycle, numeric HUD, normal-to-plane
camera, device-independent plane mapping, semantic interaction scene, zoom-stable hit testing,
Window/Crossing selection, grid, snapping, and inference. Screen and interaction products never become
model identity.

`GuiSelection` carries persistent semantic id plus an optional exact Sketch hit `candidate_id`. Sketch
selection deduplicates by both fields, allowing start/end roles of one entity to be selected together;
non-Sketch selection retains one entry per semantic id.

### Solver-backed drag

Block 110 derives semantic endpoint, midpoint, center, radius, arc, spline-control, and dimension
handles from persistent topology. Pointer samples become transient solver constraints. Release flushes
the exact final sample; successful acceptance rechecks source freshness and commits one
`Drag sketch handle` transaction. Cancel, lost capture, stale source, or failed solve restores the
original document.

After Blocks 114–115, session-backed drag constructs its baseline from historical constraints, accepted
`SketchConstraintCatalog` records, and all driving `SketchDimensionCatalog` records. Release compares
both current catalogs and the rebuilt effective system against preview snapshots, then validates every
final driving measurement. Arc-length changes that no longer match their parameter fail closed.

### Creation tools

Block 111 creates lines and line-based profiles plus construction points/centerlines through one
transaction. Block 112 creates exact full circles as `CircleProfile` plus diameter `Parameter`, arcs as
`ArcSegment`, ellipses as deterministic cubic `SplineSegment` spans, and slots as ordered line/arc
profiles. Rubber bands and sampled previews remain transient. Snap/tangent inference becomes persistent
only when Block 114 receives explicit stable targets and accepts the candidate solve.

### Spline editing

Block 113 preserves cubic `SplineSegment` as the sole persistent planar spline curve. The Core
`SketchSplineEditModel` owns disposable control- or fit-point authoring state, deterministic
Catmull-Rom-to-Bezier conversion, insertion/removal, connected endpoint handling, and C0/G1 continuity
handles. `SketchSplineGeometryEvaluator` derives samples, derivatives, curvature, control polygons,
and continuity diagnostics.

`GuiSketchSplineController` previews a complete candidate without `PartDocument` mutation, snapshots
selected source segments, rechecks freshness, rebuilds the complete Sketch, and commits one
`Edit sketch spline` transaction. Profiles, constraints, dimensions, references, and continuity are
validated again before `PartDocument::update_sketch(...)`.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

### Sketch text

Block 113 adds stable `SketchText` annotation intent with `SketchTextId`, target `SketchId`, UTF-8
template, requested font, height `ParameterId`, optional rotation `ParameterId`, plane anchor, and
explicit token-to-parameter bindings. Direct and expression-driven parameters resolve through the
existing `PartDocument` parameter authority.

Historical `blcad.part_document.mvp1` remains unchanged. Text uses explicit sidecar persistence:

```text
schema  = blcad.sketch_text.mvp8
version = 1
```

`SketchTextLayoutResolver` receives available font families and resolves requested family, configured
fallback families, then deterministic built-in `BLCAD Simplex Stroke`. Resolved strings, selected
fallback, glyph strokes, and rasterization are derived.

Canonical contract: `docs/gui-sketch-spline-text-mvp8.md`.

### Geometric constraint authoring and glyph interaction

Block 114 adds `SketchConstraintIntent` and `SketchConstraintCatalog` as stable non-dimensional
constraint authoring intent over Block-108 point/entity ids. Each record stores a stable
`SketchConstraintId`, Block-109 family, ordered semantic targets, and `manual|automatic` provenance.
Canonical sidecar persistence is:

```text
schema  = blcad.sketch_constraints.mvp8
version = 1
```

Historical embedded constraints remain adapted into every solve. Accepted sidecar constraints and one
new candidate are composed into a disposable canonical `SketchConstraintSystem`. Only
`under_constrained` or `fully_constrained` results materialize a complete candidate Sketch. Redundant,
conflicting, invalid-reference, and non-convergent candidates publish stable diagnostics without
mutation.

`GuiDocumentSession` owns the complete constraint-catalog collection through dirty state, Save/Open,
global history, and later solves. A Part saved as `model.blcad.json` stores it in
`model.blcad.json.sketch-constraints.json`. `SketchConstraintGlyphLayoutResolver` derives semantic
accepted/preview/conflict/redundancy annotations.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

### Driving/reference dimension authoring

Block 115 adds `SketchDimensionIntent` and deterministic per-Sketch `SketchDimensionCatalog` records.
Each dimension freezes stable id, family, ordered topology targets, `driving|reference` mode, and an
optional typed `ParameterId`.

Driving dimensions contribute solver residuals resolved from the existing `PartDocument` Length/Angle
parameter and expression graph. Reference dimensions contribute no residual and only measure current
topology. `SketchDimensionCatalogSystemBuilder` composes historical constraints, Block-114 records,
and all driving dimensions; it accepts only under-/fully-constrained, measurement-valid, exactly
materializable results.

`GuiSketchDimensionController` owns disposable preview, semantic annotation, source/catalog freshness,
and atomic add/literal/formula/rebind/mode transactions. Qt action and dialog state is header-only binder
presentation; it never stores dimension values. `GuiDocumentSession` publishes parameter state, solved
Sketch, constraint catalog, and dimension catalog as one history snapshot.

Canonical sidecar persistence is:

```text
schema  = blcad.sketch_dimensions.mvp8
version = 1
```

A Part saved as `model.blcad.json` stores dimensions in
`model.blcad.json.sketch-dimensions.json`. Measured values, formatted text, anchors, and solver products
remain derived.

Canonical contract: `docs/gui-sketch-dimension-authoring-mvp8.md`.

## Semantic Part-feature input and generated topology identity

Part features use typed semantic input references and expected capabilities instead of raw kernel
subshape ids. Stable generated-topology references encode producer/profile/role semantics; producer
cardinality defines expected generated elements. Recovery evaluates current semantic intent read-only
and never writes OCCT traversal/hash/map, XDE, STEP, memory, or viewport identity into model intent.

## Project, AssemblyDocument, and hierarchy authority

`Project` owns one explicit root `AssemblyDocument`, child assembly definitions, PartDocuments, and
Project-level cross-hierarchy relationship/joint records. A child enters the rooted hierarchy through
an authored `SubassemblyInstance` boundary.

Persistent local component transform authority is `(AssemblyDocumentId, local ComponentInstanceId)`.
Rooted occurrence identity is separate and includes the exact subassembly path. Traversal derives
root-first occurrence descriptors and transform chains; visible active Part leaves feed posed geometry.

Assembly target resolution separates semantic source classification from geometric capability.
Relationship/joint solving uses deterministic variables, residuals, Jacobians, diagnostics, and
freshness-gated atomic application. Failed solve/freshness checks publish no partial transforms.

## Motion, posed geometry, analysis, and exchange

Implemented motion authority includes Revolute, Prismatic, Cylindrical, Planar, and Spherical families
with local/cross-hierarchy paths and stable coordinate intent. Posed geometry composes component and
hierarchy transforms after required Part recompute. Contact/interference/sweep analysis and STEP
export consume this derived state.

Flattened and structured exchange derive stable product/occurrence identity; raw XDE/STEP entity
identity is never model authority.

## Persistence and regeneration split

Persistent or explicitly authored intent includes:

```text
parameters / expressions / workplanes / construction geometry
historical planar Sketch curves, profiles, constraints, dimensions, continuity
canonical SketchTopology when topology persistence is used
Block-113 SketchText sidecar records and parameter bindings
session-owned Block-114 SketchConstraintCatalog collections with stable targets/provenance
session-owned Block-115 SketchDimensionCatalog collections with mode/targets/ParameterId
Body / Part feature history and semantic references
Project / Assembly relationships / joints / hierarchy boundaries
stable exchange/product intent
```

Derived or transient products include:

```text
OCCT BRep and ShapeCache contents
reference-recovery evaluations
Sketch migration reports
solver variables / residuals / Jacobians / rank / DOF / diagnostics
interaction mapping / hit stacks / grid / snaps / rubber bands
semantic drag/spline handles / control polygons / fit authoring state
spline samples / derivatives / curvature / continuity diagnostics
resolved Sketch text / available-font discovery / fallback choice / glyph strokes
constraint and dimension candidate solves / solved preview topology / diagnostics
constraint/dimension glyph token / formatted value / anchor / state / hit-test presentation
temporary drag target equations and augmented topology
Assembly solve/motion proposals and freshness snapshots
posed shapes / analysis / XDE and STEP transfer identity
```

## Current boundary

Blocks 106–115 are implemented. Block 116 is the current next technical step: trim, extend, split,
Sketch corner fillet, and Sketch corner chamfer with explicit remap-or-reject behavior for profiles,
references, constraints, dimensions, and continuity intent.
