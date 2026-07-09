# Feature System

Status: target architecture. MVP-1 implements `AdditiveExtrude` and `SubtractiveExtrude` intent models; the general feature model and the bolt-circle feature are future blocks.

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
```

The finished OCCT geometry is only a cache derived from these rules. See `docs/shape-cache-mvp1-data-model.md`.

## Feature families

- Additive extrude, subtractive extrude (implemented as intent, `docs/feature-mvp1-data-model.md`).
- Edge features: fillets and chamfers (`docs/fillet-chamfer-features.md`).
- Hole features (`docs/hole-wizard.md`).
- Pattern and mirror features (`docs/pattern-and-mirror-features.md`).
- Shaft feature (`docs/shaft-wizard.md`).

All of these are stored as first-class features in the feature tree, are managed by the dependency graph, and recompute on change. They all use semantic references (`docs/semantic-references.md`), never raw kernel IDs.

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

Changing `Assembly.bolt_count` from 8 to 12 produces twelve holes; changing `Assembly.bolt_circle_radius` moves the holes outward in every dependent part. The bolt circle is the first meaningful cross-part feature test (MVP 3 in `docs/mvp-plan.md`) and is produced in practice by the hole wizard plus a circular pattern.

## Dependency-graph integration

A feature depends on its input sketch, its parameters, its target body, and every semantic reference it consumes. When any of these change, the feature is marked invalid and recomputed in topological order. See `docs/dependency-graph-mvp1-data-model.md` and `docs/recompute-plan-mvp1-data-model.md`.

## Proposed implementation sequence (beyond MVP-1)

1. Introduce a common feature base so new feature types share id, name, references, parameters, and generated-shape handling.
2. Add `FeatureReference` as an exposed-output mechanism on features.
3. Add the parametric bolt-circle / circular-hole-pattern feature (MVP 3).
4. Add edge features and pattern/mirror features as separate blocks.

## Out of scope for the first versions

- arbitrary generated-face topology beyond the controlled semantic-face seed.
- geometry-copy patterns (patterns must stay semantic, see `docs/pattern-and-mirror-features.md`).
