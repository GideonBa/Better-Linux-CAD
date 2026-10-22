# GUI Measure MVP-9

Status: headless authority introduced in Block 131; replacement-shell integration implemented as a
Block-136 slice. Assembly authoring, browser feature editing and whole-block acceptance remain open.

## Ownership and read-only boundary

`ShellWindow` explicitly owns `ShellMeasureTools` and wires it to the ribbon, document session,
viewport selection and view-change notification. There is no binder, component discovery or polling.
The 3D-Modell **Prüfen** group exposes **Abstand**, **Winkel**, **Radius / Durchmesser** and
**Körperdaten**. They open the dedicated **Messen** task dock through `GuiMeasureController`.

Measure reserves the transient `inspect.measure` task so modeling commands cannot start concurrently.
It never opens a document transaction or changes parameters, geometry, persistence, dirty state or
history. Closing Measure releases the task; existing Undo/Redo entries remain available. Explicit
shell Undo/Redo and document replacement close the measurement before changing the document.

## Target selection and freshness

`MeasureSelectionBuilder` extracts unique vertices, linear/circular edges and planar/cylindrical faces
from visible derived Body shapes. Part targets use the current ShapeCache, including transformed
Bodies and Surface Bodies. Project targets reuse the posed-leaf builder used by the viewport and
assembly consumers: hidden/suppressed occurrences are absent, and complete nested authored transforms
produce root-assembly coordinates. Component/occurrence names label the choices. Body summaries
require explicit Part Bodies; they do not treat a whole Project as one Body.

Subshape ordinals are disposable viewer identities scoped to one catalogue generation. They are
never serialized, used as Core references or remapped to later topology. The complete selection,
result, status-bar readout and witness overlay are cleared on a presentation-revision or freshness
change, including a
freshness change without a revision increment. New current geometry receives a new catalogue and
requires explicit reselection. Stale Geometry, missing or unsupported inputs fail closed.

The task offers separate **Element A/B** choosers and model-click roles. After selecting A in a pair
measurement, model clicks select B. **Auswahlfilter** restricts native picking to compatible points,
edges or faces through a capability mask and semantic whitelist. Body summaries pick existing Body
presentations. The previous mask/scope is restored on Close, Escape or dock close; target overlays
are removed. A repeated identical pair is rejected and clears the previous readout.

## Results and geometric scope

`GuiMeasureController` delegates to the existing Qt-free `GeometryMeasureQuery` and
`BodyResultInspector` authorities:

- **Distance:** point/point, point/line, point/plane, line/line, line/plane and plane/plane in mm.
  Lines and planes are supporting analytic geometry; distances may refer to their extensions beyond
  the finite selected edges/faces. This is stated in the task hint. Arbitrary trimmed-face or curved
  surface minimum-distance queries are not introduced.
- **Angle:** linear edges and planar faces, using their directions/normals and the existing smaller
  angle convention, 0–90 degrees. Mixed edge/plane measurement uses the edge-to-plane angle.
- **Radius/diameter:** circular edges or cylindrical faces, in mm. Arbitrary spline curvature and
  elliptical radii are not inferred.
- **Body summary:** existing derived volume in mm³ and solid count. Surface Bodies retain their
  derived surface/zero-solid summary.

The task displays selectable text and **Kopieren** copies only a current valid result. Values use up
to 12 significant digits. **Neu auswählen** clears the selected targets. There is no Apply operation.
A transparent mouse-pass-through overlay marks A/B and draws Distance witness points, line and value.
Explicit viewport view-change notifications reproject it after camera/zoom/resize changes; no overlay
is selectable or saved. Intersecting planes now produce a shared witness on both planes instead of
an arbitrary point on just the first plane.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[block136-measure]"
./build/dev-gui/blcad_geometry_tests "[block136-measure]"
QT_QPA_PLATFORM=xcb ./build/dev-gui/blcad_gui_tests "[block136-measure]"
```

The shell proof covers all four readouts, clipboard and clear, same-target rejection, role/filter
selection, all exit paths, selection-scope restoration, stale/replaced topology, current recompute,
unchanged dirty/history state, byte-identical saved Parts and a known translated Project-component
distance. Native proof opens the ribbon command, clicks two actual model vertices, changes camera,
picks both a circular edge and cylindrical face, and closes the reachable dock. Geometry proof checks
World-space target extraction, unique/revision-scoped identities, invalid owners and intersecting-plane
witness correctness. Optional `BLCAD_UI_CAPTURE_DIR` writes native review captures.
