# Project and Save File Format

Status: implemented seeds exist for single-part `.blcad.json`, assembly parameters, embedded project JSON, component instances, Mate/Concentric/Distance relationship intent, and the derived assembly graph/target/transform/residual/numeric-solve/DOF-diagnostic pipeline.

The save format stores parametric model intent. OCCT shapes, solver products, and local DOF diagnostics are regenerable derived data and are not the source of truth.

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

Implemented assembly records:

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

Persistence rules:

- component occurrences reference part document ids instead of duplicating parts
- project validation resolves component part references through assembly membership to project-owned parts
- explicit placement/state updates preserve component identity and referenced-part intent
- transform components must be finite
- `translation_mm` is stored in millimeters
- `rotation_deg` is stored in degrees
- transform evaluation uses the canonical active right-handed fixed-axis X-then-Y-then-Z convention
- no transform matrix, quaternion, or evaluated frame is serialized
- grounding, suppression, and visibility are persisted model intent
- storage-level direct transform edits remain legal while grounded

The exact transform semantics are canonicalized in `docs/assembly-rigid-transform-evaluation-mvp5.md`.

Changing the interpretation of `rotation_deg` would be a model-format semantic compatibility change even if the JSON shape stayed identical.

## Assembly constraint JSON

Representative Distance record:

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

Constraint persistence rules:

- constraints use typed ids and semantic target tokens
- raw OCCT topology ids are not persisted as model references
- target A/B order is preserved
- Mate and Concentric omit `distance`
- Distance requires a positive millimeter length quantity
- constraint state is `active` or `inactive`
- adding or loading a constraint does not mutate component transforms
- project structure validation checks target component identity

Signed planar Distance residuals depend on target A/B order, so persistence, graph, solver, and diagnostic layers must not reorder the relationship endpoints.

## Derived assembly data is not persisted

The implemented assembly derivation path is:

```text
AssemblyConstraintGraph
AssemblyConstraintTargetResolver
AssemblyTransformEvaluator
AssemblyConstraintEquationBuilder
shared assembly numeric system
AssemblyRigidBodySolver
AssemblySolveResultApplier
AssemblySolveDiagnosticsAnalyzer
```

Derived outputs include:

```text
constraint graph nodes/edges/groups
component-local resolved target descriptors
assembly-space point/vector/planar descriptors
planar Mate/Distance residual descriptors
flattened scaled residual vectors
central finite-difference Jacobians
normal-equation matrices and damping attempts
solver iteration state
AssemblySolveResult values
component input snapshots
unapplied transform proposals
residual summaries
Jacobian rank summaries
constrained and remaining DOF counts
local DOF classifications
consistency classifications
residual row-rank structure
```

None of those values is serialized.

They are rebuilt from persistent model intent and current component placement/state.

## Solver application and persisted transforms

`AssemblyRigidBodySolver` changes transforms only on private `Project` copies and returns an unpersisted `AssemblySolveResult`.

`AssemblySolveResultApplier` is the explicit mutation boundary. A fresh converged result must pass source-snapshot and proposal validation before proposed transforms are applied to another project copy.

After successful explicit application, the existing persistent field changes:

```text
component_instances[].transform
```

No separate `solved_transform`, `solver_pose`, or solve-cache field exists.

A later save serializes the current component transform exactly as it would after a direct explicit placement edit. The current format does not record whether a transform originated from user placement or successful solver-result application.

## DOF diagnostics remain derived

`AssemblySolveDiagnosticsAnalyzer` evaluates local Jacobian rank and remaining DOF at a private converged solver state.

The current diagnostic values are intentionally not persisted:

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

The diagnostics are local linearized values under the current numeric system and configured rank tolerances. Persisting them as authoritative model state would create stale cache semantics after component, constraint, part-parameter, or numeric-policy changes.

No `dof`, `rank`, `constraint_state`, or diagnostics cache field is added to the current assembly/project schema.

## Current implemented serialization scope

Implemented APIs include:

- `serialize_part_document_to_json` / `deserialize_part_document_from_json`
- `write_part_document_json_file` / `read_part_document_json_file`
- `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json`
- `serialize_project_to_json` / `deserialize_project_from_json`
- `write_project_json_file` / `read_project_json_file`

Serialization stores model intent only. It excludes OCCT shapes, `ShapeCache`, graph connectivity, target descriptors, evaluated frames, residual descriptors, numeric Jacobians, solver results, unapplied proposals, rank summaries, and DOF diagnostics.

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

## Future part intent records

The future part model remains intent-oriented:

```text
PartDocument
  parameters
  bodies
  body_transforms
  body_booleans
  datum_planes
  sketches
  path_curves
  features
  semantic_references
  optional cached_shapes
```

Future body records must preserve explicit body identity and ordered transform/boolean intent. Cached body shapes may exist out of band but must never replace feature/body intent.

Future path records must store ordered semantic curve references rather than raw OCCT wires. Loft section order and guide-curve identity must be explicit.

See `docs/multi-body-transform-and-path-features-roadmap.md`.

## Semantic axis persistence direction

The next assembly block introduces a generated-axis semantic reference family for Concentric.

The persistent side must remain a semantic token in the existing target record shape:

```text
AssemblyConstraintTarget
  component_instance
  semantic_reference
```

The project file should not persist evaluated axis points/directions or raw OCCT cylindrical face/edge ids.

Component-local and assembly-space axis descriptors plus Concentric residual descriptors are expected to remain regenerable derived data, following the same boundary as the current generated-face target pipeline.

## Rules

- model intent is the source of truth
- caches are regenerable and optional
- never serialize raw OCCT topology as a persistent model reference
- STEP export is a projection of recomputed model intent, not the save format
- component instances and assembly constraints are persistent intent
- active graph connectivity is regenerated
- resolved assembly target geometry is derived
- evaluated assembly-space geometry is derived
- residual descriptors and flattened residual vectors are derived
- numeric Jacobians and normal equations are derived
- solver results and unapplied proposals are derived
- local Jacobian rank and DOF diagnostics are derived
- only explicit successful proposal application changes existing persistent component transforms
- target A/B order must be preserved
- persistent solver/DOF cache data requires a separate future design and must never replace relationship intent

## Out of scope for the first versions

- binary/compressed project containers
- cached BRep as the primary model
- automatic body-ownership inference for old sketches
- arbitrary matrix transforms as the first user-facing transform operation
- drawing and BOM documents
