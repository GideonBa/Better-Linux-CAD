# GUI Sketch Regions, Profiles, Diagnostics, and Finish Sketch MVP-8

Status: Block-119 profile extraction, exact curved-region recognition and open-Sketch completion in
the replacement shell.

Block 119 turns solved planar lines, circular arcs and polynomial/rational cubic splines into selectable
bounded regions. Recognition remains a derived Geometry service. Finishing materializes an ordinary
`ClosedProfile` for line-only contours or an exact `ArcClosedProfile` for curved contours through the
existing Part transaction authority.

## Authority boundary

```text
solved explicit Sketch lines/arcs/splines
  -> SketchFinishService::analyze(...)
     connected-component partition
     -> SketchRegionFinder single-loop validation per component
     -> deterministic region candidates + stable diagnostics
  -> point-based profile selection
  -> SketchFinishService::finish(...)
     complete copied Sketch + selected exact profile
     -> one copied PartDocument candidate / update_sketch(...)
```

Qt may render fills, hover state, selected-region highlights, repair affordances, and diagnostic glyphs, but those products are transient. Geometry owns contour recognition and point-in-polygon selection. Core owns the resulting profile and document mutation.

## Region recognition

`SketchFinishService::analyze(...)` partitions explicit line, arc and spline entities into
endpoint-connected components in lexicographic source-id order. Line-only components retain the
existing line finder. Curved components use `find_single_curve_region`: endpoint degree must be two,
then connectivity determines contour order and direction. Two-edge contours are supported, including
a line plus a semicircle or two circular arcs. Gaps, branches, overlaps, degenerate edges and
self-intersections fail closed.

Curved intersection validation uses OCCT's planar curve intersector against exact circular and
weighted Bezier curves, including self-intersection within a single spline. Only intended shared
endpoints are accepted. Failed intersection convergence is a diagnostic, not an accepted profile.
Source curve coordinates, ids, directions and rational weights remain unchanged.
Core profile validation uses the existing refined curve intersector for spline pairs, replacing the
old straight-endpoint-chord check. Thus a valid bulging spline is not rejected merely because its
chord crosses another contour edge. Automatic region validation additionally uses exact OCCT curves.

Every valid component produces a `GeneratedClosedProfileCandidate` with a stable id:

```text
generated.region.<sketch-id>.<zero-based-index>
```

Regions are returned in lexicographic id order. `curve_segments` retains exact source ids for curved
candidates; `line_segments` retains the line-only contract. `source_entities()` serves selection and
profile recovery for both families. Derived polygon vertices and region fills are never persisted.
The curved pick polygon uses `1e-3 mm` deflection sampling with supplementary spline samples and a
bounded point budget. Viewport faces use exact OCCT edges; feature materialization never consumes the
pick polygon as its persistent contour.

## Diagnostics

Failed components publish `SketchRegionDiagnostic` records with the involved entity ids, the original deterministic validation message, and one of:

- `OpenContour` for gaps, incomplete chains, and non-closing contours;
- `SelfIntersection` for crossing non-adjacent edges;
- `AmbiguousTopology` for branches, duplicate edges, and unsupported component topology.

Diagnostics do not mutate the Sketch and can be mapped directly to repair selection or zoom-to-problem behavior.

## Profile selection

`select_region_at(...)` performs deterministic point-in-polygon selection over recognized candidates. For nested loops, the smallest containing contour wins, so a pick inside an enclosed spline selects
that inner region and a pick outside it selects the surrounding material. Selection is transient and
represented by the candidate `ProfileId`. Multiple valid regions require an explicit selection before finishing; a single valid region is selected automatically.

## Finish Sketch

`finish(...)` fails closed when the target Sketch is missing, no bounded region exists, any component has a diagnostic, a multi-region Sketch has no explicit selection, or the requested region is stale. On success it:

1. copies the current Sketch;
2. materializes only the selected candidate as a line-only or exact curved profile;
3. copies the `PartDocument`;
4. applies one `update_sketch(...)` operation to the copied document;
5. returns the complete candidate document and available region set.

The source document is unchanged on success or failure. GUI transaction/history authority can therefore commit the returned candidate as one Finish Sketch operation with exact undo/redo.

`analyze_unmaterialized` excludes curves already consumed by line, arc or composite profiles and
allocates collision-free generated ids. Viewport presentation, browser/curve selection, feature
preview and shell Finish use this same identity boundary. The shell materializes every recognized
independent closed component together. Selection-first Extrude materializes its raw profile together
with the feature in one transaction; cancel leaves raw geometry and history unchanged.

## Focused proof

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-regions]"
./build/dev-geometry/blcad_geometry_tests "[gui][sketch-profile-selection]"
./build/dev-geometry/blcad_geometry_tests "[integration][sketch-finish]"
./build/dev-gui/blcad_geometry_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[sketch-curved-regions]"
```

The proof covers disjoint selectable regions, reversed source directions, two-edge circular contours,
exact rational ellipse presentation, source-weight preservation, curve/self intersections and
overlaps, open/branch diagnostics, profile materialization, analytic extrusion volumes, browser
selection, cancel isolation, dimension preservation, one-step Undo/Redo and Save/Open.

## Next boundary

Block 120 owns interactive Sketch3D creation and direct manipulation. Nested line/arc/polynomial or rational spline loops now participate in the existing immediate-child
hole classifier. Both raw and materialized contours produce exact OCCT hole wires in profile
presentations and exact swept subtraction tools during additive extrusion. Sampling serves only
containment and picking; source poles, weights and authored profile identities remain unchanged.
Nested islands remain independently selectable; only immediate child loops are removed from a
selected outer region. Crossing/touching contours are excluded from inferred holes. Splitting
intersecting components into multiple cells and geometric repair remain extensions. Component recognition is not a planar
arrangement solver. Parameter-backed Circle/Rectangle profiles remain existing materialized profiles;
projected/reference curves do not enter automatic raw-region recognition.

## Open-Sketch completion

`ShellWindow::finish_active_sketch` also completes an open planar Sketch. It cancels unfinished tool
previews, validates document diagnostics and materializes independent closed line/arc/spline regions, then
restores the model workspace and camera. Open components remain ordinary editable curves; they do
not gain a fabricated profile. Completing a purely open Sketch changes no document intent or history.
The status bar explains that its open contours remain available for later editing, including browser
re-entry and Save/Open.

Self-intersecting closed loops and branched region topology still reject completion without mutation.
Region diagnostics explicitly distinguish endpoint degree greater than two from a genuine open end.
`SketchFinishService::finish` remains the strict bounded-profile extraction API. Extrude/Revolve
continue to require a real bounded profile. An existing independent closed profile can be used even
when unrelated open geometry remains in the Sketch.

Proof: `[gui][shell-sketch][sketch-finish]` covers open completion, exact geometry/history preservation,
re-entry, profile handoff beside open geometry, and rejection of invalid closed topology.

## Nested curved-region regression coverage

`[nested-spline-region]` checks a rectangle around a rational ellipse before and after outer-only
materialization, exact viewport face area, exact extrusion volume, independent inner/outer selection,
cancel isolation, source pole/weight preservation, Undo/Redo and Save/Open. The GUI workflow draws
the rectangle and ellipse through the existing authoring tools and previews both selected regions.
