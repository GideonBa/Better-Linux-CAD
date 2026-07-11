# Project and Save File Format

Status: implemented save-format seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, part component occurrences, rigid child assembly occurrences, local Mate/Concentric/Distance/Insert/Angle intent, project-level cross-hierarchy Mate/Concentric/Distance/Insert/Angle intent, persistent Revolute joint/limit/coordinate intent, semantic generated face/axis/seat targets, and current authored transform/state records.

The save format stores parametric model intent. OCCT shapes, hierarchy traversal state, flattened leaves, resolved target geometry, residual descriptors, numeric Jacobians, solver products, and diagnostics are derived and are not the source of truth.

## Project structure

The current project container has one explicit root assembly, optional project-owned child assemblies, project-owned cross-hierarchy geometric constraints, and owned parts:

```text
Project
  id
  name
  assembly                         # explicit root AssemblyDocument
  assemblies[]                     # project-owned child AssemblyDocument records
  cross_hierarchy_constraints[]    # project-level geometric relationship intent
  parts[]
```

Historical schema marker:

```text
blcad.project.mvp4
version 1
```

The marker remains while MVP-5 fields extend the container additively.

The root keeps the `assembly` field. Child assembly documents use optional additive `assemblies`. Project-level cross-hierarchy geometric relationships use optional additive `cross_hierarchy_constraints`.

Backward compatibility:

