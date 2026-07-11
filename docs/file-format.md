# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, component instances, Mate/Concentric/Distance relationship intent, semantic generated-axis targets, and the derived graph/target/transform/residual/numeric-solve/DOF-diagnostic pipeline.

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

- component occurrences reference part document ids instead of duplicating parts
- transform components must be finite
- `translation_mm` is stored in millimeters
- `rotation_deg` is stored in degrees
- transform evaluation uses active right-handed fixed-axis X-then-Y-then-Z rotation
- no transform matrix, quaternion, evaluated plane, or evaluated axis is serialized
- grounding, suppression, and visibility are persisted model intent
- storage-level direct transform edits remain legal while grounded

The exact transform semantics are canonicalized in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

Changing the interpretation of `rotation_deg` would be a model-format semantic compatibility change even if the JSON shape stayed identical.

## Assembly constraint JSON

A Concentric relationship persists semantic generated-axis tokens directly in the existing target string fields:

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

A Distance record uses the same target structure and adds a positive millimeter length value:

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
- raw OCCT topology ids are not persisted as model references
- target A/B order is preserved
- Mate and Concentric omit `distance`
- Distance requires a positive length quantity
- constraint state is `active` or `inactive`
- adding or loading a constraint does not mutate component transforms
- project structure validation checks target component identity

The semantic target string remains intentionally opaque at the record/JSON layer. Geometry consumers interpret only the families they explicitly support.

Current interpreted assembly geometry families include:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
```

The first `.axis` producer is a single-circle `SubtractiveExtrude`, documented in `docs/assembly-semantic-axis-concentric-residuals-mvp5.md`.

Changing the semantic meaning of an established token family is a model-format compatibility change even when the JSON field shape is unchanged.

## Derived assembly data is not persisted

The implemented derivation layers include:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyConstraintEquationBuilder
AssemblyConcentricConstraintEquationBuilder
shared assembly numeric system
AssemblyRigidBodySolver
AssemblySolveResultApplier
AssemblySolveDiagnosticsAnalyzer
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
component-local planar target descriptors
component-local axis target descriptors
assembly-space plane descriptors
assembly-space axis descriptors
planar Mate/Distance residual descriptors
Concentric residual descriptors
flattened scaled Mate/Distance/Concentric residual vectors
central finite-difference Jacobians
normal-equation matrices and damping attempts
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

They are rebuilt from persistent model intent and current component placement/state.

`SemanticAxisReference`, `ComponentLocalAxisDescriptor`, `AssemblySpaceAxisDescriptor`, and `ConcentricResidualDescriptor` are derived API descriptors rather than save-file records.

The six-scalar Concentric flattening order and rank `4/6` result are numeric interpretation, not persistence fields.

## Solver application and persisted transforms

`AssemblyRigidBodySolver` changes transforms only on private `Project` copies and returns an unpersisted `AssemblySolveResult`.

Mate, Distance, and Concentric all use this same solver path.

`AssemblySolveResultApplier` is the explicit mutation boundary. A fresh converged result must pass source-snapshot and proposal validation before proposed transforms are applied to another project copy.

After successful application, the existing persistent field changes:

```text
component_instances[].transform
```

No separate `solved_transform`, `solver_pose`, Concentric pose, or solve-cache field exists.

A later save serializes the current transform exactly as it would after a direct explicit placement edit. The format does not record whether the transform originated from user placement or successful Mate/Distance/Concentric solver application.

## DOF diagnostics remain derived

Current local rank and remaining-DOF values are intentionally not persisted:

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

These are local linearized values under the current numeric system and configured rank tolerances. Persisting them as authoritative state would create stale cache semantics after component, constraint, part-parameter, semantic-target, or numeric-policy changes.

Concentric diagnostics use the same derived Jacobian path as Mate/Distance.

A regular one-free-body Concentric relationship currently evaluates to:

```text
residual_component_count = 6
variable_count           = 6
jacobian_rank            = 4
remaining_dof            = 2
```

Those values remain regenerable observations, not file-format state.

No `dof`, `rank`, `constraint_state`, `concentric_state`, or diagnostics cache field is added to the current assembly/project schema.

## Current implemented serialization scope

Implemented APIs include:

- `serialize_part_document_to_json` / `deserialize_part_document_from_json`
- `write_part_document_json_file` / `read_part_document_json_file`
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json`
- `serialize_project_to_json` / `deserialize_project_from_json`
- `write_project_json_file` / `read_project_json_file`

Serialization stores model intent only. It excludes OCCT shapes, `ShapeCache`, graph connectivity, resolved planes/axes, evaluated assembly-space descriptors, residual descriptors, numeric residual vectors, Jacobians, solver results, unapplied proposals, rank summaries, and DOF diagnostics.

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

The current embedded project format is intentionally simpler. Manifest/external part references remain deferred.

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

The next assembly model-format extension is expected to come from explicit Insert relationship intent and stable semantic axial-seating target semantics. See `docs/mvp-plan.md`.
