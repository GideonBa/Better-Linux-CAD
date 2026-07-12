# Part Construction Sequence MVP-6

Status: planned post-Assembly-MVP sequence. Blocks 48-94 are not implemented yet.

Current repository work remains on Block 32 of the assembly target/relationship/joint sequence. Blocks 32-47 remain mandatory before this sequence starts. Block 48 becomes the next technical step only after Block 47 is implemented and the Assembly MVP handoff is complete.

This document is the canonical numbered implementation sequence for the first broad BLCAD Part Construction MVP after the Assembly MVP.

It consolidates and sequences existing target architecture from:

- `docs/multi-body-transform-and-path-features-roadmap.md`
- `docs/inventor-like-sketcher-and-feature-roadmap.md`
- `docs/advanced-surfacing-and-3d-sketch-mvp.md`

Those documents remain canonical planning sources for detailed use cases and long-term capability breadth. This document is authoritative for block order, authority boundaries, MVP cuts, and implementation handoffs.

## Goal

After Block 94, one `PartDocument` should support a first serious headless parametric construction workflow covering:

```text
multiple solid/surface bodies
explicit feature-to-body result authority
new-body / join / cut / intersect operation modes
body booleans
body transforms with explicit sketch/reference ownership behavior
richer extrude/cut extents
Revolve / RevolveCut
general Linear Pattern
general Circular Pattern
Mirror Feature
Fillet
Chamfer
Shell
Draft
3D sketches
semantic connected PathCurve values
Sweep / SweepCut / SweepSurface
path-following extrude/cut
Loft / LoftCut / LoftSurface
multi-section and guided lofts
first continuity-controlled loft semantics
Boundary / Fill surfaces
Trim / Extend surfaces
Stitch / Knit / Sew shell
closed-shell-to-solid conversion
multi-body STEP export
```

The goal is not to clone SolidWorks or Inventor feature-for-feature. The goal is to finish a coherent first Part Construction kernel whose persistent model intent, dependency graph, semantic references, geometry execution, recompute behavior, and exchange products use one architecture.

## Non-negotiable architecture rules

1. `PartDocument` remains persistent parametric model authority.
2. OCCT shapes, wires, faces, shells, solids, and topology maps remain derived Geometry/cache products.
3. Raw `TopoDS_*` identity, traversal index, memory address, XDE label, and STEP entity id are never persistent feature input identity.
4. Blocks 35-36 semantic generated face/edge/vertex identity and recovery are reused by later Part features.
5. Existing semantic reference and reference-recovery infrastructure is extended rather than bypassed.
6. Every feature input/output relationship participates in dependency, invalidation, and recompute planning.
7. Multi-body authority is established before body booleans, transforms, general patterns, and richer solid features.
8. Core intent and JSON/structure validation precede Geometry execution for each new authority family.
9. `ShapeCache` is generalized once for multi-body results; richer features do not invent independent shape stores.
10. A feature that creates or modifies volume states its operation mode and target/produced body explicitly.
11. Pattern and mirror features preserve parametric source identity; they do not persist copied raw geometry.
12. Fillet/Chamfer/Shell/Draft consume semantic feature/body topology references, not arbitrary OCCT picks.
13. 3D sketch entities are Core model intent in model space and become OCCT curves only in Geometry.
14. `PathCurve` stores ordered semantic curve-segment references and an explicit orientation rule.
15. Sweep and path-following extrude may share internal Geometry helpers while remaining distinct user/model feature types.
16. Loft section order, alignment/seam intent, and continuity intent are persistent and deterministic.
17. Surface bodies are first-class `Body` values, not temporary unaddressable OCCT faces.
18. Surface stitching and shell-to-solid conversion are explicit features with strict validation.
19. Existing straight Extrude/Cut remain simple fast paths.
20. No block silently broadens into GUI editing, direct modeling, Class-A surfacing, sheet metal, or topology healing.

## Frozen phase order

