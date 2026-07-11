# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, part component occurrences, rigid child assembly occurrences, Mate/Concentric/Distance/Insert/Angle geometric intent, persistent Revolute joint/limit/coordinate intent, semantic generated face/axis/seat targets, rigid hierarchy traversal, nested posed export, and the derived solve/motion pipeline.

The save format stores parametric model intent. OCCT shapes, hierarchy traversal state, flattened leaves, resolved target geometry, residual descriptors, transient motion drives, numeric Jacobians, solver products, and diagnostics are regenerable derived data and are not the source of truth.

## Project structure

The current project container has one explicit root assembly, optional project-owned child assemblies, and owned parts:

```text
Project
  id
  name
  assembly       # explicit root AssemblyDocument
  assemblies[]   # project-owned child AssemblyDocument records
  parts[]
```

Historical schema marker:

```text
blcad.project.mvp4
version 1
```

The marker remains while MVP-5 fields extend the container additively.

The root keeps the existing `assembly` field. Child assembly documents use the optional additive `assemblies` collection. Files without `assemblies` remain loadable and produce an empty child assembly collection.

Assembly document ids are unique across the explicit root and child assembly collection.

## Assembly document records

```text
AssemblyDocument
  parameters[]
  member_parts[]
  parameter_bindings[]
  component_instances[]
  subassembly_instances[]
  assembly_constraints[]
  assembly_joints[]
```

Historical assembly marker:

```text
blcad.assembly_document.mvp4
version 1
```

`component_instances`, `subassembly_instances`, `assembly_constraints`, and `assembly_joints` are additive optional collections. Older records without them load as empty collections.

## Component instance JSON

Representative part component occurrence:

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
- grounding, suppression, visibility, and placement are persistent model intent;
- no matrix, quaternion, evaluated target, or posed shape is serialized;
- storage-level direct transform edits remain legal while grounded.

Changing the interpretation of `rotation_deg` would be a format-semantic compatibility change even if the JSON shape stayed identical.

## Rigid subassembly instance JSON

Rigid child assembly occurrences are stored separately in `subassembly_instances[]`.

Representative record:

```json
{
  "id": "subassembly.gearbox.left",
  "name": "Left gearbox",
  "referenced_assembly_document": "assembly.gearbox",
  "visibility": "visible",
  "suppression_state": "active",
  "transform": {
    "translation_mm": {"x": 100.0, "y": 0.0, "z": 0.0},
    "rotation_deg": {"x": 0.0, "y": 0.0, "z": 90.0}
  }
}
```

Persistence rules:

- `id` is a stable `SubassemblyInstanceId`;
- `referenced_assembly_document` identifies a project-owned child `AssemblyDocument`;
- the project root assembly is not a valid child reference;
- direct self-reference is invalid;
- indirect child assembly cycles are invalid;
- occurrence ids are unique within one containing assembly document;
- visibility and suppression are persistent subtree state;
- the occurrence transform is persistent rigid placement intent;
- there is no grounding field and no flexible subassembly solver coordinate in the current seed;
- adding/loading an occurrence never moves child components.

Files without `subassembly_instances` load with no child occurrences.

## Assembly hierarchy format semantics

Project persistence stores documents and references, not a duplicated tree.

Persistent hierarchy authority is:

```text
explicit root assembly id
owned child AssemblyDocument records
SubassemblyInstance -> referenced child assembly id
SubassemblyInstance visibility/suppression/transform
```

The following are not serialized:

```text
parent assembly links
DFS active/completed sets
hierarchy traversal order
occurrence paths
parent transform chains
composed transforms
flattened leaf descriptors
posed leaf geometry
```

`Project::validate_assembly_hierarchy` regenerates the assembly document reference graph and rejects direct or indirect cycles.

`AssemblyHierarchyTraversal` regenerates the rooted occurrence tree in deterministic root-first pre-order. Child occurrences are ordered lexicographically by `SubassemblyInstanceId`. Repeated occurrences of one child assembly document remain distinct traversal occurrences.

