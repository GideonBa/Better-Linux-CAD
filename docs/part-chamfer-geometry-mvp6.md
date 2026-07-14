# Part Chamfer Geometry MVP-6

Status: implemented in Block 70.

Block 70 executes all three Block-68 `ChamferFeature` modes as OCCT geometry:
`EqualDistance`, `TwoDistance`, and `DistanceAngle`. Persistent intent continues to contain only
ordered semantic edge references and typed parameters; OCCT edge and face identities remain
derived recompute products.

## Edge and reference-side resolution

`ChamferAdapter` resolves each authored linear or circular edge against the current target solid
using the same endpoint or circle/profile/axis invariants as Fillet Geometry. Missing or ambiguous
topology fails explicitly.

`EqualDistance` is symmetric and requires no side identity. The asymmetric modes use a stable
derived reference-side convention:

- linear edge names prioritize their semantic Top, Bottom, Front, or Back face;
- circular hole rims require the uniquely adjacent planar face.

The first distance, or the distance in `DistanceAngle`, is measured on that reference face. The
angle is converted from persistent degrees to OCCT radians only at execution. No traversal index,
raw `TopoDS_Face`, or incidental face orientation is persisted.

## Recompute and failure boundary

The executor selects the latest target-Body producer, evaluates the mode-specific Length/Angle
parameters, and executes in a working cache. Feature, Body, and compatible final-shape products
are published only after OCCT completes and the result validates as a solid.

Parameter-only changes recompute the Chamfer alone. Upstream dimension changes rebuild the source
and recover the semantic edge and reference side on refreshed topology. Broken/ambiguous edges or
faces, invalid parameter combinations, excessive distances, incomplete OCCT operations, and
invalid solids preserve the previous valid cache.

## Verification

```text
./build/blcad_geometry_tests "[geometry][chamfer-feature]"
```

The focused suite covers single and ordered multi-edge equal-distance chamfers, both asymmetric
modes, deterministic reference-side behavior, a circular hole rim, parameter and upstream
recompute, broken-reference failure, excessive-distance failure, and transactional cache
preservation.

Blocks 72–74 ShellFeature Geometry and DraftFeature intent/JSON plus Geometry are implemented.
Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is next.
