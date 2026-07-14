# Project and Save File Format

Status: implemented save-format seeds exist for single-part model intent including persistent Solid/Surface Body records, Feature Body-result operations, Body Booleans, BodyTransform stacks, and SketchOwnership records; assembly parameters, embedded Project JSON, part component occurrences, rigid child assembly occurrences, local Mate/Concentric/Distance/Insert/Angle intent, Project-level cross-hierarchy geometric intent, local and occurrence-qualified Revolute joint intent with typed `coordinates[]` plus historical scalar compatibility, semantic generated feature/axis/seat targets, `ref:` reference-geometry targets, canonical `topo:` generated-topology semantic targets, and authored transform/state records.

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
coincident
parallel
perpendicular
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
Mate           -> omit distance and angle
Concentric     -> omit distance and angle
Insert         -> omit distance and angle
Coincident     -> omit distance and angle
Parallel       -> omit distance and angle
Perpendicular  -> omit distance and angle
Distance       -> require distance, omit angle
Angle          -> require angle, omit distance
```

Typed Core constructors remain authoritative after JSON parsing. Block 38 adds only the three accepted lowercase type spellings; local and Project-level relationship record shapes, endpoint shapes, target order, and state fields are unchanged. Historical five-family files remain compatible. Block 39 adds equations and solve participation only and makes no persistence, schema, version, or JSON-shape change.

## Local joint JSON

Local joint intent is separate from geometric constraints and uses `assembly_joints[]`.

Current supported types:

```text
revolute
prismatic
cylindrical
planar
spherical
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
  "coordinates": [
    {
      "role": "rotation",
      "kind": "angular",
      "value": {"unit": "deg", "value": 0.0},
      "lower_limit": {"unit": "deg", "value": -90.0},
      "upper_limit": {"unit": "deg", "value": 90.0}
    }
  ],
  "limits": {
    "lower": {"unit": "deg", "value": -90.0},
    "upper": {"unit": "deg", "value": 90.0}
  },
  "coordinate": {"unit": "deg", "value": 0.0}
}
```

Local joints target distinct local component ids in one containing assembly document.

`coordinates[]` is the canonical Block-42 coordinate representation. Current Revolute writers retain `limits` and scalar `coordinate` additively for older readers.

Prismatic uses the same record envelope with `"type": "prismatic"` and exactly one bounded
coordinate `{ "role": "translation", "kind": "linear", ... }` whose value and limits use
`"unit": "mm"`. Revolute-only scalar `limits` and `coordinate` fields are omitted.

Cylindrical uses `"type": "cylindrical"` and two bounded coordinates in canonical order:
Linear `translation` in `mm`, then Angular `rotation` in `deg`. Revolute-only scalar fields are
also omitted for Cylindrical.

Planar uses `"type": "planar"` and three bounded coordinates in canonical order: Linear
`translation_u`, Linear `translation_v`, then Angular `rotation_normal`.

Spherical uses `"type": "spherical"` and exactly `"coordinates": []`. It has no limits or
authored coordinate, so Revolute-only scalar fields are omitted.

## Project-level cross-hierarchy joint JSON

`cross_hierarchy_joints[]` is additive Project-level motion intent.

`revolute`, `prismatic`, `cylindrical`, `planar`, and `spherical` use this endpoint envelope. Project-level
coordinate signatures are identical to their local family signatures.

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
  "coordinates": [
    {
      "role": "rotation",
      "kind": "angular",
      "value": {"unit": "deg", "value": 0.0},
      "lower_limit": {"unit": "deg", "value": -90.0},
      "upper_limit": {"unit": "deg", "value": 90.0}
    }
  ],
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

- canonical roles are `rotation`, `translation`, `translation_u`, `translation_v`, and `rotation_normal`;
- canonical kinds are `angular` and `linear`, with exact units `deg` and `mm` respectively;
- slot order is family-defined and persisted unchanged;
- Revolute requires exactly one bounded Angular `rotation` slot;
- Prismatic requires exactly one bounded Linear `translation` slot;
- Cylindrical requires bounded Linear `translation` then bounded Angular `rotation` slots;
- Planar requires bounded Linear `translation_u`, bounded Linear `translation_v`, then bounded
  Angular `rotation_normal` slots;
- Spherical requires an empty coordinate slot list;
- readers accept slot-only, historical-only, or exactly matching dual Revolute records;
- partial legacy fields, conflicting dual records, and duplicate/missing/unknown roles fail closed;
- `revolute`, `prismatic`, `cylindrical`, `planar`, and `spherical` are supported at local and
  cross-hierarchy scope;
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

Block 35 does not persist generated-topology role matrices, producer classification, validation results, recovery results, or OCCT topology. Block 36 Geometry resolution of `topo:` identities into typed descriptors/capabilities is derived query state and likewise adds no JSON field. Block 37 relationship target compatibility and Block 40 joint target compatibility are also derived query state and add no JSON field. Block 38 extends accepted relationship `type` values with `coincident`, `parallel`, and `perpendicular` without adding a field or changing schema/version markers. Block 41 replaces scalar in-memory joint coordinate authority with typed family-defined slots. Block 42 adds `coordinates[]` while retaining the existing Revolute `limits` plus scalar `coordinate` writer fields through compatibility adapters. This is additive; schema/version markers remain unchanged.

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

Local numeric solving derives equation-enabled geometric relationship ids and optional transient local Revolute drives. Block 39 enables persisted Coincident/Parallel/Perpendicular records in the same solve/motion graph participation path as the historical five relationship families.

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

## Body persistence

Block 49 adds the top-level `bodies` array to `.blcad.json` while retaining the existing schema and
version:

```json
{
  "bodies": [
    {
      "id": "body.base",
      "name": "Base",
      "kind": "solid",
      "visibility": "visible"
    },
    {
      "id": "body.skin",
      "name": "Skin",
      "kind": "surface",
      "visibility": "hidden"
    }
  ]
}
```

The four fields are required strings. `kind` accepts only `solid` or `surface`; `visibility`
accepts only `visible` or `hidden`. Body IDs must be non-empty and unique within the Part, names
must be non-empty, and serialization orders entries lexicographically by Body ID. The serializer
always emits the array, including when empty.

Historical files without `bodies` load explicitly as zero-body Parts. An absent or empty array
does not synthesize a Body from features or the final shape. Malformed arrays/entries, missing or
non-string fields, unsupported values, and duplicate IDs fail closed.

Block 51 adds optional Body-result fields to existing Feature records:

```json
{
  "operation_mode": "join",
  "target_body": "body.base"
}
```

`operation_mode` accepts `new_body`, `join`, `cut`, or `intersect`. `target_body` and
`produced_body`, when present, are Body ID strings. NewBody requires `produced_body` and forbids
`target_body`. Modifying modes require `target_body`; an absent `produced_body` preserves target
identity. Optional fields are omitted, never written as `null`.

A Body reference without `operation_mode`, malformed/unsupported values, invalid combinations,
missing Body IDs, duplicate producers, and dependency cycles fail closed. Historical Features with
all three fields absent restore the exact null Body context and synthesize nothing. Geometry,
`ShapeCache`, and raw OCCT shapes remain outside the save format.

Block 54 adds the always-emitted, optional-on-read top-level `body_booleans` array:

```json
{
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

Operations are `add`, `subtract`, or `intersect`; result modes are `modify_target` or `new_body`.
`new_body` additionally requires `produced_body`, while `modify_target` forbids it. Tool IDs are
serialized in canonical lexicographic order. Target/tool/result references, Feature ID uniqueness,
producer/cycle rules, and the Boolean structural rules fail closed during load. Historical files
without the array restore zero BodyBooleanFeature records.

Block 56 adds always-emitted, optional-on-read `sketch_ownerships` and `body_transforms` arrays.
Ownership records store `sketch`, `owning_body`, and one of `drives_body`, `consumed_by_body`, or
`reference_only`. Transform records store stable id/body identity, `translate`, `rotate`, or
`uniform_scale`, coordinate space, optional referenced coordinate frame, the selected kind's exact
numeric/axis fields, and both associative-application flags. Transform array order is the persistent
stack order. Rotation axes use `explicit`, `datum_axis`, `construction_line`, or `semantic_edge`
identity. Older files missing either array restore empty collections; no Body, ownership, or
transform is inferred.

Block 58 adds reusable typed Part-feature input values but intentionally no standalone top-level
array. Their source/capability/role data is serialized inside the consuming Feature record beginning
with the later feature-specific Core blocks.

Block 59 begins that embedding with an optional Feature-local `extrude` object. It stores one of
`distance`, `symmetric`, `two_sided`, `through_all`, `to_next`, `to_face`, or `between`; only the
selected mode's distance-parameter or semantic-face fields are present. Optional
`taper_angle_deg` and `thin` fields retain taper and wall-thickness intent. Thin modes are
`one_sided`, `two_sided`, and `mid_plane`, with one or two Length parameter IDs as required.
ToFace/Between faces persist Block-58 `role`, `capability`, `source_kind`, and semantic source
identity. Exact historical additive Distance records keep `length_parameter` and omit `extrude`;
exact historical subtractive ThroughAll records keep `depth: "through_all"` and omit `extrude`.
Missing richer fields therefore continue to restore the historical defaults.

Block 61 adds the always-emitted, optional-on-read top-level `revolve_features` array. Each record
stores `revolve` or `revolve_cut`, a typed sketch/profile-region identity, one typed axis payload,
one strict `full`, `angle`, or `symmetric` extent object, and the mandatory Body operation/result
fields. Partial Angle stores `angle_deg` plus `positive|negative` side; Symmetric stores the total
included `angle_deg`; Full stores neither. Axis source kinds are `datum_axis`,
`construction_line`, `semantic_axis`, and `semantic_linear_edge`, each with only its matching
identity payload. Missing arrays restore zero Revolve records. Malformed mode-specific fields,
unsupported sources, invalid body-operation pairings, and unresolved references fail closed.
Canonical details are in `docs/part-revolve-intent-mvp6.md`.

Block 62 adds no persistent field: mapped profile/axis geometry, OCCT revolution shapes, Boolean
tools/results, validation diagnostics, and `ShapeCache` entries remain derived recompute products.
Canonical execution details are in `docs/part-revolve-geometry-mvp6.md`.

Block 63 adds the always-emitted, optional-on-read `part_patterns` array. Linear and Circular items
store ordered typed Feature/Body sources, concrete direction/axis references, a Count parameter,
mode-specific extent/angle intent, and mandatory Body-result fields. Linear extent references a
Length parameter and preserves spacing/total-extent plus positive/negative direction. Circular
intent preserves a finite total degree angle and mandatory equal spacing. Missing arrays restore
zero general Patterns; generated instances and OCCT copies remain derived. Canonical details are in
`docs/part-pattern-core-mvp6.md`.

Block 64 adds no persistent field. Signed placement distances, translated OCCT instances, the
Pattern tool union, Body Boolean result, derived instance order, and cache diagnostics remain
recompute products. Canonical execution details are in
`docs/part-linear-pattern-geometry-mvp6.md`.

Block 65 also adds no persistent field. Resolved axis frames, angular steps, rotated instances,
Pattern tool unions, Body Boolean results, and cache diagnostics remain derived recompute products.
Canonical execution details are in `docs/part-circular-pattern-geometry-mvp6.md`.

Block 66 adds the always-emitted, optional-on-read `mirror_features` array under the unchanged
schema version. Each item stores `id`, `name`, `type = mirror`, ordered Feature/Body `sources`, a
strict typed `mirror_plane`, and mandatory Body-operation/result fields. Plane source kinds are
`datum_plane`, `construction_plane`, or `semantic_planar_face`, each with only its matching source
identity fields. Missing arrays restore zero Mirrors; unknown top-level, source, or plane fields
fail closed. Canonical details are in `docs/part-mirror-intent-mvp6.md`.

Block 68 adds the always-emitted, optional-on-read `edge_treatments` array. Fillet records persist
ordered semantic edges and one Length radius parameter. Chamfer records persist
`equal_distance`, `two_distance`, or `distance_angle` plus their exact one- or two-parameter
signature. Edge sources are strict `semantic_linear_edge` or `semantic_circular_edge` objects.
The parameter table also accepts `type: "angle"` with unit `deg`; angle expressions remain
unsupported. Unknown fields, duplicate edges, invalid source roles, and mismatched parameter types
fail closed. Canonical details are in `docs/part-edge-treatment-intent-mvp6.md`.

Block 69 adds no persistent field. Constant-radius Fillet results, resolved OCCT edges, and
topology-match diagnostics are derived recompute products. Canonical execution details are in
`docs/part-fillet-geometry-mvp6.md`.

Block 70 also adds no persistent field. Chamfer results, derived asymmetric reference faces, and
OCCT diagnostics remain recompute products of the Block-68 records. Canonical execution details
are in `docs/part-chamfer-geometry-mvp6.md`.

Block 71 adds the always-emitted, optional-on-read `shell_features` array. Each strict record owns
`id`, `name`, `target_body`, ordered semantic planar/cylindrical `removed_faces`, one positive
Length `thickness_parameter`, and `direction` (`inward` or `outward`). Unknown record/face fields,
roles, capabilities, source kinds, and directions fail closed. Canonical details are in
`docs/part-shell-intent-mvp6.md`.

Block 73 adds the always-emitted, optional-on-read `draft_features` array. Each strict record owns
`id`, `name`, `target_body`, ordered semantic planar/cylindrical `faces`, a typed
`pull_direction`, typed `neutral_plane`, and signed Angle `angle_parameter`. Nested references
persist their Draft-specific role, Axis/Line/Plane/Face capability, source kind, and exact source
identity. Missing arrays restore zero Draft features; unknown fields, malformed references,
role/capability mismatches, duplicate faces, wrong parameter units, and zero/out-of-range angles
fail closed. Canonical details are in `docs/part-draft-intent-mvp6.md`.

Block 75 adds `Sketch3D` only to the in-memory Part Core. It intentionally does not change the
current JSON schema: Block 77 owns the additive `sketches_3d` representation after the Block-76
curve entities and cross-source references are frozen. To prevent silent data loss, the current
serializer fails explicitly when a Part owns any Block-75 3D sketch.

## Planned STEP import persistence after Block 94

This section is planned architecture, not part of the current schema. Blocks 95–101 in
`docs/step-import-sequence-mvp7.md` will freeze the exact additive JSON shape.

Planned persistent import intent includes:

```text
StepImportSourceId
canonical project-relative asset path
SHA-256 source-content digest
mode = reference | editable_body
selected imported product/body definitions
persistent BLCAD BodyId outputs for EditableBody
semantic imported-topology catalog and recovery descriptors
```

Planned persistence must not include:

```text
STEP file bytes
absolute machine-local path as model identity
TopoDS shapes
XDE/TDF labels
STEP entity numbers
reader transfer maps
OCCT traversal indices
resolved imported topology handles
derived Plane/Axis/Line/Point/Circle/Cylinder descriptors
```

Reference mode stores immutable imported Part/source intent suitable for ComponentInstance use.
EditableBody mode stores an `ImportedBodyFeature` at the beginning of normal Part feature history;
later BLCAD features remain ordinary persistent model intent. Missing or digest-mismatched assets
fail closed, and an explicit source refresh is required before persisted digest/topology recovery
state may change.
