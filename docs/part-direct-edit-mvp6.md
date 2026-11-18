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
That protection follows exact length-prefixed Pattern/Mirror copy origins, including nested copies;
malformed or over-limit ancestry rejects instead of losing manufacturing authority.

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

## Checked native construction

Geometry operates on a private deep copy of the predecessor. Planar Move translates the infinite
selected support by the authored frame vector; a tangent-only displacement rejects as ineffective.
Planar Rotate uses the explicit in-plane axis and pivot, with a nonzero angle below 90 degrees.
Offset uses signed outward normal distance; Size changes the absolute cylinder radius or diameter.
Adjacent supports extend/intersect to rebuild the trimmed domain. Delete-and-Heal must remove every
selected face without kernel errors or warnings; partial healing rejects the entire candidate.

Every result must have exactly one valid solid and finite positive material. Source and candidate
native edge/pcurve consistency over the complete parameter domain and both actual curve/vertex
endpoints must satisfy the explicit tolerance budget, with matching edge parameter ranges.
Face approximation and degenerate edge/vertex tolerances must also fit that budget. OCCT may
conservatively inflate ordinary edge/vertex tolerance metadata; those bounds are not treated as
measured geometric error or used to replace the actual consistency checks. Selected continuations
must satisfy the requested analytic support uniquely. Every unselected face must retain its complete
underlying support: planes/cylinders use analytic equality; other surfaces require identical native
surface definitions. Sparse samples do not establish that invariant. Unexpected support deletion,
creation, split or blend changes reject, preserving the previous certified cache and immutable base.

The result must also pass the shared native self-interference authority
(`BRepAlgoAPI_Check` with self-interference enabled), and every unselected input face must have
exactly one output continuation. `[direct-edit-native-si]` imports a 20×20×8 two-bore Part:
resizing the left radius from 1 to 2 preserves the right radius 1 and gives analytic volume
`3200 − 40π`; resizing to 5 produces one topologically valid Solid with positive volume but native
self-interference, and must reject before model/cache/source publication. This fixture proves the
self-interference guard; an independent unselected-split fixture remains outside its proof.

Kernel provenance retains surviving/modified continuations. The offset algorithm's generated
surfaces are accepted as continuations only after the complete support checks above. A selected
continuation also owns `face/<selection-index>` under the Direct Edit FeatureId; that index belongs
to the persisted authored selection order, never an OCCT traversal. Downstream selections use this
result-face identity and diagnose missing/ambiguous descendants explicitly.

## Backend foundation verification

The initial frozen foundation passes full Core **824 cases / 35,931 assertions**, focused new Core
**14 / 490**, focused Direct Edit/state/occurrence Geometry **10 / 374**, and affected history,
import, structured exchange and cross-hierarchy regression **30 / 3,899**. Independent checks
measure planar displacement, rotated support normals, bore radius/healed volume, original shape
volume, exact reopening/suppression and certified-cache rollback. State and occurrence scenarios
also physically reimport flat and structured STEP; no native Qt acceptance is inferred from these
backend checks.

Runtime manifest: **1,326 inputs**, SHA-256
`aba39f3a3e9a5688217f8093c2f3a91f3bc2e156abf1d67f16f66dff2786c451`.
Raw local evidence is `/tmp/blcad-through198-{core-full-01,core-focused-01,geometry-focused-02,geometry-regression-02}.log`
and `/tmp/blcad-through198-runtime-02.json`. Focused sources are
[Direct Edit Geometry](../tests/geometry/direct_edit_tests.cpp),
[variant Geometry](../tests/geometry/model_state_tests.cpp) and
[occurrence Geometry](../tests/geometry/assembly_occurrence_pose_tests.cpp).
These initial results establish the backend foundation only. Subsequent complete/native
acceptance and delivery are recorded in the MVP plan.

## Owned native workflow

`ShellDirectEditTools` owns the Model → Direct Edit action, compact mode toolbar, scrollable property
form and pinned Preview/Apply/Cancel controls. Available and selected faces retain semantic keys;
human labels show analytic carrier type and physical location. Move/Rotate/Offset/Size/Delete modes
retain inactive values while candidates include only the active operation. Radius and diameter are
explicit Size choices. Reference Parts explain the existing STEP Sources conversion action; they
never convert as a side effect of an edit attempt.

Move uses signed XYZ numeric fields in the authored frame. Rotate exposes a local axis, reference
origin and Angle binding. **Align frame to selected face** explicitly places the triad at a current
face station and assigns a right-handed orientation; **Use world frame** restores the world origin
and axes. Users can edit exact origin and X/Y directions. Formula values are read-only until the
user explicitly selects New value. New positive Length parameters plus operation signs represent
zero/negative displacement without weakening the existing parameter contract.
An inactive zero Move component preserves its bound parameter magnitude and records a zero axis
sign. Offset and Size reject zero; other finite magnitudes are preserved without silent clamping.

The task captures its entry model revision, filter/scope and selection. Preselection and command-first
face capture use the same target catalog. Double-click/History Edit restore the exact Direct Edit
record and its real input prefix. The task previews the complete candidate history, shows every
visible final Body and reports the number of checked downstream features. Apply rechecks that history
and publishes one normal document/history transaction. Failed edits retain fields and original intent,
disable Apply and mark the actual selected faces. Cancel restores selection/filter/scope and releases
the preview. Task changes or external model revisions invalidate the draft.

