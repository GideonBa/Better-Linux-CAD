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

## Remaining native integration

Connect the typed candidate to the existing owned property panel: constant/variable/face/full-round
modes, ordered groups and members, dirty-cell-only station editing, binding/value controls,
propagation/interpolation, role-specific face sets, hold lines and the supported analytic setback
policy. Show actual source-length station anchors, original-basis direction, derived full-round
size and actionable capability/failure feedback. Preserve unavailable inputs for explicit repair.

Existing-feature editing must use its evaluated input-history prefix, then validate the complete
downstream model. Native acceptance requires real station drags, face roles, repeated picks,
Cancel/Escape and selection restoration, upstream changes, repair, complete Save/Open/history/STEP
and compact property panels at both 100% and 150%. These gates remain open.

## Verification status

All five targets built. Typed-draft and legacy finishing/shell/draft/edge-chain regression:
**10 cases / 191 assertions** (`build/verification/block179/draft-final.log`). Guide and variable
Geometry regression: **17 / 9,424** (`guides-final.log`). Five guide cases cover straight/reversed
stations, original basis, explicit edit scope, a nonuniform imported split cubic with independent
Bernstein/Simpson length, periodic seams, stale sources and derived full-round size. Native panel
acceptance remains open; continue through 184 and retain 185–204 scope.
