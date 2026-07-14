# Part Multi-body STEP Export MVP-6

Status: implemented in Block 93.

## Scope

Block 93 extends `StepExporter` with `write_part_step(document, shape_cache, path)`. The operation
exports every visible Solid or Surface Body from one already-recomputed `PartDocument` into one
named XDE/STEP document. It consumes the existing Body results directly from `ShapeCache`; export
does not recompute, copy persistent intent, or select the historical single final shape.

The original single-`GeometryShape` `write_step` API remains available for compatibility.

## Selection and ordering

Only `BodyVisibility::Visible` Bodies participate. A hidden Body is excluded even when its shape is
present in the cache. Visible Bodies are ordered lexicographically by `BodyId`, independently of
authored insertion order, display name, or cache publication order. At least one visible Body is
required, and every selected Body must have a non-empty cached result.

The exporter validates the persistent kind against computed topology:

- a Solid Body must contain solid topology;
- a Surface Body must contain face topology and no solid topology.

Type mismatches fail before the STEP file is written. Both kinds are added as free XDE shape
definitions and transferred together with AP214 name support.

## Deterministic exchange identity

`BodyId` is the sole BLCAD identity input for an exported Body definition. The exchange name is:

```text
blcad:body-definition:<encoded BodyId>
```

ASCII letters, digits, `.`, `_`, and `-` remain literal. Every other UTF-8 byte is percent-encoded
with uppercase hexadecimal digits. This is the same collision-free segment discipline used by the
structured Assembly STEP exchange graph. Authored Body display names do not affect identity or
order, and STEP/XDE entity ids are never persisted.

`PartStepExportSummary` reports the ordered Body ids, exchange names, Body kinds, solid/surface
counts, total Body count, and written byte count so headless callers can inspect the exact export
selection without parsing transient STEP entity numbers.

## Immutability and cache reuse

The API receives both model and cache as `const`. It passes each cached OCCT shape directly into
its XDE definition and does not mutate `PartDocument`, `ShapeCache`, Body visibility, feature
results, or invalidation state. Missing, empty, or kind-incompatible visible results return an
`ErrorCategory::Export` diagnostic and do not create an output file.

## Persistence

Block 93 adds no BLCAD JSON fields. Body identity, kind, and visibility already persist in the
existing Part schema; exchange names are derived at export time. STEP/XDE labels and entity ids
remain transient exchange implementation details.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][multi-body-step-export]"
```

The proof covers combined Solid/Surface export, deterministic ordering and percent-encoded names,
hidden-Body filtering, file/name output, source-document and cache immutability, missing visible
results, kind-incompatible topology, no-visible-Body rejection, and empty-path rejection.

## Handoff

Blocks 48–94 are implemented. Block 94 acceptance is canonical in
`docs/part-construction-mvp6-acceptance.md`; Block 95 Qt application shell, GUI document session, and
command architecture is the current next technical step.