The physical triad shares the normal viewport manipulator mapping and numeric-unit parser. Move has
three axis handles; Rotate has an angular handle; Offset/Size use the selected face's physical station
and normal, with a half-unit factor for diameter. Numeric release retains the last exact candidate.
Escape, lost capture, window deactivation, wheel and resize restore the starting draft sample.
Ctrl+Enter applies only a current valid preview; Repeat restarts the normal creation owner.

Creation currently appends at the end of the authored history; an earlier End of Part position asks
the user to move it to the end first. Editing a feature before the marker still validates its complete
suffix. This boundary does not change Core reorder/suppression authority or allow partial execution
to certify the complete model.

The [extension inventory](gui-direct-edit-coverage-mvp9.json) adds current creation/editing and
mutation proof to the existing inventories without changing historical accepted counts.

## GUI proof sources

`[direct-edit-tools]` in [the owned-tool tests](../tests/gui/gui_direct_edit_tools_tests.cpp) covers
disposable preview, normal Apply/Undo/Redo, edit dispatch, failed/corrected inputs, parameter binding,
exact save/open and filter restoration. `[native-direct-edit]` additionally exercises physical triad
and numeric release, capture loss, frame alignment, Rotate correction and compact completion controls.
`[generic-direct-edit-source]` selects Direct Edit and Emboss through the normal generic Pattern
source field, checks the independently expected copied volume and Undo/Redo, and returns to each
feature's owned History/Edit workflow without changing its record.
The [imported workflow tests](../tests/gui/gui_direct_edit_import_tests.cpp) exercise native
radius/diameter Size, checked bore healing, failed exterior healing/oversize, immutable source bytes,
normal shared-source refresh, failed removed-face refresh and physical STEP reimport. Execution counts,
capture review and the frozen source identity belong to the acceptance record added after testing.


## Advanced Part combination fixture

[The shared fixture](../tests/support/direct_edit_integrated_fixture.hpp) combines native and
EditableBody 20×10×8 plates, a direct underside offset, an actually captured top-support Sketch/boss,
a cosmetic threaded Hole, cross Web and side Hole, G2 Full Round, an independent variable-radius
outer edge, planar Emboss, Body Pattern/placement, an offset skin and its Thicken consumer, and a
pinned material. Each consumer is created after actual predecessor execution. Trimmed-face footprint
queries select the intended physical patch while persisted references remain semantic.

[The integrated test](../tests/gui/gui_advanced_part_direct_tests.cpp), `[advanced-part-direct]`,
uses analytic removed/added volumes, a quadratic integral of the linear radius law, the independent
G2 round coefficient, density-derived mass and physical STEP reimport. It checks native and imported
fixtures, normal native Direct Edit suffix preview/editing, exact Undo/Redo/save/open, full versus
planned recompute, safe suppression and independent reordering, unchanged imported base records and
compatible versus removed-selected-face source refresh. This preserves the original 188 D02/X01
requirements; the fixture source alone is not a passing numbered acceptance record.


The focused `[direct-edit-copy-protection]` Core regression constructs actual Hole Pattern and
nested Mirror ancestors, rejects direct Size/Offset and malformed lineage atomically, then changes
the original Hole parameter successfully. `[direct-edit-existing-blend]` Geometry proof uses a real
imported box/bore with an existing radius-1 outer fillet: bore Size and Delete-and-Heal retain the
unselected fillet's cylinder axis, radius and quarter-cylinder area, while analytic material change,
base BRep bytes, source bytes/catalog and exact replay remain checked.

## Literal rotor advanced Part bridge

[The separate rotor fixture](../tests/support/advanced_rotor_fixture.hpp) reuses the existing
performance rotor's revolved hub and circular blade-pattern intent without changing that historical
fixture. Its local blade uses an exact 10 × 4 mm rectangular root neck followed by translated
circular sides, 4 mm apart and tangent to the neck. A real variable-radius root/top fillet is
authored on the rectangular neck before the curved tip joins the same editable source Body. A
Direct Edit offsets the analytic hub underside. The actually selected hub top receives a semantic
cosmetic-threaded mounting Hole, circular Hole Pattern, raised bundled-outline **R** and engraved
parameter-bound blade count before the final curved-blade pattern. This order preserves the
identification support's contributor set when blade count changes. The final visible result is
one connected rotor; the editable source blade remains hidden. Material assignments are pinned.

[The Geometry regression](../tests/geometry/advanced_rotor_tests.cpp), `[advanced-rotor]`, uses an
independent circle section, the circle/rectangular-neck intersection integral, the quarter-circle
integral of the linear radius law, and the existing Green-integral outline oracle for glyph volume.
It checks native validity and self-interference, curved blade sections and root circle witnesses
on every blade, patterned Hole axes and voids, rotor Body mass and the separately retained hidden
source contribution to Part totals. Blade count, root radius, hub height and direct depth edits
replay the complete suffix; rejected radius geometry preserves the certified cache. Independent
Body reordering, invalid producer reordering, engraving suppression, planned/fresh replay, strict
file reopen and visible-body STEP reimport retain the same physical oracle. Test source is not
execution evidence.

This arc-blade rotor is separate from the plate/housing fixture above. The actual equation/3D-guide,
constrained Loft, Offset/Thicken and differential proofs remain in the Block-174 curve/surface
milestone. The Full Round proof remains in the integrated plate. They are not represented as one
rotor; the complete combined X01/model-state/native end-to-end scope remains for Block 204.
Numbered acceptance remains owned by the MVP plan.
