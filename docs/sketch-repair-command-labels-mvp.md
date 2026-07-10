# Sketch repair command label seed

Status: implemented seed. Labels are stable presentation metadata for repair actions and undo-stack summary entries.

This document describes the first non-mutating label layer for the sketch repair infrastructure.

## Goal

The goal is to let CLI and future GUI code display repair suggestions, repair commands, and undo-stack entries without reconstructing user-facing text from low-level enum values.

The label layer must not:

- mutate a `Sketch`
- apply repair commands
- undo transactions
- change stack ordering
- persist labels to `.blcad.json`
- localize strings or introduce GUI widgets

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_command_labels.hpp`.

```text
SketchRepairCommandLabel
  title
  description

SketchRepairCommandLabeler
  label_for(SketchRepairSuggestionAction)
  label_for(SketchRepairUndoStackSummaryEntry)
```

`SketchRepairCommandLabel` is a plain presentation record. It contains no model ID, no timestamp, no command result, and no undo metadata.

## Action labels

The first label seed maps every current repair action to a deterministic title and description.

```text
AddFixedEndpointConstraint
  title: Add fixed endpoint constraint

RemoveConflictingOrientationConstraint
  title: Resolve horizontal/vertical conflict

RemoveDuplicateFixedEndpointConstraint
  title: Remove duplicate fixed endpoint constraints

RemoveDuplicateDrivingDimension
  title: Remove duplicate driving dimension

AddDrivingDimension
  title: Add driving dimension
```

The mapping intentionally includes actions that are not part of the safe command-application subset yet. This lets suggestion UIs display unsupported actions consistently while the command executor can still skip them safely.

## Undo-stack summary labels

`SketchRepairCommandLabeler::label_for(SketchRepairUndoStackSummaryEntry)` returns undo-oriented labels for stack entries.

Examples:

```text
RemoveDuplicateFixedEndpointConstraint
  title: Undo remove duplicate fixed endpoint constraints

RemoveDuplicateDrivingDimension
  title: Undo remove duplicate driving dimension
```

The summary-entry label uses the entry target inside the short description, so CLI/GUI code can show what will be affected before calling `SketchRepairUndoStack::undo_latest`.

## Debug JSON integration

`serialize_sketch_repair_undo_stack_summary_to_json` now includes these fields per entry:

```text
title
description
```

These fields are output-only. They are not persisted as model intent and must not be loaded as a source of truth for repair behavior.

## Test coverage

The tests cover:

- deterministic non-empty labels for all current repair actions
- exact titles for safe and unsupported repair actions
- undo-stack summary entry labels
- command-label fields in undo-stack summary debug JSON

## Deliberate limitations

This block does not implement localization.

It does not add GUI widgets, icons, severity colors, timestamps, grouping labels, redo labels, persistent command history, or user-overridable text.

It does not change diagnostics, suggestions, command application, transactions, undo stacks, or stack summary ordering.

A later block should add repair command presentation metadata such as stable machine-readable label IDs, optional severity/display category, and concise affected-record counts for CLI/GUI rendering.
