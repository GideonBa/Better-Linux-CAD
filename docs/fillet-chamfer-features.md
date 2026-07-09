# Fillet and Chamfer Features

Status: target architecture. Not implemented yet; listed among core objects in the architecture summary.

Fillets (rounds) and chamfers are basic parametric features, not late corrections to a finished BRep body. A fillet replaces a sharp edge with a radius; a chamfer replaces it with a bevel. Both are stored as first-class features in the feature tree so they can be edited, suppressed, recomputed, and parameter-driven.

```text
Features:
  Sketch_BaseRectangle
  Extrude_BaseBody
  HoleWizard_M6_ClearancePattern
  Chamfer_OuterEdges
  Fillet_InternalCorners
```

Edges must be stored semantically (`docs/semantic-references.md`), for example `BasePlate.outer_top_edges`, never as `Edge_17`.

## Data model

```text
FilletFeature
  id
  name
  target_body
  target_edge_references
  fillet_type            # constant_radius (MVP), variable, multi, tangent, full, face
  radius
  continuity
  propagation_mode
  generated_shape        # cache

ChamferFeature
  id
  name
  target_body
  target_edge_references
  chamfer_type           # equal_distance (MVP), two_distances, distance_angle
  distance_1
  distance_2
  angle
  propagation_mode
  generated_shape        # cache
```

Examples:

```text
FilletFeature
  target_edge_references = [HousingBase.outer_vertical_edges]
  fillet_type = constant_radius
  radius = 2 mm

ChamferFeature
  target_edge_references = [Shaft.left_end_outer_edge]
  chamfer_type = distance_angle
  distance_1 = 1.5 mm
  angle = 45 deg
```

## Propagation modes

| Mode              | Behavior |
|-------------------|----------|
| Single edge       | only the explicitly selected edge. |
| Tangent connected | tangentially continuous edges are included. |
| Edge chain        | a topologically connected edge chain. |
| Feature edges     | all edges of a given feature. |
| Semantic group    | a named edge group such as `outer_top_edges`. |

## Parameters and expressions

Radius and distance can be parameters or expressions (`docs/parameter-model.md`), e.g. `fillet_radius = wall_thickness / 4`. Changing the wall thickness then changes the fillet.

## Dependency-graph integration

A fillet/chamfer depends on the target body, the referenced edges, and its parameters.

```text
Extrude_BaseBody -> HoleWizard_M6_ClearancePattern -> Chamfer_OuterEdges -> Fillet_InternalCorners -> FinalShape
```

When an earlier feature changes, the referenced edges are re-resolved. If a radius no longer fits, the system must not fail silently; it emits a clear error, e.g. `radius = 8 mm is too large for selected edge group; maximum estimated valid radius = 3.5 mm`.

## MVP scope

- constant-radius fillet on explicitly selected edges.
- equal-distance chamfer.
- stored as own features in the feature tree.
- parameter binding for radius and distance.
- recompute through the dependency graph.
- clear error on invalid geometry.

## Proposed implementation sequence

1. Add `FilletFeature` and `ChamferFeature` intent models with semantic edge references.
2. Wire them into `PartDocument` validation and dependency-graph nodes.
3. Add OCCT fillet/chamfer execution in the geometry layer over resolved edges.
4. Add JSON serialization and roundtrip tests.
5. Add recompute tests where an earlier dimension change re-resolves the edge group.
6. Add error reporting for over-large radius / failed kernel operation.

## Out of scope for the first version

- variable radius, face fillets, full rounds, complex edge chains.
- automatic feature-edge groups.
- repair mechanisms for lost edge references.
