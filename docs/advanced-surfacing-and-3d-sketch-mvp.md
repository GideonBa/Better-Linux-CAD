# Future MVP: Advanced Surfacing, 3D Sketches, Sweep, Loft, and Surface-to-Solid

Status: partially implemented. Blocks 75–76 provide 3D Sketch Core and richer curve intent;
JSON references, Geometry conversion, Sweep, Loft, and surfacing remain planned.

This document records the long-term requirement that BLCAD should support advanced 3D curve and surface modeling. This is intentionally separate from the first closed-profile sketch MVP and from construction geometry.

## Goal

The future goal is to model smooth freeform parts from curves, profiles, guide curves, and surfaces while still preserving parametric model intent.

Target use cases include:

- turbine blades
- propeller blades
- wings and airfoils
- smooth duct transitions
- transitions between two or more pipes
- organic fairings
- bodies defined by multiple cross sections
- path-following extrudes and cuts where a profile follows a connected route
- lofts through two or more arbitrarily oriented sketch planes

This requires more than planar 2D sketches and simple straight extrudes.

## Required capabilities

BLCAD should eventually support:

- 3D sketch points
- 3D sketch lines
- 3D polylines
- 3D splines
- 3D guide curves
- reusable connected `PathCurve` records
- connecting points from different sketches on different planes with a spline
- curves whose control points come from multiple sketches or construction points
- surfaces generated from arbitrary spatial curves
- surfaces generated between two or more sketches
- surfaces generated between many non-parallel or arbitrarily oriented sketches
- sweep-style features along lines, polylines, arcs, or splines
- extrude-style features that optionally follow a connected path curve
- loft-style features between two or more profile sketches
- path- or guide-controlled lofts where a connected path/rail controls transition flow
- smooth multi-section lofts with tangent or curvature continuity through intermediate profiles
- surface filling from boundary curves
- surface stitching / knitting from multiple faces
- converting a closed set of stitched surfaces into a solid body

## 3D sketching

Block 75 allows basic point-, line-, and polyline-based sketches to exist directly in model space
instead of only inside one planar sketch. Block 76 adds Arc, Spline, Helix, Guide-Curve, and
mixed-source point-reference intent. Blocks 77–78 add references/JSON and Geometry conversion.

The target entities include:

```text
SketchPoint3D
SketchLine3D
SketchPolyline3D
SketchSpline3D
SketchGuideCurve3D
PathCurve
```

The first five entities and `Sketch3D` ownership are implemented; Block 76 additionally implements
`SketchArc3D` and `SketchHelix3D`. Reusable `PathCurve` remains planned.

A 3D spline should be able to reference:

- explicit 3D points
- construction points
- points from sketches on different construction planes
- points from sketches on generated semantic faces
- later, generated vertices or semantic edge points

Example:

```text
sketch.airfoil_root.point.leading_edge
sketch.airfoil_mid.point.leading_edge
sketch.airfoil_tip.point.leading_edge
  -> spline.leading_edge_guide
```

This lets a user define several sketches on different planes and connect corresponding points with smooth 3D guide splines.

## Path curves

A `PathCurve` should represent a connected route composed from multiple semantic curve segments.

Target model:

```text
PathCurve
  id
  segments[]
    sketch line / sketch arc / sketch spline / projected curve / construction line / 3D sketch curve / semantic edge
  closure = open | closed
  orientation_rule = profile_normal | minimum_twist | fixed_up_vector | guide_curve_later
```

Rules:

- path segments must be ordered and connected within tolerance
- branches are rejected in the first implementation
- self-intersection may be rejected in the first implementation
- path segments are semantic references, not raw OCCT edges
- the orientation rule must be explicit before profile-swept geometry is generated
- the same `PathCurve` concept should be reusable by sweep, path-extrude, path-cut, loft, and surface features

## Multi-sketch lofting

A loft feature should create a surface or solid through multiple profile sketches.

Basic path:

```text
Sketch profile A
Sketch profile B
  -> LoftSurface / LoftSolid
```

Multi-section path:

