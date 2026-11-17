# Native sketch-driven features

Block 184 owns shared Rib/Web and Emboss/Engrave creation/editing in the current desktop shell.
The [Rib Geometry](geometry-rib-web-mvp8.md), [mapped material Geometry](geometry-emboss-engrave-mvp8.md)
and [typed intent](part-emboss-engrave-intent-mvp6.md) remain the model authorities. The [main plan](mvp-plan.md)
records acceptance through 184; the evidence below closes its F03/UI01 gate.

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

## Verification

The Geometry guides pass **2 cases / 80 assertions**. The shared controls and combined native
workflows use `[parity-b184]`: creation/editing through normal History Edit, exact numeric grips,
Escape/lost capture, Ctrl+Enter, Repeat, contextual analytic supports, source-Sketch return with
both Apply and Cancel, parameter-driven pinned text, mass, literal cylindrical seam, invalid mapping,
qualified termination repair, exact Undo/Redo, save/open and STEP. Generated Rib edges feed the
ordinary Fillet tool; a crossing Web feeds the ordinary Hole and G2 Full Round tools and retains
the downstream result after an upstream thickness edit. An End-of-Part insertion validates its
complete transformed suffix before publication. Formula values stay read-only until explicit rebind.

Accepted on 2026-11-17 after all five targets build:

| Check | Cases | Assertions |
| --- | --- | --- |
| Complete Core | 810 passed | 35,441 |
| Isolated complete Geometry | 850 passed | 91,829 |
| Complete offscreen GUI | 903 passed / 90 skipped | 104,772 |
| Native xcb 100% | 11 passed | 466 |
| Native xcb compact 150% | 11 passed | 466 |
| Existing Extrude numeric HUD regression | 1 passed | 22 |

Both native runs assert full input-field visibility and reachable completion controls; captures
were inspected. The seven native-only Block-184 cases skipped offscreen execute at both scales.
This evidence does not establish every legacy native-only case among the remaining skips.
Separate xcb app startup remains healthy until its intentional 8-second timeout (exit 124).

Final runtime: **1,300 files**, SHA-256
`f99dbd52b6f2579f37893d32a9ceb80a8b897359cf311e873219c4e3b78c861d`. The runtime manifest was
rechecked unchanged before delivery. Core, Geometry and STEP-export executable digests also retain their verified
identity across the final GUI overlay correction. The owned overlay has its own widget identity;
the existing Extrude numeric HUD and the complete GUI suite resolve their correct visible widgets.

Evidence directory: `build/verification/block184/`. Final logs: `core-full-final.log`,
`geometry-full-final.log`, `gui-full-final.log`, `gui-native100-owned-overlay.log`,
`gui-native150-owned-overlay.log`, `overlay-regression.log`, `app-startup-final.log`.
Captures: `native100/`, `native150/`. Identity: `runtime-final.json`, `binary-digests.json`.
