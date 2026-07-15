# Planar Sketch Constraint Solver MVP-8

Status: implemented in Block 109.

This document is the canonical Core contract for the deterministic planar constraint solver introduced
by Block 109. The solver consumes Block-108 `SketchTopology`; Qt, screen coordinates, OCCT topology,
and sampled interaction geometry do not participate in its mathematics or identity model.

## Authority boundary

```text
canonical SketchTopology
  + canonical SketchConstraintSystem
  -> SketchConstraintSolver
  -> derived SketchSolveResult
```

`SketchTopology` remains persistent point/entity identity authority. `SketchConstraintSystem` is the
canonical solve request. `SketchSolveResult`, Jacobians, residuals, rank, DOF, convergence state, and
conflict/redundancy diagnostics are derived and are not persisted as opaque solver cache.

The solver never mutates its source topology. A caller may use the solved topology from a successful
result as a disposable candidate. Persistent mutation remains the responsibility of the existing
validated document/edit transaction boundaries.

## Canonical ordering

Constraint systems sort constraints lexicographically by stable constraint id before solving.

Solver variables contain every non-reference topology point in canonical `SketchPointId` order. Each
point contributes variables in this family-local order:

```text
X
Y
```

Therefore:

```text
point/a.x
point/a.y
point/b.x
point/b.y
...
```

is stable independently of source insertion order. Reference points are not variables and retain their
source topology coordinates.

Residual row order is canonical constraint order followed by the documented family-local row order.
Finite-difference columns are canonical solver-variable order.

## Units and scale normalization

Direct solver values use:

```text
linear values  -> millimeters
angular values -> degrees
```

The deterministic characteristic length is:

```text
max(1.0 mm, planar topology point bounding-box diagonal)
```

Length residuals are divided by this characteristic length. Dimensionless orientation residuals use
normalized dot/cross forms. Angular residuals use wrapped radians divided by pi.

Default numeric policy:

```text
convergence RMS                  1e-9
central-difference relative step 1e-7
rank absolute tolerance          1e-10
rank relative tolerance          1e-8
initial damping                  1e-6
maximum iterations               80
maximum damping attempts         8
maximum line-search steps        12
```

The central-difference coordinate step is `relative_step * characteristic_length`.

## Residual families

The direct headless `SketchSolverConstraint` request supports every Block-109 initial family:

```text
coincident
fixed
horizontal
vertical
parallel
perpendicular
collinear
equal
tangent
concentric
midpoint
symmetric
point-on-object
horizontal distance
vertical distance
aligned distance
radial
diameter
angular
```

Family target contracts are validated before numeric solving. An invalid target or unavailable
reference produces `invalid_reference`; the solver does not guess a point, center, tangent, or curve.

### Coincident

Targets: two points.

Rows, in order:

```text
dx / characteristic_length
dy / characteristic_length
```

Distinct point ids remain distinct variables even when solved to the same coordinate.

### Fixed

Target: one point or one entity with defining topology points.

A point contributes X then Y residuals against its source topology coordinate. An entity contributes
X then Y for each defining point in entity point-role order. Fixed therefore anchors intent to the
input topology snapshot rather than to an external screen position.

### Horizontal and vertical

Target: one line.

Horizontal constrains normalized line `dy`; vertical constrains normalized line `dx`.

### Parallel and perpendicular

Targets: two lines.

Parallel uses normalized 2D cross product. Perpendicular uses normalized dot product.

### Collinear

Targets: two lines.

Rows, in order:

```text
normalized direction cross product
signed second-line start distance from the first infinite line / characteristic_length
```

### Equal

Targets: two measurable line or arc entities.

Line measure is segment length. Arc measure is circumradius. The measure difference is normalized by
characteristic length.

### Tangent

Targets: two line, arc, or cubic-Bezier spline entities.

The curves must share one persistent topology endpoint. Tangent direction is evaluated at that shared
`SketchPointId`; line direction, arc circumcircle tangent, and Bezier endpoint-control direction are
supported. No coordinate-equality endpoint inference is allowed.

### Concentric

Targets: two center-bearing entities.

Implemented center-bearing entity families are Arc, CircleProfile, and CircularHolePattern. Arc center
is the three-point circumcenter; profile/pattern center is the entity's persistent center point.