```text
Sketch profile A
Sketch profile B
Sketch profile C
Sketch profile D
  -> MultiSectionLoft
```

For parallel profiles, the feature should support an arbitrary number of sections:

```text
profile[0], profile[1], ..., profile[n]
  -> smooth loft
```

For arbitrarily oriented sketches, every section must be resolved through its own workplane frame and mapped into model space before lofting.

If there are three sketches, the middle sketch should act as an intermediate section that shapes the transition. The transition through the middle sketch should be smooth when requested and should not force an unwanted hard edge at the middle section.

The continuity mode should eventually be explicit:

```text
C0 = positional continuity, visible transition edge allowed
G1 = tangent continuity, visually smooth transition
G2 = curvature continuity, smoother surface flow
```

The first implementation may start with simpler C0 or G1 behavior, but the model intent should leave room for explicit continuity settings later.

## Guide curves and path-controlled lofts

Lofting should eventually accept guide curves and optional path curves.

Example:

```text
root airfoil sketch
mid airfoil sketch
tip airfoil sketch
leading edge guide spline
trailing edge guide spline
  -> guided loft surface or solid
```

Guide curves are important for blades, wings, propellers, ducts, and pipe transitions because the profiles alone may not control twist, sweep, chord change, or transition direction precisely enough.

Guide curves may come from:

- 3D sketches
- splines connecting points from multiple planar sketches
- construction points and construction lines
- semantic edge references in later stages
- connected `PathCurve` chains

A future path-controlled loft should accept:

```text
LoftFeature
  sections[]
  path_curve optional
  guide_curves[] optional
```

This supports duct/pipe transitions where the section profiles are distributed along a route instead of only linearly interpolated.

## Sweep and path-following extrude features

A sweep feature should move a profile along a path.

Target path:

```text
profile sketch
path line / polyline / arc / spline / connected PathCurve
  -> SweepSolid / SweepSurface
```

Required sweep paths include:

- construction lines
- 3D sketch lines
- 3D polylines
- 3D splines
- connected line/arc/spline path curves
- later, semantic edges or projected curves

Sweep should support at least:

- solid sweep from a closed profile
- surface sweep from an open profile
- path orientation rules
- optional twist control
- optional guide or rail curves in later stages

Extrude and extruded cut should also be able to use a path-following mode:

```text
AdditiveExtrude
  input_profile
  direction_mode = sketch_normal | opposite_sketch_normal | vector | path
  path_curve = optional PathCurveId

SubtractiveExtrude
  input_profile
  target_body
  direction_mode = sketch_normal | opposite_sketch_normal | vector | path
  path_curve = optional PathCurveId
```

This covers features such as pipes, hoses, ribs, blade edges, swept transitions, and extruded cuts following a routed path.

## Surface generation from spatial curves

BLCAD should eventually be able to create surfaces from arbitrary curves in space.

Examples:

```text
boundary curve A
boundary curve B
boundary curve C
boundary curve D
  -> BoundarySurface
```

```text
curve network
  -> FillSurface / NetworkSurface
```

This is needed when the desired shape is not naturally described as a simple extrude, revolve, sweep, or loft.

Supported inputs should eventually include:

- curves from one sketch
- curves from several sketches on different planes
- 3D sketch curves
- projected curves
- construction geometry
- generated semantic edges in later stages
- connected `PathCurve` records

## Surface-to-solid conversion

A future surface workflow should allow users to build separate faces and then convert them into a solid if they form a closed volume.

Target path:

```text
Surface A
Surface B
Surface C
...
Surface N
  -> stitched shell
  -> solid body if shell is closed
```

The operation may be called:

```text
KnitSurfaces
StitchSurfaces
SewShell
ConvertClosedShellToSolid
```

Validation must check:

- surfaces form a closed shell
- boundary edges match within tolerance
- no large gaps remain
- face orientations are consistent
- the resulting shell is manifold enough to create a solid

The core should store the user intent to stitch or convert, while OCCT shell/solid creation remains in the geometry layer.

## Proposed model concepts

Possible future model types:

```text
SketchPoint3D
SketchCurve3D
SketchSpline3D
GuideCurve
PathCurve
PathSegmentReference
ProfileSectionReference
LoftFeature
SweepFeature
PathExtrudeFeature
PathCutFeature
BoundarySurfaceFeature
FillSurfaceFeature
SurfaceStitchFeature
ClosedShellToSolidFeature
ContinuityMode
```

Possible continuity settings:

```text
C0Position
G1Tangent
G2Curvature
```

Possible feature inputs:

```text
ProfileSectionReference
GuideCurveReference
PathCurveReference
BoundaryCurveReference
SurfaceReference
```

## Proposed implementation sequence

This is a large feature family and should be built in stages.

1. Add explicit 3D points and 3D line segments as model-intent objects.
2. Add 3D spline curves with control points or interpolation points.
3. Allow a 3D spline to reference points from multiple sketches on different planes.
4. Add JSON serialization and roundtrip tests for 3D sketch curves.
5. Add geometry-layer conversion from 3D curves to OCCT edge/wire representations.
6. Add reusable `PathCurve` records for connected line/arc/spline chains.
7. Add a simple sweep along a straight construction line.
8. Add sweep along a 3D polyline or spline path.
9. Add path-following extrude and extruded cut along a connected path curve.
10. Add a simple loft between two closed planar profile sketches.
11. Add loft support for non-parallel or arbitrarily oriented sketch planes.
12. Add multi-section loft through three or more profiles.
13. Add explicit continuity settings so intermediate sketches can be smooth transition controls instead of hard section edges.
14. Add guide-curve and path-controlled lofts using one or more 3D splines or `PathCurve` records.
15. Add surface generation from boundary curves.
16. Add surface stitching / knitting for multiple generated surfaces.
17. Add closed-shell validation and conversion to a solid.
18. Add tests using airfoil-like or blade-like section sketches.
19. Add tests for pipe-to-pipe transition lofts.

## First useful acceptance tests

A minimal implementation path should eventually prove:

- a 3D spline can connect points from three sketches on three different planes
- the 3D spline survives JSON roundtrip
- a connected `PathCurve` can be composed from several line/arc/spline segments
- a sweep can create a solid along a straight construction line
- a sweep can create a solid along a 3D spline path
- an extrude can create a solid along a connected path curve
- an extruded cut can remove material along a connected path curve
- a loft can create a body between two closed profile sketches
- a loft can create a body between two profile sketches on non-parallel planes
- a multi-section loft can use three or more profile sketches
- a middle profile can control a smooth transition without creating an unwanted hard edge when smooth continuity is requested
- guide splines or a path curve can influence a loft between root, mid, and tip profiles
- several generated surfaces can be stitched into a shell
- a closed stitched shell can be converted into a solid
- a non-closed surface set is rejected with a clear validation error

## Deliberate limitations for early versions

Early versions should not attempt to implement the full surface modeling stack at once.

Out of scope for the first surfacing increment:

- full Class-A surfacing tools
- arbitrary NURBS editing UI
- interactive surface fairing
- automatic topology repair
- automatic gap closing beyond a strict tolerance
- general-purpose surface healing
- advanced blade-design optimization
- CFD-specific blade design tools
- GUI control cages
- assembly-level surfacing
- body-level transforms and body booleans, which are tracked separately in `docs/multi-body-transform-and-path-features-roadmap.md`

## Relationship to other roadmap blocks

This block depends on earlier foundations:

```text
Construction geometry answers: where can sketches and curves be placed?
General closed sketch profiles answer: what planar profile can be used?
3D sketch and surfacing answers: how can spatial curves, multiple profiles, and surfaces form freeform geometry?
Multi-body transforms answer: how can generated bodies be moved, combined, and kept associated with owning sketches?
```

This block should not replace simpler extrude/cut features. Simple features remain important fast paths. Advanced surfacing is a later layer for forms that cannot be expressed cleanly by planar extrudes alone.

The multi-body, body-transform, body-boolean, and path-feature target is documented in `docs/multi-body-transform-and-path-features-roadmap.md`.