`AssemblyLeafOccurrenceResolver` regenerates visible-active part leaves. Its identity includes containing assembly id, subassembly occurrence path, and component id. No derived leaf identity is written back into JSON.

## Hierarchical transform semantics

Every persisted `RigidTransform` retains the same semantics at every hierarchy level:

```text
translation_mm: millimeters
rotation_deg: degrees
rotation: active right-handed fixed-axis
order: X -> Y -> Z -> translation
```

For component transform `Tc`, inner child occurrence transform `Ti`, and outer occurrence transform `To`, evaluation order is:

```text
To(Ti(Tc(p)))
```

The derived transform chain is:

```text
[Tc, Ti, To]
```

This chain is not serialized. BLCAD does not collapse arbitrary hierarchy rotations and persist a recomputed Euler triple.

## Geometric assembly constraint JSON

`assembly_constraints[]` stores geometric relationship intent.

Current type spellings:

```text
mate
concentric
distance
insert
angle
```

Every record stores id, name, type, target A, target B, and `active|inactive` state.

Target shape:

```json
{
  "component_instance": "component.plate.1",
  "semantic_reference": "feature.hole.seat"
}
```

Distance adds:

```json
"distance": {
  "unit": "mm",
  "value": 40.0
}
```

Distance must be a positive length quantity.

Angle adds:

```json
"angle": {
  "unit": "deg",
  "value": 35.0
}
```

Angle must be a finite degree quantity.

Constraint rules:

- semantic target strings, not OCCT topology ids, are persistent;
- target A/B order is preserved;
- Mate, Concentric, and Insert omit `distance` and `angle`;
- Distance requires `distance` and excludes `angle`;
- Angle requires `angle` and excludes `distance`;
- adding/loading a constraint does not mutate transforms;
- constraints currently target local component instances in the containing `AssemblyDocument` only.

Cross-hierarchy geometric constraint targets are not part of the current format semantics.

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

All limit and coordinate quantities use `deg`.

The coordinate is persistent explicit authored motion state. Loading or adding a joint does not move component transforms. The current motion API rejects requested coordinates outside persistent limits instead of clamping them.

Joints currently target local component instances in the containing `AssemblyDocument` only. Cross-hierarchy joint targets are deferred.

Files without `assembly_joints` load with an empty joint collection.

## Semantic target strings

The record/JSON layer treats semantic target strings as opaque identity tokens. Geometry consumers interpret only supported families:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

The first `.axis` and `.seat` producer is a single-circle `SubtractiveExtrude`.

`.seat` resolves to one primary axis plus one oriented seating plane from the same exact source feature/profile. Insert and Revolute reuse that endpoint.

Fields such as these are not persistent:

```text
occt_face_id
cylinder_face
axis_target
seat_target
joint_axis_shape
resolved_axis
resolved_seating_plane
```

Changing the semantic meaning of an established token family is a format compatibility change even if its JSON field shape remains unchanged.

## Numeric relationships remain derived

The current private numeric relationship set contains:

```text
constraint_ids[]
revolute_drives[]
```

It is collected for one solve/motion query and is not serialized.

Current geometric flattening:

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

The selected joint receives the requested coordinate. Other active Revolute joints in the same active combined relationship group receive their persisted coordinates. These drive values are query-time derived data.

## Derived assembly data is not persisted

Implemented derivation layers include:

```text
AssemblyConstraintGraph
AssemblyJointGraph
combined motion relationship closure
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyHierarchyTransformEvaluator
geometric constraint equation builders
AssemblyRevoluteJointEquationBuilder
shared numeric relationship system
shared numeric solve engine
AssemblyRigidBodySolver
AssemblyJointMotionSolver
AssemblySolveDiagnosticsAnalyzer
AssemblyStepExporter
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
joint graph nodes/edges/groups
combined motion groups
assembly document reference graph
cycle validation state
hierarchy occurrence descriptors
parent links
occurrence paths
parent transform chains
flattened leaf occurrence descriptors
local and assembly-space target geometry
geometric and Revolute residual descriptors
transient Revolute drive sets
scaled numeric residual vectors
finite-difference Jacobians
normal equations and damping attempts
solver iteration state
solve and motion results
component and driven-joint snapshots
unapplied transform proposals
rank and remaining-DOF summaries
per-consumer ShapeCache values
posed leaf shape copies
OCCT compounds
```

