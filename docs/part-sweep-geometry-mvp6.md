# Part Sweep Geometry MVP-6

Status: implemented in Block 81.

Block 81 executes the persistent Block-80 Sweep intent through a shared OCCT pipe-shell adapter.
Sweep, SweepCut, and SweepSurface participate in normal dependency-ordered recompute and publish
their results transactionally to both feature and Body shape caches.

## Supported paths

The first execution boundary accepts open finite paths made from:

- a ConstructionLine defined through two ConstructionPoints;
- ordered direct lines from planar sketches;
- direct three-point arcs from planar sketches;
- planar polylines expressed as connected ordered line/arc segments.

Authored segment direction is honored. The adapter rebuilds a connected OCCT spine and rejects
open/closed mismatches, disconnected segments, invalid arcs, invalid OCCT products, and detected
self-intersection. Block-79 Core validation remains authoritative for branches and repeated
junctions.

An explicit ConstructionLine is infinite and therefore has no usable sweep length. It fails
closed; a bounded line-through-two-points must be used for Block 81.

## Profiles and operations

Solid Sweep and SweepCut support rectangle, circle, line-closed, and arc/spline-closed profile
regions. SweepSurface accepts an open planar line/arc PathCurve as its profile. Composite profiles
with holes are deferred because a multi-wire pipe requires a separate topology contract.

`new_body`, `add`, `cut`, and `intersect` use the existing Body Boolean and Body-history paths.
Surface results remain surface Bodies and do not become the compatibility final solid shape.

## Orientation

The effective rule is the feature override when present, otherwise the PathCurve rule:

- `profile_normal` selects the Frenet frame;
- `minimum_twist` selects the corrected-Frenet frame;
- `fixed_up_vector` selects a constant binormal and rejects a vector parallel to the initial path
  tangent.

Block 82 extends this boundary with spatial Sketch3D paths, deterministic spline/helix sampling,
explicit twist, and guide-controlled execution. Its canonical extension is
`docs/part-sweep-3d-geometry-mvp6.md`.

## Transaction and persistence boundary

Execution starts from a copy of the current ShapeCache. A failed profile, path, orientation,
Boolean, or OCCT operation leaves all prior feature and Body products unchanged. Resolved wires,
frames, and OCCT shapes remain transient; Block 81 adds no persistent JSON fields.

## Focused proof

```text
./build/dev-geometry/blcad_geometry_tests "[geometry][sweep-feature]"
```

The focused suite covers bounded ConstructionLine, planar line/polyline/arc trajectories, all
three orientation rules, solid Sweep, SweepCut, SweepSurface, Body-cache publication,
unbounded-line rejection, fixed-up-vector validation, and transactional failure.

Blocks 48–85 are implemented. Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is next.
