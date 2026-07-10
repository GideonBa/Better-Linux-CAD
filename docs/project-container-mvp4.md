# Project container for assembly and member parts

Status: implemented seed. The project container owns one assembly document and embedded member part documents.

This document describes the first project-level container above `AssemblyDocument` and `PartDocument`.

## Goal

The goal is to let one project-level object coordinate assembly parameters and their bound member parts without requiring callers to manually update each part document.

The project container must not:

- create component instances
- place components in 3D space
- solve assembly constraints
- introduce assembly transforms or mates
- replace `AssemblyDocument` binding semantics
- bypass `PartDocument` invalidation and recompute planning

Component instances and constraints remain MVP 5 and are tracked in `docs/assembly-system.md`.

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

## Membership validation

`Project::validate_member_parts` checks that every `AssemblyDocument::member_parts()` id resolves to an owned `PartDocument`.

Extra owned parts are allowed by the seed. They are ignored by assembly parameter propagation unless the assembly explicitly registers them as members.

Duplicate owned part document ids are rejected by `Project::add_part_document`.

## Project-level parameter update

The implemented project-level update call is:

```text
Project::set_assembly_parameter_value(parameter, value) -> ProjectUpdateResult
```

It performs this sequence:

1. validate that every assembly member id resolves to an owned part document
2. update the assembly parameter value with `AssemblyDocument::set_parameter_value`
3. apply assembly bindings to each owned member part through `AssemblyDocument::apply_bindings_to`
4. ask each affected part for `PartDocument::create_recompute_plan`
5. return one `ProjectPartUpdate` per affected member part

The project does not recompute geometry itself. It returns recompute plans so the caller can decide whether to recompute through the core recompute path, the geometry executor, or a future job system.

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

The project deserializer delegates assembly parsing to `deserialize_assembly_document_from_json` and part parsing to `deserialize_part_document_from_json`. After loading, it validates that every assembly member part resolves to an embedded part document.

A manifest-based project file that references separate part files is deliberately deferred.

## Headless project export

When geometry targets are enabled, the seed adds:

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

## Test coverage

The tests cover:

- project creation and duplicate owned part rejection
- membership validation for assembly member ids
- automatic assembly binding propagation into owned member parts
- per-part recompute plans from one project-level parameter update
- project JSON roundtrip with embedded assembly and part documents
- project JSON rejection when an assembly member part is missing from the project

## Deliberate limitations

This block does not implement component instances.

It does not implement assembly transforms, free placement, grounding, mates, concentric constraints, distance constraints, constraint solving, DOF display, collision checks, or assembly-level STEP export.

It does not implement manifest-based project files, external file references, lazy part loading, dirty-file tracking, or partial project save.

A later MVP 5 seed should add `ComponentInstance` records that reference owned project part documents, including explicit instance ids, names, referenced document ids, fixed/grounded state, visibility, and an initial free transform, but still without a full constraint solver.
