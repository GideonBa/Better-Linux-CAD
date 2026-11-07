# Feature System

Status: architecture and capability catalog. Part Construction through Block 94, its GUI workflows
through Block 136, STEP import through 143 and the Block-144 layout are accepted; `mvp-plan.md`
owns exact status. Historical future-tense sections and the proposed sequence below are not an
active queue. [Blocks 145–204](inventor-parity-sequence.md) consolidate the remaining assigned
extensions: history/suppression 146–148, Hole/Thread 152–157, advanced curves/surfaces 159–174,
Fillet 175–179, Rib/Web 180–181, Emboss/Engrave 182–184 and Direct Edit 185–187. Existing typed
feature families/result contexts are extended; no generic feature-base rewrite is required first.

Features are the parametric operations a part is built from. Each feature has inputs, parameters, references, and a computed geometry output. The central rule is that a feature stores the *rule* for computing geometry, not only the finished body. The OCCT shape is a cache.

## Executable history foundation — Block 146A, 2026-11-07

`PartDocument::executable_history()` returns a read-only, typed projection of existing executable
records in dependency order for legacy Parts, or the explicit authored order described below.
Each `PartFeatureHistoryEntry` carries the exact graph `node_id` and
`PartFeatureFamily`; it owns no copied feature intent or second tree. Fifteen current typed families
include imported Body bases, scalar/path Extrude, Revolve, Sweep, Loft, Surface variants, patterns,
Mirror, Fillet, Chamfer, Shell, Draft, Body Boolean and BodyTransform. Non-executable resources,
Sketches, PathCurves and Body aliases do not become feature rows.

One private typed-family visitor supplies query and replay registration. Existing `feature_order`
loading replays on a disposable complete Part candidate through the current validated adders,
restores non-feature consumers and original collection order, rejects incomplete/duplicate/unknown
orders and cycles, and publishes only after success. No JSON marker/field/default changes in A;
querying cannot author state. Canonical JSON validation retains its existing diagnostic category and
object identity. Authored ordering/suppression intent remains 146B, effective execution
147 and browser/End of Part workflows 148. This bounded foundation does not claim their completion.

## Authored history candidates — Block 146B, 2026-11-07

`PartFeatureHistoryIntent` extends the existing executable inventory
with complete ordered IDs and sparse suppressed IDs. `with_history_intent()` builds a disposable
complete Part; `apply_history_intent()` publishes only that validated result. `diagnose_history_intent()`
returns typed malformed-input, indispensable-base, dependency-order and typed-replay rejection
reasons. Queries expose `PartFeatureState::Active/Suppressed` without authoring anything.

Explicit order keeps independent positions. Output creators include modifying operations whose
result Body differs from their target. Same-Body predecessors are rebuilt through typed
adders rather than preserving the old sequence edges. Final validation follows dependencies through
non-executable resources; acyclicity alone is insufficient. Existing graph publications validate the
stored sequence, fresh executables append as active, and removals erase both position and suppression
before an ID can be reused. Family collection positions remain separate. Imported bases retain
source/catalog ownership and cannot be suppressed; Reference Parts remain immutable.

Flags never remove typed definitions or silently cascade to dependents. Effective modifier bypass,
creator-output availability and blocked diagnostics belong to 147 below; visible browser commands
remain 148. At the 146B delivery, Geometry conservatively rejected explicit history at execution,
selection, inspection and export boundaries. The 147B evaluator below replaces that temporary
boundary with validated results. An earlier full cache is never a result for changed explicit
intent. Normal GUI transactions/opening retain the old valid document on failure; repair opening
may retain failed complete intent with Failed status and an empty cache. Versioned persistence
and strict migration rules are owned by `file-format.md`.

## Effective availability plan — Block 147A, 2026-11-07

The read-only `create_history_evaluation_plan()` derives execution entries and Body availability
from the existing typed records and authored/dependency order. An optional prefix counts authored
rows, including suppressed rows; any explicit prefix remains a partial scope even at the last row.
It never edits source definitions, authored flags, graph, invalidation or canonical bytes.