None is serialized.

## Explicit application and persisted state changes

`AssemblyRigidBodySolver` and `AssemblyJointMotionSolver` change only private project copies while solving.

A fresh converged `AssemblySolveResult` may explicitly change:

```text
component_instances[].transform
```

A fresh converged `AssemblyJointMotionResult` may additionally change:

```text
assembly_joints[selected].coordinate
```

Both application paths are atomic through project copies and validate stale input snapshots before commit.

Rigid subassembly occurrence transforms and state are changed only through explicit `AssemblyDocument` occurrence edit APIs. Hierarchy traversal or nested STEP export never rewrites them.

A later save serializes current authored transforms and coordinates. The file does not record solver iteration history, transform provenance, or motion-result provenance.

## DOF diagnostics remain derived

Current local rank and remaining-DOF values are not persistent.

Proven regular local results include:

```text
Concentric geometric relationship: rank 4/6
Insert geometric relationship:     rank 5/6
Angle non-extremal seed:            rank 1/6
Driven Revolute query:              rank 6/6 on a 9 x 6 Jacobian
```

The driven Revolute rank is a query-time observation with an explicit target coordinate. It is not persistent authority about the joint's free motion DOF.

## Posed export data remains derived

`AssemblyStepExporter` consumes `AssemblyLeafOccurrenceResolver`.

The export pipeline regenerates:

```text
visible-active flattened leaves
unique referenced leaf part ids
one ShapeCache per referenced part
independent part shape copies per leaf
full inner-to-outer transform application
one OCCT compound
STEP exchange entities
```

Repeated child assembly occurrences may produce multiple leaf descriptors referencing the same part. They share the recomputed part cache but receive independent transformed shape copies.

Hidden/suppressed subassembly paths and hidden/suppressed local components do not appear in the flattened leaf list.

No composed matrix, posed BRep, compound, STEP entity id, or export cache is saved into project JSON.

## Current serialization APIs

Implemented APIs include:

- `serialize_part_document_to_json` / `deserialize_part_document_from_json`;
- `write_part_document_json_file` / `read_part_document_json_file`;
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json`;
- `serialize_project_to_json` / `deserialize_project_from_json`;
- `write_project_json_file` / `read_project_json_file`.

## Future container direction

A later externalized container may evolve toward:

```text
ProjectFile
  metadata
  global_parameters
  documents
    root_assembly
    assemblies
    parts
  resources
    materials
    standard_parts
  optional out-of-band caches
```

The current format remains an embedded JSON project container. Manifest/external part/assembly references are deferred.

## Rules

- model intent is the source of truth;
- one explicit root assembly remains identifiable;
- child assemblies are project-owned documents referenced by stable rigid occurrence records;
- raw OCCT topology ids are never persistent model references;
- target A/B order is preserved;
- transform interpretation is stable format semantics;
- hierarchy cycles are invalid project structure;
- parent links, traversal order, occurrence paths, and flattened leaves remain derived;
- composed hierarchy transforms remain derived;
- resolved target geometry and residuals remain derived;
- numeric Jacobians and solver iteration products remain derived;
- solve/motion results and snapshots remain derived;
- local rank and DOF diagnostics remain derived;
- per-consumer shape caches and posed geometry remain derived;
- only explicit application/edit boundaries change persistent transforms, joint coordinate, or rigid occurrence state.

The next assembly block adds read-only posed interference analysis over deterministic flattened leaf occurrence pairs. Interference candidates, posed leaf shapes, OCCT common geometry, volume properties, and interference records remain derived and must not become save-file authority.
