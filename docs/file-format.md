# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, component instances, Mate/Concentric/Distance/Insert relationship intent, semantic generated face/axis/seat targets, and the derived graph/target/transform/residual/numeric-solve/DOF pipeline.

The save format stores parametric model intent. OCCT shapes, resolved target geometry, residual descriptors, numeric Jacobians, solver products, and DOF diagnostics are regenerable derived data and are not the source of truth.

## Implemented project structure

The current project format embeds one assembly and its owned parts:

```text
Project
  id
  name
  assembly
  parts[]
```

Schema marker:

```text
blcad.project.mvp4
```

The historical marker remains while MVP-5 fields extend the embedded assembly format additively.

## Assembly document records

```text
AssemblyDocument
  parameters[]
  member_parts[]
  parameter_bindings[]
  component_instances[]
  assembly_constraints[]
```

Historical assembly marker:

```text
blcad.assembly_document.mvp4
version 1
```

Older files without `component_instances` or `assembly_constraints` remain loadable and produce empty collections for those optional fields.

## Component instance JSON

Representative shape:

```json
{
  "id": "component.plate.1",
  "name": "Plate 1",
  "referenced_part_document": "part.plate",
  "visibility": "visible",
  "suppression_state": "active",
  "grounding_state": "grounded",
  "transform": {
    "translation_mm": {"x": 0.0, "y": 0.0, "z": 0.0},
    "rotation_deg": {"x": 0.0, "y": 0.0, "z": 0.0}
  }
}
```

Persistence rules:

- component occurrences reference part ids instead of duplicating parts
- transform components must be finite
- `translation_mm` is stored in millimeters
- `rotation_deg` is stored in degrees
- transform evaluation uses active right-handed fixed-axis X-then-Y-then-Z rotation
- no matrix, quaternion, evaluated plane, evaluated axis, or evaluated seating plane is serialized
- grounding, suppression, and visibility are persisted model intent
- storage-level direct transform edits remain legal while grounded

Changing the interpretation of `rotation_deg` would be a model-format semantic compatibility change even if the JSON shape stayed identical.

## Assembly constraint JSON

The same existing record shape stores Mate, Concentric, Distance, and Insert intent.

### Concentric

```json
{
  "id": "constraint.hole_alignment",
  "name": "Hole alignment",
  "type": "concentric",
  "target_a": {
    "component_instance": "component.plate.1",
    "semantic_reference": "feature.hole.axis"
  },
  "target_b": {
    "component_instance": "component.plate.2",
    "semantic_reference": "feature.hole.axis"
  },
  "state": "active"
}
```

### Insert

```json
{
  "id": "constraint.insert",
  "name": "Insert",
  "type": "insert",
  "target_a": {
    "component_instance": "component.plate.1",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "component_instance": "component.plate.2",
    "semantic_reference": "feature.hole.seat"
  },
  "state": "active"
}
```

### Distance

```json
{
  "id": "constraint.spacing",
  "name": "Spacing",
  "type": "distance",
  "target_a": {
    "component_instance": "component.plate.1",
    "semantic_reference": "feature.base_extrude.top"
  },
  "target_b": {
    "component_instance": "component.plate.2",
    "semantic_reference": "feature.base_extrude.top"
  },
  "state": "active",
  "distance": {
    "unit": "mm",
    "value": 40.0
  }
}
```

Constraint persistence rules:

- constraints use typed ids and semantic target strings
- raw OCCT topology ids are not persistent model references
- target A/B order is preserved
- Mate, Concentric, and Insert omit `distance`
- Distance requires a positive length quantity
- constraint state is `active` or `inactive`
- adding/loading a constraint does not mutate component transforms
- project structure validation checks target component identity

The semantic target string is intentionally opaque at the record/JSON layer. Geometry consumers interpret only explicitly supported families.

Current interpreted assembly target families include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first `.axis` and `.seat` producer is a single-circle `SubtractiveExtrude`.

Changing the semantic meaning of an established token family is a model-format compatibility change even if the JSON field shape is unchanged.

## Why Insert needs no new JSON shape

`AssemblyConstraintType::Insert` serializes through the existing `type` string field as:

```text
insert
```

A stable seating target serializes through the existing target string field as:

```text
feature.hole.seat
```

The geometry layer derives the endpoint's primary axis and oriented seating plane from the same exact feature/profile intent.

No fields such as the following are introduced:

```text
axis_target
seat_target
opening_face
cylinder_face
insert_pose
insert_offset
```

