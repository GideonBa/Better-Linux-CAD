# Part Sweep 3D Geometry MVP-6

Status: implemented in Block 82.

Block 82 extends the transactional Block-81 Sweep executor from planar spines to spatial
`PathCurve` sources and adds explicit twist plus an optional auxiliary guide. `Sweep`,
`SweepCut`, and `SweepSurface` continue to publish only complete, validated OCCT results.

## Spatial path execution

The resolver accepts model-space Sketch3D line, polyline, three-point arc, spline, and helix
segments. It also accepts connected paths that mix those segments with bounded ConstructionLines
and planar line/arc segments. Authored order and reversal remain authoritative.

Line, polyline, and three-point arc segments remain exact. Spline and helix sources are converted
to deterministic piecewise-linear spines before the pipe-shell operation: 32 samples for a spline
and 48 per helix turn. A helix resolves its axis, Length radius and pitch, Count turns, and
handedness on every recompute. Invalid entities, parameters, axes, disconnected paths,
self-intersection, and invalid OCCT output fail closed.

Semantic generated-edge path execution is still deferred.

## Orientation, twist, and guide

The Block-81 rules are frozen:

- `profile_normal` uses the Frenet frame;
- `minimum_twist` uses the corrected-Frenet frame;
- `fixed_up_vector` uses a constant binormal and rejects an initial parallel tangent.

An optional Angle parameter distributes an explicit rotation from zero to the authored angle over
the trajectory. An optional `guide_path` identifies a distinct open `PathCurve`; OCCT uses the
auxiliary spine with curvilinear correspondence and without contact/section scaling. Combining a
guide and explicit non-zero twist is rejected because the two controls would compete for the same
section orientation authority.

## Persistence and dependencies

`guide_path` is the only new persistent field. It is always emitted as null or a `PathCurveId` in
new Sweep records, participates in dependency invalidation and removal protection, and must differ
from the trajectory and an open surface profile. Readers also accept the older strict Block-80
record without the field. Resolved samples, frames, guide wires, and OCCT topology stay transient.

## Focused proof

```text
./build/dev-geometry/blcad_core_tests "[core][sweep-feature]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-3d]"
```

The proof covers guide dependency and compatible JSON, Angle-driven twist, an auxiliary guide,
solid sweep along a spatial spline, solid helical sweep, and the explicit guide-plus-twist failure.

Blocks 48–87 are implemented. Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is next.
