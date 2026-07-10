# Sketch repair presentation snapshot seed

Status: implemented seed. Presentation snapshots are read-only CLI/GUI-facing rows built from undo-stack summaries, command labels, and presentation metadata.

This document describes the first combined presentation layer above `SketchRepairUndoStackSummary`, `SketchRepairCommandLabeler`, and `SketchRepairPresentationMetadataProvider`.

## Goal

The goal is to provide one stable row model for callers that want to display repair undo history without manually merging low-level summary records, labels, and metadata.

The snapshot layer must not:

- mutate a `Sketch`
- apply repair commands
- undo transactions
- pop or reorder undo-stack entries
- persist snapshots to `.blcad.json`
- replace lower-level summaries, labels, or metadata APIs
- introduce localization, icons, GUI widgets, redo, or persistent history

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_presentation_snapshot.hpp`.

```text
SketchRepairPresentationSnapshotEntry
  index
  latest
  transaction_status
  action
  target
  undoable
  title
  description
  label_id
  category
  priority
  affected_counts
  affected_summary

SketchRepairPresentationSnapshot
  entries[]
  entry_count
  empty
  latest()

SketchRepairPresentationSnapshotBuilder
  build(SketchRepairUndoStackSummary) -> SketchRepairPresentationSnapshot
```

Each entry is intentionally a copy of the data needed for display. It does not store pointers to the stack, the sketch, labels, or metadata providers.

## Builder behavior

`SketchRepairPresentationSnapshotBuilder::build` consumes a `SketchRepairUndoStackSummary`.

For each summary entry it:

1. preserves `index`, `latest`, `transaction_status`, `action`, `target`, and `undoable`
2. obtains `title` and `description` from `SketchRepairCommandLabeler`
3. obtains `label_id`, `category`, `priority`, `affected_counts`, and `affected_summary` from `SketchRepairPresentationMetadataProvider`
4. appends the resulting row in the same order as the source summary

The order remains oldest transaction to newest transaction. The row marked `latest = true` is the transaction that `SketchRepairUndoStack::undo_latest` would undo next.

## Debug JSON

The seed also provides:

```text
serialize_sketch_repair_presentation_snapshot_to_json(snapshot)
```

The JSON schema marker is:

```text
blcad.sketch_repair_presentation_snapshot.debug
```

This JSON is intended as preferred presentation output for CLI/debug tools and future GUI command-history panels. It is not `.blcad.json` model intent.

A representative entry has this shape:

```json
{
  "index": 1,
  "latest": true,
  "transaction_status": "applied",
  "action": "remove_duplicate_driving_dimension",
  "target": "line.a:start|line.a:end",
  "undoable": true,
  "title": "Undo remove duplicate driving dimension",
  "description": "Restore the duplicate driving dimensions that were removed for target line.a:start|line.a:end.",
  "label_id": "undo.remove_duplicate_driving_dimension",
  "display_category": "undo_entry",
  "display_priority": "normal",
  "affected_counts": {
    "added_constraints": 0,
    "removed_constraints": 0,
    "removed_dimensions": 1,
    "total": 1
  },
  "affected_summary": "1 dimension removed"
}
```

## Relationship to lower-level outputs

`SketchRepairUndoStackSummary` remains the lower-level inspection representation.

`serialize_sketch_repair_undo_stack_summary_to_json` remains useful for debugging the stack itself and still includes labels and metadata.

`SketchRepairPresentationSnapshot` is the preferred output when callers want one row per displayed command without performing their own merge of summary, label, and metadata data.

## Test coverage

The tests cover:

- empty snapshots
- empty snapshot debug JSON
- multi-entry stack snapshots
- stable stack ordering from oldest to newest
- latest-entry preservation
- propagation of title and description
- propagation of label id, category, priority, affected counts, and affected summary
- debug JSON with the snapshot schema and representative presentation fields

## Deliberate limitations

This block does not implement localization.

It does not add icons, GUI widgets, selection state, timestamps, redo snapshots, persistent command history, multi-sketch grouping, filtering, sorting, or grouping controls.

It does not change repair suggestions, repair commands, transactions, undo behavior, stack summaries, or model validation.

A later block should add read-only filtering and grouping helpers for presentation snapshots, such as filtering by display category, priority, latest entry, or undoable state, while keeping snapshot generation deterministic and non-mutating.
