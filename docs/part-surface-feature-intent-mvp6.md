# Part Surface Feature Intent MVP-6

Status: implemented in Block 88.

Block 88 activates `BodyKind::Surface` as a persistent construction-result authority. It freezes
the Core and JSON boundary for the first six Surface features; OCCT execution remains owned by
Blocks 89–92.

## Persistent intent

The first feature family consists of:

- `BoundarySurfaceFeature`: two to four unique boundary-curve references, producing a Surface
  Body;
- `FillSurfaceFeature`: one or more unique boundary-curve references, producing a Surface Body;
- `TrimSurfaceFeature`: a Surface target plus a boundary-curve or sketch-profile trimming
  reference, producing a Surface Body;
- `ExtendSurfaceFeature`: a Surface target, boundary curve, positive Length distance parameter,
  and Surface result;
- `SurfaceStitchFeature`: at least two unique Surface references, a positive Length tolerance,
  and Surface result;
- `ClosedShellToSolidFeature`: one Surface shell reference and a Solid result.

Every feature owns a stable `FeatureId`, name, and explicit result `BodyId`. Results are separate
history outputs: an input Body cannot simultaneously be the result Body. A result Body may have
only one producing feature.

## Semantic references

`BoundaryCurveReference` selects either a persistent `PathCurveId` or a semantic generated edge.
This makes planar-sketch, 3D-sketch, construction/reference-curve, and supported generated-edge
sources available without persisting OCCT edge identity.

`SurfaceReference` selects either a `BodyKind::Surface` Body or a semantic generated face.
`TrimmingReference` selects either a `BoundaryCurveReference` or a persistent sketch/profile
region. References expose deterministic source graph nodes and stable duplicate-detection keys.

## Document graph and validation

`PartDocument::add_surface_feature` validates all referenced paths, profiles, semantic producers,
parameters, input Body kinds, and result Body kinds. It adds input-to-feature and
feature-to-result-Body dependency edges, rejects duplicate feature IDs, multiple result producers,
and cycles, and synchronizes invalidation state transactionally.

Removing a referenced PathCurve or Body is rejected by the dependency graph. Removing a Surface
feature is allowed only while its result has no dependents. Parameter changes propagate through
Extend/Stitch intent to all downstream Surface/Solid results.

## JSON contract

Part JSON always emits a top-level `surface_features` array; historical files may omit it and
restore zero Surface features. Each strict record always emits the complete field set:

```text
id, name, type, boundaries, target, trimming, boundary,
distance_parameter, surfaces, tolerance_parameter, shell, result_body
```

Inactive scalar/reference slots are `null` and inactive collections are empty arrays. Boundary,
Surface, and trimming references persist an explicit `source_kind` plus mutually exclusive source
slots. Unknown fields, missing mandatory fields, inconsistent reference variants, unsupported
types, and invalid document references fail loading. Derived OCCT faces, shells, tolerances,
topology maps, and geometry caches are never serialized.

## Focused proof

```bash
./build/blcad_core_tests "[core][surface-feature]"
```

The focused tests cover constructor invariants, all six document-integrated feature types,
dependency/invalidation propagation, input and feature removal protection, strict JSON round-trip,
malformed records, and compatibility with historical files lacking `surface_features`.

## Handoff

Blocks 48–90 are implemented. Boundary/Fill and Trim/Extend execution are canonical in
`docs/part-boundary-fill-surface-geometry-mvp6.md` and
`docs/part-trim-extend-surface-geometry-mvp6.md`. Block 91 Stitch/Knit/Sew shell Geometry is the
current next technical step. Network surfaces and arbitrary patch-network intent remain deferred
beyond the first Part Construction MVP.
