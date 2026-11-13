# Planar Sketch Constraint Solver MVP-8

Status: implemented in Block 109. Persistent non-dimensional authoring integration is implemented in
Block 114; typed driving/reference dimension integration is implemented in Block 115.

This document is the canonical mathematical and diagnostic contract for the deterministic planar
Sketch solver. Block-114 and Block-115 documents remain canonical for their persistence, GUI,
parameter, and transaction semantics.

## Authority boundary

```text
SketchTopology + SketchConstraintSystem
  -> deterministic variable/residual construction
  -> normalized damped Gauss-Newton solve
  -> final Jacobian rank / local DOF
  -> redundancy and conflict attribution
  -> SketchSolveResult
```

The solver owns no screen state, Qt state, OCCT topology, persistence sidecar, parameter editing, or
history. It consumes stable Block-108 point/entity identities and numeric values already resolved by
higher Core authoring services.

## Canonical inputs

`SketchTopology` supplies persistent point/entity identity and source coordinates.
`SketchConstraintSystem` supplies a single Sketch id and a lexicographically ordered set of uniquely
identified `SketchSolverConstraint` records.

A solver target is exactly one of:

```text
point  -> SketchPointId
entity -> SketchTopologyEntity id
```

Constraint target order is semantic. It is not normalized inside the solver because distance and angle
families assign directed roles to their operands.

## Variable ordering

Only non-reference topology points become variables. Variables are ordered lexicographically by
`SketchPointId`, with X before Y:

```text
point/a.x
point/a.y
point/b.x
point/b.y
...
```

Reference points are read-only constants. The ordering is independent of insertion order, UI selection,
allocation, OCCT traversal, or hash-table order.

## Supported families

Block 109 implements:

```text
Coincident
Fixed
Horizontal
Vertical
Parallel
Perpendicular
Collinear
Equal
Tangent
Concentric
Midpoint
Symmetric
PointOnObject
HorizontalDistance
VerticalDistance
AlignedDistance
Radial
Diameter
Angular
```

Non-dimensional target signatures are defined in
`docs/gui-sketch-constraint-authoring-mvp8.md`. Dimension family mappings are defined in
`docs/gui-sketch-dimension-authoring-mvp8.md`.

## Representative residuals

Let `L` be the characteristic Sketch length used for normalization.

Coincident points:

```text
rx = (x1 - x2) / L
ry = (y1 - y2) / L
```

Horizontal and vertical lines:

```text
horizontal: (y_end - y_start) / L
vertical:   (x_end - x_start) / L
```

Horizontal/vertical directed distance:

```text
horizontal: (x2 - x1 - d) / L
vertical:   (y2 - y1 - d) / L
```

Aligned distance:

```text
(||p2 - p1|| - d) / L
```

Radius and diameter:

```text
(radius(arc) - r) / L
(2 * radius(arc) - d) / L
```

Angular:

```text
wrap(atan2(cross(v1,v2), dot(v1,v2)) - targetRadians) / pi
```

Parallel/perpendicular/collinear, tangent, concentric, midpoint, symmetric, equal, and point-on-object
use scale-normalized geometric residuals over their stable topology targets. Curve↔curve tangent
requires a shared or touching endpoint (angle between the two corner tangents); coordinate equality
alone is insufficient.

