# Part Extrude Extent Geometry MVP-6

Status: implemented in Block 60.

## Boundary

Block 60 executes the persistent Block-59 Extrude/Cut contract in OCCT. Core intent and JSON stay
unchanged. `GeometryRecomputeExecutor` resolves one deterministic axial span, constructs an
extrusion tool, applies the selected Body operation, and commits the result through the existing
Feature/Body `ShapeCache` boundary.

Historical additive Distance and subtractive ThroughAll execution remain their established fast
paths when no richer operation context is present. Existing regression tests therefore continue to
lock their shapes and recompute behavior.

## Extent execution

```text
Distance  -> [0, distance]
Symmetric -> [-distance/2, +distance/2]
TwoSided  -> [-second_distance, +first_distance]
ThroughAll -> target projection span plus deterministic margin
ToNext -> first positive OCCT face intersection from the profile center
ToFace -> sketch plane to one semantic planar limit
Between -> ordered span between two semantic planar limits
```

Distances must resolve to positive Length parameters. ThroughAll and ToNext require a target
shape. ToNext uses an exact forward face intersection rather than a bounding-box approximation.
ThroughAll deliberately uses a target bounding projection plus margin because its tool must cross
the complete target.

The first ToFace/Between increment accepts limit planes normal to the extrusion direction. A
cylindrical or oblique limit fails explicitly instead of producing an approximate result. The
semantic source is resolved through `WorkplaneResolver`; raw OCCT face identity is never written to
Core.

## Taper

A nonzero signed taper builds a solid loft between the start polygon and its planar offset at the
end span. The offset distance is exactly:

```text
tan(taper_angle_deg) * span_depth
```

Positive taper expands outward according to polygon orientation; negative taper contracts. Empty,
collapsed, non-planar, degenerate-edge, and OCCT-invalid results fail closed. Zero taper uses the
straight-prism path.

## Thin feature

The first supported open-profile thin feature consumes exactly one non-degenerate Sketch line.
Its wall region is built in Sketch space before extrusion:

```text
OneSided -> first thickness on the left side
MidPlane -> half the first thickness on each side
TwoSided -> first thickness left, second thickness right
```

Thicknesses must be positive Length values. Rectangle profiles, line-based closed profiles, and
detected closed regions use the same richer span/taper path. Richer arc/composite-hole profile
execution remains fail-closed; their historical straight paths remain available. General open
chains and curved thin profiles are later breadth, not silently approximated.

## Body operations and recompute

The generated tool participates in the existing `FeatureBodyResultContext` matrix:

| Mode | Geometry result |
| --- | --- |
| `NewBody` | tool becomes the produced Body |
| `Join` | target fused with tool |
| `Cut` | tool subtracted from target |
| `Intersect` | common volume of target and tool |

SubtractiveExtrude without an explicit Body context retains its historical target-Feature Cut
behavior. In-place Body operations recover the preceding target producer from the dependency graph
rather than reusing their already-modified Body result, so repeated incremental execution is
idempotent. Feature and Body cache products remain derived and no partial Core mutation occurs.

## Focused proof

```text
./build/blcad_geometry_tests "[geometry][extrude-extent]"
```

The proof covers symmetric/two-sided spans, ToFace, Between, exact ToNext, ThroughAll, positive
taper, a mid-plane open-line thin feature, richer SubtractiveExtrude, all four Body operations,
incremental idempotence, and the historical executor integration.

Blocks 61–62 persistent Revolve/RevolveCut intent, JSON, and rotational Geometry are implemented.
Blocks 67 MirrorFeature Geometry, 72 ShellFeature Geometry, and 73 DraftFeature Core intent/JSON are
implemented; Block 74 DraftFeature Geometry is implemented; Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is next.