Rows are normalized center `dx`, then center `dy`.

### Midpoint

Targets: one point and one line.

Rows are normalized X then Y delta from the line endpoint midpoint.

### Symmetric

Targets: two points and one line axis.

Rows, in order:

```text
pair midpoint on axis
pair segment perpendicular to axis
```

### Point-on-object

Targets: one point and one object.

Implemented objects are line and arc. Line uses normalized signed infinite-line distance. Arc uses
radial distance from the three-point circumcircle.

### Horizontal and vertical distance

Targets: two points plus a signed linear value in millimeters.

Horizontal constrains `second.x - first.x`; vertical constrains `second.y - first.y`.

### Aligned distance

Targets: two points plus a positive linear value in millimeters.

The Euclidean point distance is constrained.

### Radial and diameter

Target: one arc plus a positive millimeter value.

The three-point arc circumradius is constrained directly or at twice its value.

### Angular

Targets: two lines plus a value in degrees.

The signed angle from the first line direction to the second uses `atan2(cross, dot)`. The difference
to the target angle is wrapped to `(-pi, pi]` and normalized by pi.

## Numeric solve engine

The solver evaluates the canonical residual vector and builds a central-difference Jacobian. Each
iteration forms damped Gauss-Newton normal equations:

```text
(J^T J + lambda I) step = -J^T r
```

Damping attempts use the deterministic sequence:

```text
initial_damping * 10^attempt
```

Line search tests deterministic powers of one half. A candidate is accepted only when RMS reaches the
convergence threshold or strictly decreases.

A solve that reaches the fixed maximum iteration count reports `non_convergent`. A stalled solve is
examined by deterministic conflict attribution; a stall without attributable single-removal conflict
also reports `non_convergent`.

## Jacobian rank and remaining DOF

After convergence, rank is computed by deterministic Gaussian elimination over the final Jacobian.
The pivot threshold is:

```text
max(rank_absolute_tolerance,
    rank_relative_tolerance * maximum_absolute_jacobian_entry)
```

Remaining planar DOF is:

```text
canonical variable count - Jacobian rank
```

This is a local differential DOF count at the converged solution. It is not the historical Block-99
endpoint-warning heuristic.

## Redundancy attribution

Constraints are processed in canonical id order. Their final-solution Jacobian rows are appended one
constraint block at a time. A non-empty constraint block is attributed as redundant when adding the
block does not increase cumulative rank.

This policy is deterministic and order-frozen. It intentionally reports canonical incremental
redundancy rather than every mathematically interchangeable member of a redundant set.

## Conflict attribution

When the full system stalls above convergence tolerance, Block 109 applies canonical remove-one
attribution:

1. Iterate constraints in canonical id order.
2. Remove exactly one constraint.
3. Re-solve the reduced system from the original variable snapshot with the same solver options.
4. Attribute the omitted constraint when the reduced system converges.

The resulting id list is stable and sorted by canonical constraint order. This is deterministic
single-removal attribution, not a claim of a globally minimal unsatisfiable subset.

## Solve status precedence

The result states are exactly:

```text
fully_constrained
under_constrained
redundant
conflicting
non_convergent
invalid_reference
```

Classification order is:

1. invalid target/reference -> `invalid_reference`;
2. deterministic iteration limit -> `non_convergent`;
3. numeric stall with non-empty remove-one attribution -> `conflicting`;
4. numeric stall without attribution -> `non_convergent`;
5. converged with canonical redundant ids -> `redundant`;
6. converged with zero remaining DOF -> `fully_constrained`;
7. otherwise -> `under_constrained` with exact remaining DOF.

Failed or diagnostic results do not mutate persistent Sketch intent.

## Persisted Sketch adapter

`SketchConstraintSystemBuilder::from_legacy(...)` projects currently persisted planar Sketch intent
onto the Block-109 solve request.

Implemented mappings are:

```text
Fixed         -> fixed
Horizontal    -> horizontal
Vertical      -> vertical
Parallel      -> parallel
Perpendicular -> perpendicular
EqualLength   -> equal
TangentContinuity -> tangent
HorizontalDistance -> horizontal distance
VerticalDistance   -> vertical distance
AlignedDistance    -> aligned distance
PointToPointDistance -> aligned distance
```

