# MVP 4 Seed: Assembly Parameters Shared Across Parts

Status: implemented first seed.

This block implements the first cross-part parametrization step from `docs/mvp-plan.md`: an assembly document owns assembly-scoped parameters, and part parameters can be bound to them so one shared value drives features in several parts. It follows the parameter-flow target in `docs/parameter-model.md`.

## Model

```text
AssemblyDocument
  id, name
  parameters          # ParameterScope::Assembly only
  member_parts        # DocumentId list
  bindings            # ParameterBinding list

ParameterBinding
  id                  # ParameterBindingId, unique in the assembly
  part_document       # must be a registered member part
  part_parameter      # ParameterId inside that part
  assembly_parameter  # ParameterId inside the assembly
```

The binding is explicit model intent: a part parameter declares that it follows an assembly parameter. A part parameter can be bound at most once. `ParameterScope` now distinguishes `part` and `assembly`; assembly documents reject part-scoped parameters and vice versa in JSON.

## Propagation

`AssemblyDocument::apply_bindings_to(PartDocument&)` pushes bound assembly values into one member part:

- the part must be a registered member.
- the bound part parameter must exist and have the same `ParameterType` (length/count) as the assembly parameter.
- each applied binding runs through `PartDocument::set_parameter_value`, so the part's dependency graph marks the affected sketches and features exactly as a direct parameter edit would.
- the returned `BindingApplication` reports the applied binding count and the affected part graph nodes.

Propagation is an explicit call, not a hidden side effect. The controlled flow is:

```text
assembly.set_parameter_value(...)
  -> assembly.apply_bindings_to(part_a) -> part_a recompute plan -> geometry recompute
  -> assembly.apply_bindings_to(part_b) -> part_b recompute plan -> geometry recompute
```

## JSON

Schema `blcad.assembly_document.mvp4`, version 1:

```json
{
  "schema": "blcad.assembly_document.mvp4",
  "version": 1,
  "document": {"id": "assembly.flange", "name": "FlangeAssembly"},
  "parameters": [
    {"id": "assembly.bolt_count", "name": "bolt_count", "type": "count",
     "scope": "assembly", "unit": "1", "value": 6}
  ],
  "member_parts": ["part.bolt_circle_plate"],
  "parameter_bindings": [
    {"id": "binding.plate.count",
     "part_document": "part.bolt_circle_plate",
     "part_parameter": "part.bolt_count",
     "assembly_parameter": "assembly.bolt_count"}
  ]
}
```

`examples/flange_assembly.blcad.json` is the checked-in reference: it binds the bolt-circle parameters of `examples/bolt_circle_plate.blcad.json` to assembly values.

## Covered by tests

- creation, parameter-scope enforcement, and unique ids/names.
- member-part validation: empty, self-reference, duplicates.
- binding validation: unknown member, unknown assembly parameter, double-binding a part parameter.
- propagation into two parts: values arrive, affected nodes include the pattern sketch and the cut feature.
- assembly value change re-propagates on the next application.
- rejection of non-member parts, missing part parameters, and length/count type mismatches.
- JSON roundtrip including a restored assembly that still propagates.
- geometry end-to-end: changing `assembly.bolt_count` from 6 to 8 recomputes both member plates to the expected volumes.

## Deliberate limitations

- propagation is pull/apply-based; there is no automatic cross-document notification yet. A project container that owns assembly and parts together is the natural place for that (`docs/file-format.md`).
- no component instances, transforms, or constraints (MVP 5, `docs/assembly-system.md`).
- no expressions over assembly parameters yet (`docs/parameter-model.md`).
- no global/project scope yet.
