# Sketch constraints and driving dimensions

This document describes the persistent planar Sketch constraint and driving-dimension intent introduced
before Interactive Sketcher MVP-8 and its current Block-109 solver integration.

The persistent record surface remains intentionally narrower than the complete Block-109 direct solver
family set. Blocks 114 and 115 own future user-facing/persistent expansion of constraint and dimension
authoring. Block 109 supplies the one deterministic headless mathematical authority those later blocks
consume.

## Persistent model intent

`SketchGeometricConstraint` stores:

```text
fixed
horizontal
vertical
parallel
perpendicular
equal_length
```

`SketchDrivingDimension` stores parameter-backed distance dimensions:

```text
horizontal_distance
vertical_distance
aligned_distance
point_to_point_distance
```

Dimensions use `SketchDimensionId`, reference two explicit line endpoint targets, and bind to an
existing `ParameterId` in `PartDocument`.

The older projected-reference `SketchConstraint` records additionally store:

```text
coincident_to_projected_point
parallel_to_projected_line
collinear_with_projected_line
```

`SketchTangentContinuity` stores explicit tangent continuity between supported curve entities.

These are persistent Sketch intent. They are distinct from the derived Block-109 residual/Jacobian/
solve result.

## Persistent validation

The historical model validates explicit deterministic references:

- geometric constraints target explicit line segments or, for `fixed`, explicit line endpoints;
- two-line constraints require two distinct explicit line targets;
- driving dimensions require two distinct explicit line endpoint targets;
- `PartDocument::add_sketch` verifies every driving-dimension parameter exists;
- dimension parameters are connected to the owning Sketch in the dependency graph.

This model validation remains the persistence boundary. General geometric feasibility and exact DOF
belong to `SketchConstraintSolver` after Block-108 topology migration.

## JSON format

Sketch JSON persists `geometric_constraints` and `driving_dimensions` arrays, for example:

```json
{
  "geometric_constraints": [
    {
      "id": "constraint.bottom.horizontal",
      "kind": "horizontal",
      "first": {"kind": "line_segment", "entity": "line.bottom"}
    },
    {
      "id": "constraint.width.equal",
      "kind": "equal_length",
      "first": {"kind": "line_segment", "entity": "line.bottom"},
      "second": {"kind": "line_segment", "entity": "line.top"}
    }
  ],
  "driving_dimensions": [
    {
      "id": "dim.width.bottom",
      "kind": "horizontal_distance",
      "first": {"kind": "line_segment_start", "entity": "line.bottom"},
      "second": {"kind": "line_segment_end", "entity": "line.bottom"},
      "parameter": "part.width"
    }
  ]
}
```

Block 109 does not change this historical PartDocument Sketch schema and does not persist a solver
cache.

## Dependency and invalidation behavior

For each driving dimension, `PartDocument` adds:

```text
<dimension-parameter> -> <sketch-id>
```

The existing Sketch-to-feature dependency then propagates changes:

```text
part.width -> sketch.dimensioned -> feature.dimensioned
```

Geometric constraints have no parameter dependencies of their own.

## Historical Geometry resolver

The optional Geometry layer retains `DimensionDrivenProfileResolver` for current recompute
compatibility. It resolves deterministic closed profiles where dimension targets address start/end of
the same explicit line:

- horizontal distance moves the next vertex along local X using historical line direction sign;
- vertical distance moves along local Y using historical line direction sign;
- aligned/point-to-point distance moves along the historical line direction;
- the resolved loop must close.

Current feature recompute may continue to use this compatibility resolver. It is not the Block-109
general solver and it does not define Interactive Sketcher DOF/conflict semantics.

## Block-109 solver integration

`SketchConstraintSystemBuilder::from_legacy(...)` consumes:

```text
SketchTopology
persisted Sketch
owning PartDocument
```

and produces a canonical `SketchConstraintSystem`.

Current mappings are:

```text
Fixed                    -> fixed
Horizontal               -> horizontal
Vertical                 -> vertical
Parallel                 -> parallel
Perpendicular            -> perpendicular
EqualLength              -> equal
TangentContinuity        -> tangent
HorizontalDistance       -> horizontal distance
VerticalDistance         -> vertical distance
AlignedDistance          -> aligned distance
PointToPointDistance     -> aligned distance
```

Driving-dimension parameters must be `ParameterType::Length`; values are consumed in millimeters.
Horizontal/vertical distance parameters are magnitudes in the historical model. The adapter derives a
signed solver target from the current Block-108 point order and coordinate direction so current
Sketches keep their authored axis orientation.

Stable solve-request ids use:

```text
constraint/<SketchConstraintId>
geometric-constraint/<SketchConstraintId>
dimension/<SketchDimensionId>
tangent/<SketchConstraintId>
```

The resulting system is sorted lexicographically by those ids.

Projected-reference constraints are translated to their semantic family. Block-108 projected point/
line topology deliberately contains no invented resolved coordinates, so the Block-109 solver reports
`invalid_reference` for those requests until the later associative-reference owner provides
coordinate-capable solver targets. The adapter must not derive solver identity from a sampled viewport
point or OCCT subshape.

## Direct solver families versus persistent authoring

The direct `SketchSolverConstraint` API supports all Block-109 initial families:

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

That does not retroactively claim the historical Sketch JSON can author every family. Blocks 114 and
115 introduce the user-facing constraint/dimension persistence and editing surfaces needed for the
remaining families. They must consume the existing Block-109 solver rather than add independent
constraint mathematics.

Canonical solver contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Test coverage

Historical coverage remains for JSON round-trip, dependency propagation, dimension-driven local profile
resolution, and feature recompute.

Block-109 focused proof additionally covers:

```text
[core][sketch-solver]
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

including current persisted constraint/dimension adaptation and a solved parameter-backed line.

## Remaining limitations

- driven/reference dimension authoring is Block 115;
- automatic inference acceptance and glyph interaction are Block 114;
- persistence/UI authoring for the complete direct solver family set is Block 114/115;
- associative projected reference coordinate targets are owned by Block 117;
- region recognition is Block 119;
- GUI live solve/direct manipulation begins in Block 110.
