# MVP 1 Recompute Plan Data Model

Status: pure data model with `PartDocument` integration, no execution.

This document describes the current state of `RecomputePlan`.

The current scope is deliberately limited:

- no geometry
- no OCCT operations in the recompute plan
- no ShapeCache update
- no feature recompute
- no GUI

## Goal

`RecomputePlan` describes which `dirty` nodes would need to be recomputed later and in which order.

The plan is not execution. It does not call feature logic, create shapes, or write a cache. It is only the ordered task list for a later recompute.

## Inputs

The plan is derived from two existing data models:

- `DependencyGraph`
- `InvalidationState`

The `DependencyGraph` provides the topological order. The `InvalidationState` says which nodes are `dirty`.

## Rules

`RecomputePlan::from_graph_and_invalidation_state(graph, invalidation_state)` follows these rules:

- the dependency graph must be acyclic
- only nodes with status `dirty` are included in the plan
- nodes with status `changed` are not included
- nodes with status `clean` are not included
- the order follows the graph's topological order

This means a changed parameter itself is not recomputed. Instead, only the sketches and features depending on it are planned.

## Example

If `part.width` was changed, the invalidation state marks:

```text
part.width               changed
sketch.base              dirty
feature.base_extrude     dirty
feature.center_hole_cut  dirty
```

The recompute plan contains:

```text
1. sketch.base
2. feature.base_extrude
3. feature.center_hole_cut
```

`part.width` is not part of the plan because the parameter is already the source of the change.

## `PartDocument` integration

`PartDocument::create_recompute_plan()` derives the plan from its internal dependency graph and its internal invalidation state.

Current flow:

```text
PartDocument::mark_parameter_changed(part.width)
  -> part.width changed
  -> dependent nodes dirty

PartDocument::create_recompute_plan()
  -> dirty nodes in topological order
```

`PartDocument::mark_all_clean()` indirectly clears the plan because no `dirty` nodes remain afterward.

## Error behavior

If the dependency graph contains a cycle, no topological order can be produced. In that case, `RecomputePlan` returns the graph's dependency error.

```text
dependency graph contains a cycle
```

## Test coverage

Current tests check:

- a clean invalidation state creates an empty plan
- `dirty` nodes are included in topological order
- `changed` nodes are not included in the plan
- independent graph branches stay out of the plan
- graph cycles are reported as dependency errors
- `PartDocument` creates a plan from its internal state
- `PartDocument::mark_all_clean()` leads to an empty plan

## Connection to geometry

The first geometry adapter for rectangle extrusion, a small `ShapeCache`, and a narrow `AdditiveExtrude` execution now exist in the optional target `blcad_geometry`. The recompute plan remains separate from that: it still describes only the order of nodes that need recalculation.

Next steps may use the plan but should stay small:

- execute `SubtractiveExtrude` from a plan node
- update the result in the existing `ShapeCache`
- continue to use OCCT only behind `blcad_geometry`
- do not build a GUI yet
