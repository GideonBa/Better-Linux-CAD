# Rooted Assembly Contact Classification and Sampled Revolute Sweep MVP-5

Status: implemented as Block 30 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for typed rooted component-occurrence pair identity in posed contact analysis, static `Separated` / `Touching` / `Interfering` classification, and bounded deterministic sampled Revolute motion analysis.

## Scope

Implemented:

- exact rooted component occurrence pair identity;
- deterministic unordered pair ordering;
- complete static pair classification, not only violations;
- explicit touching distance tolerance;
- retained positive-volume interference tolerance;
- `Separated`, `Touching`, and `Interfering` semantics;
- minimum-distance reporting for every non-interfering pair;
- overlap-volume reporting for every pair;
- bounded inclusive sampled Revolute intervals;
- root-assembly-local Revolute sweep dispatch;
- Project-level cross-hierarchy Revolute sweep dispatch;
- one fresh Project copy per sample;
- existing motion solver and atomic applier reuse;
- existing posed-leaf/part-definition geometry reuse;
- sample-indexed fail-closed errors;
- source Project immutability.

Not implemented:

- continuous collision detection;
- adaptive subdivision;
- broad-phase acceleration;
- penetration direction or penetration depth;
- collision response;
- contact forces;
- friction;
- rigid-body dynamics;
- persistent contact or sweep records;
- richer joint families.

## Persistent authority versus derived products

Persistent authority remains existing BLCAD model intent:

```text
PartDocument model intent
AssemblyDocument model intent
ComponentInstance direct transforms/state
SubassemblyInstance boundary transforms/state
local constraints and Revolute joints
Project-level cross-hierarchy constraints and Revolute joints
```

Block 30 adds no JSON field and no persistent record.

Derived and unpersisted:

```text
posed leaf shapes
rooted component occurrence pair identities
contact classification records
common OCCT shapes
minimum distances
overlap volumes
sample Project copies
motion solve results
motion proposals
sample coordinates
sample contact analyses
sampled sweep analyses
```

A contact result is not model intent. A sweep result is not a motion-history authority.

## Rooted component occurrence pair identity

One component occurrence uses the Block-29 exchange identity:

```text
AssemblyExchangeComponentOccurrenceIdentity =
  (containing assembly occurrence path,
   local ComponentInstanceId)
```

One static contact pair uses:

```text
AssemblyComponentOccurrencePairIdentity =
  (occurrence_a,
   occurrence_b)
```

The pair is unordered geometrically but stored in one canonical order.

Occurrence order is:

```text
exact SubassemblyInstanceId path sequence, lexicographically
then local ComponentInstanceId
```

The pair always satisfies:

```text
occurrence_a < occurrence_b
```

Example:

```text
([subassembly.left],  component.child)
([subassembly.right], component.child)
```

remain two different contact occurrences even when both repeated child occurrences share one persisted child-document component transform authority.

The pair identity is never:

```text
AssemblyDocumentId only
ComponentTransformAuthority only
slash-joined occurrence key
OCCT topology id
TopoDS hash
XDE label
STEP entity id
```

## Static posed geometry boundary

`AssemblyContactAnalyzer` reuses:

```text
AssemblyPosedLeafShapeBuilder
```

Therefore it inherits:

```text
Project hierarchy validation
AssemblyLeafOccurrenceResolver visibility/suppression filtering
exact rooted occurrence paths
one recompute per unique referenced PartDocumentId
one private ShapeCache per unique part
shared unposed part definitions
exact authored transforms_inner_to_outer posing semantics
```

There is no second pose model and no second hierarchy traversal.

Every canonical visible-active posed leaf becomes one exact rooted component occurrence.

The analyzer sorts these typed identities directly and evaluates every unordered pair once.

For `n` component occurrences:

```text
evaluated_pair_count = n * (n - 1) / 2
```

No broad phase exists in Block 30.

## Contact classification semantics

Options:

```text
AssemblyContactAnalysisOptions
  touching_tolerance_mm = 1.0e-6
  minimum_overlap_volume_mm3 = 1.0e-6
```

Validation:

```text
touching_tolerance_mm
  -> finite
  -> >= 0

minimum_overlap_volume_mm3
  -> finite
  -> > 0
```

