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
                        SemanticAxisReference | SemanticEdgeReference
PlaneReference          DatumPlaneId | ConstructionPlaneId | SemanticFaceReference
FaceReference           SemanticFaceReference | SemanticCylindricalFaceReference
EdgeReference           SemanticEdgeReference | SemanticCircularEdgeReference
BodyReference           BodyId
```

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
Each consuming feature serializes its references inside its own JSON record when introduced—for
example Revolve in Block 61, Mirror in Block 66, edge treatments in Block 68, and Shell/Draft in
Blocks 71/73. This keeps role and ownership inside the feature that gives them meaning.

Focused proof:

```bash
./build/blcad_core_tests "[core][part-feature-input-reference]"
```

The proof covers stable spellings, the role/capability matrix, all six reference families,
Block-35 face/edge identity reuse, dependency node identity, fail-closed construction, and
document validation. Block 59 now embeds `FaceReference` values with the `ExtrudeLimitFace` role
in ToFace/Between intent; its canonical contract is `docs/part-extrude-extent-intent-mvp6.md`.
