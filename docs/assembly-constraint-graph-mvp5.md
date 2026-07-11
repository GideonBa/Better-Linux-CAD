# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only active-constraint connectivity graph. Exact graph connected groups are consumed by the rigid-body solver and the read-only solve/DOF diagnostics layer.

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

## Rules

- every component occurrence is a node, including isolated occurrences
- every active constraint is one distinct edge
- inactive constraints create no edge
- multiple active constraints between the same component pair remain legal multi-edges
- each edge preserves its `AssemblyConstraintId`
- edge endpoints preserve target-A/target-B component identity from persistent constraint intent
- the graph does not copy semantic target geometry
- active endpoints are defensively revalidated

## Deterministic ordering

```text
nodes                 -> lexicographic ComponentInstanceId
edges                  -> lexicographic AssemblyConstraintId
adjacent constraints   -> graph edge order
members within group   -> lexicographic ComponentInstanceId
groups                 -> first member id order
```

This ordering is part of the downstream numeric contract.

`AssemblyRigidBodySolver` requires caller group input to exactly match one `connected_components()` result, including order.

`AssemblySolveDiagnosticsAnalyzer` analyzes the same exact group and uses the same graph constraint order through the shared numeric-system path.

Neither consumer silently sorts, merges, splits, or expands a caller-provided group.

## Connected groups as numeric partitions

```text
AssemblyConstraintGraph::connected_components()
  -> exact group
  -> AssemblyRigidBodySolver
  -> grounded/fixed and free/variable partition
  -> shared residual/Jacobian system
  -> AssemblySolveResult
  -> AssemblySolveDiagnosticsAnalyzer
  -> local rank and remaining DOF
```

Independent groups can be solved and diagnosed independently.

The graph itself does not decide fixed/variable participation.

Current solver policy is documented in `docs/assembly-rigid-body-solver-mvp5.md`:

- grounded components are fixed
- at least one grounded component is required
- multiple grounded components are allowed
- selected groups containing suppressed components are rejected
- visibility does not affect solve participation

Those policies do not alter graph connectivity.

## Read-only and persistence boundary

Graph construction and queries do not:

- mutate component transforms or state
- change constraints
- resolve target geometry
- evaluate assembly-space geometry
- construct residuals
- build numeric Jacobians
- solve transforms
- apply proposals
- compute rank or DOF
- mutate part intent

The graph is regenerated from persistent component and active-constraint records.

No graph node/edge/adjacency/group cache is serialized.

## Downstream implemented layers

Implemented separately from the graph:

- generated planar face target resolution
- explicit rigid-transform evaluation
- active planar Mate/Distance residual construction
- shared deterministic residual/Jacobian construction
- deterministic rigid-body solving
- explicit fresh-converged-result transform application
- read-only Jacobian-rank and remaining-DOF diagnostics

`AssemblyConstraintEquationBuilder` owns residual semantics.

The private assembly numeric-system path owns residual flattening, variable order, and finite-difference Jacobian construction.

`AssemblyRigidBodySolver` owns solve participation and optimization.

`AssemblySolveResultApplier` owns the explicit mutation boundary.

`AssemblySolveDiagnosticsAnalyzer` owns local rank and remaining-DOF classification.

None changes graph nodes or edges.

## Tests

Core graph tests cover node/edge identity, active filtering, multi-edges, deterministic ordering, adjacency, isolated nodes, connected groups, and unchanged assembly intent.

Targeted command:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Solver tests verify exact group validation and deterministic constraint ordering:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-solver]"
```

Diagnostics tests verify deterministic group/constraint ordering is preserved through local rank analysis:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-diagnostics]"
```

## Deliberate graph-layer limitations

The graph remains independent from semantic geometry resolution, transform evaluation, residual semantics, solver participation, Jacobian construction, optimization, transform application, and DOF analysis.

Those capabilities are implemented by separate downstream layers and are not graph responsibilities.

The graph also does not implement semantic axis references, richer constraint families, joints, collision, subassemblies, or assembly export.

## Current downstream boundary

Jacobian-rank and remaining-DOF diagnostics are implemented.

The next assembly block is a semantic generated-axis target family and read-only Concentric residual pipeline. Graph connectivity will continue to provide deterministic relationship partitions, but axis geometry and Concentric semantics must remain outside the graph.
