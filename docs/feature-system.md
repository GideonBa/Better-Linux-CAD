# Feature System

Status: target architecture. MVP-1 implements `AdditiveExtrude` and `SubtractiveExtrude` intent models; line-based closed profiles are supported as first general-profile inputs. The richer feature tree remains a future block.

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
- Revolve and revolve cut features: rotational bodies and rotational cuts from sketch profiles and semantic axes.
- Sweep and sweep cut features: profile swept along line, polyline, arc, spline, construction geometry, or later semantic path references.
- Loft and loft cut features: two or more profile sections, with later guide curves and continuity settings.
- Surface features: boundary surface, fill surface, network surface, trim/extend surface, stitch/knit surfaces, and closed-shell-to-solid conversion.
- Edge features: fillets and chamfers (`docs/fillet-chamfer-features.md`).
- Hole features (`docs/hole-wizard.md`).
- Thread, emboss, engrave, rib, web, shell, and draft features as later sketch-driven or face-driven feature families.
- Pattern and mirror features (`docs/pattern-and-mirror-features.md`).
- Shaft feature (`docs/shaft-wizard.md`).

All of these are stored as first-class features in the feature tree, are managed by the dependency graph, and recompute on change. They all use semantic references (`docs/semantic-references.md`), never raw kernel IDs.

The long-term sketcher and feature parity target is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

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

## Sweep, loft, and surfacing

Sweep, loft, and surfacing are not first MVP features, but they are first-class future feature families.

Target behavior:

- `SweepFeature` consumes a profile and a path curve.
- `SweepCutFeature` removes a swept volume from a target body.
- `LoftFeature` consumes two or more profile sections.
- `LoftCutFeature` removes a lofted volume from a target body.
- `GuidedLoft` uses guide curves or rails to control shape flow.
- `BoundarySurface` and `FillSurface` create surfaces from spatial curve boundaries.
- `StitchSurfaces` / `KnitSurfaces` creates a shell from surfaces.
- `ConvertClosedShellToSolid` validates and converts a closed shell into a solid body.

The detailed 3D sketch and surfacing block is in `docs/advanced-surfacing-and-3d-sketch-mvp.md`.

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

## Dependency-graph integration

A feature depends on its input sketch, selected profile region, parameters, target body, consumed construction geometry, and every semantic reference it consumes. When any of these change, the feature is marked invalid and recomputed in topological order. See `docs/dependency-graph-mvp1-data-model.md` and `docs/recompute-plan-mvp1-data-model.md`.

## Proposed implementation sequence beyond the current vertical slice

1. Introduce a common feature base so new feature types share id, name, references, parameters, and generated-shape handling.
2. Add `FeatureReference` as an exposed-output mechanism on features.
3. Add selected `ProfileRegion` inputs after automatic region detection exists.
4. Add `RevolveFeature` and `RevolveCutFeature` as the first major feature types after construction geometry and profile-region selection.
5. Add the parametric bolt-circle / circular-hole-pattern feature.
6. Add richer extrude/cut extents: symmetric, two-sided, to-object, to-next, taper/draft, and thin features.
7. Add edge features and pattern/mirror features as separate blocks.
8. Add sweep, loft, and surfacing features after 3D sketch curves and construction geometry are stable.

## Out of scope for the first versions

- arbitrary generated-face topology beyond the controlled semantic-face seed
- raw topology references in the core model
- geometry-copy patterns where semantic pattern intent is required
- full Inventor-like sketcher behavior before a stable sketch entity, profile-region, constraint, and dimension model exists
