# MVP 1 Feature Data Model

Status: pure data model with `PartDocument` integration, no geometry generation.

This document describes the current state of `Feature`, `AdditiveExtrude`, and `SubtractiveExtrude`.

The current scope is deliberately limited:

- no OCCT operations
- no BRep geometry
- no Boolean cut
- no recompute
- no ShapeCache in this feature data-model layer
- no GUI

## Goal

The feature data model represents the next layer of design intent:

- an `AdditiveExtrude` references a sketch and a length parameter
- a `SubtractiveExtrude` references a sketch and an existing target feature
- `PartDocument` validates that these references exist in the document

No geometry can be computed from this yet. Only the model intent can be stored.

## `Feature`

`Feature` is currently a compact data type with a feature type.

Current feature types:

- `additive_extrude`
- `subtractive_extrude`

Common fields:

```text
Feature
  id
  name
  type
  input_sketch
  direction
```

For MVP 1, the direction is only:

```text
+Z
```

## `AdditiveExtrude`

An `AdditiveExtrude` will later create a body from a sketch.

Current fields:

```text
AdditiveExtrude
  id = "feature.base_extrude"
  name = "BaseExtrude"
  input_sketch = "sketch.base"
  length_parameter = "part.thickness"
  direction = "+Z"
```

Validation in the feature:

- feature ID must not be empty
- name must not be empty
- input sketch ID must not be empty
- length parameter ID must not be empty

Validation in `PartDocument`:

- feature ID must be unique
- input sketch must exist in the document
- length parameter must exist in the document

## `SubtractiveExtrude`

A `SubtractiveExtrude` will later remove material from an existing target feature.

Current fields:

```text
SubtractiveExtrude
  id = "feature.center_hole_cut"
  name = "CenterHoleCut"
  input_sketch = "sketch.hole"
  target_feature = "feature.base_extrude"
  depth = "through_all"
  direction = "+Z"
```

Validation in the feature:

- feature ID must not be empty
- name must not be empty
- input sketch ID must not be empty
- target feature ID must not be empty

Validation in `PartDocument`:

- feature ID must be unique
- input sketch must exist in the document
- target feature must already exist in the document

## Order and dependencies

MVP 1 currently enforces a simple implicit order in `PartDocument`:

1. Add parameters.
2. Add a datum plane.
3. Add sketches.
4. Add an `AdditiveExtrude`.
5. Add a `SubtractiveExtrude` targeting the existing `AdditiveExtrude`.

A `DependencyGraph` now exists as a pure data model and is integrated into `PartDocument`. Feature references create graph edges:

- input sketch -> feature
- length parameter -> `AdditiveExtrude`
- target feature -> `SubtractiveExtrude`

Current validation still prevents references to missing objects.

## Test coverage

Current tests check:

- `AdditiveExtrude` stores sketch and length-parameter references.
- `AdditiveExtrude` rejects missing required fields.
- `SubtractiveExtrude` stores sketch and target-feature references.
- `SubtractiveExtrude` rejects missing required fields.
- `PartDocument` stores features.
- `PartDocument` rejects duplicate feature IDs.
- `PartDocument` rejects features with missing input sketches.
- `PartDocument` rejects `AdditiveExtrude` with a missing length parameter.
- `PartDocument` rejects `SubtractiveExtrude` with a missing target feature.
- `DependencyGraph` separately checks the fixed MVP-1 dependency order.
- `PartDocument` creates feature nodes and feature dependency edges.
- `InvalidationState` can mark dependent features as `dirty`.
- `RecomputePlan` can list `dirty` features in topological order.

## Next useful step

The first optional geometry adapter and additive execution for a single rectangle profile already exist. The next step should connect the subtractive path in a small way:

- execute `SubtractiveExtrude` from a recompute-plan node
- read the target shape from the existing geometry `ShapeCache`
- store the result as the new final shape
- keep OCCT behind the geometry adapter boundary
- do not build a GUI yet
