# Native sketch-driven features

Block 184 owns shared Rib/Web and Emboss/Engrave creation/editing in the current desktop shell.
The [Rib Geometry](geometry-rib-web-mvp8.md), [mapped material Geometry](geometry-emboss-engrave-mvp8.md)
and [typed intent](part-emboss-engrave-intent-mvp6.md) remain the model authorities. The [main plan](mvp-plan.md)
retains 184 in progress until complete native F03/UI01 acceptance.

## Owned controls and physical guides

One owned property component supplies ordered curve/support selectors, explicit source Sketch,
profile or feature-owned SketchText, target Body, typed parameter bindings and mode controls.
Rib/Web exposes thickness sides, local direction, draft, crossing policy and actual termination.
Emboss/Engrave exposes chart, literal seam/anchor, pinned font, token bindings and physical depth.
Inactive controls retain draft values; only active mode inputs enter the candidate definition.
Formula bindings remain read-only; choosing New value is an explicit rebind.

A Geometry query returns disposable guides with Core IDs, physical anchors/directions, parameter
bindings and authored units. Symmetric thickness represents half the authored value on each side;
Second-side thickness points to the physical second side. Raise/Recess depth follows the material
normal. Periodic seam guides retain the authored angle and the original carrier axis, independent
of transient UV chart rotation. GUI code receives no native topology types.

Analytic face targets carry qualified semantic references and regular interior normal stations.
Human labels describe carrier type and physical position; private fragment encodings do not become
property-panel labels. The original planar Sketch-support selector remains planar-only.

## Task, history and repair behavior

`ShellSketchDrivenTools` owns the task and controls for all four modes. Model actions, qualified
context selection, browser double-click and History Edit route to this owner. The task captures its
entry revision and selection; external changes invalidate it. Ctrl+Enter applies the checked draft;
Repeat Command restarts the accepted creation action through its normal owner. The first picked
analytic support sets an empty Emboss/Engrave chart; a Rib/Web face pick sets planar termination.
Candidate execution checks the full
downstream history before Apply publishes one atomic transaction. The viewport shows the edited
feature at its input prefix, with the other visible prefix Bodies available for actual termination
picks. End-of-Part insertion validates the suffix before publishing its outer history command.

The source-Sketch breadcrumb keeps the parent draft and joins accepted child changes into one parent
undo boundary. Cancelling the parent restores the exact checkpoint, including child edits and redo
state. Formula-driven bindings remain read-only in fields and grips; choosing New value explicitly
creates another parameter. The active grip accepts typed measures; Escape and lost mouse capture
restore its starting value without changing committed intent.

Analytic support choices follow the selected mapping. Ordered selected references retain their
identity when temporarily incompatible so a failed candidate can be corrected explicitly. The
finite trimmed support union must cover the whole mapped outline; periodic carriers can require
several quarter patches. **Center along axis** explicitly copies the first selected support's
cylinder coordinate or cone meridian station into Anchor Y. It preserves the authored seam.

Feature repair starts before the normal fresh-model guard, reconstructs the real prefix and keeps
all visible support Bodies selectable. Failed joins/mapping show qualified physical source/support
markers and disable Apply. Undo can restore the exact invalid reference; Redo restores its repaired
fresh model. Font, geometry and topology limits remain those of the linked Geometry contracts.

The [current extension inventory](gui-sketch-driven-coverage-mvp9.json) links Core CRUD, the owned
shell, normal edit entry points and registered proof filters. It extends the historical inventories
without changing their accepted family counts.

## Verification and remaining gate

The Geometry guides pass **2 cases / 80 assertions**. The shared controls and combined native
workflows use `[parity-b184]`: creation/editing through normal History Edit, exact numeric grips,
Escape/lost capture, Ctrl+Enter, Repeat, contextual analytic supports, source-Sketch return with
both Apply and Cancel, parameter-driven pinned text, mass, literal cylindrical seam, invalid mapping,
qualified termination repair, exact Undo/Redo, save/open and STEP. Generated Rib edges feed the
ordinary Fillet tool; a crossing Web feeds the ordinary Hole and G2 Full Round tools and retains
the downstream result after an upstream thickness edit. An End-of-Part insertion validates its
complete transformed suffix before publication. Formula values stay read-only until explicit rebind.

The final native **100% and compact 150% runs each pass 11 cases / 466 assertions**, including
whole-field visibility checks and reviewed captures. Complete Core passes **810 / 35,441**. All five
targets build. Complete Geometry/offscreen GUI, separate app startup and final runtime identity remain
the acceptance gate. Native evidence: `build/verification/block184/gui-native{100,150}-accepted.log`
and `native{100,150}/`. The main plan retains 184 in progress until the remaining gates pass.