Active entries require both their typed semantic/resource sources and their currently available
Body inputs. Suppressed modifiers preserve the previous Body producer but supply no semantic
feature output. Suppressed creators supply no new Body identity. An unavailable input derives a
Blocked entry with producer/input/consumer identities; it never changes that consumer's authored
Active flag or silently suppresses a closure. Independent Bodies remain available. Boolean tool
consumption follows the existing keep-tool contract. Imported bases cannot be suppressed by the
146 command; prefix exclusion still removes their output availability. Immutable Reference Part
outputs retain their existing source-owned basis.

Resource dependency traversal stops at the nearest executable producer: it does not mistake an
old sequential Body-predecessor edge for a semantic requirement. Body aliases are resolved against
availability at the current authored position and only when an actual Body owns that node.
A resource ID with the same textual prefix remains a resource. Existing typed fields supply profiles, axes, paths,
sections, topology references and Body results for every registered family. This is a disposable
projection, not an effective Geometry result, alternate persistent model, or completed H01/H02 gate.
147B remains responsible for physical execution and all cache/scope/freshness protections.

## Scoped Geometry evaluation — Block 147B, 2026-11-07

Full explicit-history and explicit prefix execution use the Core availability plan,
with its exact per-row Body predecessors, while retaining the complete original Part intent.
A fresh disposable cache removes suppressed/deleted/outside-prefix producers and stale transform
states. Active blocked inputs fail before publication; a failed kernel/source check leaves the
caller cache intact. Initial explicit-history incremental execution uses a complete fresh fallback
for correctness; later performance work may optimize verified clean producers.

Only the executor may seal a history result against exact canonical source intent, executable
lifetime generation, logical plan and scope after successful evaluation. Adding/removing executable
identities advances the transient generation, including identical ID/JSON reuse. Checkpoints retain
it; marking invalidation clean does not advance it. No lifetime/scope/cache field is persisted.
Any public cache mutation invalidates completion, and clearing retains its prior scope restriction. Complete
model consumers require a matching full result; even a prefix through the final row remains partial.
Direct single-feature calls cannot resurrect suppressed outputs outside orchestrated evaluation.
Body inspection/selection exclude unavailable creator outputs and consumed tools. Analytical local
reference queries check logical producer availability; world/picking/model consumers use validated
cache overloads. No query of authored local data certifies Geometry freshness.

Physical proof uses independent legacy geometry for all fourteen native families, exact volume and
bounds comparisons, suppression/restore and every prefix boundary. Additional cases cover independent
reorder, explicit dependent closure, identical ID/JSON lifetime reuse, a suppressed modifier before
transforms, invalid suffix publication, an imported base/boss/cut/Hole/Fillet/transform chain and its
STEP readback, immutable Reference Parts, assembly overlap/posed export and changed supplier bytes.
GUI proof combines scene selection, Measure, empty-result STEP preflight, Save/reopen, one-entry
Undo/Redo, failed transactions and explicit repair. Full/native results and final acceptance live in
`development-setup.md` and `mvp-plan.md`. Source intent stays unchanged by evaluation. Legacy caches
without history scopes retain their previous contract. Native browser/marker editing stays in 148.

## Staged browser history — Block 148A verified

`GuiPartHistoryController` owns a disposable proposal over the complete committed Part. Begin
captures canonical source and executable generation; stale sources, another task, immutable
Reference Parts or an unvalidated cache cannot authorize Apply. Suppression changes one requested
flag. The optional dependent closure is computed from effective blocked rows only after an explicit
choice. Unchecking recomputes from committed flags; restoring never silently restores other flags.
Sparse flags are normalized in authored order. A no-op proposal neither authors version 2 nor adds
history. Order proposals use the existing Core candidate validator, not Qt model order as authority.

