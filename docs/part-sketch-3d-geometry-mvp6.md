# Part 3D Sketch Geometry MVP-6

Status: implemented in Block 78.

Block 78 executes the persistent Block-75/76 3D Sketch intent in model space. The Geometry layer
resolves current coordinates and semantic point sources and emits deterministic OCCT products;
Core remains independent of OCCT topology identity.

## Conversion boundary

`Sketch3DGeometryAdapter::convert(document, sketch_id)` returns a `Sketch3DGeometryResult` with:

- resolved local points in authored insertion order;
- one typed Geometry product for every point and curve, in the fixed order Point, Line, Polyline,
  Arc, Spline, Helix;
- stable source entity ids plus vertex, edge, and wire counts for inspection;
- an immutable `GeometryShape` containing the transient OCCT result.

Guide-curve records remain semantic roles over an existing source curve and therefore do not
duplicate Geometry products.

## Supported execution

- Explicit and length-parameter-driven 3D coordinates are evaluated at conversion time.
- Curve references resolve local 3D points, ConstructionPoints, and supported semantic points from
  planar sketches through `SketchReferenceProjector` and `WorkplaneResolver`.
- Lines become edges; polylines become ordered wires; three-point arcs become circular edges.
- Fit-point splines use the authored degree and continuity hint. Control-point splines use the
  authored poles and a deterministic clamped knot vector.
- Helices are exact cylindrical-surface curves around datum or ConstructionLine axes. Radius,
  pitch, turns, and handedness are evaluated from current parameters.

Invalid or unresolved input fails conversion without mutating the Part document. Degenerate
lines, disconnected/invalid curve definitions, unavailable parameters or sources, and kernel
construction failures are reported through `Result`.

## Persistence and topology

The adapter never writes resolved coordinates, sampled spline data, or OCCT edge identity back to
Core. Re-running conversion after a parameter change therefore regenerates Geometry from the same
stable semantic intent. Block 79 may consume these curves through persistent `PathCurve` segment
references, but it must not persist these transient OCCT handles.

## Verification

```text
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-3d]"
```

The focused suite covers all supported product kinds, parameter recomputation, failure behavior,
non-mutation of serialized Core intent, and a fit spline whose three points come from planar
sketches on three differently oriented workplanes.

Blocks 48–84 are implemented. Block 83 Path-following Extrude and Extruded Cut is implemented;
Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is next.