```text
48-57   multi-body/result/transform authority
58-60   shared Part feature inputs and richer Extrude/Cut
61-62   Revolve
63-65   general Linear/Circular Pattern
66-67   Mirror Feature
68-70   Fillet and Chamfer
71-74   Shell and Draft
75-78   3D Sketch
79      PathCurve
80-83   Sweep and path-following Extrude/Cut
84-87   Loft
88-92   Surface Features and surface-to-solid
93      multi-body STEP exchange
94      integrated Part Construction MVP acceptance
```

Do not merge these phases into one feature branch.

# Phase A — Multi-body and feature result authority

## Block 48 — Body identity and PartDocument ownership

Primary boundary: Core persistent body identity.

Introduce:

```text
BodyId
Body
  id
  name
  body_kind = Solid | Surface
  visibility
```

`PartDocument` owns a deterministic body collection.

Freeze:

- unique `BodyId` scope per `PartDocument`;
- lexicographic body query/order contracts where deterministic output is required;
- body add/find/remove behavior;
- explicit dependent-removal rejection;
- no geometry behavior change yet;
- compatibility behavior for historical one-final-shape parts before feature-output migration.

Focused tag:

```text
[core][part-body]
```

Block 48 adds no JSON and no Geometry execution.

## Block 49 — Body JSON and structural validation

Primary boundary: save-format compatibility.

Add additive body persistence.

Freeze exact JSON for:

```text
bodies[]
  id
  name
  kind
  visibility
```

Historical part files without `bodies` must load through an explicitly documented compatibility path.

Validate duplicate ids, unsupported kinds, missing body references introduced by future-ready records, deterministic roundtrip order, and no hidden generated body creation during loading.

Focused tag:

```text
[core][part-body-json]
```

## Block 50 — Feature output-body and operation-mode Core intent

Primary boundary: persistent feature result authority.

Generalize feature result intent:

```text
FeatureBodyOperationMode = NewBody | Join | Cut | Intersect

Feature body result context
  operation_mode
  optional target_body
  optional produced_body
```

Freeze valid combinations:

```text
NewBody
  target_body = none
  produced_body = required

Join/Cut/Intersect
  target_body = required
  produced_body = target or explicit documented result identity
```

Migrate existing AdditiveExtrude/SubtractiveExtrude intent through backward-compatible defaults without changing historical files yet.

No Geometry execution changes in this block.

Focused tag:

```text
[core][feature-body-operation]
```

## Block 51 — Feature body-operation JSON, dependencies, and invalidation

Primary boundary: persistence and recompute graph semantics.

Serialize feature body result context.

Add dependency/invalidation edges for:

```text
feature -> input sketch/profile/parameters
feature -> target body
produced body -> producing feature
later feature -> referenced source body/feature
```

Freeze cycle and removal behavior. A body with dependent features may not disappear silently.

Historical single-body feature JSON remains readable through exact defaults.

Focused tags:

```text
[core][feature-body-operation-json]
[core][part-body-dependency]
```

## Block 52 — Multi-body ShapeCache and recompute execution

Primary boundary: Geometry cache/result authority.

Generalize `ShapeCache` from one final shape to deterministic body-scoped results:

```text
BodyId -> GeometryShape
```

Retain a compatibility final-shape query only with an explicit single-solid compatibility contract.

Recompute must:

1. follow the existing deterministic `RecomputePlan`;
2. read body-targeting feature intent;
3. update only affected body shapes;
4. preserve unaffected body cache entries;
5. fail closed on missing target/produced bodies;
6. never write OCCT shape identity back into Core.

Focused tag:

```text
[geometry][multi-body-recompute]
```

## Block 53 — Multi-body compatibility and body-result inspection

Primary boundary: application/query compatibility.

Freeze public headless queries for:

```text
body ids
body kind
body visibility
body shape summary
solid/surface body counts
```

Define when historical APIs such as `final_shape()` are legal and when multi-body ambiguity is an error.

Add a headless inspection path proving two independent solid bodies can coexist and recompute independently.

Focused tag:

```text
[geometry][multi-body-inspection]
```

## Block 54 — BodyBooleanFeature Core intent and JSON

Primary boundary: persistent body-combine intent.

Introduce:

