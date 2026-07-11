# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, component instances, Mate/Concentric/Distance/Insert/Angle geometric relationship intent, persistent Revolute joint/limit/coordinate intent, semantic generated face/axis/seat targets, and the derived graph/solve/motion/export pipeline.

The save format stores parametric model intent. OCCT shapes, resolved target geometry, residual descriptors, transient motion drives, numeric Jacobians, solver products, motion results, and DOF diagnostics are regenerable derived data and are not the source of truth.

## Implemented project structure

The current project format embeds one root assembly and its owned parts:

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
version 1
```

The historical marker remains while MVP-5 fields extend the embedded assembly format additively.

Structured ownership of child assembly documents is the next project-container extension and is not part of the current format yet.

## Assembly document records

```text
AssemblyDocument
  parameters[]
  member_parts[]
  parameter_bindings[]
  component_instances[]
  assembly_constraints[]
  assembly_joints[]
```

Historical assembly marker:

```text
blcad.assembly_document.mvp4
version 1
```

The current loader treats `component_instances`, `assembly_constraints`, and `assembly_joints` as additive optional collections. Older files without them remain loadable and produce empty collections.

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

- component occurrences reference part ids instead of duplicating parts;
- transform components must be finite;
- `translation_mm` is stored in millimeters;
- `rotation_deg` is stored in degrees;
- transform evaluation uses active right-handed fixed-axis X-then-Y-then-Z rotation;
- no matrix, quaternion, evaluated plane, evaluated axis, or evaluated seating plane is serialized;
- grounding, suppression, and visibility are persisted model intent;
- storage-level direct transform edits remain legal while grounded.

Changing the interpretation of `rotation_deg` would be a model-format semantic compatibility change even if the JSON shape stayed identical.

## Geometric assembly constraint JSON

The `assembly_constraints[]` collection stores geometric relationship intent.

Current type spellings are:

```text
mate
concentric
distance
insert
angle
```

Every record stores:

```text
id
name
type
target_a
target_b
state = active | inactive
```

Target shape:

```json
{
  "component_instance": "component.plate.1",
  "semantic_reference": "feature.hole.seat"
}
```

### Concentric example

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

### Insert example

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

### Distance value

Distance alone adds:

```json
"distance": {
  "unit": "mm",
  "value": 40.0
}
```

The value must be a positive length quantity.

### Angle value

Angle alone adds:

```json
"angle": {
  "unit": "deg",
  "value": 35.0
}
```

The value must be a finite degree quantity.

Constraint persistence rules:

- constraints use typed ids and semantic target strings;
- raw OCCT topology ids are not persistent model references;
- target A/B order is preserved;
- Mate, Concentric, and Insert omit both `distance` and `angle`;
- Distance requires `distance` and excludes `angle`;
- Angle requires `angle` and excludes `distance`;
- state is `active` or `inactive`;
- adding/loading a constraint does not mutate component transforms;
- project structure validation checks target component identity.

## Revolute joint JSON

Joint intent is separate from geometric constraint intent and uses `assembly_joints[]`.

The first supported type spelling is:

```text
revolute
```

Representative record:

```json
{
  "id": "joint.revolute",
  "name": "Plate Revolute Joint",
  "type": "revolute",
  "target_a": {
    "component_instance": "component.plate.grounded",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "component_instance": "component.plate.free",
    "semantic_reference": "feature.hole.seat"
  },
  "state": "active",
  "limits": {
    "lower": {"unit": "deg", "value": -90.0},
    "upper": {"unit": "deg", "value": 90.0}
  },
  "coordinate": {"unit": "deg", "value": 0.0}
}
```

Persistent Revolute rules:

```text
-180 deg <= lower < upper <= 180 deg
lower <= coordinate <= upper
```

All limit and coordinate quantities use unit `deg`.

The coordinate is persistent because it is explicit authored motion state. A joint record does not imply that component transforms were moved merely by loading or adding the record. Component transforms change only after explicit application of a fresh converged motion result.

The current motion API rejects requested coordinates outside the persistent limits. It does not persist a clamped value and does not silently change the request.

Files without `assembly_joints` load with an empty joint collection.

## Semantic target strings

The semantic target string is intentionally opaque at the record/JSON layer. Geometry consumers interpret only explicitly supported families.

Current interpreted assembly target families include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first `.axis` and `.seat` producer is a single-circle `SubtractiveExtrude`.

`.seat` resolves to one primary axis plus one oriented seating plane from the same exact source feature/profile. Insert and Revolute reuse that derived endpoint.

No fields such as the following are persistent:

```text
occt_face_id
cylinder_face
axis_target
seat_target
opening_face
joint_axis_shape
resolved_axis
resolved_seating_plane
```

Changing the semantic meaning of an established token family is a model-format compatibility change even if the JSON field shape remains unchanged.

## Shared numeric relationships remain derived

The current private numeric relationship set contains:

```text
constraint_ids[]
revolute_drives[]
```

The ids and drive targets are collected for one solve/motion query. The set itself is not serialized.

Current geometric constraint flattening is:

```text
Mate:
  normal_opposition.x
  normal_opposition.y
  normal_opposition.z
  signed_separation_mm / length_residual_scale_mm

Distance:
  normal_parallelism.x
  normal_parallelism.y
  normal_parallelism.z
  distance_residual_mm / length_residual_scale_mm

