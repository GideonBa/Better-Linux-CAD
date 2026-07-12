# Project and Save File Format

Status: implemented save-format seeds exist for single-part model intent, assembly parameters, embedded Project JSON, part component occurrences, rigid child assembly occurrences, local Mate/Concentric/Distance/Insert/Angle intent, Project-level cross-hierarchy geometric intent, local Revolute joint intent, Project-level occurrence-qualified Revolute joint intent, semantic generated feature/axis/seat targets, `ref:` reference-geometry targets, canonical `topo:` generated-topology semantic targets, and authored transform/state records.

The save format stores parametric and semantic model intent. OCCT shapes, hierarchy traversal state, occurrence graphs, transform authorities, generated-topology producer classification/recovery results, resolved geometry, residuals, Jacobians, solve/motion results, freshness snapshots, proposals, diagnostics, and exchange products are derived.

## Project structure

```text
Project
  id
  name
  assembly                         # explicit root AssemblyDocument
  assemblies[]                     # Project-owned child AssemblyDocument records
  cross_hierarchy_constraints[]    # Project-level occurrence-qualified geometry intent
  cross_hierarchy_joints[]         # Project-level occurrence-qualified motion intent
  parts[]
```

Historical schema marker:

```text
blcad.project.mvp4
version 1
```

The marker remains while MVP-5 collections extend the Project additively.

Backward compatibility:

```text
missing assemblies
  -> empty child assembly collection

missing cross_hierarchy_constraints
  -> empty Project cross-hierarchy geometric collection

missing cross_hierarchy_joints
  -> empty Project cross-hierarchy joint collection
```

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

`component_instances`, `subassembly_instances`, `assembly_constraints`, and `assembly_joints` are additive optional collections. Older records without them load empty collections.

Project-level `cross_hierarchy_constraints` and `cross_hierarchy_joints` do not belong inside one `AssemblyDocument`; their endpoints may address different rooted assembly occurrences.

## Component instance JSON

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

Rules:

- occurrences reference part ids instead of duplicating parts;
- transform components must be finite;
- translations use millimeters;
- rotations use degrees;
- transform evaluation is active right-handed fixed-axis X then Y then Z;
- grounding, suppression, visibility, referenced part, and direct placement are persistent model intent;
- no matrix, quaternion, target geometry, or posed shape is serialized;
- storage-level direct transform edits remain legal while grounded.

## Rigid subassembly instance JSON

A child assembly occurrence is stored in the containing assembly document's `subassembly_instances[]`:

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

Rules:

- `id` is a stable local `SubassemblyInstanceId`;
- the referenced assembly must be a Project-owned child document;
- the explicit Project root is not a valid child reference;
- direct and indirect hierarchy cycles are invalid;
- occurrence ids are unique inside the containing assembly document;
- visibility and suppression are persistent subtree state;
- the transform is persistent rigid boundary placement intent;
- there is no grounding field and no whole-subassembly solve variable in the current seed;
- adding/loading an occurrence never moves child components.

## Hierarchy persistence and transform semantics

Project persistence stores documents and references, not a duplicated rooted tree.

Persistent hierarchy authority:

```text
explicit root AssemblyDocument
owned child AssemblyDocument records
SubassemblyInstance -> referenced child assembly id
SubassemblyInstance visibility / suppression / direct RigidTransform
```

Derived and unpersisted:

```text
parent links
DFS state
rooted occurrence descriptors
parent transform chains
composed transforms
flattened leaf descriptors
posed geometry
motion/solve connectivity
```

An occurrence path inside a Project-level relationship endpoint is different: it is persistent endpoint identity.

Example:

```text
[subassembly.outer, subassembly.inner]
```

Every persisted `RigidTransform` uses:

```text
translation_mm: millimeters
rotation_deg: degrees
rotation: active right-handed fixed-axis
application order: X -> Y -> Z -> translation
```

For component transform `Tc`, inner boundary `Ti`, and outer boundary `To`:

```text
To(Ti(Tc(p)))
```

The derived inner-to-outer chain is:

```text
[Tc, Ti, To]
```

No composed matrix or recomputed Euler triple is persisted.

## Local geometric assembly constraint JSON

`assembly_constraints[]` stores local relationship intent.

Type spellings:

```text
mate
concentric
distance
insert
angle
```

Every record stores id, name, type, target A, target B, and `active|inactive` state.

Local target:

```json
{
  "component_instance": "component.plate.1",
  "semantic_reference": "feature.hole.seat"
}
```

