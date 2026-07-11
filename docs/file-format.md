# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, component instances, Mate/Concentric/Distance relationship intent, and the derived assembly graph/target-resolution/transform/residual/solver pipeline. Multi-body part records, body transforms, body booleans, persistent DOF/solve caches, and path-feature records remain future blocks.

The file format stores parametric model intent, not only computed geometry. OCCT geometry and solve products may later be cached separately, but they must never replace the persistent model relationships required to regenerate them.

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

Component instances, placement/state values, and assembly constraint model intent are persisted inside the embedded assembly document.

Derived graph connectivity, resolved targets, assembly-space frames, Mate/Distance residual descriptors, numeric Jacobians, and `AssemblySolveResult` values are rebuilt from persisted model intent and are not stored.

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

The current embedded project format is intentionally simpler than the target container. Manifest-based separate part files remain deferred.

## Assembly document records

Current implemented assembly records:

```text
AssemblyDocument
  parameters[]
  member_parts[]
  parameter_bindings[]
  component_instances[]
  assembly_constraints[]
```

The assembly document retains the historical compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

Component instances and assembly constraints are MVP-5 features added through optional fields. Older files without `component_instances` or `assembly_constraints` remain loadable and produce empty collections.

A component instance has this implemented shape:

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

A Distance assembly constraint has this implemented shape:

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

Mate and Concentric use the same target/state structure and omit `distance`.

Assembly persistence rules:

- component instances reference part document ids instead of duplicating owned `PartDocument` records
- project validation resolves every component reference through assembly membership to a project-owned part
- explicit placement/state updates preserve component id, name, and `referenced_part_document`
- transform components must be finite
- `translation_mm` is stored in millimeters
- `rotation_deg` is stored in degrees
- transform evaluation uses active right-handed fixed-axis X-then-Y-then-Z rotation, equivalent to `Rz * Ry * Rx` for column vectors
- the evaluator does not persist a matrix, quaternion, or evaluated frame
- `grounded` is persisted model intent; storage-level direct transform edits remain legal
- the rigid-body solver is the first consumer that treats grounded components as fixed solve references
- visibility remains stored state and does not affect the first solver
- suppressed components remain stored state; the first solver rejects selected groups containing them
- assembly constraints use typed ids and semantic target tokens, not raw OCCT topology ids
- Mate and Concentric omit `distance`; Distance requires a positive millimeter length quantity
- constraint state is `active` or `inactive`
- adding or loading constraints does not mutate component transforms
- project structure validation includes constraint component targets

The exact transform convention is canonicalized in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

## Derived assembly data is not persisted

The implemented assembly derivation/solve path is:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyConstraintEquationBuilder
AssemblyRigidBodySolver
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
component-local resolved target descriptors
assembly-space evaluated points/vectors/planar frames
planar Mate/Distance equation-residual descriptors
flattened weighted residual vectors
central finite-difference Jacobians
normal-equation matrices and damping attempts
AssemblySolveResult values
component input snapshots
proposed component transforms
residual summaries and iteration counts
```

None is persisted.

The graph is rebuilt from `component_instances` and active `assembly_constraints`.

Supported target descriptors are rebuilt from semantic tokens, component references, project-owned part intent, and current part parameters.

Assembly-space frames are rebuilt from component-local descriptors plus persisted `RigidTransform` values.

Planar residual descriptors are rebuilt from active constraint intent plus the current evaluated target planes.

For Mate, the current residual convention is:

```text
normal_opposition = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

For planar Distance:

```text
normal_parallelism = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

The signed Distance convention is target-order dependent. Target A/B order is part of persistent relationship intent and must not be reordered by a cache or solver layer.

`AssemblyRigidBodySolver` regenerates its deterministic variable/residual ordering and numeric Jacobian from the selected exact graph group and current project state.

The solver changes transforms only on a private `Project` copy and returns proposed transforms in an unpersisted `AssemblySolveResult`.

`AssemblySolveResultApplier` is the explicit mutation boundary. After a fresh converged result passes snapshot/proposal validation, the proposed transforms are applied through the normal assembly transform update path to a project copy and then committed by replacing the caller's `Project` value.

Therefore no new solved-transform JSON field exists. A successful explicit application changes the already-existing persisted field:

```text
component_instances[].transform
```

A later save serializes those current component transforms exactly as any other explicit transform edit would.

The file format does not record whether a transform originated from a user placement edit or a successful solver-result application.

No graph field, resolved-target field, evaluated-frame field, residual field, Jacobian field, solve-result field, proposal field, damping field, or residual-summary field is added to the current schema.

Canonical residual semantics are documented in `docs/assembly-planar-constraint-equations-mvp5.md`. Solver semantics and the explicit application boundary are documented in `docs/assembly-rigid-body-solver-mvp5.md`.

## Part document target

The future part model remains intent-oriented:

```text
PartDocument BasePlate
  parameters
    plate_width
    plate_height
    plate_thickness

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
- owned-sketch transform behavior is explicit
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

