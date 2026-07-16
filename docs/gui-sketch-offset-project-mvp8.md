# GUI Sketch Offset, Project, and Construction References MVP-8

Status: implemented in Block 117.

Block 117 adds a headless Core authority for deterministic line-chain and closed-loop offsets,
associative Project/Include records, construction-axis projection, and explicit break-link conversion.
The implementation reuses the existing `ProjectedSketchPoint`, `ProjectedSketchLine`,
semantic-reference recovery, and generic Part transaction authorities; it does not introduce a second
reference graph.

## Authority boundary

```text
ordered line selection / signed distance / external reference
  -> SketchOffsetProjectService (Core)
     validates the complete operation
     builds a disposable candidate Sketch
  -> existing Sketch reference and persistence authorities
  -> existing semantic recovery / Geometry projection consumers
```

The source Sketch is never partially mutated. Successful operations return a complete candidate and
the stable ids created by the operation.

## Offset

`offset_lines(...)` accepts an explicitly ordered set of line ids, a finite non-zero signed distance,
and an optional closed-loop requirement. Each source line is translated along its left-hand unit
normal. Adjacent translated supporting lines are intersected to produce deterministic miter joints.
Positive distance offsets to the left of each ordered line direction; negative distance offsets to the
right.

The operation fails closed when the selection is empty, contains a missing, non-line, or degenerate
entity, is not endpoint-connected in the supplied order, requests a closed loop for an open chain, or
contains a parallel corner without a unique miter intersection.

Offset results are ordinary `LineSegment` entities. Existing geometry and intent are copied unchanged.
Generated ids use `<source-id>.offset.N` with the first free deterministic suffix.

## Associative Project/Include

`project_point(...)` and `project_line(...)` add existing `ProjectedSketchPoint` and
`ProjectedSketchLine` records to a complete candidate. Construction points/lines and semantic
vertices/edges retain source identity and continue through the existing reference-recovery workflow.
Resolved coordinates are not persisted.

A construction axis is included by creating a projected line from its `ConstructionLineId`. It remains
associative and read-only until explicit break-link conversion.

Lost or ambiguous semantic targets remain governed by the existing reference-recovery and repair
workflow. Block 117 deliberately does not create parallel recovery semantics.

## Break link

`break_link_line(...)` removes one projected-line record and creates an ordinary `LineSegment` with the
same stable Sketch entity id and caller-supplied resolved endpoints. Conversion fails closed when the
projected id is missing, the resolved line is invalid, or an embedded reference constraint still
depends on the projected entity. Dependent intent is never silently discarded.

## Persistence

Block 117 adds no JSON schema. Associative projections persist through existing Sketch reference
records. Detached and offset results persist as ordinary line entities. Signed normals, miter
intersections, resolved external coordinates, and command previews remain derived or transient.

## Focused proof

```text
[core][sketch-offset-project]
```

```bash
./build/dev/blcad_core_tests "[core][sketch-offset-project]"
```

The proof is integrated into the already registered
`tests/core/projected_sketch_reference_tests.cpp` translation unit. It covers deterministic two-line
miter offset, disconnected-chain rejection, open-loop rejection, associative construction-line
projection, and projected-line break-link conversion.

## Next boundary

Block 118 owns move, rotate, scale, copy, mirror, rectangular Sketch pattern, and circular Sketch
pattern. Arc/spline offset, automatic chain discovery, bevel/round joins, variable-distance offset,
and automatic dependent-reference rewriting remain bounded later extensions.