Preview executes the complete candidate through the 147 evaluator. Apply revalidates source and
Geometry and publishes one existing Part/repair transaction; Cancel discards the proposal. No new
persistent field, feature registry or evaluator is introduced. The existing browser follows
`executable_history()` and exposes authored state separately from logical input availability.
Suppressed feature definitions and unavailable Body definitions remain inspectable; absent Body
outputs have no viewport selection identity. Scene publication omits unavailable derived supports,
Sketches and paths while retaining their intent and the fixed origin basis. Source availability
recognizes Body aliases by actual ownership and current evaluated Body shapes, including bypass.

`ShellHistoryTools` owns the task, shared property groups, context/ribbon/search actions and fixed
Apply/Cancel. Reorder offers list drag/drop and Up/Down; browser drops propose transactions without
mutating the committed tree. Widget ownership and presentation revision reject foreign or stale
drag data, and Core order validation rejects illegal positions before publication. The existing
feature editor supplies context Edit with its supported-family/immutable-source reasons; this slice
does not extend that editor's family catalog. Browser folding, scroll position, semantic selection,
filter and camera are retained where their owner still exists. Saving during preview writes
committed source. Native layout/error handling is specified in `user-interface.md`.

Whole 148 remains open. The End of Part prefix/edit/insert lifecycle and whole-block capability
promotion belong to 148B/C in `mvp-plan.md`; A's full-history preview is not a prefix editing context.

## End of Part session foundation — verified; Block 148B native integration pending

`GuiDocumentSession::begin_part_history_context()` starts the existing disposable Part workflow
with an explicitly partial evaluated cache. The document retains every suffix definition, graph
edge and authored flag. `history_prefix_count()` is transient and counts authored rows, including
suppressed rows; even a marker after the last row remains a partial editing scope. Full freshness,
ordinary complete scene/analysis and export stay unavailable. `history_prefix_ready()` checks the
exact source/lifetime and sealed prefix ownership independently of full-result freshness.

Idle marker moves compute into a disposable cache before replacing the previous marker/result.
Prefix transactions use the existing Part/Sketch mutation and sidecar paths. Newly added executable
rows are replayed at the marker using the single Core inventory and order validator; existing rows
cannot be removed or reordered by this insertion helper. Explicit future inputs reject insertion.
Successful insertion advances the marker past the inserted rows. Candidate preview prepares the
same complete intent without changing session state. A failed prefix leaves the prior draft intact;
an invalid unexecuted suffix remains defined and correctable.

Local draft Undo/Redo stores the transient marker with its in-memory history entry and recomputes
that prefix. It cannot traverse the committed workflow boundary. Save/Save As continue to publish
the complete entry checkpoint, including its Sketch sidecars. Cancel restores that checkpoint and
the original redo branch while preserving actual Save As metadata. Copied sessions share only the
immutable checkpoint; candidate documents, markers and caches remain independent.

`publish_part_workflow()` validates the complete suffix on a separate accepted session before
publishing one complete history entry and resetting the marker to ordinary end-of-history policy.
Failure retains the prefix draft. Marker-only visits add no history or persistence fields and still
revalidate complete geometry/imported suppliers. An active child task or lost prefix ownership
cannot publish. This foundation has no native marker control yet; existing feature owners are
integrated in the next serial part of 148B before native acceptance or capability promotion.

## Feature intent, not baked geometry

```text
Feature Extrude001
  type = AdditiveExtrude
  input_sketch = Sketch_BaseRectangle
  length = Parameters.plate_thickness
  direction = Sketch_BaseRectangle.normal
  output = OCCT_Shape (cache)

Feature Cut_HoleCircle
  type = SubtractiveExtrude
  input_sketch = Sketch_HoleCircle
  target_body = BasePlate
  depth = through_all
  direction = Sketch_HoleCircle.normal

Feature Revolve001
  type = RevolveFeature
  input_profile_region = Sketch_Profile.Region001
  axis = Sketch_Profile.CenterLine001
  angle = 360deg
  operation = join / cut / intersect / new_body
```

