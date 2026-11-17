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

## Verification and remaining gate

The guide/controls foundation passes **2 Geometry cases / 80 assertions** and **1 Qt case /
9 assertions** after all five targets build. It checks physical sides, binding units, raise/recess
normals, six planar support targets, retained inactive values and fixed completion controls.
Logs: `build/verification/block184/guide-final.log`, `controls-final.log`.

The native owner now provides Model actions and contextual selection, normal double-click editing,
entry-revision guards, complete-history candidate preview and atomic publication. Source Sketch
visits remain inside the parent workflow; Cancel restores its checkpoint. Qualified repair enters
before the normal freshness guard and computes the real input prefix. Completion controls remain
outside scrolling; the active grip has a numeric field with units.

Initial owner verification passes offscreen **1 / 33**, native **1 / 31** and combined existing
finishing/controls regression **15 / 356**, after all five targets build. The native test exercises
an exact bound 1.2 mm thickness, preview-only edits, Escape, Undo/Redo and unchanged reopening of
the editor. Logs: `build/verification/block184/owner-*`.

Complete native text/wrapping, termination repair, source return, generated consumers and both-scale
F03/UI01 acceptance remain in progress. The main plan retains 184 as the active block.
