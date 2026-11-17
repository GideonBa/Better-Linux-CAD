# Shared advanced finishing interaction — Block 179

Implementation is in progress; `mvp-plan.md` owns numbered acceptance. The accepted
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

The connected panel passed its first native create/edit/drag/role matrix at both scales;
explicit reopening into failed-Geometry repair and scrolled compact controls remain under verification. The implementation route continues
through Block 184; none of this advances later blocks.

## Verification status

All five targets built. Typed-draft and legacy finishing/shell/draft/edge-chain regression:
**10 cases / 191 assertions** (`build/verification/block179/draft-final.log`). Guide and variable
Geometry regression: **17 / 9,424** (`guides-final.log`). Five guide cases cover straight/reversed
stations, original basis, explicit edit scope, a nonuniform imported split cubic with independent
Bernstein/Simpson length, periodic seams, stale sources and derived full-round size. Native panel
acceptance remains open; continue through 184 and retain 185–204 scope.

The first owned-panel matrix passes **10 offscreen cases / 190 assertions** and **11 native cases /
221 assertions** at 100% and compact 150%. Selection/face Geometry passes **14 / 494**, including
actual Stitch/Thicken ancestors and a coincident unrelated Body negative. Captures confirm fixed
Apply/Cancel and the physical source-direction guide; scrolled station/role captures and failed
reopening are the remaining native checks. Logs: `panel-focused.log`, `selection-final.log`,
`native100.log`, `native150.log` in the same Block-179 verification directory.
