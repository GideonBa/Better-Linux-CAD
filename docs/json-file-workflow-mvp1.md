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

## Checked-in models

The repository contains:

```text
examples/reference_plate.blcad.json
examples/top_face_cut.blcad.json
examples/bottom_face_cut.blcad.json
examples/right_face_cut.blcad.json
examples/left_face_cut.blcad.json
examples/front_face_cut.blcad.json
```

These examples demonstrate that `.blcad.json` can store semantic generated-face references without storing OCCT face IDs, that the geometry layer can resolve workplanes before cutting, and that bounded validation can reject invalid profile placement before OCCT execution.

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

Export commands:

```bash
./build/dev-geometry/blcad_export_step examples/reference_plate.blcad.json build/reference_plate.step
./build/dev-geometry/blcad_export_step examples/top_face_cut.blcad.json build/top_face_cut.step
./build/dev-geometry/blcad_export_step examples/bottom_face_cut.blcad.json build/bottom_face_cut.step
./build/dev-geometry/blcad_export_step examples/right_face_cut.blcad.json build/right_face_cut.step
./build/dev-geometry/blcad_export_step examples/left_face_cut.blcad.json build/left_face_cut.step
./build/dev-geometry/blcad_export_step examples/front_face_cut.blcad.json build/front_face_cut.step
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

## Test coverage

Core tests cover file write/read and JSON roundtrip for top, bottom, right, left, and front derived workplanes. Geometry tests cover recompute and STEP export from JSON-restored documents, recompute from sketches on derived top, bottom, right, left, and front workplanes, and bounded validation for valid and out-of-bounds holes.

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
- arbitrary face support beyond the current selected semantic face cases

The workflow is sufficient as the first real headless file-based CAD path and as a persistence path for semantic generated-face references.
