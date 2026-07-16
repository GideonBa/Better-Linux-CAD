# GUI Sketch Spline Editing and Text MVP-8

Status: implemented in Block 113.

This contract adds fit/control-point spline authoring, insertion/removal, control polygons, continuity
handles, deterministic representation conversion, and parameter/expression-backed Sketch text. It
extends the existing Block-108 topology, Block-109 solver, Block-110 drag, and Block-111/112 creation
boundaries without introducing a second persistent curve or transaction authority.

## Authority boundary

```text
selected persistent SplineSegment chain
  -> SketchSplineEditModel disposable authoring candidate
     -> control handles / fit handles / control polygons / continuity handles
     -> deterministic cubic SplineSegment chain
  -> GuiSketchSplineController preview
     -> SketchSplineGeometryEvaluator samples / curvature / C0-G1 diagnostics
  -> one GuiDocumentSession Part transaction
     -> PartDocument::update_sketch(...)
```

The only persistent planar spline curve remains the existing cubic `SplineSegment`:

```text
start -> control1 -> control2 -> end
```

Fit points, selected handles, hover state, control polygons, sampled display curves, curvature samples,
and continuity glyph positions are transient authoring or Geometry products. They are not an
alternative model format.

## Control-point editing

`SketchSplineEditModel::from_segments(...)` accepts one or more ordered, connected spline entities.
The authoring preview exposes semantic endpoint and control-point handles plus one control polygon per
segment. Moving an internal control point replaces only that cubic segment. Moving a shared endpoint
updates the adjacent segment endpoint as part of the same candidate so the chain cannot tear.

Reference geometry is not accepted as an editable spline source. The GUI controller snapshots the
selected source segments when the editor opens and rechecks them at commit. A stale editor therefore
fails before persistent mutation.

## Fit-point editing and deterministic conversion

Control-to-fit conversion samples each source segment at its midpoint while preserving the ordered
endpoints. Fit-to-control conversion uses a uniform Catmull-Rom-to-cubic-Bezier expansion. For the
ordered fit points `P0 ... Pn`, segment `Pi -> P(i+1)` uses:

```text
C1 = Pi     + (P(i+1) - P(i-1)) / 6
C2 = P(i+1) - (P(i+2) - Pi)     / 6
```

At the chain boundaries the missing neighbour equals the endpoint. The expansion is deterministic,
uses the first existing spline id first, then stable `<base>.fit.N` ids, and produces exact shared
segment endpoints.

Fit-point insertion and removal rebuild the complete disposable chain. At least two distinct fit
points must remain. Non-finite points, duplicate consecutive points, degenerate cubic output, stale
source state, invalidated profile connectivity, or invalidated external continuity fail closed. The
source Sketch remains unchanged during preview.

## Continuity handles

Every adjacent spline pair exposes one continuity handle containing the junction, incoming tangent,
outgoing tangent, C0 state, and G1 state. Tangent alignment preserves each handle length while placing
both tangent vectors on one directed line. Successful commits persist ordinary cubic control points
and the existing `SketchTangentContinuity` records; no opaque solver result is serialized.

`SketchSplineGeometryEvaluator` publishes deterministic per-segment positions, first derivatives,
second-derivative curvature, control polygons, and C0/G1 diagnostics. These products are derived and
never model identity.

## Atomic GUI workflow

`GuiSketchSplineController` is independent of Qt widgets and owns the disposable authoring model.
`preview()` returns a complete candidate Sketch and Geometry products without changing
`PartDocument`. `commit()` performs one:

```text
GuiDocumentSession::commit_part_transaction("Edit sketch spline", ...)
```

The transaction verifies source freshness, rebuilds the complete Sketch, validates all copied profiles,
constraints, dimensions, references, and continuities, and calls `PartDocument::update_sketch(...)`.
Undo/redo therefore restores exact complete document snapshots.

## Sketch text intent

`SketchText` is persistent annotation intent with:

```text
SketchTextId
SketchId
UTF-8 template text
requested font family
height ParameterId
optional rotation ParameterId
plane anchor Point2
explicit ${token} -> ParameterId bindings
```

The height parameter must be a positive Length. Rotation, when present, must be an Angle. Binding
parameters may be direct or expression-driven; text resolves the already evaluated `PartDocument`
parameter values and therefore follows the same expression/dependency authority as other parametric
intent. Count, length, and angle values use deterministic unit-bearing formatting. Missing parameters
or unresolved template tokens fail closed.

## Text persistence

Block 113 deliberately does not reinterpret historical `blcad.part_document.mvp1`. Text annotations
use the canonical sidecar schema:

```text
schema  = blcad.sketch_text.mvp8
version = 1
```

`SketchTextCatalog` is scoped to one `DocumentId`, sorts text records lexicographically by stable id,
and rejects duplicates. The sidecar stores template text, requested font, parameter ids, anchor,
optional rotation, and ordered binding records. Resolved strings, glyph strokes, selected fallback
font, and diagnostics are derived and are not serialized.

## Font fallback and vector layout

`SketchTextLayoutResolver` receives the currently available font-family names. Resolution order is:

```text
requested family
  -> configured fallback families in order
  -> built-in BLCAD Simplex Stroke
```

A missing requested family emits `font_missing`. A system fallback emits `font_fallback`. If no
configured family exists, the deterministic built-in single-stroke vector font is used and
`font_builtin_fallback` is emitted. Layout rotates and translates the generated plane-space strokes
from the persistent anchor. Operating-system font files, glyph caches, and screen rasterization never
become persistent intent.

## Failure policy

The following publish no partial mutation:

- empty or duplicate spline source ids;
- disconnected ordered spline chains;
- non-finite or duplicate consecutive fit points;
- fewer than two retained fit points;
- collapsed tangent handles;
- stale selected source segments at commit;
- profile or continuity invalidation after insertion/removal;
- missing Sketch text target or parameter;
- non-Length text height or non-Angle rotation;
- unresolved or duplicate text binding tokens;
- malformed or unsupported sidecar JSON.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-spline-edit]"
./build/dev/blcad_core_tests "[core][sketch-text]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-spline-edit]"
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-text]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-spline]"
```

The proof covers control- and fit-point editing, insertion/removal, deterministic conversion and ids,
control polygons, C0/G1 handles and diagnostics, candidate immutability, one atomic GUI transaction,
exact undo/redo, expression-backed text substitution, sidecar roundtrip, system-font fallback, and the
built-in vector-font fallback.

## Next boundary

Block 114 owns selection-driven manual constraints, accepted automatic constraints, glyph interaction,
and conflict preview against the Block-109 solver. Block 115 owns driving/reference dimensions and
in-canvas parameter/expression editing.
