# Posed Assembly Interference, Clearance, and Contact Analysis MVP-5

Status: the original read-only interference/clearance APIs remain implemented compatibility queries. Block 30 now adds complete typed rooted contact classification and bounded sampled Revolute sweep analysis.

Canonical Block-30 contract: `docs/assembly-contact-swept-motion-mvp5.md`.

## Shared posed-geometry boundary

Flattened posed STEP export and all static assembly analysis consumers reuse:

```text
AssemblyPosedLeafShapeBuilder
```

The builder:

```text
validates Project hierarchy
-> reuses AssemblyLeafOccurrenceResolver
-> collects referenced PartDocumentId values
-> AssemblyPartShapeDefinitionBuilder
-> one recompute + one private ShapeCache per unique part
-> one shared unposed part shape definition
-> poses every leaf through exact transforms_inner_to_outer
```

No analysis API owns a second hierarchy traversal, visibility/suppression filter, part recompute path, or transform convention.

## Compatibility interference contract

`AssemblyInterferenceAnalyzer::analyze(const Project&, options)` remains unchanged:

- identity is the historical `AssemblyLeafIdentity` plus deterministic occurrence key;
- unordered leaf pairs are evaluated once in lexicographic occurrence-key order;
- overlap uses `BRepAlgoAPI_Common` plus `BRepGProp::VolumeProperties`;
- only finite common solid volume strictly above `minimum_overlap_volume_mm3` is reported;
- default overlap threshold is `1.0e-6 mm^3` and must be finite/positive;
- face/edge/point contact is not reported as interference;
- result shape remains a violation/interference-only list.

This API is retained for compatibility and is not silently migrated to a new result type.

## Compatibility clearance contract

`AssemblyClearanceAnalyzer::analyze(const Project&, options)` also remains unchanged:

- interfering pairs are reported separately;
- non-interfering pairs use `BRepExtrema_DistShapeShape`;
- pairs strictly below `clearance_threshold_mm` are clearance violations;
- default threshold is `1.0 mm`;
- exact touching is a zero-distance clearance violation;
- result shape remains a clearance-violation list plus interference list.

## Block-30 typed contact classification

`AssemblyContactAnalyzer` is the new complete pair-classification query.

Identity:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (exact rooted assembly occurrence path,
   local ComponentInstanceId)
```

Pair identity:

```text
AssemblyComponentOccurrencePairIdentity =
  canonical ordered pair of exact rooted component occurrences
```

Order is typed and deterministic:

```text
occurrence path, lexicographically
then ComponentInstanceId
```

No slash-joined occurrence key is the new contact identity authority.

The analyzer emits exactly one `AssemblyContactRecord` per visible-active unordered posed-leaf pair.

Classification order is:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
  -> Interfering

otherwise minimum_distance_mm <= touching_tolerance_mm
  -> Touching

otherwise
  -> Separated
```

Defaults:

```text
touching_tolerance_mm = 1.0e-6
minimum_overlap_volume_mm3 = 1.0e-6
```

The touching tolerance is finite/non-negative. The overlap tolerance is finite/positive.

For `Interfering`, `minimum_distance_mm` is absent. For `Touching` and `Separated`, the finite measured OCCT minimum distance is present.

`AssemblyContactAnalysis::records` is a complete classification set, not a violation list.

## Block-30 sampled Revolute sweep

`AssemblyRevoluteSweepAnalyzer` performs deterministic discrete sampling over one selected supported Revolute joint.

Supported joint scopes:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

The analyzer delegates to the existing corresponding motion solver and atomic applier.

Request:

```text
selected joint scope + AssemblyJointId
start AngleDeg
end AngleDeg
sample_count
```

Validation requires:

```text
active Revolute joint
interval endpoints within authored limits
2 <= sample_count <= 1001
```

Samples are inclusive and linear in requested joint-coordinate space. Caller direction is preserved.

Every sample starts from:

```text
Project sample_project = source_project
```

Then:

```text
existing motion solver
-> require convergence
-> existing atomic motion applier
-> AssemblyContactAnalyzer
```

No sample starts from the preceding sample. Source transforms and authored joint coordinates remain unchanged.

One sample stores:

```text
sample_index
coordinate_deg
applied_transform_count
complete AssemblyContactAnalysis
```

This is sampled motion analysis, not continuous collision detection. Contact/interference existing only between sample coordinates may be missed.

## Focused proofs

Compatibility analysis:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-interference]"
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-clearance]"
```

Typed contact classification:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
```

Sampled Revolute sweep:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
```

Block-30 coverage proves exact and near touching, positive-volume interference, separated minimum distance, repeated-child rooted pair identity, deterministic insertion-order-independent pair order, source immutability, inclusive ascending/descending sample coordinates, local and cross-hierarchy motion-path reuse, a sampled `Touching -> Interfering -> Separated` transition, and fail-closed sample context.

## Persistence boundary

Persist existing Project/Part/Assembly model intent only.

Derived/unpersisted:

```text
historical interference/clearance records
AssemblyComponentOccurrencePairIdentity values
AssemblyContactRecord values
AssemblyContactAnalysis values
sample Project copies
sample motion results/proposals
AssemblyRevoluteSweepSample values
AssemblyRevoluteSweepAnalysis values
OCCT common shapes and distance computations
```

## Current handoff

Block 30 is implemented.

The next technical step is Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed geometric target taxonomy and capability projection.

Broad-phase acceleration, penetration depth/direction, collision response, persistent contact state, adaptive/continuous collision detection, and rigid-body dynamics remain deferred.
