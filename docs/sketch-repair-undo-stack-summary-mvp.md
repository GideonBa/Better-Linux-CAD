# Sketch repair undo stack summary seed

Status: implemented seed. The summary layer is a non-mutating inspection helper for the in-memory repair undo stack.

This document describes the first read-only stack summary layer above `SketchRepairUndoStack`.

## Goal

The goal is to let CLI, GUI, and debugging code inspect which repair transactions are currently undoable before calling `undo_latest`.

The summary layer must not:

- mutate the `Sketch`
- pop transactions from the stack
- apply repair commands
- undo transactions
- persist stack history to `.blcad.json`

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_undo_stack_summary.hpp`.

```text
SketchRepairUndoStackSummaryEntry
  index
  latest
  transaction_status
  action
  target
  undoable
  added_constraint_ids[]
  removed_constraint_ids[]
  removed_dimension_ids[]

SketchRepairUndoStackSummary
  entries[]
  stack_size
  empty
  latest()

SketchRepairUndoStackSummarizer
  summarize(SketchRepairUndoStack) -> SketchRepairUndoStackSummary
```

The summary entries are ordered from oldest transaction to newest transaction. The newest entry is marked with `latest = true` and is the transaction that `SketchRepairUndoStack::undo_latest` would undo next.

## Debug JSON

The seed also provides:

```text
serialize_sketch_repair_undo_stack_summary_to_json(summary)
```

The JSON schema marker is:

```text
blcad.sketch_repair_undo_stack_summary.debug
```

The JSON output is intended for diagnostics, CLI display, future GUI command history panels, and test snapshots. It is not `.blcad.json` model intent and must not be loaded as persistent undo history.

A representative entry has this shape:

```json
{
  "index": 1,
  "latest": true,
  "transaction_status": "applied",
  "action": "remove_duplicate_driving_dimension",
  "target": "line.a:start|line.a:end",
  "undoable": true,
  "added_constraint_ids": [],
  "removed_constraint_ids": [],
  "removed_dimension_ids": ["dim.b"]
}
```

## Test coverage

The tests cover:

- empty-stack summaries
- empty-stack debug JSON
- stack-order inspection from oldest to newest transaction
- marking the latest transaction
- exposing affected constraint and dimension IDs
- debug JSON containing action names and latest markers

## Deliberate limitations

This block does not implement redo.

It does not persist undo stacks or summaries to `.blcad.json`.

It does not add GUI widgets, command labels, timestamps, user-facing descriptions, transaction grouping, multi-sketch stack coordination, or history filtering.

A later block should add stable user-facing repair command labels and short descriptions so CLI/GUI code can display stack entries without reconstructing text from low-level enum values.