```text
BodyBooleanFeature
  id
  operation = Add | Subtract | Intersect
  target_body
  tool_bodies[]
  result_mode = ModifyTarget | NewBody
  optional produced_body
  keep_tool_bodies
```

Rules derive from `docs/multi-body-transform-and-path-features-roadmap.md`:

- target and tool identities are semantic `BodyId` values;
- tool list is non-empty and deterministic;
- target may not also appear as a tool;
- duplicate tool ids are rejected;
- `NewBody` requires an explicit produced body;
- consumption/retention of tool bodies is explicit;
- dependency and removal behavior is frozen before Geometry.

Focused tag:

```text
[core][body-boolean]
```

## Block 55 — Body boolean Geometry and recompute

Primary boundary: OCCT body boolean execution.

Implement Add/Union, Subtract, and Intersect using body-scoped cached shapes.

Required proofs:

```text
two independent solids
A + B
A - B
A intersection B
multi-tool deterministic order
keep_tool_bodies true/false
ModifyTarget and NewBody
incremental recompute
source PartDocument intent unchanged by Geometry query
```

Boolean failure must identify the feature/body context and fail closed.

Focused tag:

```text
[geometry][body-boolean]
```

## Block 56 — BodyTransform and SketchOwnership Core intent + JSON

Primary boundary: persistent body transform and association intent.

Introduce the documented target concepts:

```text
BodyTransform
  id
  body
  kind = Translate | Rotate | UniformScale
  coordinate_space
  parameters
  apply_to_owned_sketches
  apply_to_owned_construction_geometry

SketchOwnership
  sketch
  owning_body
  association = DrivesBody | ConsumedByBody | ReferenceOnly
```

Mirror remains a dedicated feature in Blocks 66-67 rather than a generic transform kind in the first implementation.

Freeze transform-stack order, axis/reference identity for rotation, dependency edges, removal behavior, and JSON.

Non-uniform scale and arbitrary matrix authoring remain deferred.

Focused tags:

```text
[core][body-transform]
[core][sketch-ownership]
```

## Block 57 — Body transform Geometry and associative ownership behavior

Primary boundary: transformed body execution.

Implement translation, rotation, and uniform scale through recompute.

When requested by persistent transform intent:

```text
owned sketch workplane frames move with body
sketch-local 2D coordinates remain unchanged
owned construction geometry moves with body
projected/semantic references are re-resolved
unowned sketches do not move
```

Transform-stack order is significant and tested.

Focused tag:

```text
[geometry][body-transform]
```

# Phase B — Shared Part feature inputs and richer Extrude/Cut

## Block 58 — Part feature semantic input reference contract

Primary boundary: Core feature-input identity.

Freeze reusable persistent input references for later Part features:

```text
ProfileRegionReference
AxisReference
PlaneReference
FaceReference
EdgeReference
BodyReference
```

Reuse existing typed ids and semantic-reference infrastructure. Blocks 35-36 generated topology identity/recovery are authoritative for generated face/edge/vertex sources.

No raw OCCT identity is accepted.

The contract must distinguish:

```text
source identity
expected geometric capability
feature-specific role
```

Examples:

```text
Revolve axis role -> Axis capability
Mirror plane role -> Plane capability
Fillet edge role -> semantic Edge source
Shell removal face role -> semantic Face source
Draft pull direction -> Axis/Line capability
```

Focused tag:

```text
[core][part-feature-input-reference]
```

## Block 59 — Richer Extrude/Cut extent and thin-feature intent + JSON

Primary boundary: persistent Extrude/Cut feature breadth.

Extend existing Extrude/Cut intent with documented mature modes:

```text
extent = Distance | Symmetric | TwoSided | ThroughAll | ToNext | ToFace | Between
optional taper/draft angle
optional thin mode
operation_mode = NewBody | Join | Cut | Intersect
```

Freeze exact parameter/semantic target requirements and backward-compatible JSON defaults.

Path-following mode remains Blocks 80-83.

Focused tag:

```text
[core][extrude-extent]
```

## Block 60 — Richer Extrude/Cut Geometry

Primary boundary: OCCT extent/thin execution.

