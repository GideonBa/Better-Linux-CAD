# Sketch repair application command seed

Status: implemented seed. Commands are explicit and apply only a conservative safe subset.

This document describes the first sketch repair command layer. It consumes one selected `SketchRepairSuggestion` and applies it only when the action belongs to the implemented safe subset.

## Goal

The goal is to separate three responsibilities:

1. `SketchConstraintDiagnostics` detects simple sketch problems.
2. `SketchRepairSuggester` turns supported diagnostics into candidate actions.
3. `SketchRepairCommandExecutor` applies exactly one selected safe action only when the caller invokes it explicitly.

No repair command runs automatically during diagnostics or suggestion generation.

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_commands.hpp`.

```text
SketchRepairCommandStatus
  Applied
  SkippedUnsupported

SketchRepairCommand
  suggestion
  action
  target

SketchRepairCommandResult
  status
  message
  changed_constraint_ids[]
  changed_dimension_ids[]

SketchRepairCommandExecutor
  apply(Sketch&, SketchRepairCommand) -> Result<SketchRepairCommandResult>
```

`Sketch` also exposes the minimal mutation hooks required by the command layer:

```text
remove_geometric_constraint(SketchConstraintId)
remove_driving_dimension(SketchDimensionId)
```

These hooks are deliberately narrow. They remove one explicitly named record and return a validated result.

## Implemented safe command subset

The first command seed supports only these actions:

```text
AddFixedEndpointConstraint
RemoveDuplicateFixedEndpointConstraint
RemoveDuplicateDrivingDimension
```

`AddFixedEndpointConstraint` creates a deterministic fixed endpoint constraint id from the diagnostic target:

```text
line.free:start -> repair.fixed.line.free.start
line.free:end   -> repair.fixed.line.free.end
```

`RemoveDuplicateFixedEndpointConstraint` keeps the lexicographically first fixed endpoint constraint id for the target endpoint and removes the remaining duplicate fixed endpoint constraints.

`RemoveDuplicateDrivingDimension` keeps the lexicographically first driving dimension id for the target endpoint pair and removes the remaining duplicate dimensions.

## Unsupported suggestions

The executor intentionally skips these suggestion actions for now:

```text
RemoveConflictingOrientationConstraint
AddDrivingDimension
```

Removing one side of a horizontal/vertical conflict needs user or UI choice.

Adding a driving dimension needs parameter creation and a user-supplied dimension value. That is not implemented in this seed.

Skipped commands return `SketchRepairCommandStatus::SkippedUnsupported` and do not mutate the sketch.

## Test coverage

The seed has core tests for:

- applying a selected fixed-endpoint suggestion
- deterministic fixed endpoint constraint id generation
- removing duplicate fixed endpoint constraints while preserving the first deterministic record
- removing duplicate driving dimensions while preserving the first deterministic record
- skipping unsupported orientation-conflict suggestions without mutation

## Deliberate limitations

This block does not implement automatic repair.

It does not run commands during diagnostics or suggestion generation.

It does not implement undo, transactions, command grouping, GUI selection, automatic parameter creation, add-driving-dimension application, conflict-side choice, full solve iteration, exact DOF counting, spline-handle solving, or arbitrary model rewriting.

A later block should add a command transaction journal so repair commands can be previewed, applied, and undone safely.
