# Part 3D Sketch Geometry MVP-6

Status: implemented in Block 78.

Block 78 executes the persistent Block-75/76 3D Sketch intent in model space. The Geometry layer
resolves current coordinates and semantic point sources and emits deterministic OCCT products;
Core remains independent of OCCT topology identity.

## Conversion boundary

`Sketch3DGeometryAdapter::convert(document, sketch_id)` returns a `Sketch3DGeometryResult` with:

- resolved local points in authored insertion order;
- one typed Geometry product for every point and curve, in the fixed order Point, Line, Polyline,
  Arc, Spline, Helix;
- stable source entity ids plus vertex, edge, and wire counts for inspection;
- an immutable `GeometryShape` containing the transient OCCT result.

Guide-curve records remain semantic roles over an existing source curve and therefore do not
duplicate Geometry products.

## Supported execution

- Explicit and length-parameter-driven 3D coordinates are evaluated at conversion time.
- Curve references resolve local 3D points, ConstructionPoints, and supported semantic points from
  planar sketches through `SketchReferenceProjector` and `WorkplaneResolver`.
- Lines become edges; polylines become ordered wires; three-point arcs become circular edges.
- Fit-point splines use the authored degree and continuity hint. Control-point splines use the
  authored poles and a deterministic clamped knot vector.
- Helices are exact cylindrical-surface curves around datum or ConstructionLine axes. Radius,
  pitch, turns, and handedness are evaluated from current parameters.

Invalid or unresolved input fails conversion without mutating the Part document. Degenerate
lines, disconnected/invalid curve definitions, unavailable parameters or sources, and kernel
construction failures are reported through `Result`.

## Persistence and topology

The adapter never writes resolved coordinates, sampled spline data, or OCCT edge identity back to
Core. Re-running conversion after a parameter change therefore regenerates Geometry from the same
stable semantic intent. Block 79 may consume these curves through persistent `PathCurve` segment
references, but it must not persist these transient OCCT handles.

## Equation construction contract

The shared `EquationCurveGeometryAdapter` resolves the existing raw intent without adding authored
points. Supported circles and cubic-or-lower polynomials retain analytic curves; general functions
use a deterministic C1 cubic chain. Construction checks interval derivative/regularity bounds,
coefficient enclosures for analytic reconstruction, numeric resolution and finite work budgets.
Each accepted generic span bounds source-to-chord deviation and the actual modeled control offset.
The `adaptive_v1` construction budget is half the stored modeling tolerance; display deflection
is independent. Default/hard caps are 8,192 spans, depth 28 and 32,768 evaluations; caller limits may
only reduce work. Unresolved singularities, zero speed, tolerance or numeric limits fail conversion.

Closed intent requires structural periodicity or supported exact polynomial endpoint identity;
proximity never proves closure. Closure proves G0, with seam tangent/curvature continuity diagnosed
separately. The derived curve/start/end roles retain source identity across tolerance changes, and
closed endpoints share a modeled position only after proof. Equation products append after historical
Helix products, with one immutable native edge and no duplicate guide products. Native handles and
analysis data remain entirely in Geometry; Core and portable JSON retain the authored definition.

The existing curve-profile and PathCurve adapters consume this immutable product. Historical spline
and helix path sampling remains governed by the explicit deferred 166 policy in
[spatial Sweep](part-sweep-3d-geometry-mvp6.md). Numbered implementation/proof status is in the plan.

[Geometry regressions](../tests/geometry/equation_curve_geometry_tests.cpp) compare circles,
parabolas, exponential curves and helices with independent analytic point/tangent/length/error
oracles. They cover high frequency versus finite work, singularities/numeric resolution, exact
closed seams versus an interior seam crossing, reversal/rigid placement, physical Extrude/torus/
straight and helical Sweep volumes, parameter/domain/tolerance edits and reopened source authority.
[Shell/session regression](../tests/gui/gui_equation_curve_tests.cpp) checks native presentation,
source selection, parameter edits, exact history and persisted solids; native authoring remains 162.

The final proof passes 7 Core / 4,429 assertions and 11 Geometry / 10,400 assertions. Full suites
pass 739 Core / 33,751, 693 Geometry / 68,624, and 901 Qt cases (834 passed, 67 native-only skipped)
with 101,364 assertions. Relevant native Sketch/source consumers pass 37 cases / 1,614 assertions
at 100% and 37 / 1,615 at compact 150%, without skips. Six original captures are reviewed after
GUI exit; separate startup proves a PID-matched visible window (intentional timeout 124).
All proof uses the plan's 1,091-file frozen runtime; source/binary hashes, dependency records,
formatting and documentation links pass. This accepts 161's Geometry boundary, not native authoring.

## Verification

```text
./build/dev-geometry/blcad_geometry_tests "[geometry][sketch-3d]"
```

The focused suite covers all supported product kinds, parameter recomputation, failure behavior,
non-mutation of serialized Core intent, and a fit spline whose three points come from planar
sketches on three differently oriented workplanes.

Blocks 48–94 are implemented. Block 83 Path-following Extrude and Extruded Cut is implemented;
Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
