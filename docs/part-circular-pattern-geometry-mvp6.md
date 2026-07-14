# General Circular Pattern Geometry MVP-6

Status: implemented in Block 65.

## Boundary

Block 65 executes the persistent Block-63 `CircularPatternFeature` contract. Core remains
authority for ordered sources, axis identity, Count, total angle, equal spacing, and Body result.
`CircularPatternAdapter`, `GeometryRecomputeExecutor`, and `ShapeCache` own only derived rotations
and OCCT results. The specialized `CircularHolePattern` compatibility path is unchanged.

## Source and axis resolution

Feature sources resolve from their Feature-scoped cache result; Body sources resolve from their
Body-scoped result. An in-place target-Body source resolves the preceding producer shape from graph
order, never its own stale Pattern result.

The typed `PatternAxis` contract supports DatumAxis, ConstructionLine, semantic primary axis, and
supported semantic generated linear edge. Both axis origin and direction are resolved. Associative
reference transforms apply to both before instance rotation.

## Angular placement and order

Count is the total instance count and must resolve to at least two. For index `i`:

```text
Full circle (360 degrees): angle(i) = i * 360 / count
Partial total angle:       angle(i) = i * total_angle / (count - 1)
```

A full circle excludes a duplicate 360-degree endpoint. A partial pattern includes both authored
range boundaries. Positive angles follow the right-hand rule around the resolved axis. The adapter
returns canonical instance-major order:

```text
(instance 0, source 0..n), (instance 1, source 0..n), ...
```

Index zero is the original placement. No copied Feature record or persistent per-instance shape is
created.

## Body operations and recompute

NewBody unites original and generated instances. Join, Cut, and Intersect construct one
deterministic Pattern tool union before applying the Body operation. An index-zero source is
omitted from that tool only when its Feature/Body result is already the selected target Body;
separate sources retain their original occurrence.

Feature and Body cache entries publish only after axis/source resolution, rotations, tool union,
Body Boolean, and result validation all succeed. Count or axis-source changes therefore recompute
from the preceding producers and never reapply the Pattern to its own old result.

## Failure policy

Execution fails closed for missing or empty source/target shapes, unresolved axes, zero-length axis
direction, Count below two, an invalid total angle, disabled equal spacing, Boolean failure, or an
empty result. The caller's previous cache remains authoritative on failure.

## Proof

Focused verification:

```text
./build/blcad_geometry_tests "[geometry][circular-pattern]"
```

The suite proves full-circle endpoint exclusion, partial-angle boundary inclusion, deterministic
two-source order, Feature and Body sources, an offset semantic axis, NewBody/Join/Cut/Intersect,
exact volume and bounds, parameter-driven Count recompute, and transactional invalid-Count
failure.

Block 66 MirrorFeature Core intent and JSON is implemented in
`docs/part-mirror-intent-mvp6.md`. Blocks 67 MirrorFeature Geometry and 72 ShellFeature Geometry are
implemented; Blocks 73–74 DraftFeature intent/JSON and Geometry, Block 75 Basic 3D Sketch Core,
Block 76 richer 3D curve intent, and Block 77 3D Sketch JSON are implemented. Block 78 3D Sketch
Geometry conversion is next.
