# Composite closed profiles and holes seed

This document describes the first multi-contour profile seed.

The goal is to allow one profile operation to consume one outer contour and one or more inner contours for holes, while keeping contour intent explicit and deterministic.

## Implemented scope

The core model now has `CompositeClosedProfile`.

A composite profile stores:

- one ordered outer contour
- one or more ordered inner contours
- stable line-segment references only

Each contour is represented as an ordered list of `SketchEntityId` values. The first seed intentionally reuses the existing closed-line-loop validation path instead of adding a full region graph or sketch solver.

## Core validation behavior

`CompositeClosedProfile::create` validates:

- non-empty profile id
- one outer contour with at least three line ids
- at least one inner contour
- each inner contour has at least three line ids
- no empty line ids
- no duplicate line ids within a contour
- no line id shared between the outer contour and an inner contour
- no line id shared between inner contours

`Sketch::add_profile(CompositeClosedProfile)` validates that every contour is a closed, ordered, non-self-intersecting line loop using the current sketch entities.

## Geometry behavior

`ClosedProfileAdapter` now supports faces with inner wires:

- `make_extruded_profile_with_holes`
- `cut_profile_with_holes_through_all`

`GeometryRecomputeExecutor` accepts exactly one composite closed profile for additive extrude and subtractive through-all cut. Before OCCT geometry is created, the executor resolves each contour to local vertices, validates that inner contours are strictly inside the outer contour, rejects intersecting/overlapping inner contours, maps the contours through the resolved sketch workplane, and applies the existing generated-face bounds checks.

## Persistence behavior

The intended JSON representation is model intent only:

```json
{
  "composite_closed_profiles": [
    {
      "id": "profile.plate_with_hole",
      "outer_contour": ["outer.bottom", "outer.right", "outer.top", "outer.left"],
      "inner_contours": [
        ["hole.bottom", "hole.right", "hole.top", "hole.left"]
      ]
    }
  ]
}
```

This stores contour references only. It does not store BRep faces, wires, region-search caches, solved coordinates, GUI selection state, or OCCT topology.

## Test coverage

The tests cover:

- core storage of outer and inner contour intent
- sketch acceptance of a valid composite profile
- rejection of shared line ids across contours
- additive extrude from one composite closed profile with a hole
- subtractive through-all cut from one composite closed profile with a hole

## Deliberate limitations

This block does not implement automatic multi-region selection.

It does not support arcs, splines, trim/extend, tangent constraints, GUI profile picking, 3D sketches, sweep, loft, or surfacing.

It does not yet provide a full sketch solver or over/under-constrained diagnosis.
