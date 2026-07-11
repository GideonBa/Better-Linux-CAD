# Assembly Constraint Model Intent MVP-5

Status: implemented solver-independent assembly relationship records for Mate, Concentric, Distance, and Insert. Geometry resolution, residual construction, numeric solving, result application, and local DOF diagnostics remain separate downstream layers.

## Goal

Assembly relationship records persist what two component occurrences are intended to relate without computing where either component must move.

## Typed identity

Assembly constraints use `AssemblyConstraintId`. Ids are independent from component, document, feature, and profile ids. They must be non-empty and unique within one `AssemblyDocument`.

## Semantic component targets

`AssemblyConstraintTarget` stores:

```text
component_instance   ComponentInstanceId
semantic_reference  persistent semantic token
```

Implemented examples:

```text
component.plate.1 : feature.base_extrude.top
component.plate.2 : feature.hole.axis
component.plate.3 : feature.hole.seat
```

The record layer validates non-empty ids/strings and assembly component ownership. It intentionally keeps semantic-reference contents opaque.

Raw OCCT names such as `Face17`, `Edge4`, or a transient cylindrical-face index are not persistent model references.

Current geometry consumers interpret:

```text
feature.<feature-id>.top|bottom|right|left|front|back
feature.<feature-id>.axis
feature.<feature-id>.seat
```

## Constraint types

Persistent relationship families are:

```text
AssemblyConstraintType::Mate
AssemblyConstraintType::Concentric
AssemblyConstraintType::Distance
AssemblyConstraintType::Insert
```

Current downstream numeric solver support exists for Mate, Distance, and Concentric.

Insert has stable target/residual semantics but is deliberately not yet integrated into the shared numeric solver.

Flush, Coincident, Angle, Tangent, Lock, and joints remain future record families.

## Constraint record

```text
AssemblyConstraint
  id
  name
  type
  target_a
  target_b
  state = active | inactive
  distance = optional length quantity
```

Target A/B order is persistent relationship intent.

Order is observable in current residual conventions:

```text
Distance signed separation = dot(oB - oA, nA)
Concentric axis offset     = cross(oB - oA, dA)
Insert seating separation  = dot(sB - sA, nA)
```

`AssemblyConstraintState::Inactive` stores disabled intent without deleting the record.

## Type-specific distance validation

Distance is the only relationship type that stores a distance quantity.

Rules:

- Distance requires one finite positive length quantity
- Mate carries no distance
- Concentric carries no distance
- Insert carries no distance
- count quantities are rejected for Distance

Insert axial seating is currently zero-separation relationship semantics. A parameterized nonzero Insert offset is not hidden in this record and remains future work.

## Assembly ownership

`AssemblyDocument` owns constraints by value and exposes:

```text
add_constraint(AssemblyConstraint)
constraints()
constraint_count()
find_constraint(AssemblyConstraintId)
```

`add_constraint` validates unique ids and existing target component instances.

## Record-layer boundary

Adding or loading any constraint does not:

- mutate component transforms
- infer relationships from free placement
- resolve semantic geometry
- evaluate component transforms
- construct residuals
- flatten numeric residual vectors
- solve placement
- enforce grounding
- compute remaining DOF
- classify Jacobian rank
- recompute member-part geometry

JSON loading follows the same ownership/validation path and cannot silently move components.

## Downstream geometry families

### Planar Mate/Distance

```text
AssemblyConstraintTargetResolver::resolve
  -> AssemblyTransformEvaluator::evaluate_plane
  -> AssemblyConstraintEquationBuilder
```

Mate:

```text
normal_opposition    = nA + nB
signed_separation_mm = dot(oB - oA, nA)
```

Distance:

```text
normal_parallelism   = cross(nA, nB)
signed_separation_mm = dot(oB - oA, nA)
distance_residual_mm = signed_separation_mm - target_distance_mm
```

### Concentric

```text
AssemblyConstraintTargetResolver::resolve_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
```

```text
direction_parallelism = cross(dA, dB)
axis_offset_mm         = cross(oB - oA, dA)
```

### Insert

Canonical document: `docs/assembly-insert-intent-composite-residuals-mvp5.md`.

One persistent `.seat` endpoint derives a primary axis and oriented seating plane from the same exact circular feature/profile:

```text
AssemblyConstraintTargetResolver::resolve_insert
  -> local axis + local seating plane + separate RigidTransform
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
```

Composite residual:

```text
direction_parallelism       = cross(dA, dB)
axis_offset_mm               = cross(oB - oA, dA)
signed_seating_separation_mm = dot(sB - sA, nA)
```

The Insert builder is deterministic and read-only.

## Shared numeric, solver, and diagnostics boundary

The private shared numeric system currently selects and flattens Mate, Distance, and Concentric residual builders.

`AssemblyRigidBodySolver` and `AssemblySolveDiagnosticsAnalyzer` consume the same residual evaluator and central finite-difference Jacobian.

Regular one-free-body Concentric behavior is measured as:

```text
variable_count = 6
jacobian_rank  = 4
remaining_dof  = 2
```

The focused Insert residual test independently proves a direct `7 x 6` Jacobian rank of `5`, leaving one rotation-about-axis DOF. Insert is not yet a shared numeric family, so the solver and analyzer do not claim Insert support yet.

None of this numeric interpretation becomes persistent relationship state.

## Project structure validation

`Project::validate_assembly_constraints()` verifies that every persisted target refers to an owned assembly component instance.

`Project::validate_assembly_structure()` validates:

1. assembly members resolve to project-owned parts
2. component instances reference valid member/project parts
3. assembly constraints reference existing component instances

Several components may reference one owned `PartDocument`. Constraints relate occurrences, not duplicated part records.

Semantic geometry resolution remains a downstream geometry concern. A non-empty unsupported semantic token may persist as intent and fail only when a consumer attempts to interpret it.

## JSON representation

Historical compatibility marker:

```text
blcad.assembly_document.mvp4
version 1
```

MVP-5 relationship types extend the existing record shape additively.

Representative Insert record:

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

Compatibility rules:

- files without `assembly_constraints` load with zero constraints
- unsupported type/state strings are rejected
- Distance JSON must carry a millimeter distance value
- Mate, Concentric, and Insert omit `distance`
- target component validation runs through `AssemblyDocument::add_constraint`
- project JSON inherits constraint persistence through embedded assembly JSON

`insert` uses the existing constraint type field. `feature.hole.seat` uses the existing semantic-reference string field. No JSON shape change is required.

No graph, resolved target, assembly-space geometry, residual, numeric vector, Jacobian, solve result, rank/DOF state, or solved-transform cache is serialized by the record layer.

## Tests

Core intent coverage:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint]"
./build/dev/blcad_core_tests "[core][assembly-insert]"
```

Downstream geometry coverage:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-equation]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-concentric-solver]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

## Current downstream boundary

Mate, Distance, and Concentric participate in the shared numeric solver/DOF path.

Insert relationship intent, semantic seating targets, composite endpoint resolution, and read-only residual construction are implemented without changing the persistent record shape beyond the additive `insert` type string.

The next assembly block is Insert integration into the shared numeric residual/Jacobian system, rigid-body solver, explicit application path, and local remaining-DOF diagnostics.
