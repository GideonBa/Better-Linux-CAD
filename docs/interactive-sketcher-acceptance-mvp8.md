# Interactive Sketcher Acceptance MVP-8

Status: implemented in Block 121.

Block 121 closes Interactive Sketcher MVP-8 with one canonical, machine-readable coverage authority in `include/blcad/core/interactive_sketcher_acceptance.hpp`. The catalog covers every capability delivered in Blocks 106–120 and freezes the evidence required before a tool family is considered accepted.

## Required evidence

Every coverage record carries all eight evidence bits:

1. mouse/script equivalence: pointer-driven and direct command inputs must produce the same persistent candidate;
2. persistence/recompute: save/open and recompute preserve authored intent and stable ids;
3. exact undo/redo: one accepted command creates one history entry and restores byte-equivalent authored state;
4. conflict atomicity: invalid, redundant, conflicting, stale, or non-convergent candidates do not mutate the source;
5. reference repair: lost or ambiguous references remain explicit and repairable rather than silently rebound;
6. keyboard Apply/Cancel: Apply follows the same commit path as pointer acceptance, while Cancel restores the baseline;
7. high-DPI mapping: device-independent coordinates map to the same model-space result at supported scale factors;
8. no stale preview: a preview derived from an obsolete source revision cannot be published or committed.

The manifest test rejects missing contracts, tags, duplicate capability ids, or incomplete evidence masks.

## Coverage

The catalog contains fifteen capability families: workspace lifecycle, plane mapping, shared topology, planar solving, solver-backed drag, basic creation, conics and slots, splines and text, constraints, dimensions, modify tools, offset/project/reference tools, transforms and patterns, regions and Finish Sketch, and interactive Sketch3D.

The deferred scope listed in `docs/interactive-sketcher-sequence-mvp8.md` remains outside MVP-8. In particular, the acceptance manifest does not claim Qt wiring for the narrowed Core-only Block-117 and Block-118 services, curved-boundary region recognition, or a variational 3D solver.

## Deterministic tutorials

`docs/tutorials/interactive-sketcher-planar-mvp8.md` defines a planar tutorial that authors, constrains, dimensions, modifies, patterns, projects, selects a region, finishes, saves, reloads, and verifies undo/redo.

`docs/tutorials/interactive-sketcher-sketch3d-mvp8.md` defines a Sketch3D tutorial for axis/plane locks, typed placement, guide roles, point-handle movement, and planar-point projection.

Tutorial step ids are stable and are intended for both GUI automation and headless scripts. Implementations may render different presentation text, but the command sequence and expected persistent ids must remain equivalent.

## Performance budgets

Representative acceptance budgets are frozen as metadata rather than wall-clock assertions in ordinary CI, because shared runners are not deterministic performance environments:

| Operation | Representative size | Budget |
|---|---:|---:|
| Hover hit test | 500 entities | 8 ms |
| Solver-backed drag frame | 100 entities | 16 ms |
| Planar solve | 250 entities | 50 ms |
| Region recognition | 500 entities | 50 ms |

Dedicated benchmark runs must measure these operations with release builds, warmed caches, a fixed tutorial document, and reported median plus p95. The normal acceptance test verifies that the complete, positive, bounded budget catalog remains present.

## Focused verification

```bash
./build/dev/blcad_core_tests "[integration][interactive-sketcher]"
./build/dev/blcad_core_tests "[integration][sketch-gui-headless]"
./build/dev/blcad_core_tests "[performance][sketch-interaction]"
```