Implement the Block-59 extent matrix in deterministic increments.

Required proofs include:

- symmetric and two-sided extrusion;
- ToFace semantic target resolution;
- Between two semantic planar targets;
- tapered extrusion;
- thin feature from an open supported profile;
- NewBody/Join/Cut/Intersect body operation modes;
- unchanged historical straight Extrude/Cut results.

Focused tag:

```text
[geometry][extrude-extent]
```

# Phase C — Revolve

## Block 61 — Revolve/RevolveCut Core intent and JSON

Primary boundary: persistent rotational feature intent.

Introduce:

```text
RevolveFeature
RevolveCutFeature
```

Inputs:

```text
ProfileRegionReference
AxisReference
angle extent
side/symmetric mode
FeatureBodyOperationMode
body result context
```

Supported axis sources follow existing roadmap intent:

```text
sketch centerline when semantically modeled
construction line
DatumAxis
semantic generated axis
```

Freeze full/partial/symmetric angle semantics, principal/non-principal angle storage as appropriate for feature rotation, self-intersection failure policy, dependencies, invalidation, and JSON.

Focused tag:

```text
[core][revolve-feature]
```

## Block 62 — Revolve/RevolveCut Geometry

Primary boundary: rotational solid execution.

Implement profile-to-model-space mapping plus OCCT revolution.

Required proofs:

```text
360-degree solid revolve
partial revolve
symmetric revolve
revolve cut
groove/undercut fixture
NewBody/Join/Cut/Intersect
semantic axis update invalidates/recomputes
self-intersecting result fails closed
```

Focused tag:

```text
[geometry][revolve-feature]
```

# Phase D — General patterns

## Block 63 — General Part Pattern Core model and JSON

Primary boundary: persistent repeated-feature/body intent.

Introduce shared pattern source identity:

```text
PatternSourceReference
  FeatureId | BodyId
```

First MVP intentionally excludes arbitrary face-only patterning and raw topology copies.

Introduce:

```text
LinearPatternFeature
  sources[]
  direction reference
  count
  spacing or total extent
  direction sign

CircularPatternFeature
  sources[]
  axis reference
  count
  total angle
  equal spacing
```

Counts and distances/angles are typed parameter references where appropriate.

Freeze generated-instance ordering, dependency behavior, source suppression/removal behavior, body result interaction, and JSON.

Focused tag:

```text
[core][part-pattern]
```

## Block 64 — General Linear Pattern Geometry

Primary boundary: deterministic translational pattern execution.

Prove feature-source and body-source patterns, parameter-driven count/spacing, negative/reversed direction, NewBody/body-operation interaction, deterministic generated instance order, and incremental recompute.

Do not expand pattern intent into unrelated copied feature records.

Focused tag:

```text
[geometry][linear-pattern]
```

## Block 65 — General Circular Pattern Geometry

Primary boundary: deterministic rotational pattern execution.

Prove feature-source and body-source patterns, arbitrary semantic axis reference, full/partial angle, parameter-driven count, deterministic angular order, body-operation interaction, and incremental recompute.

The existing `CircularHolePattern` remains a specialized compatibility fast path until explicitly migrated.

Focused tag:

```text
[geometry][circular-pattern]
```

# Phase E — Mirror Feature

## Block 66 — MirrorFeature Core intent and JSON

Primary boundary: persistent feature/body mirror intent.

Introduce:

```text
MirrorFeature
  sources[] = FeatureId | BodyId
  mirror_plane = PlaneReference
  result/body operation intent
```

Freeze source ordering, plane identity, feature/body result semantics, dependency/invalidation, and JSON.

Sketch mirror remains a sketcher operation and is not this feature.

Focused tag:

```text
[core][mirror-feature]
```

## Block 67 — MirrorFeature Geometry

Primary boundary: reflected feature/body execution.

Implement deterministic reflection through the semantic plane target.

Prove feature and body source mirroring, non-axis-aligned datum plane, semantic generated planar face as mirror plane where supported, body result modes, recompute, and reference failure behavior.

Focused tag:

```text
[geometry][mirror-feature]
```

