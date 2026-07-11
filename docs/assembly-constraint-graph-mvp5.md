# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only active-constraint connectivity graph. The first rigid-body solver now consumes exact graph connected groups as its solve partition boundary.

## Goal

`AssemblyConstraintGraph` derives assembly relationship connectivity from persistent `ComponentInstance` and `AssemblyConstraint` records.

It answers connectivity questions without resolving semantic geometry, evaluating transforms, constructing residuals, or solving placement.

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

Edge endpoints are the target-A and target-B component ids from persistent constraint intent. The graph does not copy semantic target geometry.

Graph construction defensively revalidates active edge endpoints.

## Deterministic ordering

Nodes are ordered lexicographically by `ComponentInstanceId`.

Edges are ordered lexicographically by `AssemblyConstraintId`.

`adjacent_constraints` therefore returns incident constraint ids in deterministic graph edge order.

Each connected group contains lexicographically ordered component ids. Groups are ordered by the first component id in each group.

This ordering is now part of the downstream solver contract.

`AssemblyRigidBodySolver` requires the caller-provided group to exactly match one `connected_components()` result, including order. The solver does not silently sort, merge, split, or expand a group.

## Connected groups as solve partitions

The current downstream path is:

```text
AssemblyConstraintGraph::connected_components()
  -> exact connected group
  -> AssemblyRigidBodySolver
  -> fixed/variable partition
  -> deterministic residual ordering
  -> proposed component transforms
```

Independent graph groups can be solved independently.

The graph itself does not decide which components are fixed or variable. Grounding, suppression, and visibility participation are solver policies.

The first solver currently:

- treats grounded components as fixed
- requires at least one grounded component per selected group
- allows multiple grounded components
- rejects selected groups containing suppressed components
- ignores visibility for solve participation

Those policies are documented in `docs/assembly-rigid-body-solver-mvp5.md` and deliberately do not change graph connectivity.

## Read-only and persistence boundary

Graph construction and queries do not:

- mutate component transforms
- change grounding, visibility, or suppression state
- change constraint state
- resolve target geometry
- evaluate assembly-space frames
- construct residuals
- run a solver
- apply proposed transforms
- mutate part model intent

The graph is regenerated from persistent component and active-constraint records and is not serialized.

No graph node, edge, adjacency, or connected-group cache field exists in assembly/project JSON.

## Downstream implemented layers

The following are implemented separately from the graph:

- generated-face semantic target resolution
- explicit rigid-transform evaluation
- active planar Mate/Distance residual construction
- deterministic rigid-body solving over exact connected groups
- explicit fresh-converged-result transform application

`AssemblyConstraintEquationBuilder` consumes constraint records for target/residual semantics.

`AssemblyRigidBodySolver` consumes graph groups and graph edge ordering, then evaluates supported residuals through the equation builder.

`AssemblySolveResultApplier` owns the explicit transform mutation boundary.

None changes graph nodes or edges.

## Tests

`tests/core/assembly_constraint_graph_tests.cpp` covers:

- every component becoming a node
- lexicographic node order
- active constraints becoming edges
- inactive constraints being ignored
- edge id and endpoint preservation
- lexicographic edge order
- legal multi-edges
- deterministic adjacency
- isolated-component adjacency
- empty and unknown adjacency queries
- deterministic connected groups
- isolated one-node groups
- inactive constraints not joining groups
- unchanged transforms and constraint state

Targeted command:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Solver-focused tests separately verify exact graph-group input validation and constraint-insertion-order-independent solve results:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

## Deliberate graph-layer limitations

The graph remains independent from:

- semantic geometry resolution
- transform evaluation
- Mate/Distance residual semantics
- fixed/variable solve participation
- numeric Jacobian construction
- rigid-body optimization
- transform application
- remaining-DOF analysis
- under/fully/overconstrained classification
- richer constraints and joints
- collision, subassemblies, and assembly export

The first seven items are implemented by separate downstream geometry/solver layers; they are not graph responsibilities.

## Current downstream boundary

The repository-wide next assembly block is read-only Jacobian-rank and remaining-degree-of-freedom diagnostics over the implemented solver ordering and numeric model.

Graph connectivity remains the independent-group partition input for that diagnostics layer.

Concentric remains deferred until semantic axis targets and Concentric residuals exist.
