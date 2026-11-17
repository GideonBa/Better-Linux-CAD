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

All five targets built. The controller foundation plus legacy Fillet/Chamfer/Shell/Draft and
edge-chain picking regression pass **10 cases / 191 assertions** with offscreen Qt. Logs:
`build/verification/block179/{build-draft-final,draft-final}.log`. No native property-panel
acceptance is implied by these controller tests. Continue through 184 and retain 185–204 scope.