The finished OCCT geometry is only a cache derived from these rules. See `docs/shape-cache-mvp1-data-model.md`.

## Feature families

- Additive extrude, subtractive extrude (implemented as intent; rectangle/circle fast paths and line-based closed profiles are supported in the current recompute path).
- Body transform features: translate, rotate, scale, mirror, or matrix-transform a body inside one `PartDocument` while optionally moving owned sketches and construction geometry.
- Body boolean features: add/union, subtract, and intersect multiple bodies inside one `PartDocument`.
- Revolve and revolve cut features: rotational bodies and rotational cuts from sketch profiles and semantic axes.
- Sweep and sweep cut features: profile swept along line, polyline, arc, spline, construction geometry, connected path curve, or later semantic path references.
- Loft and loft cut features: two or more profile sections, with later guide curves, path curves, arbitrary sketch-plane orientations, and continuity settings.
- Path-following extrude and extruded cut: extrude semantics with an optional connected path curve instead of only a single sketch-normal direction.
- Surface features: boundary surface, fill surface, network surface, trim/extend surface, stitch/knit surfaces, and closed-shell-to-solid conversion.
- Edge features: fillets and chamfers (`docs/fillet-chamfer-features.md`).
- Hole features (`docs/hole-wizard.md`).
- Thread, emboss, engrave, rib, web, shell, and draft features as later sketch-driven or face-driven feature families.
- Pattern and mirror features (`docs/pattern-and-mirror-features.md`).
- Shaft feature (`docs/shaft-wizard.md`).

All of these are stored as first-class features in the feature tree, are managed by the dependency graph, and recompute on change. They all use semantic references (`docs/semantic-references.md`), never raw kernel IDs.

Block 58 provides the shared Core input vocabulary used by those later features:
`ProfileRegionReference`, `AxisReference`, `PlaneReference`, `FaceReference`, `EdgeReference`, and
`BodyReference`. Source identity, expected Geometry capability, and consumer role are separate;
generated face/edge sources reuse the stable Block-35 semantic topology types.

Block 59 broadens both existing Extrude Feature records without introducing a second feature tree.
`ExtrudeFeatureIntent` stores `Distance`, `Symmetric`, `TwoSided`, `ThroughAll`, `ToNext`,
`ToFace`, or `Between`, plus optional signed taper and thin intent. Distance/thickness fields are
Length-parameter references; ToFace/Between use Block-58 typed semantic face references. The
existing `FeatureBodyResultContext` remains the NewBody/Join/Cut/Intersect authority. Block 60 owns
the richer Geometry: deterministic spans, semantic planar limits, taper, the first open-line thin
profile, and all four Body operations. Historical straight Extrude/Cut remains an unchanged fast
path.

