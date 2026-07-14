# Assembly Generated Topology Reference Identity and Recovery MVP-5

Status: implemented as Block 35 of `docs/assembly-cross-hierarchy-solver-sequence-mvp5.md`.

This document is canonical for the Core semantic identity, producer support matrix, persistent assembly spelling, cardinality contract, failure policy, and read-only recovery semantics of generated cylindrical faces, linear edges, circular edges, and vertices.

Geometry target resolution remains Block 36.

## Scope

Implemented:

- producer-driven semantic identities for `GeneratedCylindricalFace`, `GeneratedLinearEdge`, `GeneratedCircularEdge`, and `GeneratedVertex`;
- first-class cylindrical-face and circular-edge reference values carrying exact source `FeatureId` plus `ProfileId` where profile identity is required;
- reuse of the existing `SemanticEdgeReference` and `SemanticVertexReference` identities for the supported rectangular additive producer;
- finite public producer role matrices with explicit expected cardinality;
- exact model-intent producer classification;
- canonical `topo:` semantic-reference spellings with strict uppercase `%HH` escaping;
- fail-closed parsing and producer/reference validation;
- explicit ambiguity and patterned-instance rejection;
- read-only generated-topology recovery through `ReferenceRecoveryEvaluator`;
- byte-for-byte assembly endpoint JSON roundtrips through the unchanged endpoint shape;
- focused Core acceptance coverage.

Not implemented in Block 35 (Block 36 status noted inline):

- OCCT face/edge/vertex lookup for these identities — intentionally still not done; Block 36 computes descriptors analytically from validated model intent rather than consulting kernel topology;
- `AssemblyConstraintTargetResolver` or hierarchy resolver branches for `topo:` sources — **implemented in Block 36**;
- Plane/Axis/Line/Point/Circle/Cylinder descriptor construction from Block-35 identities — **implemented in Block 36**;
- capability compatibility changes — **implemented in Block 37**;
- new relationship or joint families — remains Block 38 and later;
- pattern-instance semantic identity — remains deferred.

## Authority boundary

Persistent identity is producer/model intent:

```text
source FeatureId
+ producer-supported semantic family
+ producer-supported semantic role
+ source ProfileId where the producer role is profile-derived
```

The following remain forbidden persistent identity sources:

```text
TopoDS_Shape::HashCode
TopExp traversal index
BRepTools map position
XDE label tag
STEP entity number
memory address
unordered OCCT result order
```

Block 35 is Core-only. Producer classification and recovery inspect `PartDocument`, `Feature`, `Sketch`, and profile intent. They execute no OCCT operation and consume no `TopoDS_*` value.

## Reference identity variants

`GeneratedTopologyReferenceIdentity` is the explicit variant:

```text
SemanticCylindricalFaceReference
SemanticEdgeReference
SemanticCircularEdgeReference
SemanticVertexReference
```

The variant maps to Block-31 source families as:

```text
SemanticCylindricalFaceReference -> GeneratedCylindricalFace
SemanticEdgeReference            -> GeneratedLinearEdge
SemanticCircularEdgeReference    -> GeneratedCircularEdge
SemanticVertexReference          -> GeneratedVertex
```

Cylindrical-face and circular-edge references retain:

```text
source_feature : FeatureId
source_profile : ProfileId
semantic role
```

Linear-edge and vertex references reuse their established exact feature-plus-role identities.

## Producer support matrix

The first supported producers are deliberately narrow. Unsupported producers fail closed.

### RectangularAdditiveExtrude

Producer definition:

```text
FeatureType::AdditiveExtrude
input Sketch contains exactly one RectangleProfile
input Sketch contains no other profile
```

Supported generated linear-edge roles, expected cardinality `1` each:

```text
top_front
top_back
top_right
top_left
bottom_front
bottom_back
bottom_right
bottom_left
front_right
front_left
back_right
back_left
```

Supported generated vertex roles, expected cardinality `1` each:

