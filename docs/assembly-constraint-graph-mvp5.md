# Assembly Constraint Graph MVP-5

Status: implemented read-only connectivity graph over persistent assembly constraint records.

## Goal

This block derives deterministic component connectivity from the solver-independent Mate, Concentric, and Distance relationship records in `AssemblyDocument`.

The graph answers which component occurrences are connected by active relationship intent. It does not resolve semantic target geometry, evaluate transforms, construct constraint equations, move components, or compute remaining degrees of freedom.

Downstream semantic target resolution and rigid-transform evaluation are now implemented as separate layers. Their existence does not change the graph boundary.

## API

The implemented core API lives in `include/blcad/core/assembly_constraint_graph.hpp`:

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

`AssemblyConstraintGraph` is derived read-only data. It is built from an existing `AssemblyDocument`; it is not owned by the assembly and is not serialized.

## Nodes

Every current `ComponentInstance` becomes one graph node, including components that participate in no active constraint.

Node identity is the existing `ComponentInstanceId`. Nodes are stored in lexicographic id order so graph results do not depend on component insertion order.

An isolated component therefore still appears as a single-node connected component.

## Edges

Every active `AssemblyConstraint` becomes one distinct `AssemblyConstraintGraphEdge`.

An edge stores only:

```text
constraint   AssemblyConstraintId
component_a ComponentInstanceId
component_b ComponentInstanceId
```

The edge preserves the target-A and target-B component endpoints from the relationship record. Semantic reference tokens remain on the owning `AssemblyConstraint`; the graph does not copy or resolve face, edge, vertex, axis, or OCCT topology data.

Inactive constraints create no graph edge.

Several different active constraints may connect the same pair of components. These remain distinct legal multi-edges because Mate, Concentric, and Distance intent can coexist between the same occurrences.

Edges are ordered lexicographically by `AssemblyConstraintId`.

## Defensive graph validation

`AssemblyConstraintGraph::build` checks the two component endpoints of every active edge against the source `AssemblyDocument`.

The public `AssemblyDocument::add_constraint` and JSON load paths already reject dangling component targets. The graph repeats the endpoint check deliberately so graph construction does not silently accept invalid model state if another construction path is introduced later.

## Deterministic adjacency

```text
adjacent_constraints(component)
```

returns the active constraint ids incident to the requested component.

Rules:

- the component must be a graph node
- inactive constraints never appear
- multi-edges remain separate entries
- returned constraint ids follow lexicographic `AssemblyConstraintId` order
- an isolated component returns an empty successful result
- an empty or unknown component id returns a validation error

## Connected components

```text
connected_components()
```

returns independent connectivity groups over active constraint edges.

Determinism guarantees:

- component ids inside each group are lexicographically ordered
- groups are ordered by their lexicographically first component id
- inactive constraints cannot merge two groups
- repeated active constraints between one component pair do not duplicate nodes
- isolated component nodes produce one-element groups

These groups are intended to become natural solve partitions later. This block does not claim that a connected group is fully constrained, solvable, or geometrically consistent.

## Relationship to downstream geometry layers

The graph stores only connectivity. Downstream implemented geometry layers are:

```text
AssemblyConstraintTargetResolver
  AssemblyConstraintTarget
    -> ComponentLocalPlanarDescriptor + RigidTransform

AssemblyTransformEvaluator
  ComponentLocalPlanarDescriptor + RigidTransform
    -> AssemblySpacePlanarDescriptor
```

The graph does not duplicate either result.

A future equation builder can use graph groups to partition work while resolving and evaluating each constraint's own targets through those dedicated APIs.

## Read-only boundary

Building or querying the graph does not modify:

- `RigidTransform`
- component grounding state
- visibility or suppression state
- assembly constraint active/inactive state
- semantic reference tokens
- referenced `PartDocument` model intent
- assembly/project JSON

The graph is not the part `DependencyGraph` and is not a rigid-body solve graph. It only represents current assembly relationship connectivity.

## Headless inspection

`blcad_inspect_project_components` builds the graph after normal project structure validation and prints a compact summary:

```text
Constraint graph has <nodes> node(s), <active edges> active edge(s), and <groups> connected group(s)
graph_group[0] components=component.a,component.b
```

The group order uses the same deterministic ordering as the core API.

## Persistence

No JSON schema field is added for the graph.

The graph is fully regenerable from:

```text
component_instances[]
assembly_constraints[]
```

Persisting nodes, edges, adjacency, or connected groups would duplicate derivable data and could drift from model intent. Existing assembly and project schema markers therefore remain unchanged.

## Test coverage

The focused core tests cover:

- every component instance becoming a node
- lexicographically deterministic node order
- active constraints becoming edges
- inactive constraints being ignored
- edge id and endpoint preservation
- lexicographically deterministic edge order
- multiple constraints between one component pair remaining distinct
- deterministic adjacency
- isolated-component adjacency
- empty and unknown adjacency queries
- deterministic connected groups
- isolated nodes as one-element groups
- inactive constraints not joining groups
- graph construction and queries leaving transforms and constraint state unchanged

Targeted test command after a core build:

```bash
./build/dev/blcad_core_tests "[core][assembly-constraint-graph]"
```

Complete core workflow:

```bash
cmake --workflow --preset dev-build-test
```

## Deferred work from the graph layer

The graph itself remains intentionally independent from:

- semantic target geometry resolution
- rigid-transform evaluation
- Mate, Concentric, and Distance equation construction
- rigid-body solving and transform mutation
- remaining DOF computation
- underdefined, fully constrained, and overconstrained analysis
- enforced grounding
- suppression participation rules
- richer constraints, joints, collision checks, subassemblies, and assembly export

The first two items are now implemented as separate downstream geometry layers; they are not graph responsibilities.

## Next technical step

The repository-wide next assembly block is read-only planar Mate/Distance equation/residual construction.

A future builder should consume active constraint records, resolve each supported generated-face target through `AssemblyConstraintTargetResolver`, evaluate both frames through `AssemblyTransformEvaluator`, and construct documented deterministic residual data. It must not change graph connectivity, component transforms, or project model intent.
