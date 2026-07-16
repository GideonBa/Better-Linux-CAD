# Interactive Sketch3D MVP-8

Status: implemented in Block 120.

Block 120 adds a headless direct-manipulation authority around the existing persistent `Sketch3D` model. It does not introduce a second spatial-curve representation and does not claim a full variational 3D constraint solver.

## Authority split

Persistent authority remains in `Sketch3D`, `SketchPoint3D`, `SketchLine3D`, `SketchPolyline3D`, `SketchArc3D`, `SketchSpline3D`, `SketchHelix3D`, and `SketchGuideCurve3D`. Screen rays, triad hover, temporary lock highlights, rubber-band curves, typed HUD text, sampled geometry, and handle placement remain transient GUI products.

`Sketch3DInteractionService` produces complete disposable `Sketch3D` candidates. A caller may preview the candidate and commit it through its document/session transaction boundary only after source freshness is rechecked.

## Orthogonal locks

`Sketch3DLock` supports:

- free model-space placement;
- X, Y, and Z axis locks through an anchor point;
- XY, YZ, and ZX plane locks through an anchor point.

Pointer-derived coordinates are resolved first. Explicit typed X/Y/Z values then override the corresponding components. Distance-direction input requires distance, azimuth, and elevation together and rejects non-finite or negative distance values.

Example: an anchor `(0,0,0)`, distance `10`, azimuth `0`, and elevation `30 degrees` resolves to approximately `(8.6603,0,5)`.

## Point and line placement

Point placement creates explicit linear-displacement coordinates in millimetres. Line placement atomically creates two points and one `SketchLine3D`; an optional guide role creates a linked `SketchGuideCurve3D` in the same candidate. Duplicate IDs, degenerate lines, or invalid guide references fail before publication.

Guide roles reuse the existing persistent values `General`, `SweepPath`, `LoftGuide`, and `Centerline`.

## Direct handles

Moving a point handle rebuilds the complete candidate with the same point ID and preserves every line, polyline, arc, spline, helix, and guide reference that targets that point. The source `Sketch3D` is never mutated. Parameter-driven coordinates are replaced only by an explicit accepted handle edit; the UI must present that semantic change before commit.

## Planar-point projection

`project_planar_point(...)` receives the resolved model-space point plus the source `SketchId` and stable `SketchReferenceTarget`. It creates a local `SketchPoint3D` and publishes `Sketch3DProjectedPointIntent` alongside the candidate so the document/session layer can retain or present the source association explicitly. Raw screen coordinates or OCCT vertex identity never become the source reference.

## Failure policy

The service fails closed for missing handles, duplicate IDs, invalid source Sketch IDs, non-finite coordinates, incomplete distance-angle tuples, negative distances, degenerate line endpoints, and invalid existing Sketch3D references. No partial candidate is returned.

## Focused proof

```bash
./build/dev/blcad_core_tests "[gui][sketch-3d-edit]"
./build/dev/blcad_core_tests "[integration][sketch-3d-direct-manipulation]"
```
