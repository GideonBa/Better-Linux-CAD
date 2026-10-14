# GUI Sketch Modify: Trim, Extend, Split, Fillet, Chamfer MVP-8

Status: Block 116 implemented; shell hardening expands profile editing, curved corners and cubic
extension. This document describes the current contract rather than the original two-line MVP.

## Authority and transaction boundary

`SketchModifyService` (`blcad/core/sketch_modify.hpp`) owns intersection, projection and exact curve
rewrites. It builds a disposable Sketch and reports invalidated entity, endpoint and profile identities.
`GuiSketchModifyController` owns preview and re-derives the operation from the current document at
commit. Qt supplies selection and picks. Preview never changes document history.

The shell commits geometry, catalog cleanup, connector attachments, tangency and a driving connector
size through one `commit_part_sketch_intent_transaction`. Failure publishes none of these changes;
one undo restores geometry, parameters and both intent catalogs together. Corner intent authoring is
implemented in `gui_sketch_corner_constraints.hpp` and uses the combined dimension/constraint solver.

## Operations

| Operation | Supported inputs | Result |
|---|---|---|
| Trim | line, circular arc, cubic spline, full circle | remove the picked intersection-bounded span |
| Extend | line, circular arc, cubic spline | move the picked endpoint to the nearest reachable cutter |
| Split | line, circular arc, cubic spline, including ordered profile members | exact subdivision; first half retains the id |
| Fillet | two lines, or connected line/arc/spline pairs | circular connector of the requested radius, tangent to both sources |
| Chamfer | two lines, or connected line/arc/spline pairs | straight connector with equal arc-length setbacks |

Trim considers explicit curves and parameter-backed full circles as cutters. A middle span removal
creates two retained pieces; an unbounded end span shortens the source; without intersections the
whole target is removed. Trimming a full circle creates a circular arc. Trim may open an ordered
profile: the obsolete profile container is removed while remaining explicit curves survive.

Line and circular-arc extension use their supporting line/circle. Cubic extension continues the
original polynomial, rather than adding a straight tangent. It searches successively larger intervals
up to 64 original parameter lengths (stopping before a singular rational interval) and chooses the first reachable intersection with another explicit
curve. An unreachable cutter produces a diagnostic. An extended cubic retains its exact original
subcurve through parameter rescaling. Extend can open a profile and therefore invalidate its container.

Split uses exact polynomial/rational curve subdivision, splices both halves into each affected ordered contour, and
preserves the profile id. The second half receives `<source>.split.N`.

Two-line corners use the line intersection. Curved pairs must meet at one unambiguous endpoint.
Fillets solve intersections of the radius-offset curves and choose the valid solution with the
smallest total setback. Chamfer distances are measured along each source curve, including arcs and
splines. A requested setback that consumes an edge is rejected. `keep_trim=false` adds a connector
without replacing the source curves and does not add shell corner constraints.

## Profile and intent preservation

Split and corner edits preserve ordered closed, arc-closed and composite profile identities. Corner
targets in a profile must be adjacent in the same contour. Connector direction follows the contour,
regardless of selection order. A polygon receiving a fillet becomes an `ArcClosedProfile` under the
same id; a chamfer remains a polygon. Inner composite contours are treated in the same way.

Unchanged source ids and untouched endpoint relations survive. The GUI removes catalog entries whose
entity, profile or moved endpoint semantics have been invalidated; it does not silently retarget an
old whole-edge dimension to one half of a split. New corner attachments are coincident and fillets
receive tangent relations. The connector receives a driving radius (fillet) or length (chamfer); the
latter measures connector length, while the creation input specifies the two setbacks.

Legacy embedded references that cannot be remapped unambiguously still reject a destructive split
or removal and identify the blocking constraint, dimension or continuity. Projected/reference entities
are not editable source curves. Removing a profile required by an existing feature can likewise fail
recompute; the complete transaction then rolls back. These diagnostics protect existing design intent.

## Numerical and persistence rules

- New connector ids use first-free `arc.fillet.N` / `line.chamfer.N` allocation.
- Nonfinite input, zero/negative size, identical targets, off-curve split points, degenerate sources,
  ambiguous joins, consumed edges and unreachable extensions fail before publication.
- Curve intersection and offset solving use bounded numerical iteration; unsuccessful convergence
  returns an error rather than an approximate connector without tangency.
- All results persist as ordinary Core curves and profiles through the existing Part JSON. Preview
  samples, cutters, invalidation reports and numerical iteration state are transient.

## Verification

```bash
./build/dev-gui/blcad_core_tests '[core][sketch-modify]'
./build/dev-gui/blcad_geometry_tests '[geometry][sketch-modify]'
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests '[gui][sketch-modify]'
```

Core regressions cover trim, split, exact cubic continuation, line/arc, arc/arc and line/spline corners,
requested radius and tangent residuals, profile preservation/promotion, reversed selection, oversize
rejection and immutable source geometry. GUI cases cover persistent attachments, drag behavior,
combined geometry/constraint/dimension undo/redo and rejection without partial publication. Geometry
coverage verifies that rewritten curves continue through profile materialization.