Block 61 adds persistent `RevolveFeature` intent with an explicit `Revolve`/`RevolveCut` kind,
typed `RevolveProfile` and `RevolveAxis` inputs, canonical Full/partial/symmetric angle semantics,
and mandatory Body result context. The document owns validation, graph/invalidation semantics, and
strict compatible JSON. Block 62 owns OCCT revolution, rotational cuts, and self-intersection
failure. It maps the supported profile regions and typed axes into model space, executes full,
partial, and symmetric revolutions, applies all four Body operations, and publishes cache results
only after valid solid construction. Block 63 adds ordered Feature/Body Pattern sources, persistent
Linear/Circular parameters and direction/axis intent, shared Body-result semantics, graph
invalidation, and compatible JSON without copying Feature records. Block 64 executes deterministic
Feature/Body Linear Patterns for spacing/total extent, reversed direction, and all Body operations.
Block 65 executes full/partial Feature/Body Circular Patterns around typed axes with deterministic
angular order, all Body operations, and incremental recompute. Block 66 MirrorFeature Core intent
and JSON adds ordered Feature/Body sources, typed Datum/Construction/semantic planar mirror-plane
identity, Body-result and graph semantics, and compatible strict JSON. Block 67 MirrorFeature
Geometry executes typed-plane reflection, all Body-result modes, and transactional recompute.
Block 68 adds ordered semantic Fillet/Chamfer edges, Body-history ownership, dimensional
parameters, three Chamfer modes, invalidation, and strict compatible JSON. Block 69 Fillet Geometry
executes constant-radius OCCT results with semantic edge recovery and transactional recompute.
Block 70 executes all three Chamfer modes with deterministic semantic reference sides and the same
transactional recompute boundary. Block 71 adds ordered semantic Shell removal faces, positive
Length thickness, explicit Inward/Outward direction, Body history, invalidation, and strict
compatible JSON. Block 72 resolves those faces on the current target solid, executes checked
inward/outward OCCT thick-solid offsets, and publishes only valid single-solid results
transactionally. Block 73 adds ordered semantic Draft faces, Axis/Line pull direction, typed
neutral planes, signed Angle parameters, Body history, invalidation, and strict compatible JSON.
Block 74 executes those references through signed OCCT draft geometry, validates a single solid,
and publishes parameter/upstream recompute transactionally. Block 75 adds separate model-space
`Sketch3D` ownership with explicit/parameter-driven points, point-referenced lines, ordered
polylines, and graph invalidation. Block 76 adds mixed-source point references, three-point Arcs,
Fit/Control Splines, typed Helices, and Guide-Curve roles. Block 77 adds strict deterministic JSON
and source-identity-only semantic references. Block 78 executes deterministic transient OCCT
Geometry without writing topology identity back into Core. Block 79 adds PathCurve intent, Block
80 adds Sweep/SweepCut/SweepSurface intent, Blocks 81–82 execute planar then spatial paths, twist,
and guide control, Block 83 adds path-following AdditiveExtrude/SubtractiveExtrude, Block 84 adds
persistent ordered Loft/LoftCut/LoftSurface intent, Block 85 executes two-section C0 Loft Geometry
on arbitrary planes, Block 86 executes deterministic ordered multi-section Loft Geometry, and
Block 87 adds center-path/ordered-guide control plus verified G1/C1 continuity. Block 88 adds the
first persistent Boundary/Fill/Trim/Extend/Stitch/Closed-shell-to-Solid Surface intent, semantic
references, Body-result authority, dependency/removal rules, and strict JSON. Block 89 executes
Boundary and Fill Surface Geometry; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented. Block 94 closes Part Construction MVP-6 through the Core/Geometry acceptance matrix in `docs/part-construction-mvp6-acceptance.md`; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.

The long-term sketcher and feature parity target is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

The target architecture for multi-body parts, body transforms, body booleans, path-following extrudes, and multi-section/path lofts is documented in `docs/multi-body-transform-and-path-features-roadmap.md`.

## Revolve and revolve cut

`RevolveFeature` and `RevolveCutFeature` are required feature families.

Target behavior:

- consume one selected closed profile region or a thin/open profile in later versions
- consume a sketch centerline, construction axis, datum axis, or semantic model axis as the revolve axis
- support full 360-degree revolve
- support partial-angle revolve
- support symmetric-angle revolve
- support join, cut, intersect, and new-body operation modes
- preserve the axis reference semantically
- reject self-intersecting results or invalid axis/profile combinations with clear errors

Revolve is needed for shafts, pulleys, cones, turned bodies, grooves, undercuts, washers, bushings, and lathe-like geometry. Revolve cut is needed for grooves, chamfer-like turned cuts, counterbores, undercuts, and rotational voids.

## Multi-body operations, transforms, and body booleans

Block 48 begins the transition away from one implicit final body: `PartDocument` can now own
multiple semantic Body records with stable identity. Recomputable per-body shape-cache output is a
later boundary.

Target behavior:

- `BodyId` now identifies a body inside one part file.
- The implemented Block-48 `Body` seed stores id, name, Solid/Surface kind, and visibility.
- Block 49 persists that seed in deterministic, backward-compatible `bodies[]` JSON.
- Blocks 50–51 add explicit NewBody/Join/Cut/Intersect Feature result context, compatible JSON,
  target/effective-result Body validation, producer/consumer graph edges, and invalidation.
- Blocks 52–53 add recomputable Body shapes and checked inspection.
- Block 54 adds persistent Add/Subtract/Intersect BodyBooleanFeature intent, JSON, graph ordering,
  invalidation, removal protection, and explicit tool retention.
- Blocks 56–57 add the persistent transform stack, ownership intent, OCCT execution, and derived
  affine/reference cache state.
- Source-feature catalogs, material override, and richer cache keys remain planned.
- Features support `operation_mode = new_body | join | cut | intersect`.
- Body transform records support translate, rotate, uniform scale, optional non-uniform scale, and later matrix transforms.
- Transform order is stored explicitly in a `BodyTransformStack`.
- Body booleans support add/union, subtract, and intersect between target and tool bodies.
- Tool-body consumption or preservation is explicit.
- Body operations remain semantic model intent and never directly persist raw OCCT shapes.

When a body is transformed, `SketchOwnership` controls whether sketches and associated construction
references move with the body. If `apply_to_owned_sketches = true`, the derived sketch workplane
frame moves, rotates, or scales while sketch-local 2D coordinates remain unchanged.

The detailed roadmap is in `docs/multi-body-transform-and-path-features-roadmap.md`.

## Sweep, loft, path-following extrude, and surfacing

Sweep Geometry through spatial/twisted/guided paths, path-following Extrude/Extruded Cut, and
path/guide-controlled multi-section Loft Geometry through verified G1/C1 are implemented. Broader
surfacing remains a first-class feature family in the active Part Construction sequence.

Target behavior:

- Implemented `PathCurve` stores an ordered connected path made from line, arc, spline, projected, construction, semantic, or 3D-sketch segment references, with explicit direction, closure, orientation, continuity hint, and tolerance.
- Implemented `AdditiveExtrude` and `SubtractiveExtrude` use `direction = path` with a
  dependency-tracked `PathCurveId` while remaining distinct from `SweepFeature`.
- Implemented `SweepFeature` consumes a profile and a path curve and executes bounded
  ConstructionLine or planar line/arc/polyline trajectories.
- Implemented `SweepCutFeature` removes that swept volume from a target body; `SweepSurface`
  executes an open planar PathCurve profile.
- Implemented `LoftFeature` consumes two or more explicitly ordered profile sections and may
  reference a primary path, ordered guide curves, and C0/G1/G2 continuity intent.
- Implemented `LoftCutFeature` persists removal intent for a target body.
- Implemented `LoftSurfaceFeature` accepts homogeneous closed or open PathCurve sections.
- Implemented Surface Core intent includes `BoundarySurfaceFeature`, `FillSurfaceFeature`,
  `TrimSurfaceFeature`, `ExtendSurfaceFeature`, `SurfaceStitchFeature`, and
  `ClosedShellToSolidFeature`, with semantic Boundary/Surface/trimming references and explicit
  Surface/Solid result Bodies.
- Profile-section sketches in a loft may be on arbitrarily oriented planes.
- Section ordering, seam/alignment references, and optional normal flips must be explicit to avoid random twist.
- `GuidedLoft` uses guide curves or rails to control shape flow.
- Implemented `BoundarySurface` and `FillSurface` Geometry creates face-only Surface Bodies from
  planar, spatial, construction/reference, and supported semantic generated-edge boundaries.
- Implemented `TrimSurface` resolves one closed planar contour against one unambiguous target face;
  implemented `ExtendSurface` extends one recovered linear edge of a planar polygon Surface.
- Implemented `SurfaceStitch` Geometry sews at least two semantic Surface inputs into one connected,
  manifold shell within an explicit positive tolerance, failing closed on gaps or non-manifold seams.
