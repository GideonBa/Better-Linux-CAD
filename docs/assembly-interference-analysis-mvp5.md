# Posed Assembly Interference and Clearance Analysis MVP-5

Status: implemented the first read-only posed assembly interference and clearance seeds over the canonical flattened visible-active leaf boundary. Block 29 now freezes rooted exchange/component occurrence identity; Block 30 is the next richer contact/swept-motion analysis step.

## Shared posed-geometry boundary

Flattened posed STEP export and interference/clearance analysis consume:

```text
src/geometry/assembly_posed_leaf_shapes.hpp
src/geometry/assembly_posed_leaf_shapes.cpp
```

`AssemblyPosedLeafShapeBuilder`:

```text
validates Project hierarchy
-> reuses AssemblyLeafOccurrenceResolver
-> collects referenced PartDocumentId values
-> AssemblyPartShapeDefinitionBuilder
-> one recompute + one private ShapeCache per unique part
-> one shared unposed part shape definition
-> poses every leaf through exact transforms_inner_to_outer
```

Block 29 extracts the shared part-definition builder and OCCT rigid-transform conversion. Posed export and analysis therefore still agree on leaf selection, unique part recompute, and exact X-then-Y-then-Z-then-translate transform semantics.

No second hierarchy traversal or visibility/suppression policy exists in analysis.

## Current interference contract

`AssemblyInterferenceAnalyzer::analyze(const Project&, options)`:

- current leaf identity is assembly id + subassembly path + component id through `AssemblyLeafIdentity` and its deterministic occurrence key; never an OCCT topology id;
- unordered leaf pairs are derived in deterministic lexicographic occurrence-key order and each pair is evaluated once;
- overlap is derived through OCCT `BRepAlgoAPI_Common` plus `BRepGProp::VolumeProperties`;
- interference is reported only for finite positive common solid volume above `minimum_overlap_volume_mm3`;
- default overlap threshold is `1.0e-6 mm^3` and must remain finite/positive;
- face/edge/point contact is not classified as interference;
- records carry ordered leaf identities and overlap volume in `mm^3`;
- the summary carries leaf count, evaluated pair count, and recomputed part count;
- hierarchy, part recompute, shape posing, boolean, non-finite volume, and invalid-option failures are fail-closed;
- posed shapes, candidates, common shapes, and records are derived/unpersisted.

## Current clearance contract

`AssemblyClearanceAnalyzer::analyze(const Project&, options)` reuses the same posed-leaf boundary, pair order, identity contract, and positive-volume interference boundary.

It adds:

- interfering pairs are reported separately and never as clearance records;
- every remaining pair derives minimum distance in `mm` through `BRepExtrema_DistShapeShape`;
- pairs below finite positive `clearance_threshold_mm` become clearance violations;
- default clearance threshold is `1.0 mm`;
- exact touching without positive overlap is currently a zero-distance clearance violation;
- extrema failures, non-finite distances, and invalid thresholds fail closed;
- all products remain derived/unpersisted.

## Current proofs

`tests/geometry/assembly_interference_analyzer_tests.cpp` covers:

- disjoint leaves;
- exact face contact with zero common volume;
- exact positive-volume overlap values;
- deterministic repeated/insertion-order-independent results;
- suppression filtering;
- repeated nested child occurrences retaining exact subassembly paths;
- one recomputed shared part definition for repeated occurrences;
- tolerance validation.

Clearance coverage proves above-threshold pairs, exact gap distances, zero-distance touching, interference exclusion, deterministic ordering, and threshold validation.

Flattened/nested STEP tests and Block-29 structured STEP tests independently prove that the same visible-active part occurrences and transform semantics remain geometrically consistent across export consumers.

## Block-29 identity follow-up

`AssemblyExchangeGraph` now freezes a clearer rooted component occurrence identity:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

The containing assembly occurrence path is itself the exact rooted `SubassemblyInstanceId` sequence.

Block 30 should use this typed rooted occurrence identity for new contact/sweep result records instead of extending the current slash-joined `AssemblyLeafIdentity::occurrence_key` as long-term analysis authority.

This is an analysis identity migration boundary only. Block 29 does not change current interference/clearance API semantics.

## Next technical step: Block 30

Freeze and implement richer static contact classification plus a bounded deterministic sampled Revolute sweep.

The first static classification should explicitly define:

```text
Separated
Touching
Interfering
```

Requirements:

- exact rooted component occurrence pair identity;
- deterministic unordered pair order;
- explicit validated touching/separation tolerance;
- preserve positive-volume overlap as `Interfering`;
- distinguish zero/near-zero minimum distance as `Touching` under the frozen tolerance;
- retain derived/unpersisted results;
- source Project immutability.

A first swept-motion seed should accept one selected supported Revolute joint plus a bounded coordinate interval and explicit/validated sampling resolution.

It must reuse current motion solve/application and posed-leaf geometry boundaries on Project copies. It must report sampled coordinates honestly and must not claim continuous collision detection.

Still deferred:

- broad-phase acceleration unless required by measured Block-30 test/runtime pressure;
- penetration direction/depth;
- collision response;
- contact forces or rigid-body dynamics;
- arbitrary continuous collision detection;
- persistent collision/contact state.