Targets are local to the containing `AssemblyDocument`. Target A/B order is persistent.

## Occurrence-qualified endpoint JSON

Project-level cross-hierarchy geometric constraints and joints share one Core endpoint shape:

```json
{
  "occurrence_path": ["subassembly.outer", "subassembly.inner"],
  "component_instance": "component.shaft",
  "semantic_reference": "feature.bore.axis"
}
```

The explicit root occurrence uses an empty path:

```json
{
  "occurrence_path": [],
  "component_instance": "component.root_bore",
  "semantic_reference": "feature.bore.axis"
}
```

Rules:

- `occurrence_path` is an ordered root-to-current `SubassemblyInstanceId` sequence;
- element order is identity and roundtrips exactly;
- `[]` addresses the explicit root occurrence;
- `component_instance` is local to the assembly document reached by the exact path;
- semantic target text is stored as opaque identity;
- loading follows paths exactly after complete hierarchy validation;
- the reached document must contain the addressed local component;
- semantic target geometry is not resolved during Core loading/validation.

## Project-level cross-hierarchy geometric constraint JSON

`cross_hierarchy_constraints[]` is additive Project-level geometric relationship intent.

Representative record:

```json
{
  "id": "constraint.cross.main",
  "name": "Root to nested shaft",
  "type": "concentric",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root_bore",
    "semantic_reference": "feature.bore.axis"
  },
  "target_b": {
    "occurrence_path": ["subassembly.gearbox"],
    "component_instance": "component.shaft",
    "semantic_reference": "feature.bore.axis"
  },
  "state": "active"
}
```

Rules:

- ids are unique inside the Project-level cross-hierarchy geometric collection;
- local document-scoped constraint ids may use the same text;
- target A/B order is preserved exactly;
- adding/loading geometric intent never mutates component/subassembly transforms.

Distance uses:

```json
"distance": {"unit": "mm", "value": 40.0}
```

Distance is a positive finite length.

Angle uses:

```json
"angle": {"unit": "deg", "value": 35.0}
```

Angle is a finite degree quantity.

Value-family rules:

```text
Mate        -> omit distance and angle
Concentric  -> omit distance and angle
Insert      -> omit distance and angle
Distance    -> require distance, omit angle
Angle       -> require angle, omit distance
```

Typed Core constructors remain authoritative after JSON parsing.

## Local Revolute joint JSON

Local joint intent is separate from geometric constraints and uses `assembly_joints[]`.

Current supported type:

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

Local joints target distinct local component ids in one containing assembly document.

## Project-level cross-hierarchy Revolute joint JSON

`cross_hierarchy_joints[]` is additive Project-level motion intent.

Representative record:

```json
{
  "id": "joint.cross.main",
  "name": "Root to nested shaft",
  "type": "revolute",
  "target_a": {
    "occurrence_path": [],
    "component_instance": "component.root",
    "semantic_reference": "feature.hole.seat"
  },
  "target_b": {
    "occurrence_path": ["subassembly.outer", "subassembly.inner"],
    "component_instance": "component.shaft",
    "semantic_reference": "feature.bore.seat"
  },
  "state": "active",
  "limits": {
    "lower": {"unit": "deg", "value": -90.0},
    "upper": {"unit": "deg", "value": 90.0}
  },
  "coordinate": {"unit": "deg", "value": 0.0}
}
```

Persistent rules:

```text
-180 deg <= lower < upper <= 180 deg
lower <= coordinate <= upper
```

Additional rules:

- all limit/coordinate values use `deg`;
- only `revolute` is supported in the current cross-hierarchy joint seed;
- state is `active|inactive`;
- ids are unique inside the Project-level cross-hierarchy joint collection;
- local document-scoped `AssemblyJointId` values may use the same text;
- target A/B order and exact occurrence paths roundtrip unchanged;
- exactly equal endpoint identities are invalid;
- different rooted endpoint identities may address one shared local component/transform authority;
- loading/adding/updating authored coordinate intent never moves component or subassembly transforms;
- motion requests outside limits are rejected rather than clamped.

Only explicit successful fresh motion application may update the selected authored Project-level joint coordinate together with direct component transforms.

## Semantic target strings

The record/JSON layer treats semantic target strings as opaque identity. Core loading preserves them byte-for-byte and does not resolve Geometry.

### Legacy generated feature roles

Geometry consumers currently interpret:

```text
<feature-id>.top|bottom|right|left|front|back
<feature-id>.axis
<feature-id>.seat
```

