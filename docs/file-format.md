# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly-parameter JSON, embedded project JSON, component instances with explicit placement/state updates, solver-independent Mate/Concentric/Distance assembly constraint records, and a derived read-only assembly constraint graph. Multi-body part records, body transforms, body booleans, semantic assembly target resolution, solver state, and path-feature records remain future blocks.

The file format must store the full parametric model, not only computed geometry. OCCT geometry may be stored additionally as a cache, but must never be the only source of information. A file that stored only the final shape would lose parameters, features, bodies, transforms, sketches, paths, references, assembly structure, component instances, and assembly relationships.

## Implemented project structure

The current project seed uses embedded documents:

```text
Project
  id
  name
  assembly              # embedded AssemblyDocument JSON
  parts[]               # embedded PartDocument JSON records
```

Schema marker:

```text
blcad.project.mvp4
```

Component instances, placement/state values, and assembly constraint model intent are persisted inside the embedded assembly document. Derived constraint graph connectivity is rebuilt from those records and is not persisted.

## Target project structure

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

The current embedded project format is intentionally simpler than the target container. A manifest-based project file that references separate part files is deferred.

## Assembly document records

Current implemented assembly records:

```text
AssemblyDocument
  parameters[]              # assembly-scoped parameters
  member_parts[]            # DocumentId list
  parameter_bindings[]      # part parameter follows assembly parameter
  component_instances[]     # MVP-5 free placement/state model intent
  assembly_constraints[]    # MVP-5 semantic relationship model intent
```

The assembly document currently retains this schema marker and version:

```text
blcad.assembly_document.mvp4
version 1
```

The marker is historical and retained for compatibility. Component instances and assembly constraints are MVP-5 features that extend the existing assembly schema additively through optional fields. Older files without `component_instances` or `assembly_constraints` remain loadable and produce empty corresponding collections.

A component instance has this implemented JSON shape:

```json
{
  "id": "component.bolt.1",
  "name": "Bolt 1",
  "referenced_part_document": "part.bolt",
  "visibility": "visible",
  "suppression_state": "active",
  "grounding_state": "grounded",
  "transform": {
    "translation_mm": {"x": 0.0, "y": 0.0, "z": 0.0},
    "rotation_deg": {"x": 0.0, "y": 0.0, "z": 0.0}
  }
}
```

A Distance assembly constraint has this implemented JSON shape:

```json
{
  "id": "constraint.bolt_spacing",
  "name": "Bolt spacing",
  "type": "distance",
  "target_a": {
    "component_instance": "component.bolt.1",
    "semantic_reference": "bolt.main_axis"
  },
  "target_b": {
    "component_instance": "component.bolt.2",
    "semantic_reference": "bolt.main_axis"
  },
  "state": "active",
  "distance": {
    "unit": "mm",
    "value": 40.0
  }
}
```

Mate and Concentric constraints use the same target/state structure and omit `distance`.

Rules:

- component instances reference part document ids instead of duplicating owned `PartDocument` records or their model intent
- project validation must resolve each component instance to an assembly member and owned project part document
- explicit placement/state updates preserve component id, name, and `referenced_part_document`
- updated transform, visibility, suppression state, and grounding state use the same existing JSON fields and roundtrip through assembly/project files
- all translation and rotation components must be finite; `NaN` and positive or negative infinity are rejected before the component instance enters the model
- transforms are direct free-placement records until a later solver exists
- `grounded` is persisted model intent but does not yet prevent an explicit transform update
- visibility and suppression are persisted state only until future assembly consumers define their execution/export/solver effects
- assembly constraints use typed ids and semantic component target tokens, not raw OCCT topology ids
- Mate and Concentric must not contain `distance`; Distance requires a millimeter length value
- constraint state is `active` or `inactive`
- adding or loading constraints does not mutate component transforms or serialize solved placement
- project structure validation includes constraint component targets
- the read-only `AssemblyConstraintGraph` is implemented as regenerable derived data and is intentionally not serialized
- graph nodes, active edges, adjacency, and connected groups are rebuilt from `component_instances` and active `assembly_constraints`
- DOF, semantic target geometry resolution, assembly-level geometry instancing, and assembly-level STEP export are not implemented

See `docs/assembly-constraint-model-intent-mvp5.md` for the persistent record boundary and `docs/assembly-constraint-graph-mvp5.md` for the derived graph boundary.

## Part document target

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

- `serialize_part_document_to_json` / `deserialize_part_document_from_json` for in-memory part model intent.
- `write_part_document_json_file` / `read_part_document_json_file` for `.blcad.json` files.
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json` for assembly parameters, member parts, parameter bindings, component instances, current placement/state values, and assembly constraint records.
- `serialize_project_to_json` / `deserialize_project_from_json` for embedded assembly plus embedded part documents, including assembly constraint intent through the embedded assembly JSON.
- `write_project_json_file` / `read_project_json_file` for `.blcad.project.json` files.
- serialization stores model intent only; it does not serialize OCCT shapes, `ShapeCache` contents, constraint graph connectivity, resolved assembly target descriptors, solved assembly transforms, or DOF state.
- `AssemblyConstraintGraph` rebuilds nodes, active edges, adjacency, and connected groups from persisted component and constraint intent.
- `derived_workplanes` with `top/bottom/right/left/front/back` faces are persisted.

See `docs/json-serialization-mvp1.md`, `docs/json-file-workflow-mvp1.md`, `docs/assembly-parameters-mvp4.md`, `docs/project-container-mvp4.md`, `docs/component-instance-mvp5.md`, `docs/assembly-constraint-model-intent-mvp5.md`, and `docs/assembly-constraint-graph-mvp5.md`.

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
10. Extend project files from embedded documents to optional manifest/external-file references.
11. Assembly constraint model-intent records on semantic component targets are implemented; keep them independent from solver/cache state.
12. The read-only constraint graph is implemented and intentionally has no persisted graph field because nodes, active edges, adjacency, and connected groups are regenerable from component instances and active constraint records.
13. Keep the next semantic target-resolution descriptors derived and unpersisted unless a later cache design proves a concrete need.
14. Add regenerable assembly solver/cache records only after semantic target resolution and a rigid-body solver exist, and persist only data that is not safely derivable.
15. Add project-level `resources` (materials, standard parts) referenced by engineering modules (`docs/engineering-modules.md`).
16. Optionally add an out-of-band `cached_shapes` store, kept clearly separate from model intent.

## Rules

- model intent is the source of truth; cached shapes are regenerable and optional.
- never serialize raw OCCT topology as a model reference; store semantic references instead.
- STEP export runs through normal recompute and is a projection of the model, not the save format. See `docs/step-export-mvp1.md`.
- multi-body records, body transforms, body booleans, path features, component instances, and assembly constraints must be serialized as explicit model intent before geometry cache support is added for them.
- read-only constraint graph connectivity is regenerated from component and constraint model intent rather than serialized as a second source of truth.
- future semantic assembly target-resolution results should remain regenerable derived data unless an explicit cache format is introduced.

## Out of scope for the first versions

- binary or compressed container formats.
- embedding cached BRep as the primary persisted data.
- automatic inference of body ownership for old sketches.
- arbitrary matrix transforms as a first user-facing transform operation.
- drawing and BOM documents (long-term, see `docs/project-goal.md`).
