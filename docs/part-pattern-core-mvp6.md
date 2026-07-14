# General Part Pattern Core MVP-6

Status: implemented in Block 63.

## Boundary

Block 63 adds persistent, geometry-independent intent for general Linear and Circular Part
patterns. It does not copy Feature records, persist OCCT shapes, or execute transformed instances;
Linear Geometry is Block 64 and Circular Geometry is Block 65. The specialized sketch-level
`CircularHolePattern` remains unchanged.

## Shared source identity

`PatternSourceReference` stores exactly one typed `FeatureId` or `BodyId`. Face-only selections,
raw topology IDs, and raw OCCT shapes are not valid pattern sources. Source order is authored and
stable; duplicate or missing sources fail closed.

The canonical derived instance order is:

```text
(instance_index ascending, source_index in authored order)
instance_index 0 = original source placement
instance_index 1..count-1 = generated placements
```

No per-instance Feature copy is persistent. Future stable semantic instance identities derive from
the Pattern Feature ID plus this tuple.

## Linear and Circular intent

`LinearPatternFeature` stores ordered sources, a `PatternAxis` reference used as direction, a Count
parameter, `Spacing` or `TotalExtent`, a Length parameter for that extent, explicit positive or
negative direction, and mandatory `FeatureBodyResultContext`.

`CircularPatternFeature` stores ordered sources, a `PatternAxis` reference, a Count parameter, a
finite total angle in `(0, 360]` degrees, mandatory equal spacing, and mandatory Body result
context. Total angle follows the existing Revolve convention as authored degree intent; the
current Part parameter model has typed Length and Count parameters, so Count and linear extent are
the typed parameter references in this block.

Counts must resolve to at least two total instances. DatumAxis, ConstructionLine, semantic primary
axis, and semantic generated linear-edge sources reuse the Block-58 axis-reference contract.

## Body, dependency, and removal authority

NewBody, Join, Cut, and Intersect use the shared Body-result contract. Every source, direction/axis,
Count/Length parameter, target/prior Body producer, Pattern Feature, and effective result Body is
represented in the dependency graph. In-place Body-source patterns depend on the preceding Body
producer instead of creating a Body-to-itself cycle.

Parameter or semantic-axis changes invalidate the Pattern and its Body result transitively. A
referenced Body cannot be removed. Part-feature suppression/removal is not yet a mutable Part API;
therefore Block 63 neither invents a hidden suppression flag nor silently drops a source. Missing
sources during creation or loading fail transactionally.

## JSON

The optional-on-read, always-emitted `part_patterns[]` array stores both kinds in deterministic
dependency/topological order, preserving valid cross-kind source chains. Each item has
`type = linear_pattern | circular_pattern`, ordered typed `sources[]`, the concrete direction/axis
payload, Count/extent intent, and Body-result fields. Nested source and extent objects reject
unknown mode-specific shapes. Older files without the array restore zero general Patterns without
schema/version changes.

## Geometry boundary and proof

Blocks 64 and 65 execute Linear and Circular Pattern intent respectively. Both consume the ordered
Core sources and typed references without adding per-instance persistent Feature records.

Focused verification:

```text
./build/blcad_core_tests "[core][part-pattern]"
```

The suite covers source identity/order, validation, Linear/Circular fields, in-place Body chains,
dependency/invalidation, removal protection, compatible strict JSON, missing sources, and invalid
parameter types.

Block 64 Linear Pattern Geometry is implemented in
`docs/part-linear-pattern-geometry-mvp6.md`; Block 65 Circular Pattern Geometry is implemented in
`docs/part-circular-pattern-geometry-mvp6.md`; Block 66 MirrorFeature Core intent and JSON is
implemented in `docs/part-mirror-intent-mvp6.md`; Block 67 Geometry is implemented in
`docs/part-mirror-geometry-mvp6.md`. Blocks 72 ShellFeature Geometry and 73 DraftFeature Core
intent/JSON are implemented; Block 74 DraftFeature Geometry is implemented; Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is next.
