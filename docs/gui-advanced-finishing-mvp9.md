# Shared advanced finishing interaction — Block 179

The shared interaction is accepted in Block 179; `mvp-plan.md` owns numbered status. The accepted
[typed intent](part-face-full-round-intent-mvp6.md) and
[Geometry capability table](geometry-face-fillet-mvp8.md) remain authoritative.

## One typed candidate and transaction

`GuiFilletDraft` stages an immutable `FilletFeature` record, explicit parameter bindings,
new Length parameters and changed values. The existing finishing controller previews and applies
this typed candidate through the normal document transaction. Full-round bypasses the legacy
radius requirement and owns no competing scalar parameter. Cancel changes no owned intent.

Untouched group/member order, station IDs, raw fractions, endpoint roles, interpolation,
propagation and original station basis remain copied from the owned record. A no-op draft cannot
round a fraction such as `1e-20` through a displayed spin-box value. Such a fraction may still be
geometrically unresolved; preserving authored intent does not authorize an invalid Apply.

Canvas handles identify their exact radius binding. Moving one stages that binding and updates
other linked handles. Failed Geometry cannot replace the document, cache or command history.
The normal transaction provides exact Undo/Redo, including parameters provisioned by creation.
Pending/changed values must belong to the active mode; incompatible mode fields cannot leak into
Apply. Formula-driven values require an explicit rebind.

The independent-values choice creates a separate Length parameter for each changed existing
binding and reconstructs the same typed record. Every station sharing that binding follows the
same new parameter. Existing formulas and other consumers retain their original parameters.
New parameters, scalar changes and the typed add/update belong to one transaction.

## Physical guides and explicit modeling scope

`FilletGuideQuery` measures the same expanded, oriented source contours as execution, in true
curve length. It returns stable group/station bindings, physical anchors and original-basis
directions. Reversing a group preserves the authored law's physical station positions. Closed
contours retain their seam. Full-round size is read from actual planar/coaxial side supports;
G2 displays half-width rather than claiming a constant circular radius.

Queries require a matching evaluated model or explicit input-history prefix. A stale cache or
unqualified prefix rejects. Returned data is transient canvas geometry and never becomes
persistent topology identity. Independent imported cubic/periodic oracles exercise actual native
curves and physical length rather than production parameterization.

## Owned panel and editing

Model → Fillet offers Constant Radius, Variable Radius, Face Blend and Full Round in the same
owned panel. Variable groups retain authored order, member direction, interpolation and propagation.
The station table edits endpoint/fraction roles and explicit parameter bindings. Full precision
formatting and dirty-only writes preserve untouched raw literals. Reversal changes the chain basis
through the existing Core operation. Only the active group's grips appear on the model; the
read-only Law Direction arrow shows the original station basis and cannot intercept radius drags.

Face roles have separate ordered selections. Repeated picks remove a member; assigning a face or
edge already used by another role/group rejects without changing that intent. The panel explains
G1 radius versus G2 contact-setback meaning, derived full-round size and the orthogonal three-edge
setback capability. Hold-line choices bind owned paths. Switching modes filters staged parameters
and values to the active record's dependencies. Common fields retain their identities; new groups,
stations and bindings get fresh IDs. Inactive fields never become owned dependencies.

Editing restores the complete owned record and evaluates its input-history prefix. Its target
Body remains fixed while editing. Candidate publication recomputes the complete downstream
history. Cancel/Escape discard the candidate and restore the entry selection and filter/scope.
Dragging stages the exact station binding; linked values synchronize, controls are disabled during
the drag, and release refreshes physical anchors from the updated source prefix. Finishing selection
also resolves actual surviving semantic faces from selected Stitch/Thicken ancestry; coincident
unrelated Body producers cannot supply those references.

## Failed Geometry, history scope and model changes

Open for Repair initially owns the ordinary References task. Close that task, then double-click
an owned Fillet in History. Its source prefix must recompute successfully; only that qualified
input geometry is shown while the complete model remains failed. Correct the typed inputs or
values and Apply through the existing repair transaction. Publication requires the complete
result to recompute. Cancel preserves the failed record; Undo/Redo retain the failed/repaired
states and reopening the repaired file uses the normal strict reader.

The panel holds an immutable copy of its entry document and checks both document identity and
model revision. External model changes invalidate the task before another preview or Apply.
Creation at End of Part prepares the normal history insertion before deriving source guides;
later Body placement cannot move grips ahead of the marker. Existing History context commands,
feature renaming and suppressed-Fillet value correction use this same owned panel.

## Verification

The final native matrix passes **38 cases / 2,745 assertions** at 100% and **38 / 2,765**
at compact 150%. After settling posted Qt layout events before scrolling, the affected 100% subset
passes **14 / 349**. Captures show station values, face roles, fixed Apply/Cancel, repair inputs and
Setback controls. Coverage includes linked radius drags, repeated selection, mode changes, exact
history/Save/Open, failed reopening, earlier End of Part, stale drafts, suppressed edits and atomic
Setback correction. Logs and captures live in `build/verification/block179/`:
`native100-final-all.log`, `native150-layout-final.log`, `native100-layout-final.log`.

Full Core **798 / 35,311** and full Geometry **820 / 91,308** pass. Their binary digests remain
unchanged after the final UI-only corrections. Full offscreen GUI passes **899 cases, 83 skips / 104,660 assertions** (982 total, exit 0);
`gui-full.log`. The native filters above establish the relevant finishing workflows, not the entire
native-only suite. All five targets build successfully. The final runtime manifest contains **1,255 files**, SHA-256
`ece7fcb2929f3e76bb2dea6ce844a339a4eb02405a3e034bf802b454eab9ded0`.

An additional large-window 150% run passed 35 of 37 cases; two existing History size-preservation
cases observed desktop resizing from 1232×651 to 1280×674. The complete compact repeat passes
without changing those assertions. Compact verification uses `QT_SCALE_FACTOR=1.5`, repository
`BLCAD_UI_COMPACT=1` and fixture `BLCAD_COMPACT_UI=1`. This evidence qualifies the stated setup;
it does not claim every desktop/window-manager geometry.