The first Insert relationship means zero signed seating separation. A future parameterized nonzero axial Insert offset requires an explicit model-intent design rather than a hidden solver value.

## Derived assembly data is not persisted

Implemented derivation layers include:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyConstraintEquationBuilder
AssemblyConcentricConstraintEquationBuilder
AssemblyInsertConstraintEquationBuilder
shared assembly numeric system
AssemblyRigidBodySolver
AssemblySolveResultApplier
AssemblySolveDiagnosticsAnalyzer
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
component-local planar targets
component-local axis targets
component-local composite Insert axis/seat targets
assembly-space planes
assembly-space axes
assembly-space Insert endpoint geometry
Mate/Distance residual descriptors
Concentric residual descriptors
Insert residual descriptors
flattened scaled numeric residual vectors
central finite-difference Jacobians
normal equations and damping attempts
solver iteration state
AssemblySolveResult values
component input snapshots
unapplied transform proposals
residual summaries
Jacobian rank summaries
constrained and remaining DOF counts
local DOF/consistency/rank classifications
```

None is serialized.

The descriptors are rebuilt from persistent model intent and current placement/state.

Current shared numeric flattening exists for Mate, Distance, and Concentric only. Insert residual construction is derived but not yet part of the shared numeric solver.

The direct Insert residual test proves a regular local `7 x 6` Jacobian rank of `5`; that rank is a regenerable observation, not file state.

## Solver application and persisted transforms

`AssemblyRigidBodySolver` changes transforms only on private `Project` copies and returns an unpersisted `AssemblySolveResult`.

Mate, Distance, and Concentric currently use this shared solver path.

`AssemblySolveResultApplier` is the explicit mutation boundary. A fresh converged result must pass complete source-snapshot and proposal validation before transforms are applied atomically through another project copy.

After successful application, the existing field changes:

```text
component_instances[].transform
```

No separate `solved_transform`, `solver_pose`, constraint-family pose, or solve-cache field exists.

A later save serializes the current transform exactly like a direct placement edit. The format does not record transform provenance as manual versus solver-applied.

Insert has no solver result yet. Persisting an Insert constraint therefore never implies that component transforms have already been solved.

## DOF diagnostics remain derived

Current local rank and remaining-DOF values are not persisted:

```text
rank_evaluated
residual_component_count
variable_count
jacobian_rank
constrained_dof
remaining_dof
residual_row_redundancy
maximum_abs_jacobian_entry
pivot_threshold
DOF classification
consistency classification
residual rank structure
```

These values depend on current geometry, placement, constraints, and numeric policy. Persisting them as authority would create stale cache semantics.

Concentric diagnostics use the shared solver Jacobian and prove regular rank `4/6`.

Insert rank `5/6` is currently proven by a focused direct residual Jacobian. Shared diagnostic integration is the next block.

No `dof`, `rank`, `insert_state`, `concentric_state`, or diagnostics cache field is added.

## Current serialization APIs

Implemented APIs include:

- `serialize_part_document_to_json` / `deserialize_part_document_from_json`
- `write_part_document_json_file` / `read_part_document_json_file`
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json`
- `serialize_project_to_json` / `deserialize_project_from_json`
- `write_project_json_file` / `read_project_json_file`

Serialization stores model intent only. It excludes OCCT shapes, `ShapeCache`, graph connectivity, resolved planes/axes/seats, evaluated assembly-space descriptors, residuals, numeric vectors, Jacobians, solver results, unapplied proposals, rank summaries, and DOF diagnostics.

## Target project container

The future container may evolve toward:

```text
ProjectFile
  metadata
  global_parameters
  documents
    parts
    assemblies
  resources
    materials
    standard_parts
  optional out-of-band caches
```

The current embedded project format remains intentionally simpler. Manifest/external part references are deferred.

## Rules

- model intent is the source of truth
- raw OCCT topology ids are never persistent model references
- semantic token families have compatibility semantics even when stored as strings
- target A/B order is preserved
- resolved target geometry and residuals remain derived
- flattened numeric residual vectors remain derived
- numeric Jacobians and solver iteration products remain derived
- local rank and DOF diagnostics remain derived
- only explicit application of a fresh converged solve result changes persisted placement
- persistent solver/DOF caches require a separate design and must never replace relationship intent

The next assembly implementation block integrates the already-persistent Insert intent and already-derived composite Insert residual into the shared numeric solver/application/DOF pipeline. No new save-file field is required for that integration.
