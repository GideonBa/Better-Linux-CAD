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

## Spatial solver boundary — 164

`solve_spatial_sketch` consumes the existing Sketch3D plus fixed Part parameter sources and optional
Geometry queries. Its result owns a candidate Sketch3D and derived diagnostics. Every rejected
result contains the exact source sketch: coordinates, active bindings, owned scalars, constraint
IDs and authored curve-parameter seeds. Numerical perturbations never update Part/history. Core
has no OCCT or file-I/O dependency; the caller publishes one fully checked downstream transaction. Geometry may
validate all final owner curves before acceptance, including curves not directly queried by a
relation. Unchanged coordinate components and bindings retain their original records.

The stable variable layout from the ownership contract is normalized about the initial point
centroid. A rigid-motion-invariant characteristic length uses twice the maximum centroid radius and active
Length dimensions, with an explicit positive scale override. Length residuals divide by that scale;
direction/angle residuals are dimensionless and curvature-vector residuals multiply by it. Every
normalized residual component must meet `convergence_rms`; reported RMS is descriptive. Solver,
modeling and display tolerances remain separate. Parameter coordinates/scalars and integer turns
remain immutable. Auxiliary `[0,1]` curve parameters are derived solve variables; the authored seed
selects a bounded local closest-projection initialization and is never replaced by the numerical
solution. A feasible source on that selected branch remains stationary across repeated solves;
auxiliary-only changes do not rewrite authored coordinates.

| Relation | Derived residual / convention |
| --- | --- |
| Coincident / Fixed / Distance | Model-space position difference or Euclidean distance; Fixed uses the authored anchor. |
| Point on line / plane | Orthogonal cross-product or signed normal distance. Directions are normalized and singular lines reject. |
| Point on curve / face | Position minus current-candidate native curve evaluation, or the closest point on the supported trimmed-face interior. A curve parameter is normalized modeled parameter, not arc length. |
| Parallel / perpendicular | Cross-product or dot-product of normalized directions. Parallel permits either orientation. |
| Angle | Unsigned `[0,180]` degrees; exact zero/half-turn use directional vector residuals to retain endpoint rank. Oriented `(-180,180]` degrees is the wrapped angle of directions projected about the named axis; zero projected speed rejects. |
| Tangent / Smooth | Endpoint coincidence plus opposed inward unit tangents; Smooth additionally equates actual normal curvature vectors `(d2 - T·dot(T,d2))/speed²`. A regular unconstrained join has physical rank five/seven. |
| Radius / Length | Supported native circular/helix radius or actual modeled curve length; no control-polygon length or historical Sweep chord substitute. |

Entity, free-variable and constraint counts are bounded before numerical work.

Bounded damped Gauss–Newton uses the same dense linear elimination and matrix-rank helpers as the
planar solver. Stable ID ordering, finite-difference step bounds, iteration/damping/line-search and
evaluation budgets are explicit. The work budget includes residual evaluations and curve-branch
initialization queries; branch projection also has a separate iteration bound. Domain-bound auxiliary and positive scalar variables use one-sided
derivatives where required. Invalid capabilities, singular Geometry, exhausted budgets and rejected
iterations return source geometry; no last iterate is published as an approximate success.

Final Jacobian rank gives total DOF, including auxiliaries. Per-ID attribution accounts for the
auxiliary introduced by point-on-curve; a constraint that only fixes its auxiliary is geometrically
redundant. Residual diagnostics identify unsatisfied relation blocks. Gauge is the image of locally
admissible rigid velocities in free point coordinates, allowing auxiliary compensation and respecting
bound coordinate rows and external constraints; it is not an unconditional subtraction of six.
Internal DOF is total DOF minus this admissible gauge. Gauge rank accounts for finite-difference
cancellation uncertainty from the original Jacobian. Failure reports describe the attempted local
solve while the returned model remains the exact source. This is a local differential report, not a
claim of global uniqueness or of a different curve branch being infeasible.

Independent rigidity/scale/repeatability and exact-rejection proofs belong to
[Core solver tests](../tests/core/sketch_3d_solver_tests.cpp). Geometry differential/projection and
fixed G1-only impostor checks belong to
[native solver tests](../tests/geometry/spatial_solver_geometry_tests.cpp). Native interaction remains
owned by the existing [Sketch3D task contract](gui-sketch3d-interaction-mvp8.md).

### Exact interaction variables — 165

Transient `SpatialSolverOptions::fixed_variables` identify distinct free point components or
entity-owned Helix scalars in the supplied candidate. These values are removed from the numerical
system, so an accepted solve preserves the requested double exactly. Unknown/duplicate keys,
parameter-bound coordinates/scalars and auxiliary curve parameters reject. Fixed point components
also restrict the admissible rigid gauge. These pins are never constraints or persisted records;
the task analyzes the accepted candidate again without pins for its persistent DOF display.
[Independent exactness/conflict/gauge proofs](../tests/core/sketch_3d_solver_tests.cpp) cover this
boundary; pinning cannot silently override a formula or relax an incompatible requested position.
