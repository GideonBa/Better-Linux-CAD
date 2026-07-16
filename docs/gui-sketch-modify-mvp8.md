# GUI Sketch Modify: Trim, Extend, Split, Fillet, Chamfer MVP-8

Status: implemented in Block 116.

This contract adds curve trimming, extending, splitting, two-line fillet, and two-line chamfer over
the existing Block-108 Sketch entity model. Every operation rewrites a disposable candidate Sketch
that copies all unaffected intent verbatim and commits exactly one document transaction. No
operation partially mutates the source Sketch.

## Authority boundary

```text
selection / picked plane point
  -> SketchModifyService operation (headless Core)
     analytic line/arc intersection + rewrite plan
  -> remap-or-reject of profiles, embedded constraints/dimensions, tangent continuity
  -> candidate Sketch + invalidated source-entity ids
  -> GuiSketchModifyController preview
  -> one GuiDocumentSession::commit_part_transaction(...)
     re-derives the operation from the current committed Sketch
     re-validates Block-114 constraint and Block-115 dimension catalogs
  -> PartDocument::update_sketch(...)
```

`SketchModifyService` (Core, `blcad/core/sketch_modify.hpp`) owns the intersection mathematics and
the rewrite. `GuiSketchModifyController` (`blcad/gui/gui_sketch_modify.hpp`) owns preview and atomic
commit. Qt owns only action wiring and pick delivery. Intersections, projected parameters, rubber
bands, and picked points are transient; the persistent result is ordinary `LineSegment`,
`ArcSegment`, and `SplineSegment` intent.

## Implemented operations

| Operation | Inputs | Result |
|---|---|---:|
| Trim | line/arc target + pick point | shortened entity (id kept), removed-middle split, or removed entity |
| Extend | line/arc target + pick near an end | picked end moved to the nearest intersection beyond it (id kept) |
| Split | line/arc/spline target + point | two entities: first keeps the id, second gets `<id>.split.N` |
| Fillet | two line entities + radius | both lines trimmed to tangent points (id kept) + one tangent `ArcSegment` |
| Chamfer | two line entities + distance | both lines trimmed (id kept) + one connecting `LineSegment` |

Trim removes the picked span bounded by the target's nearest intersections with other line/arc
entities: an unbounded pick side shortens toward the interior, a span bounded on both sides removes
the middle and splits the target, and a target with no bounding intersection is removed entirely.
Extend treats the target support as unbounded and moves the endpoint nearest the pick to the closest
intersection strictly beyond it. Split uses De Casteljau subdivision for cubic splines.

Fillet and chamfer resolve the two lines' infinite-line intersection as the corner, move each line's
corner-side endpoint to its setback/tangent point, and insert the connector. `keep_trim=false`
inserts the connector without trimming the source lines.

## Remap or reject discipline

Every rewrite preserves or explicitly rejects affected intent before any candidate is built; the
source Sketch is never partially mutated.

- **In-place edits (id preserved, endpoint roles preserved):** trim/extend that moves one endpoint,
  and the fillet/chamfer setback of both lines keep the entity id. All referencing constraints,
  dimensions, and tangent continuity are preserved so usable constraints survive; a Horizontal
  constraint or a length dimension re-solves against the moved endpoint.
- **Invalidated ids (removed or split source):** a removed entity or the two halves of a split no
  longer carry the original endpoint semantics. If any embedded `SketchConstraint`,
  `SketchGeometricConstraint`, `SketchDrivingDimension`, or `SketchTangentContinuity` references an
  invalidated id, the operation fails closed and names the referencing intent.
- **Profiles:** any modification of an entity used by an ordered profile contour (closed,
  arc-closed, or composite) is rejected with an explicit diagnostic. Block 116 modifies loose
  geometry; profile-contour rewriting with connector splicing and profile-type promotion is a
  bounded later extension, not a silent partial mutation.
- **Block-114 / Block-115 catalogs:** the GUI commit reuses the generic Part transaction, which
  re-validates the constraint and dimension catalogs against the rewritten Sketch. A modification
  that drops an entity a catalog references fails closed with no partial mutation; in-place edits
  keep the catalog references valid.

`SketchModifyResult` returns the candidate Sketch plus the list of invalidated source ids so callers
can surface the affected geometry.

## Determinism and failure policy

- new split ids use `<source>.split.N`; fillet arcs use `arc.fillet.N`; chamfer lines use
  `line.chamfer.N`, each allocated as the first free id.
- non-finite picks, picks off the target curve, degenerate arcs, parallel or collinear
  fillet/chamfer lines, non-positive radius/distance, identical fillet/chamfer targets, missing
  targets, and unreachable extend all fail closed before any rewrite.
- fillet/chamfer require two `LineSegment` entities in this block; line/arc corners are a later
  extension.
- the GUI controller previews without mutating the document; commit re-derives the operation from
  the current committed Sketch, so a source that changed after preview cannot apply stale geometry.

## Persistence

Block 116 adds no JSON field or schema. Trimmed, extended, split, filleted, and chamfered entities
persist through the existing Sketch and PartDocument authorities. Intersections, projected
parameters, rubber-band previews, and invalidated-id lists are transient.

## Focused proof

```text
[core][sketch-modify]
[geometry][sketch-modify]
[gui][sketch-trim-extend]
```

```bash
./build/dev/blcad_core_tests "[core][sketch-modify]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-modify]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-trim-extend]"
```

The proof covers trim shorten/split-middle/remove, extend to the next intersection, line/arc/spline
split, fillet tangent-arc and chamfer line insertion, preservation of an in-place orientation
constraint, explicit rejection of split/trim that would invalidate a referencing dimension or
profile, degenerate and unknown-request rejection, a chamfer result resolving through the Geometry
region pipeline, atomic GUI preview/commit with exact undo/redo, and fail-closed commit against a
Block-114 catalog reference.

## Next boundary

Block 117 owns offset, projection/include, construction axes, and associative reference workflows.
Profile-contour rewriting under fillet/chamfer/trim, and line/arc corner treatment, remain bounded
later extensions of this contract.