The first `.axis`/`.seat` producer is a single-circle `SubtractiveExtrude`. `.seat` resolves one primary axis plus one oriented seating plane from the same feature/profile.

### Reference geometry targets

Block 32 adds:

```text
ref:datum_plane:<encoded-id>
ref:datum_axis:<encoded-id>
ref:construction_line:<encoded-id>
ref:construction_point:<encoded-id>
```

Every id byte outside `[A-Za-z0-9_-]` is escaped as uppercase `%HH`. Valid `ref:` tokens contain no `.` and stay disjoint from legacy feature-role tokens.

Endpoints persist these strings verbatim. Block-34 Plane/Axis/Line/Point resolution is derived query state and adds no JSON field.

### Generated topology semantic targets

Block 35 adds canonical producer-driven generated-topology identity strings:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<linear-edge-role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:source_rim
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:opposite_rim
topo:vertex:<encoded-feature-id>:<vertex-role>
```

The same canonical uppercase `%HH` rule applies to feature/profile ids. Valid `topo:` strings contain no `.`. The distinct `topo:` prefix is disjoint from `ref:` and from legacy feature-role strings.

Example:

```text
FeatureId = feature.hole/a%b:c
ProfileId = profile.hole/50%

-> topo:cylindrical_face:feature%2Ehole%2Fa%25b%3Ac:profile%2Ehole%2F50%25:wall
```

Block 35 producer identity is semantic model intent:

```text
source FeatureId
+ supported producer family
+ named semantic role
+ exact source ProfileId where profile-derived
```

The current endpoint serializers need no new JSON field because `semantic_reference` already stores opaque identity strings. `topo:` spellings roundtrip byte-for-byte through local and occurrence-qualified endpoints.

Block 35 does not persist generated-topology role matrices, producer classification, validation results, recovery results, or OCCT topology. Block 36 Geometry resolution of `topo:` identities into typed descriptors/capabilities is derived query state and likewise adds no JSON field. Block 37 target compatibility selection is also derived query state and adds no JSON field.

Fields such as these are not persistent:

```text
occt_face_id
occt_edge_id
occt_vertex_id
topology_traversal_index
topology_hash
brep_map_position
xde_label_tag
step_entity_number
generated_topology_producer_matrix
generated_topology_classification
generated_topology_recovery_result
cylinder_face
axis_target
seat_target
joint_axis_shape
resolved_axis
resolved_seating_plane
```

Changing the semantic meaning of an established target token family is a format compatibility change even when JSON shape is unchanged.

## Part document model intent

Part-document persistence remains canonical in the existing single-part format sections/documents. Parameters, formulas, datum/workplane intent, construction geometry, sketches/profiles, semantic-reference recovery/remap intent, and feature history are model authority.

Block 33 adds additive optional `datum_axes` records. Missing `datum_axes` remains historical-file compatible and loads an empty collection.

Block 35 adds no PartDocument JSON field. Its generated-topology classification and recovery derive from current feature/sketch/profile intent.

## Numeric solve and motion relationships remain derived

Local numeric solving derives geometric relationship ids and optional transient local Revolute drives.

Cross-hierarchy geometric solving derives:

```text
ComponentTransformAuthority
relationship-to-authority incidence
geometric solve groups
mixed scaled residual vectors
finite-difference Jacobians
```

Cross-hierarchy motion derives four relationship classes in canonical kind order:

```text
local geometry
local joint
Project cross geometry
Project cross joint
```

It also derives combined motion groups, cross-joint endpoint-to-authority mappings, transient holding drives, root-space Revolute target frames, motion residual vectors/Jacobians, and motion proposals.

Canonical Revolute scalar residual order remains:

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

These nine scalar components are never serialized.

## Solver, freshness, and diagnostics products remain derived

Do not serialize:

```text
AssemblyConstraintGraph
AssemblyJointGraph
AssemblyCrossHierarchyConstraintGraph
AssemblyCrossHierarchyMotionGraph
ComponentTransformAuthority
relationship/joint incidence
endpoint-authority mappings
solve/motion groups
holding drives
resolved targets
resolved generated topology handles
residual descriptors
scaled residual vectors
finite-difference Jacobians
matrix ranks / DOF classifications
solve/motion states
component/authority snapshots
relationship/joint snapshots
hierarchy-boundary snapshots
AssemblySemanticTargetPartSnapshot
transform proposals
```

Semantic target-producing model freshness stores the exact canonical `serialize_part_document_to_json(part)` payload only inside derived solve/motion results. It is not an additional persistent revision field.
