# JSON File Workflow

Status: file-level workflow for `.blcad.json` model files and a headless JSON-to-STEP export example.

This document describes the file workflow after in-memory JSON serialization. The repository contains checked-in `.blcad.json` model files, filesystem helpers for reading and writing model files, and a small command-line example that recomputes the model and exports STEP without a GUI.

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

## Checked-in models

The repository contains the MVP-1 reference model:

```text
examples/reference_plate.blcad.json
```

It describes a 120 x 80 x 8 mm rectangular plate with a centered 20 mm through-hole.

The repository also contains the MVP-2 seed model:

```text
examples/top_face_cut.blcad.json
```

It describes the same basic plate, but the hole sketch is placed on a derived workplane:

```text
workplane.base_top -> feature.base_extrude.top
```

The hole center in that sketch is intentionally off-center:

```text
(25, -10)
```

This demonstrates that `.blcad.json` can store semantic generated-face references without storing OCCT face IDs, that the geometry layer can resolve the workplane before cutting, and that bounded top-face validation can reject invalid profile placement before OCCT execution.

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

Usage for the MVP-1 reference model:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
```

Usage for the MVP-2 seed model:

```bash
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
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
5. Resolve sketch workplanes during subtractive recompute.
6. Validate bounded profile placement when the workplane has bounds.
7. Read the final shape from the cache.
8. Write the final shape through `StepExporter::write_step`.
9. Print the number of exported bytes.

This keeps the command-line example thin. It does not know how features are computed internally; it only wires the existing core and geometry services together.

## Validation behavior

The file helpers reject:

- empty file paths
- unreadable input files
- unwritable output files
- invalid JSON content
- unsupported schema or version
- unsupported constructs
- invalid references caught by `PartDocument`
- unresolved dependency ordering, for example a derived workplane whose source feature never appears

Geometry recompute additionally rejects circle profiles that do not lie fully inside the resolved top-face bounds.

The CLI exits with:

- `0` on successful STEP export
- `1` on model, recompute, or export errors
- `2` on invalid command-line usage

## Test coverage

Core tests cover:

- writing a `PartDocument` to a temporary `.blcad.json` file
- reading that file back into a `PartDocument`
- preserving model objects and dependency edges
- derived workplane JSON roundtrip
- rejecting empty file paths

Geometry tests cover recompute and STEP export from a JSON-restored document, recompute from a sketch on a derived top-face workplane, workplane resolution, an off-center cut whose local sketch point is mapped through the resolved workplane, and bounded validation for near-edge valid and out-of-bounds invalid holes. The CLI itself is intentionally thin and currently covered indirectly through the same APIs.

## Deliberate limitation

Not included yet:

- command-line argument parser
- CLI integration test
- automatic creation of parent directories
- multiple output formats
- model validation report mode
- graphical loading of `.blcad.json`
- ShapeCache serialization
- full topological naming
- arbitrary face support beyond the current top-face case

The workflow is sufficient as the first real headless file-based CAD path and as a persistence path for the first semantic generated-face reference.