# Phase F — Fillet and Chamfer

## Block 68 — Edge treatment Core intent and JSON

Primary boundary: persistent semantic edge-treatment intent.

Introduce:

```text
FilletFeature
  target_body
  edges[]: EdgeReference
  radius parameter

ChamferFeature
  target_body
  edges[]: EdgeReference
  mode = EqualDistance | TwoDistance | DistanceAngle
  parameters
```

Blocks 35-36 semantic generated edge identity/recovery are mandatory dependencies.

Freeze deterministic edge-reference order, duplicate rejection, reference recovery/failure behavior, body ownership, dependency/invalidation, and JSON.

Variable-radius fillets and setback/corner-management systems remain deferred.

Focused tag:

```text
[core][edge-treatment]
```

## Block 69 — Fillet Geometry

Primary boundary: constant-radius edge fillet execution.

Implement constant-radius fillets on resolved semantic edges.

Required proofs:

```text
single edge
multiple edges
parameter-driven radius
semantic edge recovery after upstream dimension edit
ambiguous/broken edge reference failure
radius too large failure
incremental recompute
```

Focused tag:

```text
[geometry][fillet-feature]
```

## Block 70 — Chamfer Geometry

Primary boundary: semantic edge chamfer execution.

Implement EqualDistance first, then TwoDistance and DistanceAngle within the same frozen Core model when Geometry semantics are explicitly proved.

Required proofs mirror Block 69 for semantic edge stability and failures.

Focused tag:

```text
[geometry][chamfer-feature]
```

# Phase G — Shell and Draft

## Block 71 — ShellFeature Core intent and JSON

Primary boundary: persistent thin-wall solid intent.

Introduce:

```text
ShellFeature
  target_body
  removed_faces[]: FaceReference
  thickness parameter
  direction = Inward | Outward
```

Freeze face-reference order, duplicate rejection, thickness sign/unit semantics, body dependency, invalidation, and JSON.

Focused tag:

```text
[core][shell-feature]
```

## Block 72 — ShellFeature Geometry

Primary boundary: thin-wall solid execution.

Implement semantic removed-face shelling.

Required proofs:

```text
closed box -> open-top shell
multiple removed faces
inward/outward thickness
parameter update
semantic face recovery
invalid thickness/non-manifold result failure
```

Focused tag:

```text
[geometry][shell-feature]
```

## Block 73 — DraftFeature Core intent and JSON

Primary boundary: persistent tapered-face intent.

Introduce:

```text
DraftFeature
  target_body
  faces[]: FaceReference
  pull_direction: AxisReference or LineReference
  neutral_plane: PlaneReference
  angle parameter
```

Freeze signed angle convention, pull direction ownership, neutral plane semantics, dependency/invalidation, duplicate face rejection, and JSON.

Focused tag:

```text
[core][draft-feature]
```

## Block 74 — DraftFeature Geometry

Primary boundary: drafted-face execution.

Prove positive/negative draft, multiple semantic faces, arbitrary pull direction, non-root neutral plane, upstream reference edits, and fail-closed invalid/tangent/self-intersecting results.

Focused tag:

```text
[geometry][draft-feature]
```

# Phase H — 3D Sketch model

## Block 75 — Basic 3D Sketch Core intent

Primary boundary: persistent spatial sketch identity.

Introduce a separate 3D sketch model rather than overloading planar `Sketch` coordinates:

```text
Sketch3D
SketchPoint3D
SketchLine3D
SketchPolyline3D
```

Geometry lives in Part model space.

First implementation supports explicit coordinates and typed parameter-driven coordinates. No general 3D constraint solver yet.

Freeze id scope, entity ownership, ordered polyline vertices, dependency/invalidation, and removal behavior.

Focused tag:

```text
[core][sketch-3d]
```

## Block 76 — 3D spline, arc, helix, and guide-curve intent

Primary boundary: richer spatial curve Core intent.

Extend `Sketch3D` with:

```text
SketchArc3D
SketchSpline3D
SketchHelix3D
SketchGuideCurve3D role/metadata
```

Spline points may reference:

