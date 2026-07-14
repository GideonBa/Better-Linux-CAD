# Part Draft Intent MVP-6

Status: implemented in Block 73.

Block 73 adds persistent tapered-face intent. Its OCCT execution is implemented by Block 74 and
specified in `docs/part-draft-geometry-mvp6.md`.

## Core model

```text
DraftFeature
  id, name
  target_body
  ordered faces[]: FaceReference
  pull_direction: AxisReference with Axis or Line capability
  neutral_plane: PlaneReference
  angle_parameter: Angle
```

At least one face is mandatory. Every face has the `DraftFace` role and may use a supported
semantic planar or cylindrical identity. Authored order is stable and duplicate semantic face
identities fail before publication.

The pull direction has the `DraftPullDirection` role. Datum/semantic axes carry Axis capability;
construction lines and semantic linear edges may explicitly carry Axis or Line capability. The
reference's orientation owns the positive pull direction. The neutral plane has the
`DraftNeutralPlane` role and may reference a datum plane, construction plane, or semantic planar
face. Its geometric location is the zero-displacement plane; flipping its normal does not invert
the draft-angle sign.

The angle parameter is a non-zero Angle strictly between -90 and +90 degrees. Positive draft
expands the selected faces away from the neutral plane while advancing along the positive pull
direction; negative draft contracts them. Block 74 preserves this convention while mapping it to
OCCT's opposite signed removal convention.

## Body history and invalidation

Draft modifies its target Body in place. Face producers, pull direction, neutral plane, angle
parameter, and the previous target-Body producer are dependencies. Draft becomes the new Body
producer. Changes to any referenced intent invalidate Draft and downstream consumers.

Missing Bodies, references or parameters, wrong roles/capabilities/units, empty or duplicate face
sets, zero/out-of-range angles, duplicate feature IDs, and cycles fail transactionally. A target
Body referenced by Draft cannot be removed.

## JSON

Part JSON emits ordered `draft_features[]`. Each record contains exactly `id`, `name`,
`target_body`, ordered `faces`, `pull_direction`, `neutral_plane`, and `angle_parameter`. Nested
references persist role, expected capability, source kind, and their exact semantic or construction
identity. Unknown fields, malformed identities, role/capability mismatches, and unsupported source
kinds fail closed.

Missing `draft_features` deserialize as zero Draft features for older files. Round trips preserve
feature/face order, Axis-versus-Line capability, neutral-plane identity, and Body-history
dependencies.

## Verification

```text
./build/blcad_core_tests "[core][draft-feature]"
```

Block 74 DraftFeature Geometry is implemented. Block 75 Basic 3D Sketch Core intent is implemented; Block 76 richer 3D curve intent is next.