Collinear is **variable-arity**: two-plus lines, a point and a line, or three-plus points (nothing
else mixed in). A reference direction and anchor are taken from the first line target, or — when only
points are given — from the first two points. Each further line contributes parallelism plus an
anchor-on-line residual; each aligned point contributes a single on-line residual
(`direction × (point − anchor)`). Two lines reduces to the original parallel + second-start-on-first
formulation, so existing behaviour is unchanged. Point↔line collinearity reuses the point-on-object
semantics (point on the line's infinite extension). The intent/catalog layers validate the arity by
signature rather than a fixed target count.

Line↔circle-profile tangent instead uses the perpendicular center-to-line distance and the circle's
dimension-driven radius baked as the constraint value. The residual is first order in length
(`|distance| − radius`), matching the coincidence/point-on-object residuals: if the line's endpoint is
also held on the same circle by a coincidence (`PointOnObject`), the contact point is that endpoint and
the residual switches to perpendicularity of the line to the radius there
(`direction · (endpoint − center)`). Both residuals are the same length order, so the coupled
"coincident endpoint + tangent" system stays well conditioned. An earlier squared distance form
(`distance² − radius²`) shared the point-on-circle radial gradient at the solution and left that coupled
system degenerate, so the solver crept to the iteration limit and reported `non_convergent`.

## Block 159 Smooth/G2

The backend extension is implemented; numbered acceptance belongs to `mvp-plan.md`.
Native Smooth authoring belongs to 162.
Reuse the existing point/entity target system with ordered targets **curve A, endpoint A, curve B,
endpoint B**. Curve identities differ, at least one operand is a spline, and the named point must
unambiguously select an endpoint of its line, arc or spline. Shared endpoint IDs remain valid;
coordinate proximity never chooses persistent junction identity.

Let `T` be the regular unit tangent pointing from the junction into each curve and
`K = (C'' − T(T·C'')) / |C'|²` its curvature vector. Use the system characteristic length `L`:

```text
G0  (endpointA − endpointB) / L
G1  TA + TB
G2  L × (KA − KB)
```

The existing exact-coincidence projection owns G0 equality; the unprojected final Jacobian still
owns rank. Curvature vectors survive regular parameter-speed changes and curve reversal; equal
unsigned radii alone cannot certify the junction. A regular line/inflection has zero curvature.
Zero-speed spline endpoints and degenerate arcs fail explicitly instead of producing fabricated
zero curvature. Preserve existing Tangent behavior and numeric diagnostic/acceptance policies.

The existing constraint sidecar writes integer version **2** when Smooth is present, and retains
version **1** without it. Both versions are read; Smooth in version 1 and noninteger/unknown
versions fail. Ordered targets persist; solved points, derivative samples, residuals, rank and
diagnostics remain derived. Historical embedded Tangent records and their behavior remain intact.

Smooth systems use dimensionless increments `q = x/L`: central differences still perturb physical
coordinates by `relativeStep × L`, while their Jacobian and damping act on `q`. `L` is the positive
actual bounding extent of variable points plus explicitly targeted points/entities, including
read-only reference curves. An empty/collapsed extent falls back to 1 mm. This avoids a millimeter
floor overpowering small-scale curvature rows or physical-unit damping freezing large models.
Systems without Smooth retain the historical scale, physical increments and numeric behavior.
Initial invalid/degenerate endpoints report `invalid_reference` with the constraint ID. A nonlinear
trial can fail; authoring publishes no solved Sketch/catalog on any rejected result. This remains
a local bounded solve and makes no global convergence promise for singular or distant seeds.

S01 proofs are in [endpoint differentials](../tests/core/planar_curve_differential_tests.cpp),
[solver/rank/persistence](../tests/core/sketch_smooth_constraint_solver_tests.cpp) and
[accepted catalog/dimension/drag consumers](../tests/gui/gui_sketch_smooth_tests.cpp).
Independent polynomial/conic derivatives, reversal and unequal speeds, scales `1e-6…1e6`, fixed and
driving equations, signed curvature/cusps, redundancy, exact release/history and reopen are tested.
Focused Core passes **12 cases / 1,938 assertions** and Qt **2 / 108**. The impossible fixed cusp
retains the already satisfied G1 equation in the same G2 system, with explicit conflict attribution.
Full Core passes **720 / 28,954** and Geometry **682 / 58,224**, without skips. Full Qt passes
**898 cases**: **831 passed / 67 native-only skips**, **101,256 assertions**, exit 0. After the
Core-only conflict-fixture extension, the final Core binary is rerun; all other runtime inputs and
Geometry/Qt/app/exporter executable hashes remain identical, retaining their complete results.

The relevant native filter combines `[parity-b159]`, `[endpoint-native-workflow]`,
`[sketch-constraints]`, `[sketch-drag]`, `[sketch-live-solve]`, `[sketch-dimensions]` and
`[shell-coverage]`. It passes **34 cases / 1,504 assertions at 100%** and **34 / 1,505 at 150%**,
without skips. Original overlap/closed-chain drag captures at both scales are reviewed; the separate
app startup has a PID-matched visible X11 window and intentional six-second timeout **124**.
These are backend and existing native consumer proofs. Native Smooth command/diagnostic authoring
and S01 closed-contour solid acceptance remain assigned to 162; full native-suite coverage is not
claimed. The final frozen runtime has **1,070 files**, SHA256
`61b8d4be6dab4d40bc20241f4006e9429c0a3f9f292ba4bc3859cbf09585fdf9`.

## Characteristic scale and degeneracy policy

The historical characteristic length is derived deterministically from topology extents with a
1 mm floor; Smooth systems use the normalized extent policy above.
Length residuals divide by this scale; angular/directional residuals use normalized vectors or `pi`.
Degenerate line/arc calculations use deterministic numerical floors for evaluation, but invalid target
kinds, missing targets, or structurally unsupported entities produce `invalid_reference` rather than
inventing geometry.

### Exact Coincident closure

Explicit point-to-point Coincident constraints define equality groups by stable point identity.
Each group receives exactly one coordinate pair before convergence is tested and throughout numeric
iteration. Unanchored groups use the mean of their member positions; an immutable reference or Fixed
target anchors the group to its source position. Distinct anchored positions are conflicting even
when their normalized separation is small. Fixed entity targets anchor all their defining points.

Central differences for the numeric step stay in this equality subspace, and every line-search
candidate is projected back into it. All constraint residuals are then reevaluated normally. The
unprojected final Jacobian still owns rank, redundancy and DOF accounting over the original canonical
variable order. Conflict probes rebuild groups from the reduced constraint set.

This preserves separate point identities and persistent relation provenance. Coordinate proximity
alone creates no relation. Point-on-object, dimensions and other residuals retain their existing
contracts; no region, profile or topology tolerance is enlarged. Consequently, an accepted explicit
Coincident junction is exactly connected for the absolute `1e-9` closure checks consumed by Finish
Sketch, including line, arc and spline endpoints. See `gui-sketch-regions-finish-mvp8.md` for the
cross-layer regression and native workflow.

## Numeric solve

The implementation uses deterministic damped Gauss-Newton iteration:

1. construct the canonical variable vector from source topology;
2. evaluate all constraint residual blocks in canonical constraint order;
3. build a central-difference Jacobian;
4. solve the damped normal equations with project-owned dense elimination;
5. accept a step only under the deterministic improvement rule;
6. stop at convergence, stall, or the fixed iteration limit.

No randomization, wall-clock termination, sparse-library ordering, or UI frame timing influences the
result.

## Rank and degrees of freedom

After a converged solve, the final Jacobian rank is computed with the canonical tolerance policy.

```text
remaining_dof = variable_count - jacobian_rank
```

Status is:

```text
fully_constrained  remaining_dof == 0 and no redundant constraint
under_constrained  remaining_dof  > 0 and no redundant constraint
redundant          at least one canonical residual block adds no rank
```

DOF is local to the final solved state. It is not a persistent property and may change at singular
configurations.

## Conflict and redundancy attribution

Redundancy is attributed in canonical constraint order by accumulating Jacobian blocks and reporting a
constraint whose non-empty residual block does not increase rank.

When the numeric solve stalls, conflict attribution removes each constraint in canonical order and
re-solves. A constraint is reported conflicting when its removal makes the reduced system converge.
If no single removal restores convergence, status is `non_convergent` rather than an invented conflict
set.

Stable constraint ids are returned unchanged. Authoring adapters may strip only their explicit internal
prefixes (`intent/` or `dimension/`) before presentation.

## Solve statuses

```text
fully_constrained
under_constrained
redundant
conflicting
non_convergent
invalid_reference
```

Only `fully_constrained` and `under_constrained` are generally publication-capable. Authoring services
may impose stricter postconditions, such as historical-materialization equality or Block-115 measured
value validation.

## Block-114 persistent constraint consumer

`SketchConstraintCatalogSystemBuilder` composes:

```text
historical embedded Sketch constraints
+ accepted blcad.sketch_constraints.mvp8 records
```

A candidate is added with an internal stable `intent/<id>` solver identity. Block 114 accepts only an
under-/fully-constrained solve that materializes exactly. Redundant, conflicting, invalid, and
non-convergent candidates remain transient.

Canonical contract: `docs/gui-sketch-constraint-authoring-mvp8.md`.

## Block-115 typed dimension consumer

`SketchDimensionCatalogSystemBuilder` composes:

```text
historical embedded Sketch constraints
+ accepted Block-114 constraints
+ all driving Block-115 dimensions
```

The existing Part parameter/expression system resolves numeric values before solver construction.
Mappings are:

```text
horizontal distance        -> HorizontalDistance
vertical distance          -> VerticalDistance
aligned / point-to-point   -> AlignedDistance
line length                -> AlignedDistance(line start, line end)
radius                     -> Radial
diameter                   -> Diameter
angle                       -> Angular
arc length                  -> calibrated equivalent Radial
reference dimension         -> no solver constraint
```

Arc length is not a new residual in Block 109. Block 115 iteratively derives an equivalent radius from
the requested length and current three-point-arc sweep, solves from the original topology, and then
measures actual arc length. At most ten deterministic calibration passes are used. Publication requires
all final driving measurements to match their parameters within the canonical relative tolerance.

This post-solve measurement check is essential: a solver-converged equivalent-radius state is not by
itself authoritative arc-length satisfaction.

Canonical contract: `docs/gui-sketch-dimension-authoring-mvp8.md`.

## Block-110 drag consumer after Blocks 114–115

Session-backed dragging builds its baseline from the same historical, constraint-catalog, and
dimension-catalog composition. The temporary pointer equation is appended only for preview. Release
must compare the current catalogs/effective system against the source snapshot and validate every final
driving measurement before materialization.

Canonical contract: `docs/gui-sketch-solver-drag-mvp8.md`.

## Persistence boundary

The solver result is always derived. Never persist:

```text
variables
residual vectors
Jacobian
rank
remaining DOF
iteration count
conflict/redundancy diagnostics
solved preview topology
arc-length calibration state
```

Persistent intent belongs to historical Sketch JSON or explicit versioned sidecars:

```text
blcad.sketch_constraints.mvp8
blcad.sketch_dimensions.mvp8
```

## Failure policy

No persistent mutation occurs for:

- wrong Sketch scope;
- missing or wrong-kind targets;
- duplicate solver ids;
- missing/forbidden numeric values;
- non-finite values;
- non-positive aligned/radial/diameter values;
- structurally invalid tangent or arc targets;
- invalid solver options;
- stalled or maximum-iteration solves rejected by the authoring consumer;
- final dimension measurement mismatch;
- materialization identity loss;
- stale source/catalog snapshots.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-solver]"
./build/dev/blcad_core_tests "[core][sketch-dof]"
./build/dev/blcad_core_tests "[core][sketch-conflict-diagnostics]"
./build/dev/blcad_core_tests "[core][sketch-constraints]"
./build/dev/blcad_core_tests "[core][sketch-dimensions]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
```

## Current next boundary

Block 116 owns topology-rewriting trim, extend, split, Sketch corner fillet, and Sketch corner chamfer.
Those operations must explicitly remap or reject constraints and dimensions before invoking this solver;
the solver does not infer semantic retargeting after topology replacement.
