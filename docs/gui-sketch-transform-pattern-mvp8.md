# GUI Sketch Transform, Mirror, and Pattern MVP-8

Status: implemented in Block 118.

Block 118 adds a headless Core authority for moving, rotating, scaling, copying, mirroring, and patterning selected ordinary planar Sketch curves. Qt remains a command and preview client; all accepted geometry is produced as a complete disposable `Sketch` candidate.

## Authority boundary

```text
selection + typed transform/pattern parameters
  -> SketchTransformPatternService (Core)
  -> complete candidate Sketch
  -> optional explicit SketchPatternIntent
  -> one later document transaction
```

`SketchTransformPatternService` is declared in `include/blcad/core/sketch_transform_pattern.hpp`. It supports `LineSegment`, `ArcSegment`, and cubic `SplineSegment` geometry. Projected/reference-generated entities remain read-only and are rejected as transform sources.

## Implemented transforms

- `move(...)` translates every selected curve and preserves its entity id.
- `rotate(...)` rotates every defining point around a finite center and preserves ids.
- `scale(...)` applies a non-zero uniform factor around a finite center and preserves ids.
- `copy(...)` creates ordinary detached copies using deterministic `<source>.copy.N` ids.
- `mirror(...)` reflects every defining point across a non-degenerate line and creates deterministic `<source>.mirror.N` ordinary curves.

Preserving ids for move/rotate/scale keeps stable entity identity. Copy and mirror intentionally create independent ordinary entities; no hidden transform relationship is introduced.

## Rectangular and circular patterns

Rectangular patterns accept two finite step vectors and positive counts. The seed occupies index `(0,0)` and is not duplicated. Every remaining instance is emitted as ordinary Sketch geometry with deterministic `<source>.rect.N` ids.

Circular patterns accept a finite center, a positive count, and a finite total angle. The seed is the first instance. Remaining instances rotate by a deterministic step of `total_angle / (count - 1)` and use `<source>.circular.N` ids.

`SketchPatternMode` is explicit:

- `Associative` returns a `SketchPatternIntent` describing stable source ids, pattern family, counts, vectors or center, and angular step.
- `Exploded` returns no pattern intent; generated curves are independent ordinary entities.

The generated candidate geometry is never opaque. A caller that owns persistent editing state can store the returned associative intent; exploded mode requires no additional persistent record.

## Failure policy

The service fails closed for empty selections, unknown or reference-only entities, non-finite parameters, zero scale, degenerate mirror axes, and zero pattern counts. Source Sketch state is never partially modified.

## Focused proof

```bash
./build/dev/blcad_core_tests "[core][sketch-transform-pattern]"
```

The registered Core proof covers stable-id move, ordinary copy and mirror, rectangular associative intent, circular exploded output, deterministic created ids, and rejection of invalid selections and degenerate parameters.

## Next boundary

Block 119 owns continuous region recognition, profile selection, open/self-crossing diagnostics, repair integration, and the solver-aware Finish Sketch workflow.