Each pair is classified in this exact order.

### 1. Common solid volume

Compute:

```text
common_shape = BRepAlgoAPI_Common(shape_a, shape_b)
overlap_volume_mm3 = BRepGProp::VolumeProperties(common_shape).Mass()
```

If:

```text
overlap_volume_mm3 > minimum_overlap_volume_mm3
```

then:

```text
classification = Interfering
minimum_distance_mm = none
```

The strict `>` boundary intentionally preserves the existing positive-volume interference convention.

### 2. Minimum distance for non-interfering pairs

Otherwise compute:

```text
minimum_distance_mm = BRepExtrema_DistShapeShape(shape_a, shape_b).Value()
```

If:

```text
minimum_distance_mm <= touching_tolerance_mm
```

then:

```text
classification = Touching
```

The touching boundary is inclusive.

Otherwise:

```text
classification = Separated
```

### Summary

```text
Interfering:
  overlap_volume_mm3 > minimum_overlap_volume_mm3

Touching:
  overlap_volume_mm3 <= minimum_overlap_volume_mm3
  and minimum_distance_mm <= touching_tolerance_mm

Separated:
  overlap_volume_mm3 <= minimum_overlap_volume_mm3
  and minimum_distance_mm > touching_tolerance_mm
```

A tiny positive common volume at or below the configured overlap threshold is not automatically `Interfering`; the pair proceeds to the minimum-distance classification boundary.

## Contact result

`AssemblyContactAnalysis` stores:

```text
component_occurrence_count
evaluated_pair_count
recomputed_part_count
records[]
```

Every evaluated pair has exactly one `AssemblyContactRecord`.

One record stores:

```text
canonical rooted pair identity
classification
overlap_volume_mm3
optional minimum_distance_mm
```

For `Interfering`:

```text
minimum_distance_mm = none
```

For `Touching` and `Separated`:

```text
minimum_distance_mm = measured finite OCCT distance
```

`records[]` is a complete classification set. It is not a list containing only touching/contact violations.

The existing `AssemblyInterferenceAnalyzer` and `AssemblyClearanceAnalyzer` remain compatibility query contracts with their established result shapes. Block 30 does not silently change those APIs.

## Sampled Revolute sweep input

Public consumer:

```text
AssemblyRevoluteSweepAnalyzer
```

Request:

```text
AssemblyRevoluteSweepRequest
  joint
  start_coordinate
  end_coordinate
  sample_count
```

Joint reference:

```text
AssemblyRevoluteSweepJointReference
  scope
  AssemblyJointId
```

Implemented scopes:

```text
RootAssemblyLocal
ProjectCrossHierarchy
```

`RootAssemblyLocal` delegates to:

```text
AssemblyJointMotionSolver
AssemblyJointMotionResultApplier
```

`ProjectCrossHierarchy` delegates to:

```text
AssemblyCrossHierarchyJointMotionSolver
AssemblyCrossHierarchyJointMotionResultApplier
```

An unknown scope fails closed.

The selected joint must:

```text
exist in the selected scope
be active
be Revolute
```

Start/end quantities must use:

```text
AngleDeg
```

Both interval endpoints must lie within the selected joint's authored limits.

## Bounded deterministic sample sequence

Block 30 accepts:

```text
2 <= sample_count <= 1001
```

The sample sequence is inclusive and linear in authored joint-coordinate space.

For:

```text
start = s
end = e
count = n
```

interior samples use:

```text
coordinate(i) = s + (e - s) * i / (n - 1)
```

for:

```text
i = 0 .. n - 1
```

The implementation assigns the first and last values directly:

```text
coordinate(0) = s
coordinate(n - 1) = e
```

This avoids endpoint drift from interpolation arithmetic.

Caller direction is preserved.

Example:

```text
0 -> 90, count 3
  = [0, 45, 90]

90 -> 0, count 3
  = [90, 45, 0]
```

Equal start/end coordinates are legal and produce repeated analysis at the same requested coordinate. Block 30 does not deduplicate caller samples.

## Fresh Project copy per sample

For every sample independently:

```text
Project sample_project = source_project
```

Then:

```text
selected existing motion solver .move(sample_project, coordinate)
-> require converged result
-> selected existing atomic motion applier .apply(sample_project, result)
-> AssemblyContactAnalyzer::analyze(sample_project)
```

