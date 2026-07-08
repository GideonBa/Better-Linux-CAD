# MVP 1 Parameter-Value Update and Numeric Recompute

Status: core operation for changing a parameter value.

This document describes the step that completes incremental recompute at the content level: a parameter value can be changed in `PartDocument`, and the dependent part of the model is recomputed afterward.

## Goal

Until this step, parameters were immutable after creation. The recompute lifecycle could only be demonstrated structurally. Now a value can actually be changed, so the computed geometry changes visibly. This satisfies the MVP-1 success criterion that changing `hole_diameter` or `thickness` recomputes the shape.

## Public interface

Core headers:

```text
include/blcad/core/parameter.hpp
include/blcad/core/part_document.hpp
```

New operations:

```text
Parameter::with_value(value)
PartDocument::set_parameter_value(id, value)
```

`Parameter::with_value` returns a copy of the parameter with a new value and the same identity. The new value goes through the same validation as during creation. `Parameter` therefore remains immutable; a change creates a new value instead of mutating an existing one.

`PartDocument::set_parameter_value` sets the value of an existing parameter and marks the parameter and its dependents as changed. Like `mark_parameter_changed`, the method returns the affected graph nodes.

## Flow

`set_parameter_value` works in this order:

1. Validate the parameter ID.
2. Find the parameter in `PartDocument`.
3. Validate the new value through `Parameter::with_value`.
4. Replace the parameter in the document with the new value.
5. Mark the parameter and its transitive dependents as `changed` or `dirty`.

The dependent part of the model thereby becomes `dirty` for the next recompute plan. The actual geometric recompute remains in the geometry layer (`GeometryRecomputeExecutor::execute_plan`).

## Interaction with recompute

A typical numeric flow for the reference part:

1. Fully compute the document: `execute_document(document, cache)`.
2. Mark the document clean: `document.mark_all_clean()`.
3. Change a value: `set_parameter_value("part.hole_diameter", 40 mm)`.
4. Derive a `RecomputePlan` from the `dirty` state. It contains only the affected cut, not the base body.
5. Recompute only the affected features with `execute_plan(...)`. The cut reads the unchanged base body from the `ShapeCache`.
6. Call `mark_all_clean()` again afterward.

A larger hole diameter removes more material, so the final volume becomes smaller.

## Validation

Current error cases:

- empty parameter ID
- parameter does not exist in the document
- value violates parameter validation, which for MVP 1 means a length greater than `0`

For MVP 1, a non-positive length value cannot already be created at the `Quantity` level. Validation in `with_value` is still kept as the single validation path so it continues to apply to later parameter types.

## Test coverage

Current tests check:

- `set_parameter_value` updates the value and marks sketch and feature as `dirty`, while unaffected parameters remain `clean`
- `set_parameter_value` rejects missing and empty parameter IDs
- the reference part recomputes only the cut after a real diameter change, and the final volume becomes smaller when the hole grows

## Deliberate limitation

Not included yet:

- formulas or expressions between parameters
- automatic geometric recompute directly from `set_parameter_value`
- document serialization as JSON or a custom file format
- parameter types other than `length`

## Next useful step

The next small step should make model intent persistent:

1. prepare JSON serialization for `PartDocument`, including parameters, sketches, and features
2. rebuild the document from JSON and recompute it
3. continue to treat the `ShapeCache` only as a computed result
4. continue not to build a general solver or GUI
