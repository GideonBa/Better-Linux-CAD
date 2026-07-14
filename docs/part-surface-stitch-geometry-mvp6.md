# Stitch/Knit/Sew Shell Surface Geometry MVP-6

Status: implemented in Block 91.

## Scope

Block 91 executes the persistent `SurfaceStitchFeature` contract from Block 88. It sews an ordered
set of at least two semantic Surface inputs into one connected shell within an explicit positive
Length tolerance. Surface inputs may be `BodyKind::Surface` results or supported semantic planar
faces; each is re-resolved on every recompute. OCCT shells, faces, and edge identities remain
cache-only and are never written back into model intent.

## Stitch contract

Stitch gathers every face across the ordered inputs, rejecting empty inputs, solids, and faceless
surfaces, and requires at least two faces total. It sews the faces with `BRepBuilderAPI_Sewing`
using the resolved tolerance as the boundary match tolerance and validates:

- **boundary match tolerance** — the positive Length tolerance parameter drives OCCT sewing;
- **remaining free edges/gaps** — the sewn result must be exactly one shell containing every input
  face. Any leftover free face or additional shell means the surfaces did not connect within
  tolerance and fails closed as an unstitched gap;
- **face orientation consistency** — the shell must pass `BRepCheck_Analyzer`, which rejects
  unorientable or inconsistently oriented shells;
- **shell connectivity** — exactly one shell whose face count equals the input face count;
- **basic manifold suitability** — no edge may join more than two faces; non-manifold sewing
  (`NbMultipleEdges`) and non-manifold shell edges fail closed.

The result stays a surface/shell Body: the checked shape contains faces and no solid. There is
intentionally no automatic topology healing and no arbitrary gap closing beyond the authored
tolerance. A stitched shell keeps its open outer free edges; only closed-shell-to-solid conversion
(Block 92) requires a fully closed shell.

## Recompute and recovery

Semantic planar-face inputs are recovered from current producer geometry on every execution, and
Surface-Body inputs are re-read from the shape cache. Tolerance-parameter changes re-resolve and
re-sew the shell. Input resolution, sewing, validation, and feature/Surface-Body cache publication
are transactional; a failed stitch preserves the previous cache. Successful results contain faces
and no solid and never become the legacy single-solid `final_shape`.

## Persistence

Block 91 adds no JSON fields. Existing `surface_features`, semantic Surface references, and the
Length tolerance parameter contain all persistent intent. Generated shells, faces, and sewing
tolerances are never serialized.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][surface-stitch]"
```

The proof covers stitching two adjacent Surface bodies into one connected shell through the full
recompute pipeline, tolerance-driven recompute, direct-adapter edge-sharing connectivity, closed
failure when surfaces do not connect within tolerance, and rejection of a single-surface input and
a non-positive tolerance.

## Handoff

Blocks 48–94 are implemented. Closed-shell-to-solid conversion is canonical in
`docs/part-closed-shell-to-solid-geometry-mvp6.md`; `SurfaceStitchFeature` supplies its
consistently oriented, sufficiently manifold shell input. Block 93 multi-body STEP export and deterministic body naming is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is the current next technical step.
