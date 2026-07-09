# Automatic profile region detection seed

This document describes the first deterministic automatic profile region detection seed.

The goal is to remove the need to manually predeclare every `ClosedProfile` for the simplest sketch cases while still avoiding a full sketch solver, automatic trimming, or general region graph search.

## Implemented scope

The optional geometry layer now has `SketchRegionFinder`.

It can inspect one sketch and produce a `GeneratedClosedProfileCandidate`:

- stable generated profile id
- ordered sketch line ids
- ordered local 2D vertices

The generated id currently has this deterministic shape:

```text
generated.region.<sketch-id>.0
```

The first seed supports exactly one simple single-contour loop.

## Supported input geometry

The region finder consumes local line-segment geometry that has already resolved into deterministic sketch-local lines.

Supported sources:

- unordered explicit `LineSegment` entities
- explicit lines with per-line driving dimensions after they resolve to local line segments
- `ReferenceGeneratedLine` helper entities after their projected-reference endpoint constraints resolve to local line segments

The finder deliberately does not inspect raw OCCT topology, raw face ids, raw edge ids, BRep handles, or GUI selection state.

## Validation behavior

The finder rejects:

- fewer than three lines
- duplicate line ids
- duplicate undirected edges
- degenerate edges
- disconnected loops and gaps
- branches
- ambiguous continuation choices
- non-closing loops
- self-intersecting regions
- multiple regions in one sketch

This is intentionally conservative. If a sketch has multiple possible regions, the first implementation should fail instead of guessing.

## Recompute integration

`GeometryRecomputeExecutor` now uses the region finder when a sketch has no explicit rectangle, circle, or closed profile.

This allows:

- additive extrude from one detected simple region
- subtractive through-all cut from one detected simple region

The detected local vertices are still mapped through the resolved sketch workplane and checked against generated-face bounds before OCCT geometry is created.

## Persistence behavior

The region finder itself does not serialize solver state or cached generated vertices.

If a generated region is user-selected and should become persistent model intent, the selected candidate can be converted to a normal `ClosedProfile` through `SketchRegionFinder::make_closed_profile`. That `ClosedProfile` stores only stable model intent:

```json
{
  "id": "generated.region.sketch.main.0",
  "line_segments": ["line.a", "line.b", "line.c", "line.d"]
}
```

This keeps JSON deterministic and avoids storing transient region-search caches.

## Test coverage

The tests cover:

- deterministic region detection from unordered explicit lines
- deterministic generated profile candidate IDs
- conversion of a generated candidate into a persistable `ClosedProfile`
- rejection of branched regions
- additive extrude recompute from a detected region
- subtractive through-all cut recompute from a detected region

## Deliberate limitations

This block does not implement a full sketch solver.

It does not detect multiple regions, inner loops, holes, islands, or nested contours.

It does not trim or extend curves.

It does not support arcs, splines, tangent constraints, driven dimensions, GUI region selection, 3D sketches, sweep, loft, surfacing, or arbitrary generated topology.
