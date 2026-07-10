# Sketch repair transaction and undo seed

Status: implemented seed. Repair transactions are in-memory records for explicit apply/undo flows.

This document describes the first transaction layer above `SketchRepairCommandExecutor`.

## Goal

The goal is to make safe repair commands reversible without introducing a full GUI undo stack, persistent journal, full sketch snapshots, or automatic model rewriting.

The flow is deliberately explicit:

1. `SketchConstraintDiagnostics` creates a diagnostic report.
2. `SketchRepairSuggester` creates non-mutating suggestions.
3. The caller selects one suggestion and creates a `SketchRepairCommand`.
4. `SketchRepairTransactionExecutor::apply` invokes the command and captures minimal undo metadata.
5. The caller may later explicitly invoke `SketchRepairTransactionExecutor::undo` for that transaction.

No transaction is created or undone automatically during diagnostics or suggestion generation.

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_transactions.hpp`.

```text
SketchRepairTransactionStatus
  Applied
  SkippedUnsupported
  Undone

SketchRepairTransaction
  status
  command
  command_result
  added_geometric_constraints[]
  removed_geometric_constraints[]
  removed_driving_dimensions[]

SketchRepairTransactionUndoResult
  status
  message
  restored_constraint_ids[]
  restored_dimension_ids[]
  removed_constraint_ids[]

SketchRepairTransactionExecutor
  apply(Sketch&, SketchRepairCommand) -> Result<SketchRepairTransaction>
  undo(Sketch&, SketchRepairTransaction) -> Result<SketchRepairTransactionUndoResult>
```

The transaction stores only affected records, not full sketch snapshots.

## Implemented undo coverage

The seed supports undo for the current safe command subset:

```text
AddFixedEndpointConstraint
RemoveDuplicateFixedEndpointConstraint
RemoveDuplicateDrivingDimension
```

For `AddFixedEndpointConstraint`, the transaction records the added `SketchGeometricConstraint`. Undo removes that exact constraint id.

For `RemoveDuplicateFixedEndpointConstraint`, the transaction records the removed `SketchGeometricConstraint` records. Undo restores those exact records through normal sketch validation.

For `RemoveDuplicateDrivingDimension`, the transaction records the removed `SketchDrivingDimension` records. Undo restores those exact records through normal sketch validation.

Unsupported or skipped command results produce `SkippedUnsupported` transactions. Undoing such a transaction returns `SkippedUnsupported` and does not mutate the sketch.

## Safety behavior

Undo checks for conflicting record IDs before restoring removed constraints or dimensions.

If a conflicting constraint or dimension id already exists, undo fails with a validation error instead of overwriting model intent.

The transaction layer uses the same narrow `Sketch` mutation hooks as the command layer:

```text
remove_geometric_constraint(SketchConstraintId)
remove_driving_dimension(SketchDimensionId)
add_constraint(SketchGeometricConstraint)
add_dimension(SketchDrivingDimension)
```

## Test coverage

The tests cover:

- applying and undoing a deterministic fixed endpoint constraint
- applying and undoing removal of duplicate fixed endpoint constraints
- applying and undoing removal of duplicate driving dimensions
- skipped unsupported transactions remaining non-undoable and non-mutating

## Deliberate limitations

This block does not implement a GUI undo stack.

It does not persist transactions to `.blcad.json`.

It does not support multi-sketch transactions, command grouping, redo, conflict-side choice, parameter-creating repairs, full solve iteration, exact DOF counting, spline-handle solving, or arbitrary model rewriting.

A later block should add a small in-memory repair undo stack that can push applied transactions, pop undo in reverse order, and optionally expose stack summaries to CLI/GUI code.
