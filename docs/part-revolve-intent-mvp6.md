# Revolve/RevolveCut Core Intent MVP-6

Status: implemented in Block 61.

## Boundary

Block 61 adds persistent rotational feature intent without executing OCCT geometry. The Core model
owns a `RevolveFeature` value whose `RevolveFeatureKind` discriminator is either `Revolve` or
`RevolveCut`. This keeps both families in one typed document collection while preserving their
different body-operation rules. Block 62 owns profile mapping, rotational solid/tool construction,
boolean execution, and geometric validity checks.

## Inputs and body result authority

Every Revolve record requires:

- a `ProfileRegionReference` with the `RevolveProfile` role;
- an `AxisReference` with the `RevolveAxis` role and `Axis` capability;
- one explicit `RevolveAngleExtent`;
- one mandatory `FeatureBodyResultContext`.

Supported axis identities are `DatumAxis`, an axis-capable `ConstructionLine`,
`SemanticAxisReference`, and an axis-capable semantic generated linear edge. A semantically modeled
sketch centerline enters through the shared construction-line/axis identity boundary; raw sketch
entity indices and OCCT topology are not persistent axis identity.

`Revolve` accepts NewBody, Join, and Intersect. `RevolveCut` requires Cut. NewBody requires an
explicit produced Body and no target. Modifying modes require a target Body and use either an
explicit produced Body or the target identity as the effective result.

## Angle semantics

The extent modes are exact and mutually exclusive:

```text
Full       canonical 360-degree revolution; no angle or side field
Angle      authored magnitude 0 < angle < 360 plus Positive/Negative side
Symmetric  authored total included angle 0 < angle < 360, centered about the profile plane
```

Partial values are stored exactly and are not reduced to a principal angle. Thus `270 deg` remains
`270 deg`; direction is represented by the side enum rather than by a signed or normalized angle.
Non-finite, zero, negative, and 360-degree partial/symmetric values fail validation. A full
revolution is represented only by `Full`, removing the ambiguous `Angle(360)` spelling.

## Graph and invalidation

`PartDocument::add_revolve_feature` validates the referenced profile, axis, and Body records before
mutating the document. Dependency edges are:

```text
profile sketch -> revolve feature
axis source -> revolve feature
target/prior body producer -> revolve feature
revolve feature -> body:<effective-result-body>
```

In-place Join/Cut/Intersect advances the existing Body producer chain just like Extrude. A new
result Body cannot already have a producer, cycles fail closed, and the graph plus invalidation
state update transactionally. Revolve IDs share the global Feature-ID namespace and participate in
`mark_feature_changed`, recompute plans, and Body-result invalidation.

## JSON

Part JSON emits a top-level `revolve_features[]` array. Missing arrays restore as empty so files
written before Block 61 remain compatible. Each record contains:

```json
{
  "id": "revolve.main",
  "name": "Main revolve",
  "type": "revolve",
  "profile": {"sketch": "sketch.profile", "profile": "profile.main"},
  "axis": {"source_kind": "datum_axis", "datum_axis": "axis.z"},
  "extent": {"mode": "angle", "angle_deg": 270.0, "side": "negative"},
  "operation_mode": "new_body",
  "produced_body": "body.main"
}
```

Axis payloads are strict per source kind. Extent fields are strict per mode: Full rejects angle and
side, Angle requires both, and Symmetric requires only its total angle. Body context uses the
existing `new_body|join|cut|intersect`, `target_body`, and `produced_body` spellings.

## Failure policy and Block-62 handoff

Core rejects malformed identity, role/capability mismatches, missing document sources, invalid
angles, invalid kind/operation pairings, duplicate IDs, missing Body context, producer conflicts,
and dependency cycles. Whether a valid profile/axis/angle sweep self-intersects is a geometric
question. Block 62 must detect that during OCCT construction, fail closed without publishing a
partial Body result, and preserve the last valid cache state. Until then, document-level Geometry
execution rejects Revolve/RevolveCut explicitly instead of silently skipping it.

Focused proof:

```text
./build/blcad_core_tests "[core][revolve-feature]"
```

Block 62 is the next boundary: Revolve/RevolveCut Geometry.
