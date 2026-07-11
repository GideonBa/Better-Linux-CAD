# Project container for assembly and member parts

Status: implemented seed. The project container owns one assembly document and embedded member part documents and now validates the current MVP-5 component/constraint structure carried by the assembly.

This document describes the first project-level container above `AssemblyDocument` and `PartDocument`.

## Goal

The original MVP-4 goal is to let one project-level object coordinate assembly parameters and their bound member parts without requiring callers to manually update each part document.

The project container must not:

- solve assembly constraints
- resolve semantic assembly target geometry
- replace `AssemblyDocument` binding or constraint-record semantics
- bypass `PartDocument` invalidation and recompute planning
- emit assembly-level geometry or an assembly-level STEP file

Component instances and explicit free-placement/state updates are handled by `docs/component-instance-mvp5.md`. Solver-independent Mate, Concentric, and Distance records are handled by `docs/assembly-constraint-model-intent-mvp5.md`. The project container validates and persists those assembly-owned records but does not solve them.

## Implemented records

The implemented API lives in `include/blcad/core/project.hpp`.

```text
Project
  id
  name
  assembly
  part_documents[]

ProjectPartUpdate
  part_document
  binding_application
  recompute_plan

ProjectUpdateResult
  part_updates[]
  updated_part_count()
```

`Project` owns an `AssemblyDocument` by value and owns each `PartDocument` by value. This is still a single-process, in-memory model. It is not a document database and does not yet support external part references.

## Membership and assembly-structure validation

`Project::validate_member_parts` checks that every `AssemblyDocument::member_parts()` id resolves to an owned `PartDocument`.

`Project::validate_component_instances` checks that every component instance references an assembly member part and that the referenced part resolves to an owned `PartDocument`.

`Project::validate_assembly_constraints` checks that every assembly constraint target A and target B component id resolves to an existing assembly component instance.

`Project::validate_assembly_structure` runs all three validations and is the preferred project-level check before using assembly structure.

Extra owned parts are allowed by the seed. They are ignored by assembly parameter propagation unless the assembly explicitly registers them as members.

Duplicate owned part document ids are rejected by `Project::add_part_document`.

## Project-level parameter update

The implemented project-level update call is:

```text
Project::set_assembly_parameter_value(parameter, value) -> ProjectUpdateResult
```

It performs this sequence:

1. validate assembly member ids against owned part documents
2. validate component instance member/project references
3. validate assembly constraint component targets
4. update the assembly parameter value with `AssemblyDocument::set_parameter_value`
5. apply assembly bindings to each owned member part through `AssemblyDocument::apply_bindings_to`
6. ask each affected part for `PartDocument::create_recompute_plan`
7. return one `ProjectPartUpdate` per affected member part

The project does not recompute geometry itself. It returns recompute plans so the caller can decide whether to recompute through the core recompute path, the geometry executor, or a future job system.

Constraint validation is structural only. Assembly parameter propagation does not resolve constraints, mutate component transforms, or build a solve graph.

## JSON persistence

The implemented JSON API lives in `include/blcad/core/project_json.hpp`.

```text
serialize_project_to_json(Project)
deserialize_project_from_json(string)
write_project_json_file(Project, path)
read_project_json_file(path)
```

The schema marker is:

```text
blcad.project.mvp4
```

The seed uses an embedded-document project file:

```json
{
  "schema": "blcad.project.mvp4",
  "version": 1,
  "project": {
    "id": "project.flange",
    "name": "FlangeProject"
  },
  "assembly": {
    "schema": "blcad.assembly_document.mvp4"
  },
  "parts": [
    {
      "schema": "blcad.part_document.mvp1"
    }
  ]
}
```

The project serializer/deserializer delegates assembly persistence to `serialize_assembly_document_to_json` / `deserialize_assembly_document_from_json` and part persistence to the part JSON helpers. Therefore component instances, updated component state, and `assembly_constraints` are carried automatically by embedded assembly JSON.

After loading, project deserialization validates member parts, component instances, and assembly constraint component targets through `validate_assembly_structure`.

A manifest-based project file that references separate part files is deliberately deferred.

## Headless project export and inspection

When geometry targets are enabled, the project block adds:

```text
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
```

The command:

1. reads a project file
2. looks up the assembly parameter
3. parses the new value as a length or count based on the existing parameter type
4. calls `Project::set_assembly_parameter_value`
5. recomputes every owned project part through `GeometryRecomputeExecutor::execute_document`
6. exports each final part shape as `<part_document_id>.step`

This command is a headless example, not a GUI workflow. It does not solve assembly placement and does not emit an assembly-level STEP file.

The MVP-5 assembly blocks use the separate non-geometry `blcad_inspect_project_components` command for listing component instances, persisted placement/state values, and stored assembly constraint type/state/semantic targets.

## Test coverage

The original project-container tests cover:

- project creation and duplicate owned part rejection
- membership validation for assembly member ids
- automatic assembly binding propagation into owned member parts
- per-part recompute plans from one project-level parameter update
- project JSON roundtrip with embedded assembly and part documents
- project JSON rejection when an assembly member part is missing from the project

The MVP-5 component tests separately cover project-level component reference validation, placement/state updates, shared `PartDocument` ownership, and project JSON roundtrip after those updates.

The MVP-5 assembly constraint tests separately cover constraint target validation in project structure, shared part ownership across constrained occurrences, and project JSON roundtrip for Mate, Concentric, and Distance records.

## Deliberate limitations

The project container does not solve component placements, constraints, or DOF.

It does not resolve semantic constraint target geometry, construct the next read-only constraint graph itself, enforce grounding, perform collision checks, or emit assembly-level STEP.

It also does not implement manifest-based project files, external part references, lazy part loading, dirty-file tracking, or partial project save.
