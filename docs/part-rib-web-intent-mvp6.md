# Rib and Web intent

`mvp-plan.md` owns numbered acceptance. Rib and Web share `SketchDrivenFeature`, an immutable
normal Part feature with one owned target Solid Body. Geometry is the separate Block-181 gate;
native creation/editing is 184. Existing open-line Thin Extrude retains its separate authority.

## Authored source and modes

`RibWebIntent` names an owned planar Sketch and exactly one ordered curve list or owned profile.
Lists support geometric line, arc, rational cubic and equation-curve identities; construction curves
and foreign/missing sources reject. Profiles retain their original parameter/curve dependencies.
Neither generated BRep nor OCCT traversal identities enter persistent intent.

Rib/Web kind, perpendicular/parallel extrusion, symmetric/first/second/asymmetric thickness,
optional draft and union/reject crossing policy are explicit. Asymmetric thickness owns two
positive Length bindings; other side modes own one. Draft uses an Angle strictly between −45° and
45°. A parameter cannot satisfy a Length field by also being bound as draft.

Direction components are in the source Sketch frame and retain their authored magnitude/literals.
Geometry normalizes the vector and verifies its relationship to the selected extrusion mode.
Perpendicular uses the Sketch normal; parallel lies in the Sketch plane. Source Sketch/support
placement therefore moves the physical frame through the existing resolver.

Termination is a positive distance binding, an owned face with the existing planar-limit capability,
or an owned Solid Body. Persist the selected source, never a precomputed bounding distance.
Actual trimming, coverage and first-intersection checks belong to Geometry.

## Ownership, history and atomic changes

The ordinary Part registry owns add/update/remove, normal parameter and Sketch dependencies,
Body predecessor/output, suppression, reorder and history prefixes. Imported/reference-part policy,
unique feature/node identity and result-face producer validation apply normally.

Before rewiring a target Body alias, earlier consumers retain its previous producer snapshot. A
limit Body built earlier from the same target can therefore terminate a later Rib without a false
cycle; the limit remains a real prerequisite. Reordering that Rib ahead of its limit rejects.
Parameter changes and feature replacement validate a candidate before publication. Failure preserves
intent, graph and history. Generated face roles are eligible for ordinary downstream references.

## Persistence

Necessary Part V15 adds `sketch_driven_features` only when nonempty and requires explicit executable
order. The strict record stores kind, source, local direction, side/thickness, optional draft,
termination and crossings. Unknown modes, incompatible fields, malformed references, duplicate
curves and extra fields reject. V14 advanced finishing may coexist inside V15; documents without
this family retain their earlier necessary version and omit the new array entirely.

Normal Part/Project serialization replays the family together with existing features. The opaque
feature definition allows later Emboss/Engrave variants without a second Part registry or an
independent history engine.

## Verification

Block 180 is accepted. All five targets build after the complete Part-registry ABI rebuild.
Focused Core **6 / 73** and complete Core **804 / 35,384** pass. Legacy physical-guide Geometry
**5 / 83** and shared finishing Qt **13 passed / 1 native skip / 314 assertions** pass. These
regressions retain the prior finishing boundary; Rib/Web Geometry remains explicitly unavailable
until 181. No new native authoring is claimed before 184.

Core coverage `[rib-web]` / `[parity-b180]` proves raw literals, modes, ownership, normal graph/
update/remove, parameter rollback, strict necessary JSON, suppression/reorder, Project roundtrip
and a historical placed Body termination. The runtime manifest covers **1,263 files**, SHA-256
`aedf1c24cc5413545376844f865e13a33fef6c834d48fa2e0602ed5275b6d56f`.
Logs/manifest: `build/verification/block180/`.
