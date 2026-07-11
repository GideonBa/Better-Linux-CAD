# Posed Assembly Interference Analysis MVP-5

Status: implemented the first read-only posed assembly interference-analysis seed over the flattened visible-active leaf occurrence boundary.

## Shared posed-geometry boundary

The posed STEP exporter and the interference analyzer now consume one internal geometry-layer builder:

```text
src/geometry/assembly_posed_leaf_shapes.hpp
src/geometry/assembly_posed_leaf_shapes.cpp
```

`AssemblyPosedLeafShapeBuilder` validates the project hierarchy, reuses `AssemblyLeafOccurrenceResolver` (no second traversal or visibility/suppression policy), recomputes each unique referenced leaf part exactly once into one derived `ShapeCache`, and poses every leaf shape through its exact `transforms_inner_to_outer` chain. Each authored `RigidTransform` level keeps X-then-Y-then-Z-then-translate semantics; the whole chain is composed into one rigid `gp_Trsf`, so each leaf occurrence costs exactly one OCCT shape transformation.

## Analysis contract

`AssemblyInterferenceAnalyzer::analyze(const Project&, options)`:

- leaf identity is assembly id + subassembly path + component id (`AssemblyLeafIdentity` with a stable `/`-joined occurrence key); never an OCCT topology id.
- unordered leaf pairs are derived in deterministic lexicographic occurrence-key order and each pair is evaluated exactly once; self-pairs cannot occur.
- overlap is derived through OCCT `BRepAlgoAPI_Common` plus `BRepGProp::VolumeProperties`.
- interference is reported only for a finite positive common solid volume above `minimum_overlap_volume_mm3` (finite, positive, default `1.0e-6`); face/edge/point contact is not interference.
- records carry the two ordered leaf identities and the overlap volume in `mm^3`; the analysis summary carries leaf count, evaluated pair count, and recomputed part count.
- fail-closed on invalid hierarchy, unresolved leaf parts, recompute failures, missing final shapes, transform failures, boolean failures, non-finite volumes, and invalid tolerances.
- all posed shapes, pair candidates, common shapes, and records are derived and unpersisted.

## Covered by tests

`tests/geometry/assembly_interference_analyzer_tests.cpp`:

- disjoint leaves: one evaluated pair, no records.
- exact face contact: zero common volume, no records.
- overlapping pair: one record with ordered identities and the exact `200 mm^3` common volume.
- repeated identical analyses and component insertion-order reversal produce equal results.
- suppressed leaves are excluded from leaves and pairs.
- repeated nested child occurrences: one overlapping and one distant occurrence of the same child assembly; the record preserves the subassembly path and part caches are reused (`recomputed_part_count == 1`).
- tolerance validation rejects zero, negative, and non-finite values.

The refactored exporter keeps every existing posed/nested export test green.

## Still deferred

- broad-phase acceleration, contact classification, minimum-distance/clearance analysis, penetration direction/depth, collision response, motion sweeping, and physics.
- persistent collision state of any kind.