```text
explicit 3D points
construction points
semantic points from planar sketches on different planes
later recovered generated vertices/edge points
```

Freeze fit/control-point representation, degree/continuity constraints for the first seed, helix axis/pitch/turn semantics, dependencies, and failure behavior.

Focused tag:

```text
[core][sketch-3d-curves]
```

## Block 77 — 3D Sketch JSON and semantic references

Primary boundary: save-format and cross-source reference stability.

Serialize `Sketch3D` and all Block-75/76 entities.

Freeze unambiguous semantic-reference grammar for 3D points/curves and deterministic entity order.

Prove a 3D spline can semantically reference points from three planar sketches on three different planes and roundtrip without copying resolved coordinates into the source references.

Focused tag:

```text
[core][sketch-3d-json]
```

## Block 78 — 3D Sketch Geometry conversion

Primary boundary: model-space curve execution.

Convert supported 3D points/lines/polylines/arcs/splines/helices into deterministic OCCT point/edge/wire products.

No OCCT edge identity is written back into Core.

Required proofs include a 3D spline connecting points from three differently oriented planar sketches.

Focused tag:

```text
[geometry][sketch-3d]
```

# Phase I — PathCurve

## Block 79 — Connected PathCurve Core intent, JSON, and validation

Primary boundary: reusable semantic path identity.

Introduce the existing documented concept:

```text
PathCurve
  id
  segments[]: PathSegmentReference
  closure = Open | Closed
  orientation_rule = ProfileNormal | MinimumTwist | FixedUpVector
  optional fixed_up_vector/reference
  optional continuity_hint = C0 | G1 | G2
```

Supported segment sources include:

```text
planar sketch line/arc/spline
projected/reference-driven curve
construction line
3D sketch line/polyline/arc/spline/helix
semantic generated edge when Block-35/36 recovery supports it
```

Validation requires deterministic ordered connectivity within explicit tolerance. Branch points and self-intersecting paths may be rejected in the first MVP.

Focused tag:

```text
[core][path-curve]
```

# Phase J — Sweep and path-following Extrude/Cut

## Block 80 — Sweep feature Core intent and JSON

Primary boundary: persistent path-swept feature intent.

Introduce:

```text
SweepFeature
SweepCutFeature
SweepSurfaceFeature
```

Inputs:

```text
profile/profile-region reference
PathCurveId
orientation rule inherited or overridden explicitly
optional twist parameter
FeatureBodyOperationMode for solid variants
body result context
```

Closed profile -> solid sweep. Supported open profile -> surface sweep.

Guide/rail controls remain Block 82.

Focused tag:

```text
[core][sweep-feature]
```

## Block 81 — Basic Sweep Geometry

Primary boundary: first shared path sweep execution.

Implement:

```text
straight construction-line path
planar line/arc/polyline path
minimum first orientation contract
solid Sweep
SweepCut
SweepSurface
```

Prove explicit orientation-rule behavior and fail closed on unsupported twist/branch/self-intersection conditions.

Focused tag:

```text
[geometry][sweep-feature]
```

## Block 82 — 3D path, twist, and guide-controlled Sweep

Primary boundary: spatial sweep execution breadth.

Extend the same Sweep engine to:

```text
3D polyline
3D spline
helix
mixed connected PathCurve
explicit twist
optional guide/rail curve where supported
```

Freeze minimum-twist and fixed-up-vector behavior before Geometry implementation.

Prove a solid sweep along a 3D spline and a helical sweep.

Focused tag:

```text
[geometry][sweep-3d]
```

## Block 83 — Path-following Extrude and Extruded Cut

Primary boundary: existing feature extension over shared path execution.

Extend existing:

```text
AdditiveExtrude
SubtractiveExtrude
```

with:

```text
direction_mode = Path
path_curve = PathCurveId
```

Reuse Block-79 path identity and Block-81/82 internal path-frame execution where semantically applicable.

Keep user/model feature type distinct from `SweepFeature` as required by the existing roadmap.

Focused tag:

```text
[geometry][path-extrude]
```

# Phase K — Loft

