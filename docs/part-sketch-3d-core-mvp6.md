# Part Basic 3D Sketch Core MVP-6

Status: implemented in Block 75.

Block 75 introduces a separate model-space sketch domain instead of extending planar `Sketch`
coordinates beyond their workplane. It freezes the in-memory Core identity and ownership boundary
for `Sketch3D`, `SketchPoint3D`, `SketchLine3D`, and `SketchPolyline3D`.

## Coordinate and entity contract

Every point owns three `SketchCoordinate3D` values. A coordinate is exactly one of:

- a finite signed explicit `LinearDisplacementMm` in Part model space
- a typed reference to an existing Part `Length` parameter

This allows zero and negative explicit coordinates while rejecting Angle, Count, non-finite, or
missing coordinate inputs. Parameter values remain governed by the existing positive-Length
parameter contract; richer signed parameter expressions are outside this block.

A `Sketch3D` owns points, lines, and polylines in one sketch-local `SketchEntityId` scope. Lines
reference two distinct owned point ids. Polylines preserve their authored vertex order, require at
least two owned vertices, and reject consecutive duplicates. Entity creation never copies or
implicitly creates points.

Point removal fails while a line or polyline references that point. Lines and polylines can be
removed independently. A 3D sketch id shares the Part-document sketch node namespace with planar
sketches, preventing ambiguous dependency identities.

## Part-document and dependency contract

`PartDocument` owns 3D sketches independently from planar sketches and exposes add, find, count,
and remove operations. Adding a sketch validates all parameter-bound coordinates transactionally,
adds one graph node, and connects every distinct coordinate parameter to that node. Parameter
changes therefore invalidate the 3D sketch and place it in a recompute plan.

Removing a 3D sketch removes its graph node and incoming coordinate dependencies. Removal fails if
later model intent depends on that sketch. Invalid additions leave ownership, graph, and
invalidation state unchanged.

## Deliberate boundary

Block 75 provides no general 3D constraint solver, curve geometry, OCCT conversion, or planar
workplane. Block 76 adds spline, arc, helix, and guide-curve intent in
`docs/part-sketch-3d-curves-core-mvp6.md`. Block 77 provides additive JSON and cross-source semantic
references in `docs/part-sketch-3d-json-mvp6.md`. Block 78 converts the stable intent to geometry.

## Verified behavior

- finite signed explicit and typed parameter-driven coordinates
- shared sketch-local entity-id scope and owned endpoint validation
- exact ordered polyline vertices
- line/polyline point-removal protection
- parameter dependencies, invalidation, and recompute planning
- transactional wrong-unit, missing-reference, and planar/3D id-collision rejection
- document-level removal and graph cleanup

```text
./build/dev-geometry/blcad_core_tests "[core][sketch-3d]"
```

Blocks 48–94 are implemented. Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.

## Spatial constraint ownership — 163

The existing Sketch3D owns `SpatialConstraint` records in a separate sketch-local
`SketchConstraintId` scope. Point/curve IDs, insertion order and historical coordinates remain the
same authority. `add_constraint`, same-ID replacement and removal validate complete ordered target
roles; atomic entity replacements retain collection position and every still-valid constraint.
Referenced local entities require explicit constraint removal/rebind before deletion.

A target is an existing point reference, a local curve with Whole/Start/End role, an external
PathSegmentReference with the same role, or a typed Axis/Plane/Face input using `SpatialConstraint`
role. External sources are read-only and contribute their existing dependency node; the same owner
must use a local curve role. No resolved external coordinate, sampled pole or kernel index is owned.
Source loss, wrong units, same-owner external aliases and dependency cycles reject publication
without changing the source graph, invalidation state or coordinates.

| Relation | Structural target capability |
| --- | --- |
| Coincident / Distance | Two point-valued roles: existing point references or curve endpoints. Identical point references are rejected; distinct IDs at equal coordinates remain distinct. |
| Fixed | One local point-valued role plus a finite persistent model-space XYZ anchor. |
| Axis / plane parallel | A whole straight line plus a read-only axis or plane. |
| Point on line / plane / curve / face | A point-valued role and the corresponding line/axis, plane, whole curve or typed face. |
| Parallel / perpendicular / Angle | Two straight-line/axis directions; oriented Angle additionally names its axis. |
| Tangent / Smooth | Two explicit curve endpoint roles. Actual speed/differential capability and feasibility belong to the derived solver boundary. |
| Radius / Length | Radius supports whole arcs/helices; Length supports whole supported curves, including polylines. |

Dimensional intent is exactly a literal typed Quantity or existing parameter ID. Distance, radius and
length require positive Length; Angle requires Angle. Unsigned angles lie in [0,180] degrees;
oriented-about-axis angles lie in (-180,180]. The solver owns the corresponding residual/branch
conventions. Legacy Part expression parameters retain their existing Length/Count grammar; this
schema does not add global Angle formulas. Fixed anchors use signed XYZ millimetres. Point-on-curve
stores a finite normalized branch seed in [0,1], distinct from spatial distance or equation-language
variables; exact evaluation conventions belong to 164.

`spatial_variable_layout` orders explicit point coordinates by bytewise point ID then X/Y/Z,
explicit helix radius/pitch by helix ID then scalar role, and auxiliary curve parameters by constraint
ID. Parameter-bound coordinates/scalars remain immutable and are excluded; integer Count turns
never become continuous DOF. This is a deterministic variable plan, not a rank/DOF or solve result.
Parameter/formula edits validate dependent typed intent transactionally.

[The JSON contract](part-sketch-3d-json-mvp6.md#spatial-constraints-and-owned-scalars--163) owns portable
records. [Core schema tests](../tests/core/sketch_3d_constraint_tests.cpp) cover the matrix, units,
ordering, source/cycle guards, exact replacements and compatibility. Solver/diagnostics and native
constraint/curve authoring remain the separate 164/165 boundaries in the numbered plan.
