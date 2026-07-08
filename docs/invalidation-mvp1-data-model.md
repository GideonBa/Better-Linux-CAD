# MVP 1 Invalidation-State Data Model

Status: pure data model with `PartDocument` integration, no recompute.

This document describes the current state of `InvalidationState`.

The current scope is deliberately limited:

- no geometry
- no OCCT operations
- no ShapeCache in this invalidation layer
- no recompute
- no GUI

## Goal

`InvalidationState` stores which nodes of the `DependencyGraph` are clean, directly changed, or affected by a change.

This allows MVP 1 to track:

- which parameter was changed
- which sketches depend on it
- which features depend on it
- which nodes should no longer be considered clean before a later recompute

The model does not execute anything. It only marks state.

## Status values

Current status values:

```text
clean
changed
dirty
error
```

Meaning:

- `clean`: the node is currently not marked as affected
- `changed`: the node was changed directly, for example a parameter
- `dirty`: the node transitively depends on a changed node
- `error`: reserved for later recompute or validation errors

In this step, `dirty` only means that the node should be reconsidered before geometry computation. It does not mean that recompute has been executed or has failed.

## Graph-based marking

`InvalidationState::mark_changed(graph, node_id)` uses the existing `DependencyGraph`.

Process:

1. The changed node must exist in the graph.
2. The changed node is marked as `changed`.
3. All transitively dependent nodes are marked as `dirty`.
4. The affected nodes are returned in deterministic order.

Example for `part.width`:

```text
part.width
  -> sketch.base
  -> feature.base_extrude
  -> feature.center_hole_cut
```

After marking:

```text
part.width               changed
sketch.base              dirty
feature.base_extrude     dirty
feature.center_hole_cut  dirty
```

Unaffected nodes remain `clean`.

## `PartDocument` integration

`PartDocument` owns an `InvalidationState`.

After successfully adding parameters, sketches, and features, the invalidation state is synchronized with the dependency graph. New graph nodes start as `clean`.

Current API:

```text
PartDocument::invalidation_state()
PartDocument::mark_parameter_changed(ParameterId)
PartDocument::create_recompute_plan()
PartDocument::mark_all_clean()
```

`mark_parameter_changed()` does not change a parameter value. The method only describes the state after a parameter change. The actual parameter-value update and recompute follow later.

## Error behavior

Empty node IDs are validation errors.

```text
invalidation node id must not be empty
changed node id must not be empty
```

A changed node must exist in the dependency graph.

```text
changed node must exist in dependency graph
```

`PartDocument::mark_parameter_changed()` accepts only parameters that exist in the document.

```text
parameter must exist in part document
```

## Test coverage

Current tests check:

- stable text names for status values
- graph nodes start as `clean`
- empty invalidation nodes are rejected
- changed nodes are marked as `changed`
- transitive dependents are marked as `dirty`
- unaffected graph branches remain `clean`
- all nodes can be reset to `clean`
- missing changed nodes are rejected
- `PartDocument` synchronizes invalidation state with its graph
- `PartDocument` marks affected nodes after a parameter change
- `PartDocument` can derive a recompute plan from that state

## Next useful step

The first optional geometry adapter and additive execution already exist. Invalidation remains separate and only marks nodes. The next step should stay small:

- execute `SubtractiveExtrude` from a recompute-plan node
- treat the existing geometry `ShapeCache` as input and output
- use OCCT only behind this adapter boundary
- do not build a GUI yet
