# Project container for assemblies and owned parts

Status: implemented MVP-4 seed, additively extended by MVP-5 rigid child assembly ownership. `Project` now owns one explicit root assembly, project-owned child assembly documents, and project-owned part documents.

This document records the project-container boundary. Exact rigid hierarchy and nested export contracts are canonical in `docs/assembly-rigid-subassembly-nested-export-mvp5.md`.

## Original MVP-4 goal

The original MVP-4 goal was to let one project-level object coordinate root assembly parameters and their bound member parts without requiring callers to manually update each part document.

That root-parameter propagation contract remains unchanged.

The project container itself does not:

- solve geometric assembly constraints;
- resolve semantic assembly target geometry;
- replace `AssemblyDocument` relationship-record semantics;
- bypass `PartDocument` invalidation/recompute planning;
- persist solver, hierarchy traversal, or geometry cache authority.

Solver, motion, hierarchy traversal, leaf flattening, and assembly geometry consumers are separate layers above the persistent container records.

## Current project records

The implemented API lives in `include/blcad/core/project.hpp`.

```text
Project
  id
  name
  assembly                         # explicit root
  child_assembly_documents[]
  part_documents[]

ProjectPartUpdate
  part_document
  binding_application
  recompute_plan

ProjectUpdateResult
  part_updates[]
  updated_part_count()
```

`Project` owns all current documents by value. It is still a single-process in-memory model, not a document database.

The root API remains:

```text
Project::assembly()
```

Child assembly documents are owned separately:

```text
Project::child_assembly_documents()
Project::find_assembly_document(id)
```

`find_assembly_document` resolves the root or an owned child. Child assembly ids are unique across the complete project assembly-document set.

External part/assembly references and lazy loading remain deferred.

## Project structure validation

`Project::validate_member_parts` checks every root and child `AssemblyDocument::member_parts()` id against the project-owned part collection.

`Project::validate_component_instances` checks every root and child component occurrence against its containing assembly member-part set and the project-owned parts.

`Project::validate_assembly_constraints` checks geometric relationship target component ids inside each containing assembly document.

`Project::validate_assembly_joints` checks joint target component ids inside each containing assembly document.

`Project::validate_subassembly_instances` checks that each rigid child occurrence references a project-owned child assembly document rather than a missing document or the explicit project root.

`Project::validate_assembly_hierarchy` validates child references and rejects direct/indirect assembly-document cycles across the root and all owned child assembly documents.

`Project::validate_assembly_structure` runs the full member/component/constraint/joint/hierarchy validation chain and is the preferred project-level structural guard before geometry, motion, or hierarchy consumers.

Extra owned parts and child assemblies are allowed. An unreferenced child assembly is not part of the rooted occurrence tree, but its document graph is still hierarchy-cycle validated so invalid latent cycles cannot enter project state.

## Root assembly parameter update

The implemented project-level parameter update call remains:

```text
Project::set_assembly_parameter_value(parameter, value) -> ProjectUpdateResult
```

This API is explicitly the root assembly parameter update boundary.

It performs:

1. validate complete current project assembly structure;
2. update the root `AssemblyDocument` parameter value;
3. apply root assembly bindings to owned root member parts;
4. ask each affected part for `PartDocument::create_recompute_plan`;
5. return one `ProjectPartUpdate` per affected root member part.

The project does not recompute geometry itself. It returns recompute plans.

The current API does not provide a generic child-assembly parameter update path. That is a later application-layer design question and is separate from the rigid subassembly occurrence/export seed.

## JSON persistence

The implemented JSON API lives in `include/blcad/core/project_json.hpp`.

```text
serialize_project_to_json(Project)
deserialize_project_from_json(string)
write_project_json_file(Project, path)
read_project_json_file(path)
```

Historical schema marker:

```text
blcad.project.mvp4
version 1
```

Current embedded shape:

```json
{
  "schema": "blcad.project.mvp4",
  "version": 1,
  "project": {
    "id": "project.example",
    "name": "ExampleProject"
  },
  "assembly": {
    "schema": "blcad.assembly_document.mvp4"
  },
  "assemblies": [
    {
      "schema": "blcad.assembly_document.mvp4"
    }
  ],
  "parts": [
    {
      "schema": "blcad.part_document.mvp1"
    }
  ]
}
```

`assembly` is the explicit root.

`assemblies[]` is the optional additive collection of project-owned child assembly documents. Older project files without it remain loadable and produce zero child assemblies.

Project serialization delegates each assembly record to `serialize_assembly_document_to_json` and each part record to the part JSON helpers. Therefore component occurrences, rigid subassembly occurrences, constraints, and joints are carried by each embedded assembly record.

After load, project deserialization validates the complete structure and hierarchy.

No constraint graph, joint graph, hierarchy traversal, parent link, occurrence path, transform chain, flattened leaf descriptor, solve result, or shape cache is persisted in project JSON.

A manifest-based project file with separate document files remains deferred.

## Headless project consumers

### Root parameter update and per-part STEP

```text
blcad_export_project <input.blcad.project.json> <assembly-parameter-id> <value> <output-dir>
```

The command updates one root assembly parameter, obtains affected part recompute plans, recomputes project parts through `GeometryRecomputeExecutor`, and writes per-part STEP outputs.

It does not solve assembly placement.

### Component inspection

```text
blcad_inspect_project_components <input.blcad.project.json>
```

This remains a non-geometry inspection example for current root component/relationship data.

### Posed assembly export

```text
blcad_export_posed_assembly <input.blcad.project.json> <output.step>
```

The geometry consumer may optionally solve/apply one root geometric constraint group before export. `AssemblyStepExporter` then validates the complete root/child hierarchy, derives flattened visible-active leaves, recomputes referenced parts, applies full rigid transform chains, and emits one OCCT compound through the existing STEP writer.

Nested export behavior is not persistence logic; it is a derived geometry consumer of project records.

## Test coverage

Original MVP-4 project tests cover:

- project creation and duplicate owned part rejection;
- root membership validation;
- automatic root assembly binding propagation;
- per-part recompute plans from a root project-level parameter update;
- project JSON roundtrip;
- rejection when a registered member part is missing.

Later MVP-5 coverage separately proves:

- component reference validation and placement/state roundtrip;
- geometric constraint and joint structural validation;
- child assembly id uniqueness including the root id;
- rigid `SubassemblyInstance` JSON roundtrip;
- project JSON roundtrip with explicit root plus owned child assemblies;
- backward compatibility without `assemblies[]`;
- missing-child and root-reference rejection;
- indirect hierarchy cycle rejection;
- deterministic hierarchy traversal;
- canonical visible-active flattened leaf resolution;
- nested posed assembly STEP export.

## Deliberate limitations

The project container does not itself solve component placement, motion, or DOF.

Current ordinary constraint/joint solving remains local to the explicit root assembly APIs. Flexible child assembly solver variables and cross-hierarchy constraint/joint targets are not implemented.

The container does not persist resolved semantic geometry, graph connectivity, hierarchy traversal, composed transforms, flattened leaves, collision/interference state, or exchange-format geometry.

Manifest-based project files, external document references, lazy loading, dirty-file tracking, and partial project save remain deferred.