Concentric:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / length_residual_scale_mm
  axis_offset_mm.y / length_residual_scale_mm
  axis_offset_mm.z / length_residual_scale_mm

Insert:
  direction_parallelism.x
  direction_parallelism.y
  direction_parallelism.z
  axis_offset_mm.x / length_residual_scale_mm
  axis_offset_mm.y / length_residual_scale_mm
  axis_offset_mm.z / length_residual_scale_mm
  signed_seating_separation_mm / length_residual_scale_mm

Angle:
  angle_alignment
```

One transient Revolute drive flattens as:

```text
direction_alignment.x
direction_alignment.y
direction_alignment.z
axis_offset_mm.x / length_residual_scale_mm
axis_offset_mm.y / length_residual_scale_mm
axis_offset_mm.z / length_residual_scale_mm
signed_seating_separation_mm / length_residual_scale_mm
twist_alignment_sine
twist_alignment_cosine
```

The selected motion joint receives the requested coordinate. Other active Revolute joints in the same active combined relationship group receive their currently persisted coordinates. These transient drive values are not stored as a numeric cache.

## Derived assembly data is not persisted

Implemented derivation layers include:

```text
AssemblyConstraintGraph
AssemblyJointGraph
combined motion relationship closure
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
geometric constraint equation builders
AssemblyRevoluteJointEquationBuilder
shared assembly numeric relationship system
shared numeric solve engine
AssemblyRigidBodySolver
AssemblySolveResultApplier
AssemblyJointMotionSolver
AssemblyJointMotionResultApplier
AssemblySolveDiagnosticsAnalyzer
AssemblyStepExporter
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
joint graph nodes/edges/groups
combined motion component groups
component-local planar targets
component-local axis targets
component-local composite axis/seat targets
assembly-space planes/axes/seating frames
Mate/Distance/Concentric/Insert/Angle residual descriptors
Revolute drive residual descriptors
transient Revolute drive sets
flattened scaled numeric residual vectors
central finite-difference Jacobians
normal equations and damping attempts
solver iteration state
AssemblySolveResult values
AssemblyJointMotionResult values
component input snapshots
driven-joint input snapshots
unapplied transform proposals
residual summaries
Jacobian rank summaries
constrained and remaining DOF counts
per-export ShapeCache values
posed component shape copies
OCCT assembly compounds
```

None is serialized.

## Explicit application and persisted state changes

`AssemblyRigidBodySolver` and `AssemblyJointMotionSolver` change only private `Project` copies while solving.

`AssemblySolveResultApplier` is the ordinary geometric mutation boundary. A fresh converged result must pass component source-snapshot and proposal validation before transforms are applied atomically through another project copy.

After successful geometric application, existing fields may change:

```text
component_instances[].transform
```

`AssemblyJointMotionResultApplier` additionally validates every driven-joint input snapshot. On one project copy it then applies the embedded component transform proposals and changes:

```text
assembly_joints[selected].coordinate
```

Only after both operations succeed is the project copy committed.

A later save serializes current transforms and the selected authored coordinate. The format does not persist solve iteration history, transform provenance, or motion-result provenance.

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

Proven regular local results include:

```text
Concentric geometric relationship: rank 4/6
Insert geometric relationship:     rank 5/6
Angle non-extremal seed:            rank 1/6
Driven Revolute query:              rank 6/6 on a 9 x 6 Jacobian
```

The driven Revolute rank is a query-time observation with an explicit target coordinate. It is not a persistent claim that the underlying joint has zero motion DOF.

## Current serialization APIs

Implemented APIs include:

- `serialize_part_document_to_json` / `deserialize_part_document_from_json`;
- `write_part_document_json_file` / `read_part_document_json_file`;
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json`;
- `serialize_project_to_json` / `deserialize_project_from_json`;
- `write_project_json_file` / `read_project_json_file`.

Serialization stores model intent only.

## Current compatibility policy

The current MVP uses additive fields under historical schema markers where the loader can define an unambiguous empty/default meaning.

Examples:

```text
missing component_instances -> empty component collection
missing assembly_constraints -> empty geometric constraint collection
missing assembly_joints -> empty joint collection
```

A semantic reinterpretation of existing transform conventions, target token families, quantity units, or relationship type spellings is not an additive change and requires an explicit compatibility design.

## Target project container

The next project-container extension introduces project-owned child assembly documents plus rigid subassembly occurrences from the root or another child assembly. The hierarchy must be cycle-free.

The future broader container may evolve toward:

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

- model intent is the source of truth;
- raw OCCT topology ids are never persistent model references;
- semantic token families have compatibility semantics even when stored as strings;
- target A/B order is preserved;
- geometric constraint intent and joint/motion intent remain separate record families;
- joint limits and authored coordinates are persistent only as explicit user intent;
- resolved target geometry and residuals remain derived;
- transient motion drive sets remain derived;
- flattened numeric residual vectors remain derived;
- numeric Jacobians and solver iteration products remain derived;
- solve/motion results and their input snapshots remain derived;
- local rank and DOF diagnostics remain derived;
- only explicit application of a fresh converged result changes persisted placement;
- successful motion application may additionally change the selected authored joint coordinate;
- persistent solver/DOF caches require a separate design and must never replace relationship intent.
