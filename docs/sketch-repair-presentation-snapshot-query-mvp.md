# Sketch repair presentation snapshot query seed

Status: implemented seed. Snapshot queries are read-only filters over `SketchRepairPresentationSnapshot` records.

This document describes the first query layer above the sketch repair presentation snapshot API.

## Goal

The goal is to let CLI and future GUI code derive filtered command-history views without mutating the original snapshot, undo stack, transaction records, or sketch.

The query layer must not:

- mutate a `Sketch`
- apply repair commands
- undo transactions
- pop or reorder undo-stack entries
- mutate the source `SketchRepairPresentationSnapshot`
- persist queried snapshots to `.blcad.json`
- introduce custom sorting, grouping, GUI widgets, icons, redo, or persistent history

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_presentation_snapshot_query.hpp`.

```text
SketchRepairPresentationSnapshotQuery
  optional category_filter
  optional priority_filter
  latest_only
  undoable_only
  matches(SketchRepairPresentationSnapshotEntry)

SketchRepairPresentationSnapshotQueryCounts
  category_count(category)
  priority_count(priority)
  total_count()

SketchRepairPresentationSnapshotQueryResult
  query
  snapshot
  counts

SketchRepairPresentationSnapshotQueryEngine
  execute(snapshot, query) -> SketchRepairPresentationSnapshotQueryResult
```

`SketchRepairPresentationSnapshotQuery` is a lightweight options record. The default query matches every entry.

## Supported filters

The first query seed supports:

```text
category_filter
priority_filter
latest_only
undoable_only
```

`category_filter` can filter by the existing presentation categories:

```text
safe_apply
requires_user_choice
requires_parameter_value
undo_entry
```

`priority_filter` can filter by:

```text
low
normal
high
```

`latest_only` keeps only the entry marked as the next entry that would be undone.

`undoable_only` keeps only entries whose `undoable` flag is true.

Filters are combined with AND semantics. For example, `category = undo_entry`, `priority = normal`, `latest_only = true`, and `undoable_only = true` keeps only entries that satisfy all four predicates.

## Order preservation

`SketchRepairPresentationSnapshotQueryEngine::execute` preserves the order of the source snapshot.

The resulting snapshot is a new copied snapshot containing matching entries only. The original snapshot is not modified and remains usable as the default presentation export.

This seed deliberately does not add sorting or grouping. Those operations would create a second ordering model and should be introduced separately if needed.

## Counts

`SketchRepairPresentationSnapshotQueryCounts` is computed from the filtered snapshot.

It exposes category totals and priority totals so CLI and future GUI code can display compact badges without scanning entries manually.

```text
category_count(SafeApply)
category_count(RequiresUserChoice)
category_count(RequiresParameterValue)
category_count(UndoEntry)

priority_count(Low)
priority_count(Normal)
priority_count(High)

total_count()
```

The count helpers describe the query result, not the original unfiltered snapshot.

## Debug JSON

The seed also provides:

```text
serialize_sketch_repair_presentation_snapshot_query_result_to_json(result)
```

The JSON schema marker is:

```text
blcad.sketch_repair_presentation_snapshot_query.debug
```

A representative result has this shape:

```json
{
  "schema": "blcad.sketch_repair_presentation_snapshot_query.debug",
  "version": 1,
  "query": {
    "category": "undo_entry",
    "priority": "normal",
    "latest_only": true,
    "undoable_only": true
  },
  "entry_count": 1,
  "empty": false,
  "category_counts": {
    "safe_apply": 0,
    "requires_user_choice": 0,
    "requires_parameter_value": 0,
    "undo_entry": 1
  },
  "priority_counts": {
    "low": 0,
    "normal": 1,
    "high": 0
  },
  "entries": []
}
```

The query JSON is for diagnostics, CLI output, future GUI inspection, and tests. It is not `.blcad.json` model intent and must not be loaded as persistent command history.

## Relationship to snapshots

`SketchRepairPresentationSnapshot` remains the unfiltered presentation output.

`SketchRepairPresentationSnapshotQueryResult` is for derived read-only views. It should be used when a caller wants to show only specific rows, for example only undoable rows or only the latest undo row.

The unfiltered snapshot JSON remains intact and should remain the default export when no filter is needed.

## Test coverage

The tests cover:

- empty queries over empty snapshots
- category filtering
- priority filtering
- latest-only filtering
- undoable-only filtering
- combined filters with AND semantics
- preservation of original snapshot order in filtered results
- category count helpers
- priority count helpers
- query-result debug JSON with query metadata and filtered entries

## Deliberate limitations

This block does not implement custom sorting.

It does not add grouping, saved filters, search text, label-id filters, pagination, GUI widgets, icons, localization, timestamps, redo, persistent command history, multi-sketch grouping, parameter-creating repairs, full solve iteration, exact DOF counting, or arbitrary model rewriting.

A later block should add stable presentation snapshot grouping helpers, such as grouping entries by display category or priority while preserving deterministic within-group order.
