# General Closed Sketch Profiles MVP

Status: first line-based closed-profile MVP implemented.

This document records the implemented first step from primitive-only sketches toward general CAD sketch profiles. The implementation is intentionally narrow: it supports ordered line-segment loops and uses them as one closed planar profile for additive or subtractive extrudes. Newer canonical sketch/feature contracts govern later profile capabilities; GUI editing of the complete implemented sketch authority is planned in Block 99.

The larger Inventor-like sketcher and sketch-driven feature roadmap is documented in `docs/inventor-like-sketcher-and-feature-roadmap.md`.

## Implemented scope

The first implementation adds:

- `SketchEntityId` as a stable typed ID for sketch entities
- `LineSegment` sketch entities with stable IDs and endpoint coordinates
- `ClosedProfile` objects referencing ordered `LineSegment` IDs
- validation that a closed profile has at least three line segments
- validation that referenced line segments exist in the owning sketch
- validation that line segments are ordered and connected
- validation that line-segment IDs are unique inside the closed profile
- validation that a closed profile does not self-intersect initially
- JSON serialization and deserialization for `line_segments` and `closed_profiles`
- geometry-layer conversion from closed profile vertices to an OCCT wire and face
- additive extrude support for exactly one closed profile
- subtractive through-all extrude support for exactly one closed profile
- workplane resolution before closed-profile geometry execution
- bounds validation for closed-profile vertices on bounded derived workplanes
- geometry tests for a non-rectangular triangle prism
- geometry tests for a non-circular triangle through-all cut
- checked-in `.blcad.json` examples for a triangle prism and triangle cut plate

## Current model path

The implemented path is:

```text
LineSegment[]
  -> ClosedProfile
  -> ordered local vertices
  -> resolved workplane points
  -> OCCT polygon wire
  -> OCCT face
  -> AdditiveExtrude or SubtractiveExtrude
```

For additive extrudes, the closed profile is extruded along the resolved workplane normal by the feature length parameter.

For subtractive through-all extrudes, the closed profile is converted into a through-all prism along the resolved workplane normal and then used as a Boolean cut tool.

## JSON shape

Sketch JSON now supports these fields in addition to the existing rectangle and circle profile arrays:

```json
{
  "line_segments": [
    {
      "id": "line.a",
      "start": {"x": 0.0, "y": 0.0},
      "end": {"x": 20.0, "y": 0.0}
    }
  ],
  "closed_profiles": [
    {
      "id": "profile.triangle",
      "line_segments": ["line.a", "line.b", "line.c"]
    }
  ]
}
```

The line segment order is significant. The end point of each line must match the start point of the next line. The final line must connect back to the first line.

## Examples

The repository contains two line-based closed-profile examples:

```text
examples/triangle_prism.blcad.json
examples/triangle_cut_plate.blcad.json
```

The triangle prism example demonstrates additive extrusion from a non-rectangular closed profile.

The triangle cut plate example demonstrates subtractive through-all extrusion from a non-circular closed profile.

## Validation rules

The current validation rejects:

- empty line segment IDs
- zero-length line segments
- duplicate sketch entity IDs inside one sketch
- closed profiles with fewer than three line segments
- duplicate line-segment references inside one closed profile
- closed profiles that reference missing line segments
- line chains whose consecutive endpoints do not connect
- line chains whose final endpoint does not close back to the first start point
- self-intersecting line loops
- closed-profile vertices outside a bounded resolved workplane

## Recompute behavior

The recompute executor now accepts:

```text
AdditiveExtrude:
  exactly one RectangleProfile
  or exactly one ClosedProfile
```

and:

```text
SubtractiveExtrude:
  exactly one CircleProfile
  or exactly one ClosedProfile
```

Rectangle and circle remain fast-path primitives. Closed profiles are an additional broader path, not a replacement.

## Test coverage

Core tests cover:

- line segment construction
- zero-length line rejection
- closed profile construction
- duplicate closed-profile line references
- ordered-loop validation
- disconnected-loop rejection
- self-intersection rejection
- JSON roundtrip for line segments and closed profiles

Geometry tests cover:

- additive triangle prism recompute
- subtractive triangle through-all cut recompute
- updated additive/subtractive profile validation messages

## Deliberate limitations

The first closed-profile MVP does not implement:

- arcs
- circles as general sketch entities
- rectangles as generated constrained sketch entities
- ellipses
- splines
- polygons
- slots
- sketch text
- projected geometry
- trimmed curves
- offset profiles
- sketch fillets or sketch chamfers
- sketch constraints
- tangent, parallel, perpendicular, or dimensional sketch constraints
- automatic region detection from unordered curves
- multiple independent closed profiles in one feature
- inner holes in one profile
- profile selection from multiple regions
- arbitrary non-planar sketch geometry
- revolve or revolve cut features
- 3D sketch splines connecting points from sketches on different planes
- loft, sweep, boundary surface, surface stitching, or closed-shell-to-solid features
- GUI sketch editing

These are intentionally tracked in the broader Inventor-like sketcher and feature roadmap.

## Relationship to other roadmap blocks

The current MVP 2 semantic-face workplane seed proves where sketches can be placed on generated planar faces.

This closed-profile MVP proves that a single planar sketch can define a non-rectangular closed area from explicit line segments.

Construction geometry remains the next foundational block for placing user-defined planes, lines, and points freely in 3D.

The Inventor-like sketcher roadmap is the long-term target for full 2D sketch entities, editing tools, constraints, dimensions, automatic region detection, revolve/revolve cut, richer feature creation, and GUI sketch editing.

Advanced surfacing remains a later block for 3D curves, guide splines, sweeps, lofts, boundary surfaces, surface stitching, and closed-shell-to-solid conversion.