Horizontal/vertical parameter magnitudes use current topology direction to preserve the historical
signed-axis convention. Length parameters are read in millimeters.

The older projected-reference constraint records are also translated to their semantic solver family,
but Block-108 projected point/line entities deliberately contain no invented resolved coordinates.
Such a solve therefore reports `invalid_reference` until the associative reference workflow supplies a
persistent/derived coordinate-capable solver target at its later owner block.

Block 109 does not add persistence/UI authoring records for every direct solver family. Blocks 114 and
115 own user-facing constraint and dimension authoring. The direct solver API exists now so those later
consumers share one headless mathematical authority.

## Focused proof

Focused tags:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

The proof covers canonical variable/constraint ordering, under-constrained remaining DOF, a fully
constrained solve, every initial residual family, canonical redundancy attribution, stable conflict
attribution, invalid-reference classification, non-convergence classification, and adaptation of
current persisted geometric constraints/driving dimensions into the solver.

## Block-110 live drag consumer

Block 110 is the first continuous GUI consumer of this solver. It does not add solver mathematics. A
semantic handle maps to one of four transient target forms:

```text
Point        -> Coincident(controlled point, temporary reference point)
LineMidpoint -> Midpoint(temporary reference point, line)
ArcCenter    -> Concentric(arc, temporary reference center entity)
ArcRadius    -> Radial(arc, source-center-to-pointer distance)
```

The temporary constraint id is `zz.gui.drag.target`; temporary topology ids are
`__gui.drag.pointer` and `__gui.drag.center`. They exist only in the augmented solve request and are
removed before preview/commit. `FullyConstrained`, `UnderConstrained`, and `Redundant` are accepted
preview states; `Conflicting`, `NonConvergent`, and `InvalidReference` refuse the drag candidate.

Move samples may be coalesced by the GUI, but the exact release pointer is synchronously solved before
commit. Qt renders the derived solve result/DOF and never evaluates substitute residuals.

Canonical integration contract: `docs/gui-sketch-solver-drag-mvp8.md`.

## Block-110 live drag consumer

Block 110 is the first continuous GUI consumer of this solver. It does not add solver mathematics. A
semantic handle maps to one of four transient target forms:

```text
Point        -> Coincident(controlled point, temporary reference point)
LineMidpoint -> Midpoint(temporary reference point, line)
ArcCenter    -> Concentric(arc, temporary reference center entity)
ArcRadius    -> Radial(arc, source-center-to-pointer distance)
```

The temporary constraint id is `zz.gui.drag.target`; temporary topology ids are
`__gui.drag.pointer` and `__gui.drag.center`. They exist only in the augmented solve request and are
removed before preview/commit. `FullyConstrained`, `UnderConstrained`, and `Redundant` are accepted
preview states; `Conflicting`, `NonConvergent`, and `InvalidReference` refuse the drag candidate.

Move samples may be coalesced by the GUI, but the exact release pointer is synchronously solved before
commit. Qt renders the derived solve result/DOF and never evaluates substitute residuals.

Canonical integration contract: `docs/gui-sketch-solver-drag-mvp8.md`.

## Block-110 live drag consumer

Block 110 is the first continuous GUI consumer of this solver. It does not add solver mathematics. A
semantic handle maps to one of four transient target forms:

```text
Point        -> Coincident(controlled point, temporary reference point)
LineMidpoint -> Midpoint(temporary reference point, line)
ArcCenter    -> Concentric(arc, temporary reference center entity)
ArcRadius    -> Radial(arc, source-center-to-pointer distance)
```

The temporary constraint id is `zz.gui.drag.target`; temporary topology ids are
`__gui.drag.pointer` and `__gui.drag.center`. They exist only in the augmented solve request and are
removed before preview/commit. `FullyConstrained`, `UnderConstrained`, and `Redundant` are accepted
preview states; `Conflicting`, `NonConvergent`, and `InvalidReference` refuse the drag candidate.

Move samples may be coalesced by the GUI, but the exact release pointer is synchronously solved before
commit. Qt renders the derived solve result/DOF and never evaluates substitute residuals.

Canonical integration contract: `docs/gui-sketch-solver-drag-mvp8.md`.

## Next boundary

Block 111 reuses the solver for disposable candidates produced by basic creation commands. Automatic
constraint authoring remains Block 114 and dimension editing remains Block 115.
