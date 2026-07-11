# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only active-constraint connectivity graph. Exact graph connected groups are consumed by the shared Mate/Distance/Concentric rigid-body solver and solve/DOF diagnostics.

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

This ordering is part of the shared solver and diagnostic contracts.

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

The solver requires the caller-provided group to exactly match one graph connected component, including order.

Independent graph groups can be solved independently.

The graph itself does not decide which components are fixed or variable. Grounding, suppression, and visibility participation are solver policies.

Current solver policy:

- grounded components are fixed
- at least one grounded component is required per selected group
- multiple grounded components are allowed
- selected groups containing suppressed components are rejected
- visibility has no solve participation effect

## Constraint-family independence

Mate, Distance, and Concentric all use the same graph edge form because connectivity is independent from geometry family.

For a Concentric record, downstream flow is:

```text
AssemblyConstraintGraph edge / persistent Concentric record
  -> AssemblyConstraintTargetResolver::resolve_axis
  -> AssemblyTransformEvaluator::evaluate_axis
  -> AssemblyConcentricConstraintEquationBuilder
  -> ConcentricResidualDescriptor
  -> shared numeric residual/Jacobian system
  -> AssemblyRigidBodySolver
  -> AssemblySolveDiagnosticsAnalyzer
```

The graph does not need a special axis node or Concentric edge type.

The shared numeric system consumes graph-ordered constraint ids and selects the appropriate residual builder from the persistent constraint type.

## Target A/B order

The graph edge preserves constraint identity and endpoint component ids but does not own target geometry or residual semantics.

Target A/B order remains in the persistent `AssemblyConstraint` record.

Planar Distance and Concentric raw offset residuals have target-order-dependent conventions, so downstream builders read the original constraint rather than reconstructing target order from graph endpoint sorting.

## Read-only and persistence boundary

Graph construction and queries do not:

- mutate component transforms
- change grounding, visibility, or suppression
- change constraint state
- resolve planar or axis targets
- evaluate assembly-space geometry
- construct Mate, Distance, or Concentric residuals
- flatten numeric residual vectors
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
- Concentric residual construction
- shared Mate/Distance/Concentric numeric residual/Jacobian system
- deterministic rigid-body solving
- explicit fresh-converged-result application
- local Jacobian-rank and remaining-DOF diagnostics

None changes graph nodes or edges.

## Tests

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Graph tests cover every component as a node, active/inactive filtering, lexicographic ordering, multi-edges, deterministic adjacency, isolated nodes, deterministic connected groups, and unchanged model state.

Solver/diagnostics integration tests separately verify exact group input, graph-ordered mixed constraint consumption, residual dimension, and deterministic rank behavior.

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

Concentric residuals are integrated into the shared graph-ordered numeric, solver, and DOF path without changing graph connectivity semantics.

The next repository-wide assembly step is stable Insert intent and read-only composite Insert residual semantics. Insert will also remain an ordinary graph relationship edge; its geometry and residual interpretation belong downstream.
