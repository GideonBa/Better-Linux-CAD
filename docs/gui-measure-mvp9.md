# GUI Measure MVP-9

Status: implemented in Block 131.

`GuiMeasureController` is the read-only inspection command for fresh derived Geometry. It accepts
typed point, linear-edge, planar-face, circular-edge, and cylindrical-face descriptors and reports:

- point/edge/face distance with transient witness points;
- angle between edge directions or face normals;
- radius and diameter for circular edges and cylindrical faces;
- volume and solid count for an existing recomputed Body.

The analytic query lives in `GeometryMeasureQuery`; Qt types are not part of that boundary. The GUI
controller owns only the current selection and readout. It never opens a document transaction,
changes selection identity, serializes overlays, or publishes a `ShapeCache`. Missing, incompatible,
degenerate, or stale inputs fail closed and retain the document and last valid derived result.

`MainWindow::measure_controller()` exposes the same transient controller to the application shell.
The status text, witness line/points, and readout disappear on cancel and are not model identity.

Focused proof: `[gui][measure]`.

## Next boundary

Interactive Modeling MVP-9 is complete. Block 132 starts STEP Import MVP-10 with persistent STEP
source identity, explicit Reference/EditableBody modes, JSON, and freshness.
