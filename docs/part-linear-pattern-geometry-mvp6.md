# General Linear Pattern Geometry MVP-6

Status: implemented in Block 64.

## Boundary

Block 64 executes the persistent Block-63 `LinearPatternFeature` contract. Core remains authority
for ordered sources, direction identity, Count/Length parameters, spacing mode, direction sign,
and Body result. `LinearPatternAdapter`, `GeometryRecomputeExecutor`, and `ShapeCache` own derived
transforms and OCCT results only.

## Source and direction resolution

Feature sources resolve from their Feature-scoped cache result; Body sources resolve from their
Body-scoped result. An in-place target-Body source resolves the preceding producer shape from graph
order, never its own stale Pattern result.

Direction reuses the typed `PatternAxis` contract: DatumAxis, ConstructionLine, semantic primary
axis, and supported semantic generated linear edge. Reference transforms are applied before the
direction is normalized. `Negative` reverses the resolved vector without rewriting authored intent.

## Instance placement and order

Count is the total instance count and must resolve to at least two. For index `i`:

```text
Spacing:     distance(i) = i * extent
TotalExtent: distance(i) = i * extent / (count - 1)
```

The adapter returns the Block-63 canonical instance-major order:

```text
(instance 0, source 0..n), (instance 1, source 0..n), ...
```

Index zero is the original placement. Later indices are independent translated OCCT shapes. No
unrelated Feature record or persistent per-instance shape is created.

## Body operations and recompute

NewBody unites original and generated instances. Join, Cut, and Intersect build one deterministic
Pattern tool union before applying the Body operation. An index-zero source is omitted from that
tool only when its Feature/Body result is already the selected target Body; separate source Bodies
retain their index-zero occurrence.

Feature and Body results publish only through a working `ShapeCache` after source resolution,
instance generation, tool union, Body Boolean, and validation all succeed. Count, extent, source,
or direction changes therefore recompute from the same preceding producers and cannot apply a
Pattern repeatedly to its old result.

## Failure policy

Execution fails closed for missing/empty source or target shapes, a zero direction, Count below
two, invalid spacing, unresolved typed references, Boolean failures, or empty/non-solid results.
The caller's previous cache remains authoritative on failure.

## Proof

Focused verification:

```text
./build/blcad_geometry_tests "[geometry][linear-pattern]"
```

The suite proves deterministic two-source ordering, Feature- and Body-source NewBody patterns,
spacing and total-extent mapping, reversed direction, Join/Cut/Intersect, parameter-driven Count
and spacing recompute, exact volume/bounds, and transactional invalid-Count failure.

Block 65 General Circular Pattern Geometry is implemented in
`docs/part-circular-pattern-geometry-mvp6.md`; Block 66 MirrorFeature Core intent and JSON is
implemented in `docs/part-mirror-intent-mvp6.md`; Block 67 Geometry is implemented in
`docs/part-mirror-geometry-mvp6.md`. Blocks 72 ShellFeature Geometry and 73 DraftFeature Core
intent/JSON are implemented; Block 74 DraftFeature Geometry is implemented; Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is next.
