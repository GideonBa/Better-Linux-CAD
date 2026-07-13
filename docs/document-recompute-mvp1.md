# MVP 1 Document Recompute and Reference Part

Status: optional geometry execution for a full document recompute.

This document describes the step that connects the individual MVP-1 building blocks into one continuous flow: a `PartDocument` is fully computed into a `ShapeCache` and exported as a STEP file. The code lives in the optional target `blcad_geometry`.

## Goal

This step closes the first vertical MVP-1 line into an end-to-end flow:

- `PartDocument` describes the design intent.
- `GeometryRecomputeExecutor` computes all document features into a `ShapeCache` in topological order.
- `StepExporter` writes the final shape as a STEP file.
- The `ShapeCache` remains a reproducible result generated from the model.

## Boundary between core and ShapeCache

The MVP-1 specification names a `shape_cache` as a field of `PartDocument`. The concrete `ShapeCache`, however, holds OCCT geometry and therefore must remain outside the OCCT-free core target.

The chosen solution satisfies both requirements:

- `PartDocument` remains in `blcad_core` and free of OCCT.
- The `ShapeCache` remains in `blcad_geometry`.
- `GeometryRecomputeExecutor` connects both by reading the document and generating the cache from the model.

The document therefore does not own the cache directly. The cache is a computed result next to the document, exactly as required by the ShapeCache rules.

## Public interface

Header:

```text
include/blcad/geometry/recompute_executor.hpp
```

New operation:

```text
execute_document(document, shape_cache)
```

`execute_document` reads the topological order of the document's dependency graph and executes every feature node in that order. Non-feature nodes such as parameters and sketches are skipped. The topological order ensures that an `AdditiveExtrude` runs before the dependent `SubtractiveExtrude`.

By contrast, `execute_plan` executes only the `dirty` nodes of a `RecomputePlan`. `execute_document` ignores the invalidation state and rebuilds the entire cache.

Since Block 52, explicit Body-result Features additionally write deterministic `BodyId`-scoped
results. Body nodes in a plan are derived boundaries; their producer/consumer Feature nodes perform
the Geometry work. Explicit Body documents commit full and incremental execution transactionally,
and an incremental plan preserves Body entries it does not affect. Historical zero-Body documents
retain the original partial-result-on-failure compatibility behavior.

`BodyResultInspector` provides the public headless post-recompute view of Body IDs, kind,
visibility, producing Feature, shape summary, and Solid/Surface counts.

## Recompute lifecycle

The executor works on a `const PartDocument` and does not modify its invalidation state. Calling `mark_all_clean()` after a successful recompute remains the caller's responsibility. This keeps execution free of document mutation.

A typical flow:

1. Fully compute the document: `execute_document(document, cache)`.
2. Mark the document clean: `document.mark_all_clean()`.
3. After a parameter change, call `mark_parameter_changed(...)`.
4. Derive a `RecomputePlan` from the `dirty` state.
5. Recompute only the affected features with `execute_plan(...)`.
6. Call `mark_all_clean()` again afterward.

An incremental plan for a hole change contains only the cut. The base body remains clean and must be present in the `ShapeCache` from an earlier recompute.

## Reference part

The end-to-end test creates the reference part from the specification:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

The test proves the MVP-1 geometry success criteria:

- all four parameters are set
- the plate is created as an `AdditiveExtrude`
- the hole is created as a `SubtractiveExtrude` cut through the body
- the final shape has exactly one solid and a smaller volume than the full plate
- the final shape is exported as a valid STEP file

## Test coverage

Current tests check:

- `execute_document` fully computes the reference part and exports a valid, non-empty STEP file
- recompute follows a clean/dirty lifecycle: after `mark_all_clean()`, the recompute plan is empty; a hole change creates an incremental plan only for the cut

## Deliberate limitation

Not included yet:

- a method for changing a parameter value in `PartDocument`
- therefore no real numeric incremental recompute yet
- document serialization as JSON or a custom file format
- ShapeCache as a direct field of `PartDocument`
- GUI

## Next useful step

The next small step should complete the content side of incremental recompute:

1. add an operation to change a parameter value in `PartDocument`
2. use it to test a real numeric incremental recompute by changing `hole_diameter` or `thickness` and checking shape and volume
3. then prepare JSON serialization of the model intent
4. continue not to build a general solver or GUI
