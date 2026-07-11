# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only active-constraint connectivity graph. Exact connected groups are consumed by the shared Mate/Distance/Concentric solver and solve/DOF diagnostics. Insert records are also ordinary graph edges; their numeric integration is downstream.

## Goal

`AssemblyConstraintGraph` derives relationship connectivity from persistent `ComponentInstance` and `AssemblyConstraint` records.

It answers connectivity questions without resolving semantic geometry, evaluating transforms, constructing residuals, solving placement, or computing DOF.

## API

```text
AssemblyConstraintGraph
  build(AssemblyDocument)
  nodes()
  edges()
  node_count()
  edge_count()
  adjacent_constraints(component)
  connected_components()

AssemblyConstraintGraphEdge
  constraint
  component_a
  component_b
```

## Node and edge rules

Every component occurrence becomes a graph node, including isolated components.

Every active assembly constraint becomes one distinct edge.

Inactive constraints create no graph edge.

Multiple active constraints between the same component pair remain legal multi-edges because every edge preserves its `AssemblyConstraintId`.

Edge endpoints are target-A and target-B component ids from persistent intent. The graph does not copy semantic target geometry.

Graph construction defensively revalidates active endpoints.

## Deterministic ordering

Nodes are ordered lexicographically by `ComponentInstanceId`.

Edges are ordered lexicographically by `AssemblyConstraintId`.

`adjacent_constraints` returns incident constraint ids in graph order.

Each connected group contains lexicographically ordered component ids. Groups are ordered by their first component id.

This ordering is part of solver and diagnostics contracts.

## Connected groups as solve partitions

```text
AssemblyConstraintGraph::connected_components
  -> exact connected group
  -> AssemblyRigidBodySolver
  -> fixed/variable partition
  -> graph-ordered shared residual vector
  -> proposed transforms
  -> optional explicit application
  -> AssemblySolveDiagnosticsAnalyzer
```

The solver requires caller input to exactly match one graph connected component, including order.

Independent graph groups can be solved independently.

The graph does not decide fixed/variable participation. Grounding, suppression, and visibility are solver policy.

Current solver policy:

- every grounded component is fixed
- at least one grounded component is required per selected group
- multiple grounded components are allowed
- selected groups containing suppressed components are rejected
- visibility has no solve participation effect

## Constraint-family independence

Mate, Distance, Concentric, and Insert all use the same graph edge form because connectivity is independent from geometry family.

Concentric downstream path:

```text
graph edge / persistent Concentric record
  -> resolve_axis
  -> evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> Concentric residual
  -> shared numeric system
  -> solver / diagnostics
```

Insert downstream path:

```text
graph edge / persistent Insert record
  -> resolve_insert
  -> local primary axis + seating plane
  -> evaluate_axis + evaluate_plane
  -> AssemblyInsertConstraintEquationBuilder
  -> Insert residual
```

Insert currently stops before the shared numeric system. This does not require a special graph node or edge type.

The next numeric integration will consume the same graph-ordered Insert constraint id and route it to the dedicated Insert builder.

## Target A/B order

Graph edges preserve constraint identity and endpoint component ids but do not own geometry or residual semantics.

Target A/B order remains in the persistent `AssemblyConstraint` record.

Current order-sensitive conventions include:

```text
Distance: dot(oB - oA, nA)
Concentric: cross(oB - oA, dA)
Insert seating: dot(sB - sA, nA)
```

Downstream builders must read the original record and must not reconstruct target order from sorted graph endpoints.

## Read-only and persistence boundary

Graph construction and queries do not:

- mutate component transforms
- change grounding, visibility, or suppression
- change constraint state
- resolve planar, axis, or seat targets
- evaluate assembly-space geometry
- construct Mate, Distance, Concentric, or Insert residuals
- flatten numeric residual vectors
- build Jacobians
- run a solver
- apply transform proposals
- compute DOF
- mutate part intent

The graph is regenerated from persistent component and active-constraint records and is not serialized.

No graph node, edge, adjacency, or connected-group cache field exists in assembly/project JSON.

## Downstream implemented layers

Implemented separately from the graph:

- generated-face target resolution
- generated-axis target resolution
- generated-seat/composite Insert endpoint resolution
- rigid-transform plane/axis evaluation
- Mate/Distance residual construction
- Concentric residual construction
- Insert residual construction
- shared Mate/Distance/Concentric numeric residual/Jacobian system
- deterministic rigid-body solving
- explicit fresh-converged-result application
- local shared Jacobian-rank/remaining-DOF diagnostics
- direct Insert residual rank-five seed

None changes graph nodes or edges.

## Tests

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
./build/dev/blcad_core_tests "[core][assembly-insert]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-insert]"
```

Graph tests cover every component as a node, active/inactive filtering, lexicographic ordering, multi-edges, deterministic adjacency, isolated nodes, deterministic connected groups, and unchanged model state.

Insert core/geometry tests separately prove that Insert uses ordinary persistent constraint intent and downstream composite geometry/residual interpretation without graph mutation.

## Deliberate graph-layer limitations

The graph remains independent from semantic geometry, transform evaluation, residual semantics, numeric Jacobians, rigid-body optimization, transform application, DOF analysis, richer constraints/joints, collision, subassemblies, and assembly export.

Those remain downstream responsibilities.

## Current downstream boundary

Mate, Distance, and Concentric are integrated through the graph-ordered shared numeric/solver/DOF path.

Insert intent and read-only composite residual semantics are implemented without changing graph connectivity. The next step is to add Insert to the existing graph-ordered shared numeric path; graph behavior itself should remain unchanged.
