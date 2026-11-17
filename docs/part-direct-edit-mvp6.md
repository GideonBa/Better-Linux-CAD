# Direct face editing

Numbered acceptance is owned by [the MVP plan](mvp-plan.md). Blocks 185–188 retain the original
[D01/D02 requirements](inventor-parity-test-plan.md#d01-direct-face-edits-and-source-preservation).
This document records the implementation contract; readiness of an individual slice does not close
the complete advanced Part gate.

## Core authority

`DirectEditFeature` owns one existing Solid Body and its exact predecessor FeatureId. It stores an
ordered nonempty set of semantic `FaceReference` selections, each with expected Planar, Cylindrical
or AnySurface capability. Persistent identity never depends on OCCT traversal order, STEP entity
numbers or geometry witnesses alone. Result-face origins must exist upstream of the captured producer;
imported selections must belong to the selected immutable base output and match catalog capability.
Geometry must additionally recover exactly one current physical face and reject stale/lost/ambiguous
source recovery. Compatible source refresh preserves the historical catalog capture baseline.

Move and Rotate initially require planar supports; Size requires cylinders; Offset requires planes
or cylinders. Delete-and-Heal accepts a declared surface capability and invokes explicit adjacent
support extension. Move XYZ magnitudes are Length parameters and use separately authored -1/0/+1
axis signs; Offset has explicit reverse direction. Size uses positive radius or diameter; Rotate uses
an Angle, a local nonzero axis and the explicit frame origin as pivot. Frames contain finite model-space
origins and orthonormal X/Y axes; Z is their right-handed cross product. Resolving a frame from a
selection is an authoring action, never an implicit change during replay.

The initial blend policy is `RejectAffectedBlends`; the healing policy is `ExtendAdjacent` with
explicit [1e-7,1e-3] mm tolerance. Unsupported capabilities or policies reject instead of choosing a
fallback. Whole-Body transforms continue through the existing `BodyTransform` authority.

## Transactions and protected sources

Normal add/update/remove commands validate in disposable Part candidates and register parameter,
selected-face and predecessor dependencies. An appended direct edit must capture the current target
producer. Reorder reconstructs the same typed commands, so moving before that producer or across a
conflicting Body modifier rejects. Suppression uses ordinary history state and effective availability;
a suppressed direct edit leaves its predecessor available, while a required suppressed creator
blocks its dependent edit. Removal cannot erase a producer with downstream consumers.

Parameter changes validate the direct edit's dimensions and bounds transactionally, including formula
updates. Geometry still owns physical feasibility and operation effects. Reference-mode Parts reject
mutation until explicitly converted into a separate Editable Part. No direct edit rewrites the imported
base feature, original STEP asset or source registry. Faces authored by a semantic Hole or Thread
reject Direct Edit with an instruction to edit the authored parameters, preserving thread metadata.

## Persistence and focused proof

[Part V16](file-format.md#direct-edit-intent--part-v16) is necessary only when this feature family is
present. Its strict version-1 operation records preserve frame, capability, predecessor, bindings and
policy without serializing resolved topology or GUI state. Higher necessary envelopes retain this
family. Project members use the existing Part codec and existing revision-pinned resource handling.

Core regression source is [direct_edit_intent_tests.cpp](../tests/core/direct_edit_intent_tests.cpp)
with `[direct-edit][parity-b185]`: all operation records, dimensions, frame/policy bounds, duplicate
selections, exact reopen, strict dispatch, parameter rollback, history reorder/suppression, producer
removal, result-face ancestry and authored Hole protection. Execution evidence is recorded after the
frozen build/test gate; this source inventory alone is not an acceptance claim.
