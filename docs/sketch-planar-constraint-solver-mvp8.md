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
use scale-normalized geometric residuals over their stable topology targets. Tangent requires a shared
persistent endpoint; coordinate equality is insufficient.

## Characteristic scale and degeneracy policy

The characteristic length is derived deterministically from topology extents with a positive floor.
Length residuals divide by this scale; angular/directional residuals use normalized vectors or `pi`.
Degenerate line/arc calculations use deterministic numerical floors for evaluation, but invalid target
kinds, missing targets, or structurally unsupported entities produce `invalid_reference` rather than
inventing geometry.

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