```text
missing assemblies
  -> empty child assembly collection

missing cross_hierarchy_constraints
  -> empty project cross-hierarchy constraint collection
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

`component_instances`, `subassembly_instances`, `assembly_constraints`, and `assembly_joints` are additive optional collections. Older records without them load as empty collections.

Project-level `cross_hierarchy_constraints` do not belong inside one `AssemblyDocument`; endpoints may address different rooted assembly occurrences.

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
- `translation_mm` is millimeters;
- `rotation_deg` is degrees;
- transform evaluation uses active right-handed fixed-axis X-then-Y-then-Z rotation;
- grounding, suppression, visibility, and placement are persistent model intent;
- no matrix, quaternion, evaluated target, or posed shape is serialized;
- storage-level direct transform edits remain legal while grounded.

Changing the interpretation of `rotation_deg` is a format-semantic compatibility change even if the JSON shape remains identical.

## Rigid subassembly instance JSON

Rigid child assembly occurrences are stored in `subassembly_instances[]` of the containing assembly document.

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

- `id` is a stable local `SubassemblyInstanceId`;
- `referenced_assembly_document` identifies a project-owned child `AssemblyDocument`;
- the project root assembly is not a valid child reference;
- direct self-reference is invalid;
- indirect assembly cycles are invalid;
- occurrence ids are unique within one containing assembly document;
- visibility and suppression are persistent subtree state;
- the occurrence transform is persistent rigid boundary placement intent;
- there is no grounding field and no whole-subassembly solver coordinate in the current seed;
- adding or loading an occurrence never moves child components.

## Assembly hierarchy format semantics

Project persistence stores documents and references, not a duplicated tree.

Persistent hierarchy authority is:

```text
explicit root assembly id
owned child AssemblyDocument records
SubassemblyInstance -> referenced child assembly id
SubassemblyInstance visibility/suppression/transform
```

The following are derived and not serialized:

```text
parent assembly links
DFS active/completed sets
hierarchy traversal order
rooted occurrence descriptors
parent transform chains
composed transforms
flattened leaf descriptors
posed leaf geometry
```

An occurrence path stored inside a cross-hierarchy relationship endpoint is different: it is persistent endpoint identity, not a persisted traversal descriptor cache.

Example persistent endpoint path:

```text
[subassembly.outer, subassembly.inner]
```

The path says which exact authored child-occurrence chain the endpoint addresses. Derived hierarchy traversal still regenerates all occurrence descriptors and parent transform chains.

## Hierarchical transform semantics

Every persisted `RigidTransform` retains the same semantics at every hierarchy level:

```text
translation_mm: millimeters
rotation_deg: degrees
rotation: active right-handed fixed-axis
order: X -> Y -> Z -> translation
```

For component transform `Tc`, inner child occurrence transform `Ti`, and outer occurrence transform `To`, point evaluation order is:

```text
To(Ti(Tc(p)))
```

The derived inner-to-outer chain is:

```text
[Tc, Ti, To]
```

This chain is not serialized. BLCAD does not collapse arbitrary hierarchy rotations and persist a recomputed Euler triple.

## Local geometric assembly constraint JSON

`assembly_constraints[]` inside one `AssemblyDocument` stores local geometric relationship intent.

Type spellings:

```text
mate
concentric
distance
insert
angle
```

Every record stores id, name, type, target A, target B, and `active|inactive` state.

Local target shape:

```json
{
  "component_instance": "component.plate.1",
  "semantic_reference": "feature.hole.seat"
}
```

Local constraints target component instances in their containing `AssemblyDocument` only.

Target A/B order is persistent.

## Project-level cross-hierarchy geometric constraint JSON

`cross_hierarchy_constraints[]` is an additive Project-level collection.

The exact endpoint shape is:

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

Representative relationship:

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

Persistent cross-hierarchy rules:

- relationship ids are unique within the Project-level cross-hierarchy collection;
- local `AssemblyConstraintId` values remain scoped by containing assembly document and may use the same text as a Project-level cross-hierarchy id;
- `occurrence_path` is an ordered root-to-current `SubassemblyInstanceId` sequence;
- path element order is identity and is preserved exactly;
- `[]` addresses the explicit root assembly occurrence;
- `component_instance` is local to the assembly document reached by the exact path;
- semantic target text is stored as opaque identity;
- target A/B order is preserved exactly;
- adding/loading a relationship never mutates component or subassembly transforms.

Core project structure validation first validates the ordinary assembly hierarchy, then follows each endpoint path exactly from the root through authored `SubassemblyInstance` links and requires the addressed local component to exist in the reached assembly document.

Loading does not resolve semantic feature geometry and does not call OCCT.

## Distance quantity JSON

Local and Project-level cross-hierarchy Distance constraints use:

```json
"distance": {
  "unit": "mm",
  "value": 40.0
}
```

Distance is a positive finite length quantity and must use `mm`.

## Angle quantity JSON

Local and Project-level cross-hierarchy Angle constraints use:

```json
"angle": {
  "unit": "deg",
  "value": 35.0
}
```

Angle is a finite degree quantity and must use `deg`.

## Geometric relationship value-family rules

For both local and Project-level cross-hierarchy geometric relationship records:

```text
Mate        -> omit distance and angle
Concentric  -> omit distance and angle
Insert      -> omit distance and angle
Distance    -> require distance, omit angle
Angle       -> require angle, omit distance
```

The Core typed relationship constructors remain authoritative after JSON parsing.

## Revolute joint JSON

Joint intent is separate from geometric constraint intent and uses local `assembly_joints[]`.

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

The coordinate is persistent authored motion state. Loading or adding a joint does not move component transforms. The current motion API rejects requested coordinates outside persistent limits instead of clamping them.

Joints currently target local component instances in the containing assembly document only. Cross-hierarchy joint targets are deferred.

## Semantic target strings

The record/JSON layer treats semantic target strings as opaque identity tokens. Geometry consumers currently interpret supported families:

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

The current local private numeric relationship set contains local geometric constraint ids plus optional transient Revolute drives. It is collected for one solve/motion query and is not serialized.

Current geometric flattening remains derived:

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

Cross-hierarchy relationship-to-transform-authority incidence and future solve groups remain derived and are not serialized.

## Derived assembly data is not persisted

Implemented or planned derivation layers include:

```text
AssemblyConstraintGraph
AssemblyJointGraph
combined motion relationship closure
AssemblyHierarchyTraversal
AssemblyLeafOccurrenceResolver
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyHierarchyTransformEvaluator
AssemblyHierarchyConstraintTargetResolver
geometric constraint equation builders
AssemblyRevoluteJointEquationBuilder
shared numeric relationship system
shared numeric solve engine
AssemblyRigidBodySolver
AssemblyJointMotionSolver
AssemblySolveDiagnosticsAnalyzer
AssemblyStepExporter
future cross-hierarchy relationship-to-authority incidence
future cross-hierarchy solve groups
```

Derived outputs include graph nodes/edges/groups, hierarchy occurrence descriptors, parent links, parent transform chains, flattened leaves, local and root-space target geometry, residual descriptors, transient drives, numeric residuals, Jacobians, normal equations, damping attempts, solver iteration state, solve/motion results, snapshots, unapplied proposals, diagnostics, per-consumer `ShapeCache` values, posed leaf shape copies, and OCCT compounds.

None is serialized.

## Explicit application and persisted state changes

`AssemblyRigidBodySolver` and `AssemblyJointMotionSolver` change only private project copies while solving.

A fresh converged local `AssemblySolveResult` may explicitly change:

```text
component_instances[].transform
```

A fresh converged `AssemblyJointMotionResult` may additionally change the selected authored joint coordinate.

Application paths are atomic through project copies and validate stale input snapshots before commit.

Rigid subassembly occurrence transforms and state change only through explicit occurrence edit APIs. Hierarchy traversal, nested STEP export, and cross-hierarchy relationship loading never rewrite them.

A later save serializes current authored transforms, coordinates, and relationship intent. The file does not record solver iteration history, transform provenance, or solve-result provenance.

## DOF diagnostics remain derived

Current rank and remaining-DOF values are not persistent.

Regular local observations include:

```text
Concentric geometric relationship: rank 4/6
Insert geometric relationship:     rank 5/6
Angle non-extremal seed:            rank 1/6
Driven Revolute query:              rank 6/6 on a 9 x 6 Jacobian
```

Future cross-hierarchy diagnostics will use unique free active transform-authority count, not rooted geometric occurrence count, when repeated occurrences share one child-document transform authority.

## Posed export and analysis data remain derived

Posed leaf descriptors, recomputed part caches, transformed shape copies, interference pairs, clearance distances, OCCT compounds, and STEP entity identity are regenerated by their consumers.

The save format stores authored placement/state and relationship intent only.

## Current cross-hierarchy format handoff

Implemented:

```text
Core AssemblyHierarchyConstraintEndpoint
Project-owned AssemblyHierarchyConstraint
cross_hierarchy_constraints[] project JSON
exact endpoint path and target-order roundtrip
mm/deg value-family roundtrip
backward-compatible absent-field loading
exact authored occurrence-path structure validation
reached local component validation
```

Next derived work is Block 25 in `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`: deterministic active relationship-to-`ComponentTransformAuthority` incidence and connected cross-hierarchy solve groups.
