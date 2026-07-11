# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only active-constraint connectivity graph. Exact graph connected groups are consumed by the current planar rigid-body solver and read-only solve/DOF diagnostics.

Semantic generated-axis resolution and read-only Concentric residual construction are also implemented downstream from graph records. Concentric numeric-system/solver integration is next.

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

Inactive constraints do not create graph edges.

Multiple active constraints between the same component pair remain legal distinct multi-edges because each edge preserves its `AssemblyConstraintId`.

Edge endpoints are target-A and target-B component ids from persistent constraint intent. The graph does not copy semantic target geometry.

Graph construction defensively revalidates active edge endpoints.

## Deterministic ordering

Nodes are ordered lexicographically by `ComponentInstanceId`.

Edges are ordered lexicographically by `AssemblyConstraintId`.

`adjacent_constraints` returns incident constraint ids in graph edge order.

Each connected group contains lexicographically ordered component ids. Groups are ordered by the first component id.

This ordering is part of downstream solver and diagnostic contracts.

## Connected groups as solve partitions

Current planar path:

```text
AssemblyConstraintGraph::connected_components
  -> exact connected group
  -> AssemblyRigidBodySolver
  -> fixed/variable partition
  -> deterministic planar residual ordering
  -> proposed transforms
  -> optional explicit application
  -> AssemblySolveDiagnosticsAnalyzer
```

The current solver requires the caller-provided group to exactly match one graph connected component, including order.

Independent graph groups can be solved independently.

The graph itself does not decide which components are fixed or variable. Grounding, suppression, and visibility participation are solver policies.

Current solver policy:

- grounded components are fixed
- at least one grounded component is required per selected group
- multiple grounded components are allowed
- selected groups containing suppressed components are rejected
- visibility has no solve participation effect

## Concentric downstream path

A Concentric constraint is already represented as the same graph edge form as Mate or Distance because connectivity is independent from geometry family.

The implemented read-only Concentric path is:

```text
AssemblyConstraintGraph edge / persistent Concentric record
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
```

The graph does not need a new node or edge type for semantic axes.

The next block will teach the shared numeric system and solver to consume Concentric residuals from graph-ordered constraint ids.

## Target A/B order

The graph edge preserves constraint identity and endpoint component ids but does not own target geometry or residual semantics.

Target A/B order remains in the persistent `AssemblyConstraint` record.

Planar Distance and Concentric raw offset residuals have target-order-dependent conventions, so downstream builders must read the original constraint record rather than reconstruct target order from graph endpoint sorting.

## Read-only and persistence boundary

Graph construction and queries do not:

- mutate component transforms
- change grounding, visibility, or suppression
- change constraint state
- resolve planar or axis targets
- evaluate assembly-space geometry
- construct Mate, Distance, or Concentric residuals
- build numeric Jacobians
- run a solver
- apply transform proposals
- compute DOF
- mutate part model intent

The graph is regenerated from persistent component and active-constraint records and is not serialized.

No graph node, edge, adjacency, or connected-group cache field exists in assembly/project JSON.

## Downstream implemented layers

Implemented separately from the graph:

- generated-face planar target resolution
- generated-axis target resolution
- rigid-transform plane/axis evaluation
- planar Mate/Distance residual construction
- read-only Concentric residual construction
- shared planar numeric residual/Jacobian system
- deterministic planar rigid-body solving
- explicit fresh-converged-result application
- local planar Jacobian-rank and remaining-DOF diagnostics

None changes graph nodes or edges.

## Tests

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Graph tests cover every component as a node, active/inactive filtering, lexicographic ordering, multi-edges, deterministic adjacency, isolated nodes, deterministic connected groups, and unchanged model state.

Solver/diagnostics tests separately verify exact group input and graph-ordered numeric consumption.

Concentric tests verify axis/residual construction from persistent Concentric records but do not yet claim solver consumption.

## Deliberate graph-layer limitations

The graph remains independent from:

- semantic geometry resolution
- transform evaluation
- residual semantics
- fixed/variable participation
- numeric Jacobian construction
- rigid-body optimization
- transform application
- DOF analysis
- richer constraints and joints
- collision, subassemblies, and assembly export

These are downstream layer responsibilities.

## Current downstream boundary

Generated-axis and Concentric residual layers are implemented without changing graph connectivity.

The next repository-wide assembly step is Concentric integration into the shared graph-ordered numeric residual/Jacobian system, rigid-body solver, and DOF diagnostics.