```text
top_front_right
top_front_left
top_back_right
top_back_left
bottom_front_right
bottom_front_left
bottom_back_right
bottom_back_left
```

The role names are existing semantic box roles. Feature insertion order is not part of identity.

### SingleCircleSubtractiveExtrude

Producer definition:

```text
FeatureType::SubtractiveExtrude
input Sketch contains exactly one CircleProfile
input Sketch contains no other profile
no CircularHolePattern in the input Sketch
direct target feature is a supported RectangularAdditiveExtrude
```

Supported generated cylindrical-face role, expected cardinality `1`:

```text
wall
```

Supported generated circular-edge roles, expected cardinality `1` each:

```text
source_rim
opposite_rim
```

`source_rim` is the circular boundary on the semantic source-sketch side of the cut. `opposite_rim` is the corresponding boundary on the other cut side. Their Geometry/topology lookup and current-shape cardinality proof belong to Block 36; Block 35 freezes only producer-role identity and expected cardinality.

The exact source `CircleProfileId` is part of cylindrical-face and circular-edge identity. A reference carrying another profile id is invalid even when the feature contains a circle with identical numeric geometry.

## Canonical persistent `topo:` spelling

Assembly endpoints continue to persist one opaque semantic-reference string. Block 35 adds these canonical strings:

```text
topo:cylindrical_face:<encoded-feature-id>:<encoded-profile-id>:wall
topo:linear_edge:<encoded-feature-id>:<linear-edge-role>
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:source_rim
topo:circular_edge:<encoded-feature-id>:<encoded-profile-id>:opposite_rim
topo:vertex:<encoded-feature-id>:<vertex-role>
```

Every id byte outside:

```text
[A-Za-z0-9_-]
```

is escaped as uppercase `%HH`.

For example:

```text
FeatureId = feature.hole/a%b:c
ProfileId = profile.hole/50%

->

topo:cylindrical_face:feature%2Ehole%2Fa%25b%3Ac:profile%2Ehole%2F50%25:wall
```

Canonical parsing rejects:

- lowercase hex escapes;
- truncated escapes;
- escapes of unreserved id bytes;
- raw reserved id bytes;
- missing or extra segments;
- unsupported families;
- unsupported role tokens;
- empty decoded ids.

Valid `topo:` strings contain no `.`. They remain disjoint from legacy `<feature-id>.<role>` spellings. The distinct `topo:` prefix also remains disjoint from the Block-32 `ref:` grammar.

## Producer classification and validation

`classify_generated_topology_producer` reads the current source feature and exact source sketch/profile intent and returns one supported producer kind or a validation error.

`validate_generated_topology_reference` then validates:

```text
source feature exists
producer is supported
reference family is supported by that producer
semantic role is listed in the producer matrix
expected cardinality is exactly one
profile-derived references retain the exact source ProfileId
```

Validation does not probe OCCT topology. Block 36 starts from this validated producer identity and then derives the corresponding geometric descriptor analytically from current model intent, without consulting kernel topology.

## Ambiguity and failure policy

Fail closed on:

```text
missing source feature
missing source sketch
unsupported feature producer
unsupported AdditiveExtrude profile family
zero-circle SubtractiveExtrude producer
multiple CircleProfiles
one CircleProfile mixed with another profile
unsupported direct target producer
family/role mismatch
wrong source ProfileId
non-cardinality-one role contract
malformed topo: spelling
```

A multi-circle or mixed-profile subtractive sketch is semantically ambiguous for the current first producer matrix. No result-vector position or profile traversal order is used as a fallback.

## Pattern identity policy

`CircularHolePattern` generated subelements remain unavailable in Block 35.

A target such as:

```text
pattern hole result index 3
```

is not accepted because the current pattern model has no first-class persistent per-instance semantic id. The validator reports:

```text
patterned generated topology is unavailable until stable per-instance semantic identity exists
```

Transient expanded-hole vector position is not promoted to persistent target identity.

## Recovery

