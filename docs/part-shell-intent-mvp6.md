# Part Shell Intent MVP-6

Status: implemented in Block 71.

Block 71 adds persistent thin-wall solid intent. Its OCCT execution is implemented by Block 72 in
`docs/part-shell-geometry-mvp6.md`.

## Core model

```text
ShellFeature
  id, name
  target_body
  ordered removed_faces[]: FaceReference
  thickness_parameter: Length
  direction: Inward | Outward
```

At least one removed face is mandatory. Each entry has the `ShellRemovalFace` role and may be a
supported semantic planar or cylindrical face. Authored order is stable and duplicate semantic
identities fail before publication.

Thickness is always a positive millimeter Length. `Inward` and `Outward` are the only sign
authority; a signed thickness is deliberately not persisted. This keeps parameter units and
offset direction independent and unambiguous.

## Body history and invalidation

Shell modifies its target Body in place. Its removed-face producers, thickness parameter, and the
previous producer of the target Body are dependencies. After insertion, Shell becomes the new
Body producer. Thickness or upstream face changes therefore invalidate Shell and downstream Body
consumers; referenced Bodies cannot be removed.

Missing Bodies, missing/unsupported face producers, incorrect roles, duplicate faces, missing or
wrong-unit thickness parameters, duplicate feature IDs, and dependency cycles fail
transactionally.

## JSON

Part JSON always emits an ordered `shell_features[]` array. Each record contains exactly `id`,
`name`, `target_body`, ordered `removed_faces`, `thickness_parameter`, and `direction`. Face
records use strict `semantic_planar_face` or `semantic_cylindrical_face` spellings with the
`shell_removal_face` role and `face` capability.

Missing arrays deserialize as zero Shell features for older files. Unknown fields, directions,
roles, capabilities, source kinds, and malformed identities fail closed. Round trips preserve
feature/face order and Body-history dependencies deterministically.

## Verification

```text
./build/blcad_core_tests "[core][shell-feature]"
```

Blocks 72–74 ShellFeature Geometry and DraftFeature intent/JSON plus Geometry are implemented.
Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is next.
