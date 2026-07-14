# MirrorFeature Core Intent and JSON MVP-6

Status: implemented in Block 66.

## Boundary

Block 66 adds persistent, geometry-independent Part-level mirror intent. It does not reflect OCCT
shapes, copy Feature records, or alter sketch geometry. Sketch mirror remains a separate sketcher
operation. MirrorFeature Geometry is Block 67.

## Source identity and order

`MirrorSourceReference` stores exactly one typed `FeatureId` or `BodyId`. Sources are mandatory,
unique, and retain authored order. Face-only selections, raw topology IDs, and raw shapes are not
valid sources. No reflected per-source Feature record is persisted.

## Mirror plane and Body result

`MirrorFeature` owns one `PlaneReference` with role `MirrorPlane` and capability `Plane`. Supported
source identities are:

```text
DatumPlaneId
ConstructionPlaneId
SemanticFaceReference to a supported generated planar face
```

NewBody, Join, Cut, and Intersect reuse mandatory `FeatureBodyResultContext`. Target and effective
result Bodies must already exist. Feature and Body sources must resolve to existing document
intent when the Mirror is added.

## Dependency, invalidation, and removal

Every ordered source and the mirror-plane source feed the Mirror node. The Mirror node produces
`body:<effective-result-body>`. Modifying operations advance the prior target-Body producer chain.
An in-place Body source is replaced by the preceding producer dependency so the graph remains
acyclic and future recompute cannot consume its own result.

Source or plane-producer changes invalidate the Mirror and its Body result transitively. Bodies
referenced as source, target, or result cannot be removed. Adding invalid intent is transactional.

## JSON

The always-emitted, optional-on-read `mirror_features[]` array is additive under the unchanged
schema version. Each entry stores:

```text
id, name, type = mirror
ordered sources[] with kind = feature | body
mirror_plane with concrete source_kind and source identity
operation_mode, target_body/produced_body as required
```

Datum, Construction, and semantic planar plane forms use strict mode-specific fields. Unknown
top-level, source, or plane fields fail closed. Pattern and Mirror arrays are restored together by
dependency availability, preserving valid Pattern/Mirror source chains. Older files without
`mirror_features` restore zero Mirrors and rewrite with an empty array.

## Geometry boundary and proof

Geometry recompute reports the exact Mirror Feature ID and the Block-67 boundary instead of
silently marking persistent intent clean.

Focused verification:

```text
./build/blcad_core_tests "[core][mirror-feature]"
```

The suite proves source identity/order, validation, all plane identity kinds, Body producer-chain
advancement, dependency/invalidation, removal protection, deterministic JSON order, strict
malformed-field rejection, and backward compatibility.

Block 67 Geometry is implemented in `docs/part-mirror-geometry-mvp6.md`; Block 68 Edge Treatment
Core/JSON is implemented in `docs/part-edge-treatment-intent-mvp6.md`. Block 69 Fillet Geometry is
the next boundary.
