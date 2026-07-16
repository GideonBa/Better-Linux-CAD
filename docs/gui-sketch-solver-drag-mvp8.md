# Solver-Backed Sketch Drag MVP-8

Status: implemented in Block 110. Session-backed Block-114 constraint and Block-115 dimension reuse is
implemented.

This document is the canonical GUI/Core integration contract for solver-backed planar Sketch mouse
dragging. Pointer motion remains transient; persistent geometry changes publish only through one final
`GuiDocumentSession` transaction.

## Authority boundary

```text
screen pointer
  -> Block-107 plane mapping and semantic hit
  -> GuiSketchDragHandle
  -> stable SketchPointId / SketchTopologyEntity role
  -> transient drag constraint
  -> historical constraints + Block-114 catalog + Block-115 driving dimensions
  -> Block-109 SketchConstraintSolver
  -> complete preview topology and Sketch
  -> final freshness and measurement validation
  -> one Drag sketch handle transaction
```

GUI code owns pointer capture, coalescing, preview display, and release/cancel lifecycle. Core topology,
constraint composition, dimension values, solve mathematics, materialization, and persistence remain
below Qt.

## Semantic handles

The controller derives handles from persistent topology roles:

```text
Endpoint
Midpoint
Center
Radius
Arc
Spline
Dimension
```

Current direct target kinds are:

```text
Point
LineMidpoint
ArcCenter
ArcRadius
```

A handle stores a stable point id or entity id. Screen pixels, sampled curve indices, OCCT edges,
widget pointers, and temporary solver variables never become handle identity.

Reference geometry is read-only and rejects drag start.

## Baseline creation

The historical `create(const PartDocument&, SketchId)` path migrates the current Sketch and adapts its
embedded constraints.

The session-backed `create(const GuiDocumentSession&, SketchId)` path additionally resolves:

```text
accepted SketchConstraintCatalog
accepted SketchDimensionCatalog
```

Its baseline system is therefore:

```text
historical embedded constraints
+ Block-114 accepted constraints
+ Block-115 driving dimensions
```

Reference dimensions add no residual. Before drag begins, the baseline must solve to
`under_constrained` or `fully_constrained`, and every driving dimension measurement must match its bound
parameter.

## Pointer coalescing

`begin(handle)` freezes the baseline. `queue_pointer(...)` keeps only the newest unprocessed pointer
sample. `process_pending()` solves exactly one queued sample. This prevents event backlog from becoming
model history while preserving deterministic final release behavior.

The controller tracks:

```text
pending_pointer
processed_pointer
latest_preview
solve_count
```

A preview never mutates `PartDocument`, sidecar catalogs, or global history.

## Transient drag equations

Point handle:

```text
controlled persistent point coincident with one transient drag point
```

Line midpoint:

```text
transient drag point is midpoint of the persistent line
```

Arc center:

```text
persistent arc is concentric with a transient center-bearing arc entity
```

Arc radius:

```text
persistent arc receives one transient Radial value from center-to-pointer distance
```

Transient point/entity ids use reserved internal drag identifiers and are stripped before publication.

## Preview solve and materialization

For every processed pointer:

1. augment source topology with required transient drag geometry;
2. append one transient drag constraint to the frozen baseline system;
3. solve through `SketchConstraintSolver`;
4. reject redundant, conflicting, invalid-reference, or non-convergent results;
5. strip transient topology;
6. materialize the complete historical Sketch;
7. re-migrate and require exact topology equality;
8. publish an immutable `GuiSketchDragPreview`.

Preview status, residuals, DOF, and diagnostics remain derived.

## Block-115 dimension behavior

Ordinary driving distances, line lengths, radius, diameter, and angle constraints are present directly
in the frozen baseline system. Dragging therefore moves remaining DOF while preserving those values.

Arc length is represented during the frozen preview solve by the equivalent radius calibrated at drag
start. Because changing an arc point can also change sweep, final release performs an additional actual
arc-length measurement. A preview whose final measured arc length no longer matches the parameter is
rejected rather than publishing a stale equivalent-radius approximation.

Reference dimensions do not restrict dragging and are remeasured from the accepted final topology.

## Release and atomic commit

Release first flushes the exact final pointer sample. Commit then requires:

- one active handle;
- no pending sample;
- one valid final preview;
- the original Part and Sketch still active;
- current source topology equal to the frozen source topology;
- current Block-114 catalog equal to its snapshot;
- current Block-115 catalog equal to its snapshot;
- rebuilt complete effective system equal to the frozen baseline system;
- final topology satisfying every driving dimension measurement;
- exact historical Sketch materialization.

Only then does the controller publish:

```text
GuiDocumentSession::commit_part_transaction("Drag sketch handle", ...)
```

The transaction updates the complete Sketch, recomputes the Part, clears selection, and appends one
global history entry. Constraint and dimension catalogs remain unchanged but are part of the same
session history snapshot.

## Cancel and lost interaction

Cancel clears active handle, queued pointer, processed pointer, preview, and solve count. Since preview
never changed persistent state, no compensating document transaction is required.

The Qt binder also cancels on lost capture, workspace exit, incompatible command start, or invalid
release. The original Part/Sketch/catalog state remains authoritative.

## Freshness and failure policy

No mutation occurs for:

- missing or stale Sketch;
- invalid/removed handle;
- reference handle;
- invalid pointer coordinates;
- failed baseline solve;
- failed preview solve;
- transient topology/materialization mismatch;
- changed constraint catalog;
- changed dimension catalog;
- changed effective system;
- final driving-dimension measurement mismatch;
- Part recompute failure;
- stale or out-of-order release.

## Undo and redo

The drag contributes exactly one `Drag sketch handle` history entry. `GuiDocumentSession` restores the
complete Part JSON plus Block-114 and Block-115 catalog snapshots, then recomputes derived geometry.
There is no independent drag, constraint, or dimension undo stack.

## Focused proof

```bash
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-drag]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[integration][sketch-live-solve]"
QT_QPA_PLATFORM=offscreen ./build/dev-gui/blcad_gui_tests "[gui][sketch-dimensions]"
```

The proof covers semantic handles, pointer coalescing, exact release flush, immutable previews,
constraint-catalog reuse, dimension-catalog reuse, measurement preservation, stale-catalog refusal,
exact materialization, and one atomic history transaction.

## Current next boundary

Block 116 introduces trim, extend, split, Sketch fillet, and Sketch chamfer. Those topology-changing
commands must explicitly remap or reject drag handles, constraints, and dimensions before any later
solver-backed direct manipulation occurs.
