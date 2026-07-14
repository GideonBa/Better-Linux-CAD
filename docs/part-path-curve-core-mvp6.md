# Part PathCurve Core MVP-6

Status: implemented in Block 79.

Block 79 introduces reusable persistent path identity for Sweep, path-following Extrude/Cut, Loft,
and later Surface features. A `PathCurve` owns ordered semantic segment references; it never owns
OCCT edges or traversal indices.

## Persistent model

Each PathCurve stores:

```text
id, name
segments[]
closure = open | closed
orientation_rule = profile_normal | minimum_twist | fixed_up_vector
optional fixed_up_vector
optional continuity_hint = c0 | g1 | g2
connection_tolerance_mm
```

Each segment also stores its authored `reversed` direction. Supported sources are explicit planar
Sketch lines/arcs/splines, projected and reference-generated planar lines, ConstructionLines, 3D
Sketch lines/polylines/arcs/splines/helices, and semantic generated edges. Source kind and exact
owner/entity identity are explicit in JSON.

## Validation and dependencies

- ids, names, segment lists, tolerance, and FixedUpVector intent are validated strictly;
- every referenced source and its declared curve kind must exist;
- finite authored planar and 3D curves are checked in order within the stored positive tolerance;
- authored reversal participates in endpoint ordering;
- closed paths must close within the same tolerance;
- repeated source segments, branch points, and repeated internal junctions fail closed;
- unresolved infinite or derived sources are valid as standalone semantic paths, but cannot be
  silently joined without a provable endpoint connection;
- source nodes feed the PathCurve dependency node, so source removal is protected and invalidation
  propagates without copying geometry.

## JSON compatibility

`path_curves` is always emitted and optional on read. Every path and segment record has a strict
source-dependent field set; malformed, unknown, mistyped, missing-source, disconnected, and
ambiguous records fail loading. Files from before Block 79 load with zero PathCurves.

No resolved endpoints, sampled curves, or OCCT identities are persisted.

## Verification

```text
./build/dev-geometry/blcad_core_tests "[core][path-curve]"
```

The focused suite covers tolerance-sensitive planar connectivity, mixed authored direction,
3D-Sketch dependency/removal protection, all four source families, all orientation metadata,
strict JSON, and backward compatibility.

Blocks 48–87 are implemented. Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is next.
