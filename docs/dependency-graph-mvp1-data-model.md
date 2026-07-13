# MVP 1 Dependency Graph Data Model

Status: data model with `PartDocument` Feature/Body producer-consumer integration; recompute
execution remains separate.

This document describes the current state of `DependencyGraph`.

The current scope is deliberately limited:

- no geometry
- no OCCT operations
- no ShapeCache in this graph layer
- no recompute
- no GUI

## Goal

The dependency graph stores which MVP-1 objects depend on each other. This makes the design intent visible and testable before any geometry is computed.

An edge:

```text
A -> B
```

means:

```text
B depends on A.
```

If `A` is marked changed, `InvalidationState` marks `B` and its transitive dependents dirty;
`RecomputePlan` orders that work. Geometry execution remains outside this graph model.

## Nodes

For MVP 1, nodes are simple stable string IDs.

Examples:

```text
part.width
part.height
part.thickness
part.hole_diameter
sketch.base
sketch.hole
feature.base_extrude
feature.center_hole_cut
body:body.base
body:body.result
```

The graph deliberately uses no OCCT or GUI types. Internally, it also knows no concrete `ParameterId`, `SketchId`, or `FeatureId` classes. `PartDocument` translates the typed IDs into these stable node IDs.

## Edges

`add_dependency(dependency, dependent)` stores a directed edge.

Rules:

- empty source nodes are rejected
- empty target nodes are rejected
- self-dependencies are rejected
- duplicate edges are rejected
- missing nodes are created automatically when an edge is added
- node and edge insertion order is preserved

Explicit `add_node(node_id)` is idempotent. If the node already exists, the existing index is returned.

## Queries

Current queries:

- `has_node(node_id)`
- `has_dependency(dependency, dependent)`
- `direct_dependents(node_id)`
- `transitive_dependents(node_id)`
- `topological_order()`
- `has_cycle()`

`direct_dependents()` returns only direct successors.

`transitive_dependents()` returns all indirectly affected nodes in deterministic traversal order. The start node itself is not returned.

`topological_order()` returns an order in which dependencies appear before their dependent nodes. If the graph contains a cycle, a `dependency` error is returned.

## Reference plate

The current MVP-1 test graph describes the reference plate:

```text
part.width          -> sketch.base
part.height         -> sketch.base
part.hole_diameter  -> sketch.hole
sketch.base         -> feature.base_extrude
part.thickness      -> feature.base_extrude
sketch.hole         -> feature.center_hole_cut
feature.base_extrude -> feature.center_hole_cut
```

This makes it testable that:

- `sketch.base` comes after `part.width` and `part.height`
- `feature.base_extrude` comes after `sketch.base` and `part.thickness`
- `sketch.hole` comes after `part.hole_diameter`
- `feature.center_hole_cut` comes after `sketch.hole` and `feature.base_extrude`

This is not recompute execution yet. It is only the ordered dependency structure.

## `PartDocument` integration

`PartDocument` owns an internal `DependencyGraph`.

When objects are added, nodes and edges are created automatically:

- `add_parameter()` creates a parameter node.
- `add_sketch()` creates a sketch node.
- rectangle profiles create edges from width and height parameters to the sketch.
- circle profiles create an edge from the diameter parameter to the sketch.
- `add_feature()` creates a feature node.
- every feature creates an edge from the input sketch to the feature.
- `AdditiveExtrude` additionally creates an edge from the length parameter to the feature.
- `SubtractiveExtrude` additionally creates an edge from the target feature to the feature.
- adding a Body creates a canonical `body:<BodyId>` node;
- explicit Feature Body context connects target Body to Feature and Feature to result Body;
- later Body consumers depend on the current Body node;
- an in-place modifier advances the producer chain as `previous producer -> modifier -> body` to
  avoid a false body/modifier self-cycle;
- a duplicate producer or real graph cycle rejects the Feature transactionally;
- removing an unused Body removes its node and incident edges, while produced/referenced Bodies
  cannot be removed.

The graph is readable through `PartDocument::dependency_graph()`. The accessor is intentionally `const` so external callers cannot bypass document invariants.

## Error behavior

Validation errors use `ErrorCategory::Validation`.

Examples:

```text
dependency node id must not be empty
dependency source node id must not be empty
dependency dependent node id must not be empty
```

Structural graph errors use `ErrorCategory::Dependency`.

Examples:

```text
dependency edge must not point to itself
dependency edge must be unique
dependency graph contains a cycle
```

## Test coverage

Current tests check:

- an empty graph starts without nodes and edges
- explicit nodes are stored
- duplicate nodes are not created again
- empty node IDs are rejected
- edges automatically create missing nodes
- empty source and target nodes are rejected
- self-dependencies are rejected
- duplicate edges are rejected
- direct dependents are found
- transitive dependents are found
- unknown nodes return empty query results
- the reference plate has a valid topological order
- cycles are detected
- `PartDocument` creates graph nodes for parameters, sketches, and features
- `PartDocument` creates graph edges from profile and feature references
- the MVP-1 reference plate is topologically ordered through the document graph
- `InvalidationState` uses the graph to mark dependent nodes as `dirty`
- `RecomputePlan` uses the graph's topological order to sort `dirty` nodes
- Feature/Body producer chains propagate invalidation into later consumers and Body results
- in-place Body updates remain acyclic and topologically ordered
- duplicate producers and real cycles fail before graph mutation
- dependent Body removal is rejected and unused Body-node removal is complete

## Next useful step

The graph remains separate and describes dependencies. Block 52 now supplies the Body-specific
execution boundary:

- execute Body-scoped Feature and Body nodes from a recompute plan
- inspect the deterministic Body results now produced by the Block-52 `ShapeCache`
- use OCCT only behind this adapter boundary

Block 53 is the next step and freezes public Body-result inspection.
