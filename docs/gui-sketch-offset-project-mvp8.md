# GUI Sketch Offset, Project, and Construction References MVP-8

Status: implemented in Block 117.

Block 117 adds a headless Core authority for deterministic line-chain and closed-loop offsets, associative Project/Include records, construction-axis projection, and explicit break-link conversion. The implementation reuses the existing `ProjectedSketchPoint`, `ProjectedSketchLine`, semantic-reference recovery, and generic Part transaction authorities; it does not introduce a second reference graph.

## Authority boundary

```text
ordered Sketch selection / external semantic or construction reference
  -> SketchOffsetProjectService (Core)
     validates connectivity, direction, distance, and reference identity
  -> disposable candidate Sketch
  -> one caller-owned document transaction
  -> existing persistence and reference-recovery authorities
```

`include/blcad/core/sketch_offset_project.hpp` owns offset mathematics and Project/Include intent creation. GUI consumers supply ordered entity selections and resolved projection coordinates; Qt remains responsible only for command state, preview presentation, and pick delivery.

## Offset contract

`offset_lines(...)` accepts an ordered selection of line entities, a signed non-zero distance, and an optional closed-loop requirement. Positive distance offsets to the left of each directed source edge. Adjacent shifted supports are intersected to create deterministic miter joints. The operation fails closed for missing or non-line entities, disconnected chains, non-closed loop requests, degenerate source edges, non-finite or zero distance, and parallel ambiguous corners.

Source geometry and intent are copied verbatim. Offset entities are ordinary `LineSegment` records with deterministic first-free identifiers of the form `<source-id>.offset.N`. No source profile is silently rewritten and no source constraint or dimension is transferred onto the new geometry.

Example: an ordered L-shaped chain `(0,0)->(10,0)->(10,8)` offset by `+2` becomes `(0,2)->(8,2)->(8,8)`; the shared corner is the intersection of both shifted supports rather than two disconnected endpoints.

## Associative Project/Include

`project_point(...)` and `project_line(...)` add existing `ProjectedSketchPoint` or `ProjectedSketchLine` intent to a disposable candidate Sketch. Construction points/lines and semantic vertices/edges therefore retain their source identifiers and continue through the existing lost/ambiguous reference-recovery workflow.

Construction axes use `ProjectedSketchLine::create_from_construction_line(...)`; no synthetic finite axis is persisted while the link remains associative. Semantic geometry uses the corresponding semantic vertex/edge constructors.

## Break-link conversion

`break_link_line(...)` explicitly replaces one projected line with an ordinary `LineSegment` using the caller-provided resolved endpoints. The detached line keeps the projected entity identifier, which preserves stable selection identity while removing the external source link. Break-link fails closed when the projected reference does not exist or a historical projected-reference constraint still depends on it; callers must remove or deliberately replace those constraints first.

After conversion, subsequent source-model changes and reference recovery do not move the detached line because it is ordinary Sketch geometry.

## Persistence and recovery

Block 117 adds no JSON schema. Associative references already persist through the existing projected-reference records and semantic-reference persistence. Offset and detached geometry persist through ordinary Sketch line serialization. Lost and ambiguous references continue to use the existing reference-recovery diagnostics; break-link is the explicit user decision that exits that workflow.

## Focused proof

```text
[core][sketch-offset-project]
[geometry][sketch-offset-project]
[gui][sketch-project]
```

The Core proof covers deterministic mitered chain offset, disconnected/open-loop rejection, associative construction-axis projection, and explicit break-link conversion. Geometry and GUI consumers should reuse the same Core candidate and transaction boundary rather than reimplementing offset or reference semantics.

## Next boundary

Block 118 owns move, rotate, scale, copy, mirror, rectangular Sketch pattern, and circular Sketch pattern. Curved offset, variable offset, profile-region offset, automatic corner-style alternatives, and projected-reference constraint migration during break-link remain explicit later extensions.