`ReferenceRecoveryEvaluator` now has a generated-topology overload returning `GeneratedTopologyReferenceRecovery`.

The result retains:

```text
exact GeneratedTopologyReferenceIdentity
Resolved | Lost
message
```

Recovery is read-only:

```text
validate current producer intent
  -> supported exact producer/profile/role
       => Resolved
  -> missing / unsupported / ambiguous / patterned / mismatched
       => Lost with the exact validation message
```

No reference status/remap record is written automatically. No feature, sketch, parameter, dependency edge, component transform, or assembly endpoint is mutated.

The existing persistent `ReferenceStatusRecord` / `ReferenceRemapRecord` format remains unchanged in Block 35. If a later application command persists a repair decision, that command must store semantic producer-role intent only; it must never store raw OCCT topology identity.

## JSON and save-format compatibility

Block 35 adds no JSON field and does not change local or occurrence-qualified endpoint shapes.

Existing assembly JSON continues to persist:

```text
component/occurrence identity
+ semantic_reference string
```

Therefore canonical `topo:` strings roundtrip byte-for-byte through the existing local and Project endpoint serializers without Geometry execution.

Generated topology producer matrices, classification results, validation results, and recovery results are derived and are not serialized.

## Focused acceptance coverage

```bash
./build/dev/blcad_core_tests "[core][semantic-generated-topology-reference]"
./build/dev/blcad_core_tests "[core][semantic-generated-topology-recovery]"
```

`tests/core/generated_topology_reference_tests.cpp` proves:

- the rectangular producer publishes exactly 12 linear-edge roles and 8 vertex roles;
- the single-circle subtractive producer publishes exactly one wall and two rim roles;
- every published role has expected cardinality `1` and is unique inside its producer matrix;
- adversarial feature/profile ids roundtrip through canonical uppercase `%HH` spellings;
- malformed and non-canonical spellings fail closed;
- supported edge, vertex, cylindrical-face, and circular-edge identities validate;
- the exact source `CircleProfileId` is retained;
- feature insertion order changes serialized feature order but cannot change semantic target spelling;
- unsupported additive producers fail closed;
- multi-circle subtractive producers report ambiguity;
- patterned generated topology is rejected until per-instance semantic identity exists;
- recovery reports `Resolved` for supported current producer intent;
- recovery reports `Lost` with the explicit pattern-identity failure for patterned targets;
- recovery leaves the complete serialized source `PartDocument` unchanged;
- assembly JSON preserves `topo:` semantic-reference strings byte-for-byte.

## Handoff

Block 36 (generated face/edge/vertex Geometry target resolution) is implemented. Its canonical contract lives in `docs/assembly-general-geometric-target-roadmap.md`. `AssemblyConstraintTargetResolver::resolve_geometric` parses `topo:` sources before legacy feature-role parsing, validates Block-35 producer identity through `validate_generated_topology_reference`, and projects:

```text
GeneratedCylindricalFace -> Cylinder + Axis
GeneratedLinearEdge      -> Line
GeneratedCircularEdge    -> Circle + Axis + Point(center)
GeneratedVertex          -> Point
```

Descriptors are computed analytically from validated feature/sketch/profile intent for both component-local and exact rooted transform semantics; acceptance is `[geometry][assembly-generated-topology-target-resolution]`. Block 36 added no compatibility rule, relationship family, or JSON field.

Block 37 explicit relationship compatibility, Block 38 generic relationship intent/JSON, Block 39 generic equations/shared solve integration, and Block 40 joint target compatibility are implemented. Blocks 39–40 consume generated target capabilities through the same projection contract and add no generated-topology JSON field.

Blocks 41–43 typed coordinates, additive compatible JSON, and shared vector drives are implemented in their canonical joint documents. Blocks 44–47 Prismatic, Cylindrical, Planar, and passive Spherical are implemented in their canonical documents. Blocks 48–90 Part Construction through deterministic 3D Sketch Geometry conversion are implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is next.