The next sample does not start from the preceding sample.

Therefore:

```text
sample 1 does not mutate sample 0
sample 2 does not accumulate sample 1 numeric drift
sampling direction does not change the source pose
source Project remains unchanged
```

The current solver and applier contracts remain the sole motion authority. Block 30 does not copy Revolute residual mathematics, finite differences, transform proposal logic, or freshness/application semantics.

## Sweep result

`AssemblyRevoluteSweepAnalysis` stores:

```text
selected joint reference
start_coordinate_deg
end_coordinate_deg
samples[]
```

Each `AssemblyRevoluteSweepSample` stores:

```text
sample_index
coordinate_deg
applied_transform_count
AssemblyContactAnalysis contact_analysis
```

Thus every pair classification and measured non-interfering clearance remains tied to:

```text
exact rooted occurrence pair
sample index
requested Revolute coordinate
```

A consumer can derive first observed touching/interference, minimum sampled clearance, or classification transitions from these records.

Block 30 does not add a second persistent event log or infer continuous first-contact time between samples.

## Sampled analysis is not continuous collision detection

The Block-30 contract is explicitly:

```text
deterministic discrete sampled motion analysis
```

It is not:

```text
continuous collision detection
swept-volume exact intersection
conservative advancement
exact time-of-impact computation
```

A collision or contact that exists only between two requested sample coordinates may be missed.

Increasing `sample_count` increases discrete resolution but does not change this semantic contract.

## Failure policy

Static contact analysis fails closed on:

- invalid Project/hierarchy/leaf structure;
- invalid touching tolerance;
- invalid overlap-volume tolerance;
- part recompute failure;
- missing final part shape;
- posing failure;
- OCCT common-operation failure;
- non-finite overlap volume;
- OCCT minimum-distance failure;
- non-finite minimum distance;
- OCCT exceptions.

Sweep validation fails closed before sampling on:

- invalid Project structure;
- empty joint id;
- non-angle start/end quantities;
- sample count below 2;
- sample count above 1001;
- unsupported joint scope;
- missing selected joint in that scope;
- inactive selected joint;
- unsupported non-Revolute selected joint;
- start/end interval outside authored joint limits.

During sampling the complete sweep fails on the first failing sample.

Sample execution failures report:

```text
sample index
sample coordinate in degrees
execution stage
underlying Error category/object/message where available
```

Stages include:

```text
coordinate construction
root-local motion solve
root-local motion application
cross-hierarchy motion solve
cross-hierarchy motion application
contact analysis
```

A successful prefix is not returned as a partial successful sweep.

## Focused coverage

Static contact:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-contact]"
```

Proves:

- `Separated` classification and exact minimum distance;
- exact face `Touching`;
- near-touching inclusive tolerance behavior;
- positive-volume `Interfering` before distance classification;
- overlap-volume reporting;
- optional distance semantics;
- exact repeated-child rooted occurrence pair identity;
- deterministic insertion-order-independent pair order;
- one recompute for a repeated shared part;
- source Project immutability;
- tolerance validation.

Sampled Revolute sweep:

```bash
./build/dev-geometry/blcad_geometry_tests "[geometry][assembly-revolute-sweep]"
```

Proves:

- inclusive deterministic `[0, 45, 90]` sample coordinates;
- caller direction preservation through `[90, 45, 0]`;
- one local Revolute sweep observing `Touching -> Interfering -> Separated`;
- exact rooted pair identity in every sample;
- root-local existing motion solver/applier reuse;
- Project-level cross-hierarchy existing motion solver/applier reuse;
- fresh-copy source immutability for transforms and authored joint coordinates;
- unit, limit, scope, and sample-count validation;
- sample-indexed fail-closed contact-analysis errors.

## Next technical step

Implement Block 31 only from `docs/assembly-general-geometric-target-roadmap.md`: typed assembly geometric target taxonomy and capability projection.

Block 31 must separate selected semantic source kind from solver geometric capability while preserving existing generated face, `.axis`, and `.seat` target strings and numeric behavior.

Do not add DatumAxis persistence, reference-geometry JSON, generic relationship families, or richer joint families in Block 31.
