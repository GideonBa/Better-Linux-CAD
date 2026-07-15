# Integrated Part Construction MVP-6 Acceptance

Status: implemented in Block 94. Part Construction MVP-6 is complete.

## Boundary

Block 94 adds no feature family and changes no persistent model contract. It turns the implemented
Blocks 48–94 contracts into one executable headless acceptance surface under:

```text
[integration][part-construction-mvp]
```

The suite combines selected existing Core and Geometry fixtures with a dedicated end-to-end
roundtrip/recompute/export fixture. This avoids duplicating feature algorithms while proving that
their shared Body, dependency, persistence, semantic-reference, cache, and exchange authorities
compose correctly.

## Acceptance matrix

### Mechanical Part Construction

Representative Geometry fixtures cover independent Body-producing Extrude and Revolve, associative
BodyTransform, Add/Subtract/Intersect BodyBoolean execution, semantic Fillet and Chamfer edges,
Shell face removal, signed Draft, deterministic Linear and Circular Pattern ordering, Mirror across
Part planes, and transactional fail-closed semantic recovery.

### Routed and swept construction

The focused suite executes 3D Sketch-backed spline and helix Sweep paths, SweepCut and SweepSurface,
and path-following subtractive Extrude. The paired Core fixtures roundtrip every 3D Sketch entity,
PathCurve source family, Sweep kind, orientation rule, twist, and optional guide identity.

### Loft and Surface construction

Representative fixtures execute guide-controlled G1 Loft, reject unsupported G2 transactionally,
build spatial Boundary/Fill surfaces, stitch Surface Bodies, and convert a stitched closed shell to
a Solid Body. Core fixtures roundtrip Loft and all first Surface-feature intents.

## Dedicated end-to-end proof

`part_construction_mvp_acceptance_tests.cpp` authors two Bodies deliberately in reverse Body-id
order and proves:

- canonical full `PartDocument` JSON roundtrip;
- no `TopoDS`, XDE, or STEP identity in Core JSON;
- full deterministic recompute and Body inspection ordered by `BodyId`;
- a parameter edit invalidates and recomputes only the affected Body;
- the unaffected Body retains its computed volume;
- deterministic multi-body STEP definition order and file production;
- Geometry recompute and export leave persistent Part intent unchanged.

The selected fail-closed fixtures additionally cover lost semantic edges/planes and unsupported
continuity without partially publishing replacement geometry.

## Focused proof

Both layers participate in the acceptance tag:

```bash
./build/blcad_core_tests "[integration][part-construction-mvp]"
./build/blcad_geometry_tests "[integration][part-construction-mvp]"
```

The current suite contains 17 Core cases and 22 Geometry cases, including the dedicated integrated
workflow. It runs headlessly and does not depend on a GUI.

## Persistence and authority result

Block 94 adds no JSON fields. The acceptance suite confirms that Blocks 48–94 remain the only
owners of Part intent and derived OCCT/XDE products remain outside Core persistence. Deterministic
orders use their declared stable BLCAD identities rather than insertion, traversal, memory, or
kernel entity order.

## Handoff

Part Construction MVP-6 is complete through Block 94 and GUI Feature Validation MVP-7 is accepted
through Block 105. Interactive Sketcher MVP-8 begins in Block 106; Interactive Modeling MVP-9
follows after its acceptance in Block 122, and STEP Import MVP-10 in Block 132.
