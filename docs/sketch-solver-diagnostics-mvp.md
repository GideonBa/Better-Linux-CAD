# Sketch solver diagnostics seed

This document describes the first sketch solver diagnostics seed.

The goal is not to solve constraints. The goal is to inspect the current deterministic sketch-intent subset and report obvious under-constrained and over-constrained situations without rewriting geometry.

## Implemented scope

The core model now has a lightweight diagnostics layer:

- `SketchDiagnosticSeverity`
- `SketchDiagnosticKind`
- `SketchConstraintDiagnostic`
- `SketchDiagnosticReport`
- `SketchConstraintDiagnostics`

`SketchConstraintDiagnostics::analyze` accepts a `Sketch` and returns a report. It does not mutate the sketch, the document, geometry, or dependency graph.

## Under-constrained diagnostics

The first seed emits warnings for these simple cases:

- explicit line start endpoints not referenced by the current deterministic constraint/dimension subset
- explicit line end endpoints not referenced by the current deterministic constraint/dimension subset
- cubic Bezier spline `control1` and `control2` handles, which are still free because no spline-handle solver exists
- sketches with profile intent but no driving dimensions

For this seed, an endpoint counts as constrained if it is referenced by:

- a fixed endpoint constraint
- a fixed whole-line constraint
- a projected-point coincidence constraint
- a driving dimension endpoint target

This is intentionally conservative. It is not a real degrees-of-freedom count.

## Over-constrained diagnostics

The first seed emits errors for these deterministic conflicts:

- one line has both horizontal and vertical constraints
- one endpoint has multiple fixed endpoint constraints
- multiple driving dimensions target the same endpoint pair

It also emits warnings for duplicate horizontal or duplicate vertical orientation constraints on the same line. These are redundant in the current subset, not necessarily fatal.

## Debug JSON output

Diagnostic snapshots are intentionally not model intent.

They are not serialized into `.blcad.json` through `PartDocument` JSON. Instead, the diagnostic layer offers `serialize_sketch_diagnostic_report_to_json` for debug/output artifacts.

The debug schema is:

```json
{
  "schema": "blcad.sketch_diagnostics.debug",
  "version": 1,
  "sketch": "sketch.main",
  "warning_count": 2,
  "error_count": 0,
  "diagnostics": [
    {
      "severity": "warning",
      "kind": "unconstrained_line_endpoint",
      "target": "line.a:start",
      "message": "line segment start endpoint is not constrained by the current deterministic subset"
    }
  ]
}
```

## Test coverage

The tests cover:

- simple under-constrained line endpoint warnings
- free spline control-point warnings
- undimensioned-profile warnings
- horizontal/vertical conflicts
- duplicate fixed endpoint diagnostics
- duplicate driving-dimension target diagnostics
- debug JSON serialization outside model intent

## Deliberate limitations

This block does not implement a constraint solver.

It does not calculate exact degrees of freedom.

It does not solve or repair sketches.

It does not validate geometric feasibility beyond the explicit deterministic checks above.

It does not solve spline handles, tangent continuity, projected references, arcs, trim/extend operations, or composite profile constraints.

It does not provide GUI highlighting or automatic repair actions.
