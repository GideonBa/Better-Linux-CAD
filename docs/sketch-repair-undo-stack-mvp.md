# Sketch repair undo stack seed

Status: implemented seed. The stack is an in-memory LIFO helper for explicit repair transaction undo flows.

This document describes the first stack layer above `SketchRepairTransactionExecutor`.

## Goal

The goal is to let application, CLI, or later GUI code store applied repair transactions and undo them in strict reverse order without introducing redo, persistent journals, full sketch snapshots, or automatic repair execution.

The flow remains explicit:

1. `SketchConstraintDiagnostics` creates a diagnostic report.
2. `SketchRepairSuggester` creates non-mutating suggestions.
3. The caller selects a suggestion and creates a `SketchRepairCommand`.
4. `SketchRepairTransactionExecutor::apply` applies the command and captures minimal undo metadata.
5. `SketchRepairUndoStack::push` stores only applied and undoable transactions.
6. `SketchRepairUndoStack::undo_latest` pops and undoes the latest transaction by delegating to `SketchRepairTransactionExecutor::undo`.

No stack operation runs automatically during diagnostics, suggestion generation, or command application.

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_undo_stack.hpp`.

```text
SketchRepairUndoStackStatus
  Pushed
  RejectedNotUndoable
  Undone
  Empty

SketchRepairUndoStackResult
  status
  transaction_status
  message
  remaining_stack_size
  restored_constraint_ids[]
  restored_dimension_ids[]
  removed_constraint_ids[]

SketchRepairUndoStack
  empty()
  size()
  transactions()
  push(SketchRepairTransaction) -> Result<SketchRepairUndoStackResult>
  undo_latest(Sketch&) -> Result<SketchRepairUndoStackResult>
  clear()
```

The stack stores `SketchRepairTransaction` records directly and does not serialize sketch snapshots.

## Push behavior

`push` accepts only transactions where `transaction.undoable()` is true.

Applied but non-undoable transactions and skipped unsupported transactions return `RejectedNotUndoable` and are not stored.

A successful push returns `Pushed` and the new stack size.

## Undo behavior

`undo_latest` is strict LIFO.

If the stack is empty, it returns `Empty` and does not mutate the sketch.

If the latest transaction can be undone successfully, `undo_latest` delegates to `SketchRepairTransactionExecutor::undo`, removes that transaction from the stack, and returns `Undone` together with the remaining stack size and affected record ids.

If undo fails with a validation error, the stack keeps the transaction so the caller can inspect or retry after resolving the conflict.

## Safety behavior

The stack layer does not bypass transaction safety checks.

Undo still uses the transaction executor, which validates record restoration and refuses to overwrite existing model intent.

The stack layer does not inspect or mutate private sketch storage directly.

## Test coverage

The tests cover:

- rejecting non-undoable transactions without changing stack size
- pushing undoable transactions
- undoing multiple transactions in strict LIFO order
- preserving earlier transactions until later undos
- empty-stack undo returning a non-mutating `Empty` result

## Deliberate limitations

This block does not implement redo.

It does not persist undo stacks to `.blcad.json`.

It does not support multi-sketch stack coordination, command grouping, transaction squashing, GUI command history widgets, parameter-creating repairs, full solve iteration, exact DOF counting, or arbitrary model rewriting.

A later block should add non-mutating stack summaries for CLI/GUI inspection so callers can display which transactions are currently undoable before invoking `undo_latest`.
