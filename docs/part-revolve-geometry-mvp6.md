# Revolve/RevolveCut Geometry MVP-6

Status: implemented in Block 62.

## Boundary

Block 62 executes the persistent Block-61 rotational feature contract in OCCT. Core remains the
authority for feature kind, profile/axis identity, angle extent, operation mode, and Body result.
`RevolveAdapter`, `GeometryRecomputeExecutor`, and `ShapeCache` own only derived geometry.

## Profile and axis resolution

The selected `ProfileRegionReference` is resolved through its sketch workplane into model-space
geometry. Rectangle, circle, line-based closed, arc/spline closed, and composite closed profiles
retain their exact selected profile identity; circle and arc profiles become OCCT curve wires rather
than faceted persistent data. Composite profiles preserve inner contours as holes.

Axis resolution supports:

- explicit and construction-line-derived `DatumAxis`;
- axis-capable `ConstructionLine`;
- the semantic primary axis of a supported circular subtractive-extrude producer;
- semantic generated linear edges.

Owned workplane/reference transforms are consumed from `ShapeCache`, so transformed profiles and
reference axes remain associative. No OCCT topology ID becomes persistent input identity.

## Angle execution

The Block-61 modes map to deterministic OCCT sweeps:

```text
Full                 start 0°, sweep +360°
Angle / Positive     start 0°, sweep +angle
Angle / Negative     start 0°, sweep -angle
Symmetric            start -angle/2, sweep +angle
```

Symmetric execution rotates a copied start face before the sweep; authored intent is not rewritten.
Full, partial, and symmetric results must contain a valid positive-volume solid.

## Body operations and recompute

NewBody publishes the revolved tool directly. Join, Cut, and Intersect reuse the shared deterministic
Body Boolean adapter. Modifying operations recover the preceding target-Body producer from graph
order, so incremental recompute never applies a feature repeatedly to its own old result.

Successful execution publishes both Feature- and Body-scoped cache entries. Single-solid Parts keep
the compatibility final shape; multi-body Parts clear it. Direct feature execution, plan execution,
and whole-document execution use a working cache and publish only after all profile, revolution,
boolean, and storage steps succeed.

Changing a semantic axis producer invalidates the Revolve through the Block-61 dependency edge.
Recompute executes the producer before the consuming Revolve and replaces the affected Body result.

## Failure policy

Geometry fails closed when:

- the profile or axis cannot be resolved;
- the axis is zero length, outside the profile plane, or normal to it;
- the profile is non-planar, has zero area, lies on the axis, or crosses the axis;
- OCCT produces no solid, a non-positive volume, or an invalid/self-intersecting result;
- a modifying operation has no target shape;
- Join/Cut/Intersect fails or produces no solid result.

An axis-crossing profile is rejected before OCCT because its rotational sweep is necessarily
self-intersecting. `BRepCheck_Analyzer` and positive-volume inspection guard the resulting shape as
well. Failure leaves the caller's previous cache byte-for-byte authoritative.

## Proof

Focused verification:

```text
./build/blcad_geometry_tests "[geometry][revolve-feature]"
```

The suite covers 360-degree, negative partial, and symmetric revolves; NewBody, Join, Cut, and
Intersect; a revolved groove/undercut fixture; semantic-axis invalidation and incremental recompute;
and transactional rejection of a self-intersecting axis-crossing profile.

Blocks 67 MirrorFeature Geometry, 72 ShellFeature Geometry, and 73 DraftFeature Core intent/JSON are
implemented; Block 74 DraftFeature Geometry is implemented; Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is next.
