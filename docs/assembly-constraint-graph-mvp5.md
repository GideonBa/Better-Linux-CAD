# Assembly Constraint Graph MVP-5

Status: implemented deterministic read-only connectivity graph over persistent assembly constraint records.

## Goal

This block derives component connectivity from the solver-independent Mate, Concentric, and Distance records in `AssemblyDocument`.

The graph answers which component occurrences are connected by active relationship intent. It does not resolve semantic geometry, evaluate transforms, construct residuals, solve placement, or compute remaining degrees of freedom.

## API

The core API lives in `include/blcad/core/assembly_constraint_graph.hpp`:

```text
AssemblyConstraintGraphEdge
  constraint
  component_a
  component_b

AssemblyConstraintGraph
  build(AssemblyDocument)
  nodes()
  edges()
  node_count()
  edge_count()
  adjacent_constraints(ComponentInstanceId)
  connected_components()
```

The graph is derived read-only data. It is not owned by `AssemblyDocument` and is not serialized.

## Nodes

Every `ComponentInstance` becomes one graph node, including components with no active constraints.

Node identity uses `ComponentInstanceId`.

Nodes are stored in lexicographic id order so results do not depend on insertion order.

An isolated component therefore appears as a one-node connected group.

## Edges

Every active `AssemblyConstraint` becomes one distinct `AssemblyConstraintGraphEdge`.

An edge stores:

```text
constraint   AssemblyConstraintId
component_a ComponentInstanceId
component_b ComponentInstanceId
```

The edge preserves target-A and target-B component endpoints but does not copy semantic target geometry.

Inactive constraints create no edge.

Several active constraints may connect the same component pair. They remain distinct legal multi-edges because different Mate or Distance relationships may coexist between the same occurrences.

Edges are ordered lexicographically by `AssemblyConstraintId`.

## Defensive graph validation

`AssemblyConstraintGraph::build` checks both component endpoints of every active edge against the source `AssemblyDocument`.

Normal constraint insertion and JSON loading already reject dangling component targets. The graph repeats the check so graph construction cannot silently accept invalid model state if another construction path appears later.

## Deterministic adjacency

```text
adjacent_constraints(component)
```

returns active constraint ids incident to the requested component.

Rules:

- the component must be a graph node
- inactive constraints never appear
- multi-edges remain separate
- returned ids follow lexicographic `AssemblyConstraintId` order
- an isolated component returns an empty successful result
- an empty or unknown component id returns a validation error

## Connected components

```text
connected_components()
```

returns independent groups over active relationship edges.

Determinism guarantees:

- component ids inside each group are lexicographically ordered
- groups are ordered by their lexicographically first component id
- inactive constraints cannot join groups
- repeated active constraints do not duplicate nodes
- isolated nodes produce one-element groups

The graph does not claim that a connected group is fully constrained or geometrically consistent.

## Downstream solver partition role

The implemented planar residual builder operates on individual active constraints and does not modify graph connectivity.

The next rigid-body solver seed should use deterministic connected groups as natural solve partitions:

```text
AssemblyConstraintGraph::connected_components()
  -> one component-id group
  -> collect active constraints whose endpoints are inside the group
  -> build supported Mate/Distance residual descriptors
  -> solve variable transforms for that group
```

Constraint collection and residual flattening must remain deterministic, preferably by `AssemblyConstraintId`, so solver output does not depend on insertion order.

The graph itself must not absorb solver state, residual values, or transform proposals.

## Read-only boundary

Building or querying the graph does not modify:

- `RigidTransform`
- component grounding state
- visibility or suppression state
- constraint active/inactive state
- semantic target tokens
- referenced part model intent
- assembly/project JSON

The graph is distinct from the part `DependencyGraph` and from future nonlinear solve state.

## Headless inspection

`blcad_inspect_project_components` builds the graph after project structure validation and prints a compact summary:

```text
Constraint graph has <nodes> node(s), <active edges> active edge(s), and <groups> connected group(s)
graph_group[0] components=component.a,component.b
```

The group order uses the same deterministic API ordering.

## Persistence

No JSON field is added for graph data.

The graph is fully regenerated from:

```text
component_instances[]
assembly_constraints[]
```

Persisting nodes, edges, adjacency, or connected groups would duplicate derived state and could drift from relationship intent.

## Downstream implemented layers

The following are now implemented separately from the graph:

- generated-face semantic target resolution
- explicit rigid-transform evaluation
- active planar Mate/Distance residual construction

`AssemblyConstraintEquationBuilder` uses constraint records directly for target identity and residual semantics. It does not change graph nodes or edges.

## Tests

The focused core tests cover:

- every component becoming a node
- lexicographically deterministic node order
- active constraints becoming edges
- inactive constraints being ignored
- edge id and endpoint preservation
- lexicographically deterministic edge order
- legal multi-edges
- deterministic adjacency
- isolated-component adjacency
- empty and unknown adjacency queries
- deterministic connected groups
- isolated one-node groups
- inactive constraints not joining groups
- unchanged transforms and constraint state

Targeted test command:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

## Deliberate limitations

The graph remains independent from:

- semantic geometry resolution
- rigid-transform evaluation
- Mate/Distance residual construction
- rigid-body solving and transform mutation
- remaining-DOF analysis
- under/fully/overconstrained state analysis
- grounding enforcement
- suppression solver participation
- richer constraints and joints
- collision checks
- subassemblies
- assembly-level export

The first three items are implemented as separate downstream geometry layers.

## Current downstream boundary

The repository-wide next assembly block is a first rigid-body solver seed.

The solver should consume one deterministic graph connected group, define grounded/fixed and variable component participation, collect supported active Mate/Distance constraints in deterministic order, build residual descriptors, and return proposed transforms before any explicit application step.

Concentric remains deferred until semantic axis targets and Concentric residuals exist.
