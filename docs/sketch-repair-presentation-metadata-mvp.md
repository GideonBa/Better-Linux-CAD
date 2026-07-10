# Sketch repair presentation metadata seed

Status: implemented seed. Presentation metadata is read-only display information for repair actions and undo-stack summary entries.

This document describes the first metadata layer above repair command labels and undo-stack summaries.

## Goal

The goal is to give CLI and future GUI code stable machine-readable metadata without deriving display behavior from low-level enum strings.

The metadata layer must not:

- mutate a `Sketch`
- apply repair commands
- undo transactions
- change undo-stack ordering
- persist metadata to `.blcad.json`
- replace the existing repair labels
- introduce localization, icons, or GUI widgets

## Implemented records

The implemented API lives in `include/blcad/core/sketch_repair_presentation_metadata.hpp`.

```text
SketchRepairDisplayCategory
  SafeApply
  RequiresUserChoice
  RequiresParameterValue
  UndoEntry

SketchRepairDisplayPriority
  Low
  Normal
  High

SketchRepairAffectedCounts
  added_constraints
  removed_constraints
  removed_dimensions
  total()

SketchRepairPresentationMetadata
  label_id
  category
  priority
  affected_counts
  affected_summary

SketchRepairPresentationMetadataProvider
  metadata_for(SketchRepairSuggestionAction)
  metadata_for(SketchRepairUndoStackSummaryEntry)
```

## Stable label ids

The seed uses deterministic label ids so presentation code can switch on stable identifiers instead of translated text.

Examples:

```text
repair.add_fixed_endpoint_constraint
repair.remove_conflicting_orientation_constraint
repair.remove_duplicate_fixed_endpoint_constraint
repair.remove_duplicate_driving_dimension
repair.add_driving_dimension
undo.remove_duplicate_driving_dimension
```

The ids are presentation identifiers, not model ids. They must not be serialized into `.blcad.json` as model intent.

## Categories

The first category set is intentionally small:

```text
safe_apply
requires_user_choice
requires_parameter_value
undo_entry
```

`safe_apply` covers currently safe command actions such as deterministic fixed endpoint insertion and duplicate-record removal.

`requires_user_choice` covers actions where the system must not choose a side automatically, such as horizontal/vertical conflict resolution.

`requires_parameter_value` covers actions where a future UI or caller must supply parameter/value intent before application, such as adding a driving dimension.

`undo_entry` covers existing undo-stack summary entries.

## Priority

The first priority set is deliberately lightweight:

```text
low
normal
high
```

Current mapping:

```text
RemoveConflictingOrientationConstraint -> high
RemoveDuplicateFixedEndpointConstraint -> normal
RemoveDuplicateDrivingDimension       -> normal
AddFixedEndpointConstraint             -> low
AddDrivingDimension                    -> low
Undo-stack entries                     -> normal
```

This priority is presentation guidance only. It does not affect command execution, validation, transaction undo, or recompute order.

## Affected counts and summary

Undo-stack entry metadata computes:

```text
added_constraints
removed_constraints
removed_dimensions
total
```

It also produces a concise `affected_summary`, for example:

```text
1 dimension removed
2 constraints removed
1 constraint added
No affected sketch records
```

The affected counts are derived from `SketchRepairUndoStackSummaryEntry`; no sketch state is inspected or changed.

## Debug JSON integration

`serialize_sketch_repair_undo_stack_summary_to_json` includes the metadata fields next to the existing label fields:

```text
label_id
display_category
display_priority
affected_counts
affected_summary
```

These fields are output-only. They are intended for CLI display, future GUI panels, and tests.

## Test coverage

The tests cover:

- stable label ids for repair actions
- display categories for safe, choice-required, and parameter-value-required actions
- display priority mapping
- empty affected counts for action-only metadata
- undo-stack affected record counts
- affected summary strings
- summary debug JSON metadata fields

## Deliberate limitations

This block does not implement localization.

It does not add icons, colors, timestamps, GUI widgets, redo metadata, persistent history records, user-overridable labels, or multi-sketch grouping metadata.

It does not change diagnostics, repair suggestions, command application, transactions, undo stacks, stack summaries, geometry recompute, or `.blcad.json` persistence.

A later block should add an optional presentation snapshot object that combines title, description, metadata, and existing summary fields into one CLI/GUI-friendly read-only record.
