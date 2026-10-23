# Part Guided and Continuity-controlled Loft Geometry MVP-6

Status: implemented in Block 87.

Block 87 executes the persistent primary-path, ordered-guide, and continuity intent introduced in
Block 84. It extends the same transactional solid/surface and Body-result pipeline used by Blocks
85–86.

## Center-path control

An optional open `PathCurve` is resolved through the shared planar/spatial path resolver. It must
pass through every authored section center in section order. Between authored sections the adapter
inserts deterministic sampled control sections whose centers follow the path. The authored profile
sections themselves remain exact Loft sections.

Section `i` of `n` binds to normalized path-length station `i / (n - 1)` (three sections:
0, 0.5, 1; five sections: 0, 0.25, 0.5, 0.75, 1). Placement is not inferred by intersecting
arbitrary section planes with the trajectory. Planes may have distinct normals and in-plane roll,
but their profile centers must meet these stations within the binding tolerance (1e-5 mm).
Corresponding sections require equal edge counts; controlled interpolation also requires matching
edge kinds. Different sizes and polygon outlines are supported within that correspondence.

The control sampler retains resolved lines and circular arcs. Arc lengths and sampled positions
use the actual circle, including reversed arcs; replacing an arc with two chords changes the section
stations and is incorrect. Splines retain the shared resolver's deterministic 32 line spans. Four
subintervals per authored section interval (three intermediate control sections) constrain the Loft.
The resulting interpolated surface is not an exact analytic sweep of the entire trajectory.

This supports planar and model-space Sketch3D line, polyline, arc, sampled spline, and sampled helix
sources already accepted by the shared path contract. Degenerate, disconnected, closed, or
misbound paths fail before cache publication.

## Ordered guide control

Guides are resolved as ordered open `PathCurve` values. Their order distributes them
deterministically over seam-relative boundary points; two guides on a four-point profile therefore
bind the seam point and the opposite point, suitable for leading/trailing-edge control. Every guide
must pass through its bound point on every authored section at the corresponding normalized section
station.

Intermediate control sections interpolate matching authored edge kinds, then bind their selected
boundary points to sampled guide positions. Mismatched edge kinds/counts, excessive guides, or a
guide that misses an authored station fail closed. OCCT compatibility rewriting remains disabled.

## Continuity contract

- `c0` executes positional Loft continuity.
- `g1` requests the stronger OCCT C1 approximation and verifies that every resulting face surface
  reports at least C1 continuity before publication.
- `g2` remains persisted but fails explicitly because this implementation cannot yet guarantee and
  verify curvature continuity. It is never silently degraded to G1.

The continuity claim concerns the longitudinal Loft surface through section stations. Profile
corners remain intentional boundaries between side faces.

## Persistence and transaction

Block 87 adds no JSON fields. Block-84 `path_curve`, ordered `guide_curves`, and `continuity` values
are already persistent dependency authority. Resolved paths, sampled control sections, and OCCT
surfaces remain derived Geometry/cache products. Any control, continuity, Boolean, or shape failure
leaves prior feature and Body products unchanged.

## Focused proof

```text
./build/dev-gui/blcad_geometry_tests "[geometry][guided-loft]"
```

Coverage proves a center-path-controlled duct transition, a root/mid/tip blade-like Loft with
leading and trailing guides plus verified G1/C1 surfaces, and transactional G2 rejection.
`tests/geometry/mixed_path_loft_tests.cpp` (`[mixed-path-loft]`) adds three/five unequal rectangle and
trapezoid profiles on differently oriented and rolled planes, following a line–cubic spline–reversed
quarter-circle trajectory. An independent formula-based oracle checks section vertices and edge
midpoints against actual OCCT boundary faces, plus an intermediate arc station (distance <2e-5 mm).
It checks valid solids/surfaces, open-section center control, closed-section guide control, canonical
JSON/recompute, middle-profile edits, rejected upstream path/plane/G2 edits retaining the previous
native shape, invalid circular control geometry, and a constant-section Sweep comparison.
The first three cases failed against the old chord-based arc sampler before the correction.

Blocks 48–94 are implemented. Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
