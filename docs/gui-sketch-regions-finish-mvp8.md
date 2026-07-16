# GUI Sketch Regions, Profiles, Diagnostics, and Finish Sketch MVP-8

Status: implemented in Block 119.

Block 119 turns solved visible planar line geometry into selectable bounded-region products and adds a fail-closed Finish Sketch candidate. Region recognition remains a derived Geometry service; the persistent result of finishing is an ordinary `ClosedProfile` added through `PartDocument::update_sketch(...)`.

## Authority boundary

```text
solved visible non-reference Sketch lines
  -> SketchFinishService::analyze(...)
     connected-component partition
     -> existing SketchRegionFinder single-loop validation per component
     -> deterministic region candidates + stable diagnostics
  -> point-based profile selection
  -> SketchFinishService::finish(...)
     complete copied Sketch + selected ClosedProfile
     -> one copied PartDocument candidate / update_sketch(...)
```

Qt may render fills, hover state, selected-region highlights, repair affordances, and diagnostic glyphs, but those products are transient. Geometry owns contour recognition and point-in-polygon selection. Core owns the resulting profile and document mutation.

## Region recognition

`SketchFinishService::analyze(...)` partitions explicit line entities into endpoint-connected components. Each component is validated by the existing `SketchRegionFinder`, so duplicate edges, gaps, branches, degenerate edges, ambiguous topology, and self-intersection reuse the established deterministic failure policy.

Every valid component produces a `GeneratedClosedProfileCandidate` with a stable id:

```text
generated.region.<sketch-id>.<zero-based-index>
```

Regions are returned in lexicographic id order. Derived polygon vertices and region fills are never persisted.

## Diagnostics

Failed components publish `SketchRegionDiagnostic` records with the involved entity ids, the original deterministic validation message, and one of:

- `OpenContour` for gaps, incomplete chains, and non-closing contours;
- `SelfIntersection` for crossing non-adjacent edges;
- `AmbiguousTopology` for branches, duplicate edges, and unsupported component topology.

Diagnostics do not mutate the Sketch and can be mapped directly to repair selection or zoom-to-problem behavior.

## Profile selection

`select_region_at(...)` performs deterministic point-in-polygon selection over recognized candidates. Selection is transient and represented by the candidate `ProfileId`. Multiple valid regions require an explicit selection before finishing; a single valid region is selected automatically.

## Finish Sketch

`finish(...)` fails closed when the target Sketch is missing, no bounded region exists, any component has a diagnostic, a multi-region Sketch has no explicit selection, or the requested region is stale. On success it:

1. copies the current Sketch;
2. materializes only the selected candidate as an ordinary `ClosedProfile`;
3. copies the `PartDocument`;
4. applies one `update_sketch(...)` operation to the copied document;
5. returns the complete candidate document and available region set.

The source document is unchanged on success or failure. GUI transaction/history authority can therefore commit the returned candidate as one Finish Sketch operation with exact undo/redo.

## Focused proof

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-regions]"
./build/dev-geometry/blcad_geometry_tests "[gui][sketch-profile-selection]"
./build/dev-geometry/blcad_geometry_tests "[integration][sketch-finish]"
```

The proof covers two disjoint selectable regions, stable point selection, open-contour diagnostics, fail-closed Finish Sketch, automatic selection of one region, profile materialization, and source-document immutability.

## Next boundary

Block 120 owns interactive Sketch3D creation and direct manipulation. Curved multi-region recognition, nested contour/hole classification, and automatic geometric repair beyond deterministic diagnostics remain explicit later extensions.