## Block 84 — ProfileSectionReference and Loft Core intent + JSON

Primary boundary: persistent ordered loft section intent.

Introduce:

```text
ProfileSectionReference
  sketch/profile region
  optional alignment_reference
  flip_normal
  optional rotation_offset

LoftFeature
LoftCutFeature
LoftSurfaceFeature
  sections[]
  optional path_curve
  guide_curves[]
  continuity
  FeatureBodyOperationMode for solid variants
  body result context
```

First validation requires at least two sections and deterministic explicit section order.

Closed sections may produce solid lofts. Supported open sections may produce surface lofts.

Freeze closed-profile seam/alignment semantics before Geometry.

Focused tag:

```text
[core][loft-feature]
```

## Block 85 — Two-section Loft Geometry on arbitrary planes

Primary boundary: first loft execution.

Implement two-section solid/surface loft between profile sections resolved through their own workplane frames.

Profiles may be non-parallel or arbitrarily oriented.

Required proofs:

```text
parallel sections
non-parallel sections
arbitrarily oriented workplanes
closed solid loft
open surface loft
LoftCut
NewBody/Join/Cut/Intersect
stable alignment/seam behavior
```

Focused tag:

```text
[geometry][loft-feature]
```

## Block 86 — Multi-section Loft

Primary boundary: three-or-more-section execution.

Implement deterministic ordered multi-section lofting.

A middle section must shape the transition. Section correspondence may not depend on incidental OCCT wire order.

Prove three and more profile sections and parameter-driven intermediate-section edits.

Focused tag:

```text
[geometry][multi-section-loft]
```

## Block 87 — Guided and continuity-controlled Loft

Primary boundary: loft flow and continuity intent execution.

Implement guide/path-controlled loft behavior using semantic 3D curves/`PathCurve` values.

Continuity contract:

```text
C0 Position
G1 Tangent
G2 Curvature
```

C0 and G1 must be fully proved. G2 must either be implemented with explicit OCCT/geometry guarantees or fail closed as an unsupported persisted option until a dedicated Geometry increment; do not silently degrade G2 to G1.

Required fixtures include duct transition and airfoil/blade-like root/mid/tip sections with leading/trailing guide curves.

Focused tag:

```text
[geometry][guided-loft]
```

# Phase L — Surface bodies and Surface Features

## Block 88 — Surface feature Core intent and JSON

Primary boundary: persistent surface-construction feature intent.

`BodyKind::Surface` from Block 48 is now an active feature result authority.

Introduce first surface feature set:

```text
BoundarySurfaceFeature
FillSurfaceFeature
TrimSurfaceFeature
ExtendSurfaceFeature
SurfaceStitchFeature
ClosedShellToSolidFeature
```

Persistent input references:

```text
BoundaryCurveReference
SurfaceReference
semantic trimming curve/profile reference
```

`NetworkSurface` and arbitrary patch-network intent remain deferred after the first Part Construction MVP.

Freeze surface result body identity, dependency/invalidation, JSON, and removal behavior.

Focused tag:

```text
[core][surface-feature]
```

## Block 89 — Boundary and Fill Surface Geometry

Primary boundary: first freeform surface execution.

Implement surfaces from semantic boundary curves and supported curve networks within the frozen Boundary/Fill contracts.

Inputs may come from planar sketches, 3D sketches, construction/reference curves, and semantic generated edges.

Required proofs include four-boundary surface and fill of a supported closed boundary.

Focused tag:

```text
[geometry][surface-boundary-fill]
```

## Block 90 — Trim and Extend Surface Geometry

Primary boundary: explicit surface editing feature execution.

Implement semantic trim and extend operations on `BodyKind::Surface` results.

Trim inputs and target surfaces remain persistent semantic references. Generated OCCT subshape ids remain cache-only.

Prove recompute/reference recovery after upstream boundary changes and fail closed on ambiguous trimming results.

Focused tag:

```text
[geometry][surface-trim-extend]
```

## Block 91 — Stitch/Knit/Sew shell Geometry

Primary boundary: multi-surface shell construction.

Implement `SurfaceStitchFeature` over an ordered semantic surface set.

