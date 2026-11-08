# Part Feature Semantic Input Reference Contract MVP-6

Status: implemented in Block 58.

## Purpose and authority

Block 58 freezes reusable Core input references for later Part features:

```text
ProfileRegionReference
AxisReference
PlaneReference
FaceReference
EdgeReference
BodyReference
```

Each value keeps three independent facts:

```text
source identity                 existing typed Core/semantic identity
expected geometric capability  what the consuming feature needs
feature-specific role          why the feature consumes the source
```

No reference contains an OCCT object, topology traversal index, address, hash, XDE label, or STEP
entity number. Geometry resolution remains derived and later feature implementations must validate
these Core references before creating OCCT inputs.

## Capabilities and roles

The frozen capability vocabulary is:

```text
ProfileRegion | Axis | Line | Plane | Face | Edge | Body
```

The first role matrix covers the consumers planned in Blocks 59–94: profile inputs, Revolve and
Pattern axes, Draft pull direction, Mirror/Draft planes, Extrude limit faces, Fillet/Chamfer edges,
Shell/Draft faces, and target/tool/source Bodies. Role/capability mismatches fail during reference
construction. In particular:

```text
RevolveAxis        requires Axis
MirrorPlane        requires Plane
FilletEdge         requires Edge
ShellRemovalFace   requires Face
DraftPullDirection accepts Axis or Line
```

`AxisReference` deliberately accepts a typed construction line or semantic linear edge while
recording whether the consumer requests Axis or Line capability. The source family is therefore
not conflated with the requested Geometry projection.

## Source matrix

```text
ProfileRegionReference  SketchId + ProfileId
AxisReference           DatumAxisId | ConstructionLineId |
                        SemanticAxisReference | SemanticEdgeReference | SketchLineReference
PlaneReference          DatumPlaneId | ConstructionPlaneId | SemanticFaceReference
FaceReference           SemanticFaceReference | SemanticCylindricalFaceReference
EdgeReference           SemanticEdgeReference | SemanticCircularEdgeReference
BodyReference           BodyId
```

153B2 extends shared plane validation for the existing workplane identity namespace:
`DatumPlaneId` may identify an actual fixed datum, DerivedWorkplane or ResultFaceWorkplane.
Derived sources retain their existing producer validation; captured sources use
`validate_result_face_workplane_reference`, including Body, contributor and lifetime checks.
Missing workplanes remain errors. This uses the existing source spelling and dependency node,
without copying a captured face into a new fixed datum or introducing another persistence format.

The semantic cylindrical-face and circular-edge identities are the exact Block-35 generated
topology types. Validation delegates those sources, and semantic linear edges, to
`validate_generated_topology_reference`; their producer-owned role/cardinality and recovery rules
remain authoritative. Planar face and semantic axis seeds reuse their existing typed feature
identity.

Every reference exposes a dependency-ready `source_node_id()`. Profile regions depend on their
Sketch graph node, generated sources on their producing Feature, and Bodies on `body:<BodyId>`.

## Validation and persistence handoff

Read-only overloads of `validate_part_feature_input_reference` verify document membership and,
where applicable, generated-topology producer support. Missing Sketch/Profile, datum,
construction, Feature, Body, and unsupported topology sources fail explicitly.

Block 58 defines reusable persistent value types but adds no orphan top-level Part JSON arrays.
Each consuming feature serializes its references inside its own JSON record when introduced:
Revolve consumes these values in Block 61, Mirror in Block 66, edge treatments in Block 68, Shell
in Block 71, and Draft in Block 73. Draft's canonical persistence and ownership contract is
`docs/part-draft-intent-mvp6.md`. This keeps role and ownership inside the feature that gives them
meaning.

Focused proof:

```bash
./build/blcad_core_tests "[core][part-feature-input-reference]"
```

The proof covers stable spellings, the role/capability matrix, all six reference families,
Block-35 face/edge identity reuse, dependency node identity, fail-closed construction, and
document validation. Block 59 now embeds `FaceReference` values with the `ExtrudeLimitFace` role
in ToFace/Between intent; its canonical contract is `docs/part-extrude-extent-intent-mvp6.md`.


## Sketch-line axis input — Issue #110

`SketchLineReference` stores one nonempty `(SketchId, SketchEntityId)` pair. Its length-delimited
display/selection identity is collision-free even when ids contain separators; it does not encode
coordinates. `AxisReference::create_sketch_line` currently accepts only `RevolveAxis` and exposes
`Axis` capability and source kind `sketch_line`. Pattern/Draft are not extended by this issue.
Validation requires exactly one editable `LineSegment` in that Sketch, independently of its normal,
construction or centerline role. The source node is the Sketch, whose own plane/parameter
dependencies feed downstream recompute. Body Rotate stores the same reference through its separate
`BodyTransformRotationAxisKind::SketchLine` variant. Missing, unsupported or ambiguous sources fail
with the exact source identity and an instruction to repair it, never with positional retargeting.


## Semantic Hole inputs — Block 152B

Hole support uses the existing PlaneReference variants with `hole_support_plane`; explicit face
limits use existing FaceReference variants with `hole_limit_face`. Capabilities remain Plane and
Face respectively, including imported topology with the same canonical catalog identity. Stable
Hole placements either own signed finite support-frame coordinates or reference an existing
Sketch/free-or-projected-point identity. The referenced Sketch is a graph input; coordinates are
resolved from that source rather than copied. Missing point input is reported by Core reference
validation and strict reopening; restoring the same source identity recovers the Hole. Generated
result-edge and captured result-face producer validation recognizes the registered Hole family.
Physical support coplanarity, trimmed boundaries and generated bore references remain at 153.

## Current result Face inputs — Block 153C

`FaceReference::create_result` accepts the existing `ResultFaceReference` producer/Body/origins
identity with Face capability. The source kind is `result_face`; its dependency source is the
captured producer. Model validation reuses the result-face ownership/ancestry contract, allowing
curved faces for Face consumers while preserving planar-only imported workplane validation.
Geometry follows every contributor on the current referenced Body and requires exactly one face.
Finishing additionally verifies that face belongs to the actual target. Missing, suppressed,
out-of-prefix, deleted, split and stale imported sources fail closed. A generic Face identity does
not imply Plane capability: planar extent/workplane consumers still check actual geometry.

Draft's explicitly selected input faces also follow OCCT `Generated` replacement faces, because
Draft reports the changed supporting surface through that history channel. This exception applies
to the selected faces only; generated geometry does not inherit adjacent support identities.
All descendants remain recorded so subsequent splits are ambiguous rather than guessed.
