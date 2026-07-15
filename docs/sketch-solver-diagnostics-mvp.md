# Sketch solver diagnostics

This document distinguishes the historical lightweight Sketch diagnostic seed from the deterministic
Block-109 solver diagnostics.

The seed remains available for compatibility/presentation workflows. It is not exact DOF or general
constraint feasibility authority after Block 109. Exact local DOF, solver conflict/redundancy
attribution, convergence, and invalid-reference state now come from `SketchConstraintSolver`.

## Historical lightweight diagnostic seed

The existing seed types remain:

```text
SketchDiagnosticSeverity
SketchDiagnosticKind
SketchConstraintDiagnostic
SketchDiagnosticReport
SketchConstraintDiagnostics
```

`SketchConstraintDiagnostics::analyze(const Sketch&)` inspects historical Sketch records read-only. It
does not migrate topology, solve variables, mutate the document, or rewrite geometry.

### Seed under-constraint warnings

The heuristic seed warns about:

- explicit line endpoints not referenced by its known constraint/dimension subset;
- cubic Bezier control handles treated as free by the historical heuristic;
- profile intent without driving dimensions.

For the seed, an endpoint counts as constrained when referenced by fixed intent, projected-point
coincidence, or a driving dimension. This remains a conservative presentation heuristic and is not a
true DOF count.

### Seed over-constraint warnings/errors

The seed recognizes explicit record patterns:

- one line with both horizontal and vertical constraints;
- one endpoint with duplicate fixed endpoint constraints;
- multiple driving dimensions targeting the same endpoint pair;
- duplicate horizontal/vertical orientation constraints.

These checks remain useful before/without topology migration, but they do not replace general nonlinear
feasibility analysis.

### Seed debug JSON

`serialize_sketch_diagnostic_report_to_json(...)` produces a debug artifact with schema:

```text
blcad.sketch_diagnostics.debug
version 1
```

The debug artifact is not PartDocument model intent and is not persisted in `.blcad.json`.

## Block-109 exact solver diagnostics

Block 109 introduces a separate derived diagnostic boundary in `SketchSolveResult`.

Result states are exactly:

```text
fully_constrained
under_constrained
redundant
conflicting
non_convergent
invalid_reference
```

`SketchSolverDiagnosticKind` is:

```text
invalid_reference
redundant_constraint
conflicting_constraint
non_convergent
```

Each diagnostic contains:

```text
kind
constraint_id
message
```

Diagnostics are stable, headless solver output. They are not persistent model intent.

## Exact remaining DOF

Block 109 counts every non-reference Block-108 point as two canonical planar variables in
lexicographic `SketchPointId`, `X -> Y` order.

After a converged solve, the final normalized residual Jacobian is rank-reduced with the frozen solver
rank tolerances. Exact local remaining DOF is:

```text
remaining_dof = canonical_variable_count - final_jacobian_rank
```

Therefore `under_constrained` now carries a numeric remaining-DOF value. This supersedes the seed's
endpoint-warning heuristic for solver-aware consumers.

The count is a local differential DOF count at the converged state. It is not a global symbolic proof
for every disconnected or singular configuration.

## Redundancy attribution

At the final solution, constraint Jacobian blocks are appended in canonical constraint-id order. If a
non-empty block does not increase cumulative rank, its stable id is added to
`redundant_constraint_ids` and receives a `redundant_constraint` diagnostic.

This is deterministic canonical incremental attribution. For mathematically interchangeable duplicate
constraints, the earlier canonical constraint remains contributing and the later canonical id is
reported redundant.

## Conflict attribution

A stalled full system above convergence tolerance is checked by deterministic single-removal analysis:

```text
for constraint in canonical id order:
  remove exactly constraint
  re-solve from original variable snapshot
  if reduced system converges:
    attribute constraint
```

Attributed ids are published in stable canonical order and each receives a
`conflicting_constraint` diagnostic.

This is intentionally not described as a minimum unsatisfiable subset algorithm. Several members of
an inconsistent set may each be individually removable and therefore each be reported.

## Non-convergence

`non_convergent` is distinct from `conflicting`.

It is reported when:

- the deterministic maximum iteration count is reached; or
- numeric solving stalls above tolerance and the stable remove-one analysis cannot attribute a
  single-removal conflict.

The result contains iteration count and normalized residual summary so callers can distinguish poor
initial geometry/numeric difficulty from an explicitly attributed conflicting set.

## Invalid references

Constraint target family/type validation occurs before numeric solving. Missing `SketchPointId`,
missing entity, wrong target type, unsupported object capability, or a tangent pair without a shared
persistent topology endpoint produces `invalid_reference`.

This includes historical projected-reference records after legacy adaptation when Block-108 topology
has no persistent resolved coordinates for the projected point/line. Block 109 does not use sampled
Block-107 interaction geometry or OCCT lookup identity to fill the gap.

## Residual summary

`SketchSolverResidualSummary` publishes derived:

```text
residual_count
characteristic_length
initial_rms
final_rms
final_max_abs
```

Length residuals are normalized by deterministic characteristic length. Angular/orientation residuals
are dimensionless normalized families. The summary is useful to diagnostics and later GUI solve-status
presentation but is not persisted.

## Status precedence

Classification is frozen:

1. invalid target/reference -> `invalid_reference`;
2. maximum deterministic iteration limit -> `non_convergent`;
3. stall with non-empty remove-one attribution -> `conflicting`;
4. stall without attribution -> `non_convergent`;
5. convergence with canonical redundant ids -> `redundant`;
6. convergence with zero remaining DOF -> `fully_constrained`;
7. otherwise -> `under_constrained`.

No failure/diagnostic state mutates persistent Sketch intent.

## GUI and repair consumers

The historical seed remains available to existing repair/presentation code until those consumers are
migrated deliberately.

Block 110 publishes Block-109 baseline/live remaining DOF and solve status into the existing Sketch
status surface. Conflicting, non-convergent, or invalid-reference drag solves refuse/cancel the preview
without persistent mutation. Blocks 114/115 use stable solver constraint ids for glyph/
dimension conflict interaction. Block 119 may combine solver diagnostics with region/profile repair.

Those consumers must not parse diagnostic prose to recover identity; stable constraint ids and enum
kinds are the machine boundary.

Canonical solver contract: `docs/sketch-planar-constraint-solver-mvp8.md`.

## Focused proof

Historical seed tests remain in `[core][sketch-diagnostics]` coverage.

Block-109 exact diagnostics are proven by:

```text
[core][sketch-dof]
[core][sketch-conflict-diagnostics]
```

covering exact remaining DOF, canonical duplicate-constraint redundancy attribution, stable conflicting
constraint ids, invalid reference, and non-convergence classification.
