# Project and Save File Format

Status: target architecture. MVP-1 implements single-part `.blcad.json` model-intent persistence; the multi-document project file is a future block.

The file format must store the full parametric model, not only computed geometry. OCCT geometry may be stored additionally as a cache, but must never be the only source of information. A file that stored only the final shape would lose parameters, features, and references.

## Project structure (target)

```text
ProjectFile
  metadata
  global_parameters
  documents
    parts
      BasePlate
      CoverPlate
      Gasket
    assemblies
      MainAssembly
  resources
    materials
    standard_parts
  cached_shapes
```

## Part document (target)

```text
PartDocument BasePlate
  parameters
    plate_width = 120 mm
    plate_height = 80 mm
    plate_thickness = Assembly.plate_thickness

  datum_planes
    XY
    TopPlane

  sketches
    Sketch_BaseRectangle
    Sketch_HoleCircle

  features
    Extrude_Base
    Cut_HoleCircle
    Chamfer_OuterEdges

  semantic_references
    top_mounting_face
    outer_top_edges
    main_center_axis

  cached_shape
    OCCT_BRep_Shape (cache only)
```

## Current implemented scope

- `serialize_part_document_to_json` / `deserialize_part_document_from_json` for in-memory model intent.
- `write_part_document_json_file` / `read_part_document_json_file` for `.blcad.json` files.
- serialization stores model intent only; it does not serialize OCCT shapes or ShapeCache contents.
- `derived_workplanes` with `top/bottom/right/left/front/back` faces are persisted.

See `docs/json-serialization-mvp1.md` and `docs/json-file-workflow-mvp1.md`.

## Proposed implementation sequence

1. Keep the single-part `.blcad.json` intent format as the stable base.
2. Add per-feature serialization as new feature types land (holes, fillets, chamfers, patterns, mirrors, shaft).
3. Add semantic-reference serialization (`docs/semantic-references.md`).
4. Add expression and scope serialization (`docs/parameter-model.md`).
5. Introduce a multi-document project container holding several parts.
6. Add the assembly document (component instances, constraints), see `docs/assembly-system.md`.
7. Add project-level `resources` (materials, standard parts) referenced by engineering modules (`docs/engineering-modules.md`).
8. Optionally add an out-of-band `cached_shapes` store, kept clearly separate from model intent.

## Rules

- model intent is the source of truth; cached shapes are regenerable and optional.
- never serialize raw OCCT topology as a model reference; store semantic references instead.
- STEP export runs through normal recompute and is a projection of the model, not the save format. See `docs/step-export-mvp1.md`.

## Out of scope for the first versions

- binary or compressed container formats.
- embedding cached BRep as the primary persisted data.
- drawing and BOM documents (long-term, see `docs/project-goal.md`).
