# Checked Rib and Web Geometry

The [typed intent](part-rib-web-intent-mvp6.md) owns persistence and dependencies. `mvp-plan.md`
owns numbered acceptance. Construction is implemented; the complete Block-181 consumer/exchange
matrix remains in progress. Native authoring belongs to 184.

## Construction and capability boundary

| Input / mode | Construction policy |
| --- | --- |
| Perpendicular straight, arc, rational cubic or equation skeleton | Native curves in the resolved Sketch frame; signed offsets of the complete connected planar wire. |
| Connected open chains | Miter joins, explicit first/second sides and end connectors; ambiguous endpoint branches reject. |
| Closed profile skeleton | Outer offset minus the inner band; preserve the actual enclosed material. A collapsed inner offset does not become a filled disk. |
| Parallel extrusion | Straight planar curtains, with thickness along the Sketch normal; collapsed/tangent or nonplanar curtains reject. |
| Draft | Straight/circular skeletons and checked ruled section volumes; arbitrary spline draft remains unsupported. |
| Multiple curves / Web crossings | Explicit native union or rejection of overlapping bands. Every result must remain connected to the target. |
| To face | The selected actual planar face must cover the complete native section. |
| To Body | First intersected forward planar support, normal to extrusion, covering the entire section. Partial/curved/ambiguous first support rejects. |

Direction is normalized only in Geometry and transformed from the source Sketch frame. The
existing resolver carries explicit Sketch/support placement. Moving a target Body does not
implicitly move an independent world Sketch.

Perpendicular side offsets are authored-left/right relative to the Sketch normal and ordered
spine. Symmetric thickness uses ±t/2; First uses +t/0; Second uses 0/−t; Asymmetric uses +t1/−t2.
Draft adds `z*tan(angle)` to each outward side. Total width is therefore linear in depth; both
end sections must retain more than the numerical thickness threshold. Native validity and
self-interference checks reject collapsed, branched or crossing offset regions.

The native offset carrier retains its source wire and distinguishes open versus closed results.
Physical side selection is checked against the authored curve's tangent and Sketch normal.
Connected wire edges retain their builder-produced source correspondence for generation history.
These choices preserve actual miters and provenance; independent square-ended edge boxes would
change corner material.

Body bounds only bound an oversized termination tool. Actual face intersections choose the first
support; the existing finite-face clipping check proves its entire section. A failed first support
cannot silently select a farther face. An earlier placed Body derived from the same target retains
its original predecessor through normal historical input snapshots.

## Checked publication and semantic consumers

Construction deep-copies the target and uses nondestructive Booleans. Require native validity,
absence of self-interference, unchanged solid count and positive added material. Disconnected and
no-op candidates preserve the old cache. Unrelated Bodies and imported source ownership remain
under the normal executor's freshness/transaction rules.

New side/end/base/cap roles derive from authored curves/profiles. Boolean history carries existing
face origins. When a source face splits, its old origin remains as nonselectable provenance so
old ambiguous references still reject. New fragment identities use the authored contributors and
actual positive-length contacts with pre-Boolean face witnesses. Contact tests use bounded native
sections and exclude area overlap and point contact. No topology traversal index becomes identity.
Unresolved repeated signatures remain ambiguous.

The same normal executor stores feature and Body results for Rib and Web. History prefixes,
parameter edits, suppression/reorder, downstream recompute and strict Part/Project replay use the
ordinary authority. Hole/Fillet/full-round and physical exchange acceptance remain under verification.

## Verification

Initial construction matrix: **8 cases / 78 assertions**, including independent visible material
increments: straight 64 mm³, asymmetric 192, parallel 152, quarter-circle `8π`, cross Web 92,
open miter 48 and closed rectangular band 120. Draft integrates the actual linear thickness law.
An earlier Thicken-derived/placed limit gives 128 mm³ and retains its own 200 mm³ Body; a thickness
edit and explicit owned Sketch placement replay correctly. Source BRep bytes and rejected candidates
remain unchanged. Logs: `build/verification/block181/geometry-closed-final.log`.

Construction/finishing regression passes **37 / 9,788**; complete Core **804 / 35,384** and focused
Qt **13 passed / 1 skipped / 314 assertions** pass. All five targets build. Frozen foundation
runtime: **1,270 files**, SHA-256
`0f628424e62630f25ad3be3ef086d08b50661cbb4cc07a4e72b762d1ef7be56d`.
The complete consumer/import/exchange matrix and final full Geometry gate remain pending;
`mvp-plan.md` retains Block 181 in progress.
