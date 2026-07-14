# Closed Shell to Solid Surface Geometry MVP-6

Status: implemented in Block 92.

## Scope

Block 92 executes the persistent `ClosedShellToSolidFeature` contract from Block 88. It converts one
closed, consistently oriented, sufficiently manifold Surface shell into an explicit `BodyKind::Solid`
result that participates in later body booleans, transforms, and features through the same Block-48
authority. The shell input is a semantic `SurfaceReference` — normally a stitched Surface Body from
Block 91 — re-resolved on every recompute. OCCT solids and topology identities remain cache-only.

## Conversion contract

Conversion resolves the shell input and validates, with explicit diagnostics, that it is:

- **one connected shell** — exactly one `TopoDS_Shell`; a bare face or multiple disconnected shells
  fail closed;
- **closed** — every edge is shared by exactly two faces, so any free edge fails closed as an open
  shell;
- **manifold** — no edge is shared by more than two faces;
- **consistently oriented** — the shell passes `BRepCheck_Analyzer`.

A validated shell is marked closed and built into a solid with `BRepBuilderAPI_MakeSolid`.
`BRepLib::OrientClosedSolid` fixes the solid so it encloses positive volume, and the result must be
exactly one topologically valid solid with a strictly positive volume. There is intentionally no
automatic gap closing or topology healing: open, non-manifold, disconnected, and degenerate
zero-volume inputs are all rejected rather than repaired.

## Recompute and recovery

The shell input is re-resolved from current producer geometry on every execution, so upstream
stitch, boundary, or dimension changes flow through. Input resolution, solid construction,
validation, and feature/Body cache publication are transactional; a failed conversion preserves the
previous cache. The produced Solid Body publishes both a feature-shape and a body-shape entry, and a
single-body Solid document additionally exposes the historical compatible final shape.

## Persistence

Block 92 adds no JSON fields. The existing `surface_features` shell reference and Solid result Body
id contain all persistent intent. Generated solids, shells, and orientation state are never
serialized.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][surface-to-solid]"
```

The proof covers full-pipeline conversion of a six-face box (boundary faces → stitched shell →
solid) with Solid-body inspection and volume, direct-adapter conversion of a stitched box shell,
and closed failure for an open (free-edge) shell and for a lone face that is not a connected shell.

## Handoff

Blocks 48–94 are implemented. Multi-body STEP export is canonical in
`docs/part-multi-body-step-export-mvp6.md`; Block 94 acceptance is canonical in
`docs/part-construction-mvp6-acceptance.md`. Block 92 completes the surface-to-solid transition; the
converted Solid Body reuses the Block-48 Body authority for downstream modeling and Block 93
exchange. Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