- path segments are semantic references to sketch, construction, projected, future 3D-sketch, or generated curve sources
- path records store ordered connected chains, not raw OCCT wires
- loft section order is explicit
- loft section sketches may be on arbitrary planes
- section alignment/seam metadata should precede complex closed-profile lofts

The detailed target is in `docs/multi-body-transform-and-path-features-roadmap.md`.

## Current implemented scope

- `serialize_part_document_to_json` / `deserialize_part_document_from_json` for in-memory part model intent.
- `write_part_document_json_file` / `read_part_document_json_file` for `.blcad.json` files.
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json` for assembly parameters, members, bindings, component placement/state, and constraint records.
- `serialize_project_to_json` / `deserialize_project_from_json` for embedded assembly plus embedded parts.
- `write_project_json_file` / `read_project_json_file` for `.blcad.project.json` files.
- serialization stores model intent only; it does not serialize OCCT shapes, `ShapeCache`, graph connectivity, resolved targets, evaluated assembly-space frames, residual descriptors, numeric Jacobians, solver results, proposals, iteration state, or DOF state.
- `AssemblyConstraintGraph` rebuilds relationship connectivity.
- `AssemblyConstraintTargetResolver` rebuilds supported component-local generated-face descriptors.
- `AssemblyTransformEvaluator` derives assembly-space points, vectors, and planar frames using the documented transform convention.
- `AssemblyConstraintEquationBuilder` derives active planar Mate/Distance residual descriptors using the documented target-order conventions.
- `AssemblyRigidBodySolver` derives proposed free-component transforms and solve diagnostics from one exact graph-connected group without mutating source project state.
- `AssemblySolveResultApplier` explicitly applies fresh converged proposals to the existing persistent component transform fields.
- `derived_workplanes` with supported semantic faces are persisted as model intent.

See the feature-specific assembly documents and `docs/mvp-plan.md` for exact status.

## Proposed implementation sequence

1. Keep the single-part `.blcad.json` intent format as the stable base.
2. Add per-feature serialization as new feature types land.
3. Add semantic-reference serialization.
4. Add expression and scope serialization.
5. Add `BodyId`, `Body`, and feature-output body serialization.
6. Add `body_transforms` and sketch-ownership transform flags.
7. Add `body_booleans` for add/subtract/intersect between bodies.
8. Add `path_curves` as ordered semantic path chains.
9. Add path-following extrude/cut and loft feature serialization.
10. Extend project files from embedded documents to optional manifest/external-file references.
11. Assembly constraint model-intent records are implemented; keep them independent from derived solve state.
12. Constraint graph connectivity is implemented and unpersisted.
13. Generated-face target descriptors are implemented and unpersisted.
14. Rigid-transform evaluation is implemented and evaluated frames are unpersisted.
15. Planar Mate/Distance residual construction is implemented and residual descriptors are unpersisted.
16. The first rigid-body solver and explicit application boundary are implemented; solver products remain unpersisted and only applied transforms use existing component placement fields.
17. Keep the next DOF/rank diagnostic descriptors regenerable and unpersisted.
18. Add persistent solver/DOF cache records only if later consumers require data that cannot be safely regenerated.
19. Add project-level `resources` for engineering modules.
20. Optionally add an out-of-band `cached_shapes` store separated from model intent.

## Rules

- model intent is the source of truth; caches are regenerable and optional.
- never serialize raw OCCT topology as a model reference; store semantic references instead.
- STEP export runs through normal recompute and is a projection of the model, not the save format.
- component instances and assembly constraints are explicit persistent model intent.
- active constraint graph connectivity is regenerated.
- resolved assembly targets remain derived data.
- evaluated assembly-space geometry remains derived data.
- planar Mate/Distance residual descriptors remain derived data.
- numeric Jacobians and solver iteration products remain derived data.
- `AssemblySolveResult` and proposed transforms remain derived data.
- a fresh converged result changes persistent placement only through explicit application to existing `component_instances[].transform` fields.
- persisted target A/B order must be preserved because signed Distance residual semantics depend on it.
- `rotation_deg` uses the exact convention documented in `docs/assembly-rigid-transform-evaluation-mvp5.md`; changing that convention is a model-format semantic compatibility change even if JSON shape is unchanged.
- future persistent DOF or solver-cache data requires an explicit cache design and must not replace relationship intent.

## Out of scope for the first versions

- binary or compressed container formats.
- embedding cached BRep as the primary persisted data.
- automatic inference of body ownership for old sketches.
- arbitrary matrix transforms as a first user-facing transform operation.
- drawing and BOM documents.
