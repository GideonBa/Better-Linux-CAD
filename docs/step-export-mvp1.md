# MVP 1 STEP Export

Status: optional OCCT STEP export for the final shape.

This document describes STEP export for MVP 1. The code lives in the optional target `blcad_geometry`, so it remains outside the pure core target. The export completes the first vertical MVP-1 line: from parameters, sketch, and feature through the OCCT shape to the STEP file.

## Goal

This step writes a computed `GeometryShape` as a STEP file:

- only a `GeometryShape` is exported, in practice the final shape from the `ShapeCache`
- the exporter encapsulates the OCCT STEP writer behind the core/OCCT boundary
- `blcad_core` remains free of OCCT and knows no STEP writer
- expected errors are reported as `ErrorCategory::Export`

## CMake target

The export is built only with the geometry preset:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
ctest --preset dev-geometry
```

The STEP writer additionally requires the OCCT toolkits `TKSTEP` and `TKXSBase`.

## Public interface

The public header is:

```text
include/blcad/geometry/step_exporter.hpp
```

Current type:

- `StepExporter`: OCCT adapter for STEP export

Current operation:

```text
write_step(shape, path)
```

`write_step` returns the size of the written file in bytes. Like the other adapters, `StepExporter` accesses the OCCT shape as a `friend` of `GeometryShape` through the non-public header `src/geometry/geometry_shape_internal.hpp`.

## Operation

Currently implemented:

```text
write_step(shape, path)
```

Rules:

- the target path `path` must not be empty
- the shape must be a non-empty `GeometryShape`
- the STEP schema is fixed to `AP214`
- the shape is transferred as `STEPControl_AsIs`
- after writing, the file size is checked; an empty file is an error
- expected OCCT and filesystem errors are reported as `ErrorCategory::Export`

## Test coverage

Current geometry tests check:

- a cut plate is written as a non-empty STEP file whose size matches the reported byte count and whose content starts with the STEP header `ISO-10303-21`
- an empty shape is rejected as an export error without creating a file
- an empty path is rejected as an export error

## Deliberate limitation

Not included yet:

- STEP import (planned after the Part Construction MVP in Blocks 95–101 of
  `docs/step-import-sequence-mvp7.md`)
- metadata roundtrip
- colors or materials
- assembly STEP
- STL or other export
- calling export directly from `PartDocument` or recompute

## Next useful step

With export implemented, the first vertical MVP-1 line is technically complete. The next useful step is to connect these building blocks into one end-to-end flow:

1. integrate the `ShapeCache` into `PartDocument`
2. automatically call `mark_all_clean()` after successful recompute
3. create and export the reference part from the specification as an end-to-end test
4. continue not to build a general solver or GUI
