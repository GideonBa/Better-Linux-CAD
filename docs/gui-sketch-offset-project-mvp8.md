# GUI Sketch Offset, Project, and Construction References MVP-8

Status: Core implemented in Block 117; interactive shell commands added in the October 2026 follow-up.

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
contains a reversed parallel corner without a unique miter intersection. Forward collinear
segments retain their shared offset endpoint.

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
pattern. Automatic chain discovery and individual circular-arc offsets are implemented by the follow-up below.
Mixed curved chains, spline offsets, bevel/round joins, variable-distance offset, and automatic
dependent-reference rewriting remain later extensions.

## Interactive shell follow-up (October 2026)

The shell now exposes **Versatz**, **Geometrie projizieren**, and **Verknüpfung lösen** with numeric
preview, Apply/Cancel, one atomic undo entry, and existing Save/Open persistence. The shared
`GuiSketchOperationController` and `ShellSketchTools` dialog-free command surface are also exercised
by the registered `[gui][sketch-offset-project]` and `[gui][sketch-project]` proofs.

**Versatz** accepts an unordered selected line chain or loop. `offset_chain` discovers endpoint
connectivity and locally reverses traversal without changing the source entities or their direction.
Branches and disconnected selections are rejected. Open chains start at the lexicographically smaller
endpoint; closed loops start with the lexicographically smallest entity id in its stored direction.
The sign of the distance is relative to that traversal (positive = left). Consecutive collinear
segments share a joint; reversed/consumed edges are rejected. Closed line offsets immediately create
an ordinary `ClosedProfile` in addition to their new edges. Existing source profiles are preserved.

A single circular arc supports exact signed radial offset, preserving its angular extent and
orientation. Inward offsets that consume the radius are rejected. Mixed line/arc chains, spline
offsets, bevel/round joins, and variable distance remain extensions.

**Geometrie projizieren** offers currently resolvable construction points/lines and semantic
feature vertices/edges in a source chooser. The preview resolves through `SketchReferenceProjector`
and the current shape cache. Apply persists the original associative reference identity. Missing
sources and cyclic document dependencies reject the operation; there is no screen-coordinate fallback.

**Verknüpfung lösen** accepts one selected projected line. The user specifies a positive finite
segment length; the new ordinary line is centered at the resolved projected anchor along its current
projected direction and retains the entity id. Resolution is repeated before Apply. Existing embedded
reference constraints still block conversion and must be handled explicitly. Point detachment and
automatic dependent-reference rewriting remain extensions. Candidate cloning preserves unrelated
free-standing Sketch points, profiles, and intent.

All previews render independent dashed curve paths. Failed previews remove the previous candidate;
Cancel/Escape restores ordinary selection and drag handles. Both the shell and Core regression suite
cover point preservation, chain reordering/reversal, collinear joins, arc offsets, rejected branching
and collapsed geometry, projection, detachment, and atomic history.

```bash
./build/dev-gui/blcad_core_tests "[core][sketch-offset-project]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-offset-project]"
```
