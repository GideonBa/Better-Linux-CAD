# Reference-generated profile helpers

This document describes the first seed for turning reference-driven sketch helper geometry into deterministic profile input.

The goal is to keep projected-reference helper geometry explicit and reproducible without adding a full sketch solver or automatic region detector yet.

## Implemented seed

The core model now has `ReferenceGeneratedLine` as an explicit helper-line intent record.

A `ReferenceGeneratedLine` stores:

- a helper line entity id
- the coincident projected-point constraint that defines the start endpoint
- the coincident projected-point constraint that defines the end endpoint
- an optional projected-line direction constraint

This is intentionally not a full sketch solver. The record says which already-resolvable constraints define a deterministic helper line.

## Geometry resolver

The optional geometry layer has `ReferenceGeneratedProfileResolver`.

It can resolve one `ReferenceGeneratedLine` by using `ReferenceDrivenSketchHelper` to evaluate the start and end projected-point constraints. If a direction constraint is present, the resolver verifies that the resolved helper line direction is consistent with the resolved projected-line constraint.

It can also resolve a `ClosedProfile` into ordered local vertices while allowing the profile line ids to be supplied by reference-generated helper-line records.

The resolver validates that the resolved profile lines are ordered and connected before returning profile vertices.

## Test coverage

The geometry tests cover:

- resolving deterministic helper profile vertices from projected-point endpoint constraints
- validating projected-line direction consistency
- rejecting a helper line whose endpoint-derived direction does not match the required projected-line direction

## Test workflow fix

The previous reported test output showed all geometry tests as `Not Run`. That usually means CTest saw discovered geometry test entries, but the `blcad_geometry_tests` executable was not built or not present in the expected build tree.

`CMakePresets.json` now includes workflow presets that run configure, build, and test together:

```bash
cmake --workflow --preset dev-build-test
cmake --workflow --preset dev-geometry-build-test
```

The second workflow is the relevant one for the geometry test suite.

## Current limitation

This seed adds the helper-line record and geometry resolver, but it deliberately avoids a broad rewrite of the existing `Sketch` validation and JSON deserialization path in the same step.

The remaining integration work is to make reference-generated helper lines first-class sketch JSON entities and to connect them directly into feature recompute and invalidation.