Validate:

```text
boundary match tolerance
remaining free edges/gaps
face orientation consistency
shell connectivity
basic manifold suitability
```

Output remains a surface/shell body until explicit closed-shell conversion.

No automatic topology healing or arbitrary gap closing.

Focused tag:

```text
[geometry][surface-stitch]
```

## Block 92 — Closed shell to solid conversion

Primary boundary: explicit surface-to-solid transition.

Implement `ClosedShellToSolidFeature`.

Require a closed, consistently oriented, sufficiently manifold stitched shell. Reject open/non-manifold inputs with explicit diagnostics.

The produced result is an explicit `BodyKind::Solid` identity and participates in later body booleans/transforms/features through the same Block-48 authority.

Focused tag:

```text
[geometry][surface-to-solid]
```

# Phase M — Exchange and integrated Part Construction MVP

## Block 93 — Multi-body STEP export and deterministic body naming

Primary boundary: Part exchange over body identity.

Extend Part STEP export to support multiple visible solid/surface bodies.

Reuse `BodyId` as BLCAD exchange definition identity input and generate collision-free deterministic exchange names using the established percent-encoding discipline where applicable.

Freeze:

```text
body export order
visibility filtering
solid/surface body handling
shared shape/result reuse
source PartDocument immutability
```

Do not persist STEP/XDE entity ids.

Focused tag:

```text
[geometry][multi-body-step-export]
```

## Block 94 — Integrated Part Construction MVP acceptance and headless workflows

Primary boundary: end-to-end Part Construction MVP proof.

This block introduces no new feature family unless an integration defect requires a narrowly scoped correction.

Add representative headless fixtures proving:

### Multi-body mechanical part

```text
Body A from Extrude
Body B from Revolve
BodyTransform on B
A - B through BodyBooleanFeature
Fillet semantic edges
Chamfer semantic edges
Shell selected semantic faces
Draft selected faces
Linear Pattern of a feature
Circular Pattern of another feature
Mirror Feature across DatumPlane
```

### Routed/swept part

```text
3D Sketch
PathCurve
Sweep solid
SweepCut
path-following Extrude/Cut
```

### Lofted freeform part

```text
root/mid/tip profile sections on arbitrary planes
3D guide curves
multi-section guided Loft
surface Boundary/Fill support fixture
surface Stitch
closed shell -> Solid
```

Prove:

```text
full JSON roundtrip for every new persistent family
stable dependency/invalidation/recompute
incremental recompute of affected bodies only
semantic reference recovery/fail-closed behavior
no raw OCCT identity in Core JSON
multi-body STEP export
source model immutability for Geometry queries
headless reproducibility/insertion-order independence where ordering is declared deterministic
```

Focused tag:

```text
[integration][part-construction-mvp]
```

After Block 94, the first Part Construction MVP is considered complete.

## What "Part Construction MVP complete" means

After Block 94 BLCAD should have a serious first parametric Part kernel for common mechanical and first freeform workflows.

It does not mean SolidWorks Part or Inventor Part product parity.

Still deferred beyond Block 94:

```text
production GUI modeling/picking/previews
full Class-A surfacing
arbitrary NURBS control-cage editing
variable-radius fillets
advanced fillet corner/setback systems
face-only and sketch-driven general pattern breadth beyond the frozen first pattern source model
advanced topology healing and automatic gap repair
direct modeling / push-pull
sheet-metal feature system
weldments / structural members
mold/core/cavity toolsets
full thread/emboss/rib/web specialized features
feature configurations/design tables
full history rollback/UI feature suppression workflows
part-level finite-element or manufacturing modules
```

The next phase after Block 94 must be planned from measured gaps in the integrated Part Construction MVP rather than by adding several unrelated feature families to Block 94.

## Immediate repository handoff

Current next technical step remains Block 34.

The active sequence is:

```text
Blocks 34-47
  finish Assembly MVP

then

Block 48
  Body identity and PartDocument ownership
```

Do not begin Block 48 before Block 47 unless the implementation sequence is deliberately re-planned and all dependent documentation is updated together.
