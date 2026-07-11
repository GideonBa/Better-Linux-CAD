# Assembly Constraint Graph MVP-5

Status: implemented read-only connectivity graph over persistent assembly constraint records.

## Goal

This block derives deterministic component connectivity from the solver-independent Mate, Concentric, and Distance relationship records in `AssemblyDocument`.

The graph answers which component occurrences are connected by active relationship intent. It does not resolve semantic target geometry, evaluate constraint equations, move components, or compute remaining degrees of freedom.

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

`blcad_inspect_project_components` now builds the graph after normal project structure validation and prints a compact summary:

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

Persisting nodes, edges, adjacency, or connected groups would duplicate derivable cache data and could drift from model intent. Existing assembly and project schema markers therefore remain unchanged.

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

## Deferred work

The following remain outside this graph block:

- semantic constraint-target geometry resolution
- interpretation of semantic target tokens as faces, axes, edges, or vertices
- rigid-transform evaluation for resolved assembly references
- Mate, Concentric, and Distance equation construction
- rigid-body solving and transform mutation
- remaining DOF computation
- underdefined, fully constrained, and overconstrained state analysis
- enforced grounding
- solver participation rules for suppressed components
- Insert, Angle, Tangent, Flush, Coincident, and Lock constraints
- joints and limits
- collision/interference checks
- subassemblies
- assembly-level geometry instancing and STEP export

## Next technical step

Add a read-only semantic assembly target resolution seed before introducing constraint equations or a rigid-body solver.

The first resolver should start from `AssemblyConstraintTarget`, resolve its `ComponentInstanceId` to the referenced project-owned `PartDocument`, and support the currently implemented generated-face semantic reference family as component-local geometric descriptors. Unsupported target families, including not-yet-implemented semantic axes required for full Concentric solving, must fail explicitly.

The resolver must not change component transforms, constraint records, or part model intent. Constraint equation construction, assembly-space solve math, remaining DOF, and solved transform updates remain later blocks.
