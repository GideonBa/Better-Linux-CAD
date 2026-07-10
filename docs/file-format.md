# Project and Save File Format

Status: target architecture. MVP-1 implements single-part `.blcad.json` model-intent persistence; the multi-document project file, multi-body part records, body transforms, body booleans, and path-feature records are future blocks.

The file format must store the full parametric model, not only computed geometry. OCCT geometry may be stored additionally as a cache, but must never be the only source of information. A file that stored only the final shape would lose parameters, features, bodies, transforms, sketches, paths, and references.

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

  bodies
    body.base
    body.tool

  body_transforms
    transform.move_tool
    transform.rotate_insert

  body_booleans
    boolean.subtract_tool_from_base

  datum_planes
    XY
    TopPlane

  sketches
    Sketch_BaseRectangle
    Sketch_HoleCircle
    Sketch_LoftRoot
    Sketch_LoftTip

  path_curves
    path.sweep_centerline

  features
    Extrude_Base
    Cut_HoleCircle
    PathExtrude_Rib
    Loft_DuctTransition
    Chamfer_OuterEdges

  semantic_references
    top_mounting_face
    outer_top_edges
    main_center_axis

  cached_shapes
    body.base -> OCCT_BRep_Shape (cache only)
    body.tool -> OCCT_BRep_Shape (cache only)
```

## Multi-body and transform target records

Future part files should persist explicit body intent:

```json
{
  "bodies": [
    {"id": "body.base", "name": "Base", "kind": "solid"},
    {"id": "body.tool", "name": "Tool", "kind": "solid"}
  ],
  "body_transforms": [
    {
      "id": "transform.move_tool",
      "body": "body.tool",
      "kind": "translate",
      "vector": {"x": 10.0, "y": 0.0, "z": 0.0},
      "apply_to_owned_sketches": true,
      "apply_to_owned_construction_geometry": true
    }
  ],
  "body_booleans": [
    {
      "id": "boolean.subtract_tool",
      "operation": "subtract",
      "target_body": "body.base",
      "tool_bodies": ["body.tool"],
      "result_mode": "modify_target",
      "keep_tool_bodies": false
    }
  ]
}
```

Rules:

- body records are model intent, not cached shape records
- body transforms preserve ordered transform-stack intent
- body booleans reference semantic `BodyId` values
- owned-sketch transform behavior is explicit, not inferred silently
- cached body shapes are optional and never replace body/feature intent

## Path-feature target records

Future files should persist connected path curves and multi-section profile references:

```json
{
  "path_curves": [
    {
      "id": "path.rib_sweep",
      "segments": [
        {"kind": "sketch_line", "sketch": "sketch.path", "entity": "line.a"},
        {"kind": "sketch_arc", "sketch": "sketch.path", "entity": "arc.b"},
        {"kind": "sketch_spline", "sketch": "sketch.path", "entity": "spline.c"}
      ],
      "closure": "open"
    }
  ],
  "features": [
    {
      "id": "feature.path_extrude",
      "type": "additive_extrude",
      "input_profile": {"sketch": "sketch.profile", "profile": "profile.rib"},
      "direction_mode": "path",
      "path_curve": "path.rib_sweep",
      "operation_mode": "new_body"
    },
    {
      "id": "feature.loft",
      "type": "loft",
      "sections": [
        {"sketch": "sketch.root", "profile": "profile.root"},
        {"sketch": "sketch.mid", "profile": "profile.mid"},
        {"sketch": "sketch.tip", "profile": "profile.tip"}
      ],
      "guide_curves": ["path.leading_edge", "path.trailing_edge"],
      "operation_mode": "new_body"
    }
  ]
}
```

Rules:

- path segments are semantic references to sketch, construction, projected, 3D-sketch, or generated semantic curve sources
- path records store ordered connected chains, not raw OCCT wires
- loft section order is explicit
- loft section sketches may be on arbitrary planes
- section alignment/seam metadata should be added before complex closed-profile lofts

The detailed target is in `docs/multi-body-transform-and-path-features-roadmap.md`.

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
5. Add `BodyId`, `Body`, and feature-output body serialization to the single-part format.
6. Add `body_transforms` and sketch-ownership transform flags.
7. Add `body_booleans` for add/subtract/intersect between bodies.
8. Add `path_curves` as ordered semantic path chains.
9. Add path-following extrude/cut and loft feature serialization.
10. Introduce a multi-document project container holding several parts.
11. Add the assembly document (component instances, constraints), see `docs/assembly-system.md`.
12. Add project-level `resources` (materials, standard parts) referenced by engineering modules (`docs/engineering-modules.md`).
13. Optionally add an out-of-band `cached_shapes` store, kept clearly separate from model intent.

## Rules

- model intent is the source of truth; cached shapes are regenerable and optional.
- never serialize raw OCCT topology as a model reference; store semantic references instead.
- STEP export runs through normal recompute and is a projection of the model, not the save format. See `docs/step-export-mvp1.md`.
- multi-body records, body transforms, body booleans, and path features must be serialized as explicit model intent before geometry cache support is added for them.

## Out of scope for the first versions

- binary or compressed container formats.
- embedding cached BRep as the primary persisted data.
- automatic inference of body ownership for old sketches.
- arbitrary matrix transforms as a first user-facing transform operation.
- drawing and BOM documents (long-term, see `docs/project-goal.md`).
