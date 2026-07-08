# MVP 1 JSON File Workflow

Status: file-level workflow for `.blcad.json` model files and a headless JSON-to-STEP export example.

This document describes the step after in-memory JSON serialization. The repository now contains a real `.blcad.json` model file, filesystem helpers for reading and writing model files, and a small command-line example that recomputes the model and exports STEP without a GUI.

## Goal

The goal is to prove a headless CAD workflow:

```text
.blcad.json -> PartDocument -> ShapeCache -> STEP
```

The workflow remains deliberately narrow:

- the JSON file stores model intent
- the deserializer rebuilds `PartDocument` through the validated construction APIs
- the geometry layer recomputes a fresh `ShapeCache`
- the STEP exporter writes the final computed shape
- no OCCT geometry is serialized into the JSON file
- no GUI is involved

## Public core helpers

Header:

```text
include/blcad/core/part_document_json.hpp
```

File-level helpers:

```text
write_part_document_json_file(document, path)
read_part_document_json_file(path)
```

`write_part_document_json_file` serializes the document and writes a UTF-8 JSON file. It returns the written file size.

`read_part_document_json_file` reads a UTF-8 JSON file and rebuilds a `PartDocument` through `deserialize_part_document_from_json`.

Both helpers report expected errors through `Result<T>` and `ErrorCategory::Validation`.

## Reference model

The repository contains the MVP-1 reference model:

```text
examples/reference_plate.blcad.json
```

It describes a 120 x 80 x 8 mm rectangular plate with a centered 20 mm through-hole:

```text
width         = 120 mm
height        = 80 mm
thickness     = 8 mm
hole_diameter = 20 mm
```

The file stores:

- document identity
- four length parameters
- the standard XY datum plane
- one rectangle sketch
- one circle sketch
- one additive extrude
- one subtractive through-all cut

## Headless export example

The repository contains a small command-line example:

```text
examples/blcad_export_step.cpp
```

It is built only when the optional geometry target is enabled:

```bash
cmake --preset dev-geometry
cmake --build --preset dev-geometry
```

Usage:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
```

Depending on the exact CMake build directory, the binary may be located under the configured geometry build directory. The command accepts exactly two arguments:

```text
blcad_export_step <input.blcad.json> <output.step>
```

## Execution flow

The CLI performs this sequence:

1. Read the `.blcad.json` file through `read_part_document_json_file`.
2. Rebuild the `PartDocument` from JSON.
3. Create a fresh `ShapeCache`.
4. Execute `GeometryRecomputeExecutor::execute_document`.
5. Read the final shape from the cache.
6. Write the final shape through `StepExporter::write_step`.
7. Print the number of exported bytes.

This keeps the command-line example thin. It does not know how features are computed internally; it only wires the existing core and geometry services together.

## Validation behavior

The file helpers reject:

- empty file paths
- unreadable input files
- unwritable output files
- invalid JSON content
- unsupported schema or version
- unsupported MVP-1 constructs
- invalid references caught by `PartDocument`

The CLI exits with:

- `0` on successful STEP export
- `1` on model, recompute, or export errors
- `2` on invalid command-line usage

## Test coverage

Core tests cover:

- writing a `PartDocument` to a temporary `.blcad.json` file
- reading that file back into a `PartDocument`
- preserving model objects and dependency edges
- rejecting empty file paths

Geometry tests already cover recompute and STEP export from a JSON-restored document. The CLI itself is intentionally thin and currently covered indirectly through the same APIs.

## Deliberate limitation

Not included yet:

- command-line argument parser
- CLI integration test
- automatic creation of parent directories
- multiple output formats
- model validation report mode
- graphical loading of `.blcad.json`
- ShapeCache serialization

The workflow is sufficient as the first real headless file-based CAD path.
