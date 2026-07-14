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
sketch_ownership:sketch.base
transform.move_body
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
- BodyTransform stacks advance Body producers in authored order without a self-cycle
- coordinate/rotation references and matching SketchOwnership nodes feed transform nodes
- transform invalidation propagates through later transforms into the Body result

## Next useful step

The graph remains separate and describes dependencies. Block 52 now supplies the Body-specific
execution boundary:

- execute Body-scoped Feature and Body nodes from a recompute plan
- inspect the deterministic Body results now produced by the Block-52 `ShapeCache`
- use OCCT only behind this adapter boundary

Block 53 freezes public Body-result inspection. Block 54 adds BodyBooleanFeature target/tool/result
dependencies, in-place producer advancement, invalidation, cycle rejection, and removal protection.
Block 56 adds ordered BodyTransform producer advancement and SketchOwnership association nodes;
Block 57 executes those planned transform nodes in Geometry.
Block 58 references expose dependency-ready `source_node_id()` values: profile regions point to
their Sketch node, datum/construction references to their typed source node, semantic topology to
its producer Feature, and Body references to `body:<BodyId>`. Consuming features add the actual
edges when their Core intent is introduced.
Block 59 is the first such consumer: every Extrude extent/thin Length parameter feeds the Feature,
and every ToFace/Between semantic face producer Feature feeds the consuming Extrude. JSON loading
defers those consumers until referenced producer Features exist; missing or cyclic dependencies
fail closed.
Block 60 consumes that ordering in Geometry. In-place Join/Cut/Intersect execution identifies the
preceding target-Body producer among the Feature dependencies, so incremental re-execution starts
from the same predecessor rather than applying the operation repeatedly to its own result.
Block 61 applies the same graph contract to Revolve/RevolveCut: profile Sketch and typed axis source
feed the Revolve Feature, target/prior Body producers precede modifying operations, and the Feature
produces `body:<effective-result-body>`. Source changes therefore invalidate rotational intent and
its Body result before Block-62 Geometry consumes the plan. The executor resolves the ordered
profile, typed axis, semantic producer, and prior target-Body dependencies and publishes the
revolved Body result only after successful OCCT construction.
Block 63 adds each ordered Pattern Feature/Body source, direction/axis source, Count/Length
parameter, and target/prior Body producer as an input to the Pattern node. The Pattern produces its
effective Body node. An in-place Body source is replaced by the preceding Body producer edge so
the authored dependency remains acyclic and incremental execution cannot consume its own result.
Block 64 consumes that ordering in Geometry. Feature/Body sources resolve before deterministic
instance generation; in-place operations read the preceding producer shape, and only the completed
Linear Pattern result replaces the Feature- and Body-scoped cache entries.
Block 65 consumes the same graph ordering for Circular Pattern execution. Typed axis origin and
direction resolve after their semantic producers; deterministic rotations and the atomic Body
operation complete before the Circular Pattern replaces Feature- and Body-scoped cache entries.
Block 66 adds every ordered Mirror Feature/Body source and typed plane source as dependencies of
the Mirror node, which produces its effective Body. Modifying Mirrors advance the prior producer;
an in-place Body source is replaced by that producer edge to prevent a Body self-cycle.
Block 68 adds every ordered semantic edge producer and dimensional parameter as a dependency of
its Fillet/Chamfer node. The preceding target-Body producer feeds the treatment, and the treatment
becomes the next producer of that same Body, preserving acyclic in-place feature history.
Block 71 applies the same in-place history contract to ShellFeature. Every ordered semantic
removed-face producer and the positive Length thickness parameter feed the Shell node; the prior
target-Body producer precedes it, and Shell becomes the new producer of that Body. Thickness and
upstream topology changes therefore invalidate Shell and all later Body consumers.
Block 73 applies it to DraftFeature. Every ordered semantic face producer, pull-direction source,
neutral-plane source, and signed Angle parameter feeds the Draft node; the preceding target-Body
producer feeds Draft, and Draft becomes the next producer of that Body. Reference or angle changes
therefore invalidate Draft and every later Body consumer without introducing a Body self-cycle.
Block 75 adds one node per `Sketch3D`. Every distinct Length parameter used by a point coordinate
feeds that node, so coordinate changes invalidate the spatial sketch and future curve/feature
consumers. Removing an unconsumed 3D sketch removes its node and incoming parameter edges.
Block 76 additionally feeds that node from referenced ConstructionPoints and planar Sketches, from
typed Helix axes, and from the Helix radius, pitch, and turn parameters. Missing or wrong-unit
sources fail before ownership or graph state changes.
