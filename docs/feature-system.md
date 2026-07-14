# Feature System

Status: target architecture. Additive/Subtractive Extrude, multi-body identity/results, Body Booleans, associative BodyTransform/SketchOwnership execution, and Block-58 reusable Part feature semantic inputs are implemented; richer Extrude/Cut intent is the Block-59 boundary.

Features are the parametric operations a part is built from. Each feature has inputs, parameters, references, and a computed geometry output. The central rule is that a feature stores the *rule* for computing geometry, not only the finished body. The OCCT shape is a cache.

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
Geometry without writing topology identity back into Core. Block 79 PathCurve intent is next.

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

Sweep, loft, path-following extrude, and surfacing are not first MVP features, but they are first-class future feature families.

Target behavior:

- `PathCurve` stores an ordered connected path made from line, arc, spline, projected, construction, semantic, or 3D-sketch segment references.
- `AdditiveExtrude` and `SubtractiveExtrude` may later use `direction_mode = path` with a `PathCurveId`.
- `SweepFeature` consumes a profile and a path curve.
- `SweepCutFeature` removes a swept volume from a target body.
- `LoftFeature` consumes two or more profile sections.
- `LoftCutFeature` removes a lofted volume from a target body.
- `LoftFeature` can later accept a path curve or guide curves.
- Profile-section sketches in a loft may be on arbitrarily oriented planes.
- Section ordering, seam/alignment references, and optional normal flips must be explicit to avoid random twist.
- `GuidedLoft` uses guide curves or rails to control shape flow.
- `BoundarySurface` and `FillSurface` create surfaces from spatial curve boundaries.
- `StitchSurfaces` / `KnitSurfaces` creates a shell from surfaces.
- `ConvertClosedShellToSolid` validates and converts a closed shell into a solid body.

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