- Implemented `ClosedShellToSolid` Geometry validates one closed, consistently oriented, manifold
  shell and builds a positive-volume Solid Body, rejecting open/non-manifold/disconnected inputs.

The detailed 3D sketch and surfacing block is in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

The multi-body and path-feature target is in `docs/multi-body-transform-and-path-features-roadmap.md`.

## FeatureReference

A `FeatureReference` is a stable, semantic handle onto a geometric unit a feature produces ("top face of the base plate" rather than "Face 17"). Features expose the references they create so later features can bind to them. This is the mechanism that makes feature order robust against geometry changes.

## Bolt circle as a parametric object

A bolt circle must not be stored as a set of independent circles. It is its own parametric construct so the same radius/count can be reused across parts.

```text
Sketch_HoleCircle
  workplane = UpperFacePlane
  pattern:
    type = circular_hole_pattern
    center = sketch_origin
    radius = Assembly.bolt_circle_radius
    count = Assembly.bolt_count
    hole_diameter = Assembly.bolt_clearance_diameter
```

Changing `Assembly.bolt_count` from 8 to 12 produces twelve holes; changing `Assembly.bolt_circle_radius` moves the holes outward in every dependent part. The bolt circle is produced in practice by the hole wizard plus a circular pattern.

A first part-scoped seed of this pattern is implemented: `CircularHolePattern` with count parameters, dependency edges, recompute expansion into circular cuts, and JSON persistence. See `docs/bolt-circle-pattern-mvp3.md`. Assembly-scoped sharing has a first seed too: `AssemblyDocument` parameter bindings can drive the pattern parameters of several parts (`docs/assembly-parameters-mvp4.md`).

## Dependency-graph integration

A feature depends on its input sketch, selected profile region, parameters, target body, consumed construction geometry, body transform stack, path curve, guide curves, profile sections, and every semantic reference it consumes. When any of these change, the feature or body is marked invalid and recomputed in topological order. See `docs/dependency-graph-mvp1-data-model.md` and `docs/recompute-plan-mvp1-data-model.md`.

## Proposed implementation sequence beyond the current vertical slice

1. Introduce a common feature base so new feature types share id, name, references, parameters, and generated-shape handling.
2. Add `FeatureReference` as an exposed-output mechanism on features.
3. Add selected `ProfileRegion` inputs after automatic region detection exists.
4. Add `BodyId`, `Body`, and feature output-body metadata so a part can own multiple bodies. Body identity/ownership, output-body intent, persistence, and dependency semantics are implemented through Block 51.
5. Use the implemented Block-52 `ShapeCache` support for multiple body shapes.
6. Execute the implemented Block-56 body transform records for translate, rotate, and uniform scale.
7. Apply the implemented SketchOwnership intent to owned workplane frames when explicitly requested.
8. Extend the implemented Block-55 body boolean Geometry only through explicit later feature contracts.
9. Add `RevolveFeature` and `RevolveCutFeature`.
10. Extend the implemented circular-hole-pattern seed (`docs/bolt-circle-pattern-mvp3.md`) with assembly-scoped parameters, skip instances, and hole semantics.
11. Add richer extrude/cut extents: symmetric, two-sided, to-object, to-next, taper/draft, and thin features.
12. Add path curve records and path-following extrude/cut.
13. Add sweep, loft, and surfacing features after 3D sketch curves, arbitrary sketch-plane section mapping, and construction geometry are stable.
14. Add edge features and pattern/mirror features as separate blocks.

## Out of scope for the first versions

- arbitrary generated-face topology beyond the controlled semantic-face seed
- raw topology references in the core model
- geometry-copy patterns where semantic pattern intent is required
- full Inventor-like sketcher behavior before a stable sketch entity, profile-region, constraint, and dimension model exists
- automatic multi-body inference before explicit `Body` and `BodyTransform` records exist
- path-following feature twist minimization before path-frame rules exist
