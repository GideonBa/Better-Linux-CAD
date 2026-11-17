# Checked Rib and Web Geometry

The [typed intent](part-rib-web-intent-mvp6.md) owns persistence and dependencies. `mvp-plan.md`
owns numbered acceptance. Block 181 is accepted, including ordinary semantic consumers and physical exchange.
Native authoring belongs to 184.

## Construction and capability boundary

| Input / mode | Construction policy |
| --- | --- |
| Perpendicular straight, arc, rational cubic or equation skeleton | Native curves in the resolved Sketch frame; signed offsets of the complete connected planar wire. Single smooth spline offsets use a bounded native approximation of the exact normal-offset law. |
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
Connected wire and ribbon edges retain their builder-produced source correspondence for generation history.
Spline offsets require planar approximation error ≤1e−8 mm and independently checked spatial
curve-on-surface error ≤1e−7 mm; invalid bands reject before extrusion.
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
ordinary authority. Captured termination faces also depend on their owned Body availability.

Rib-defined face partitions survive subsequent Boolean consumers. This retains the physical arm
regions needed for full-round finishing after a Hole; ordinary models keep their existing coplanar
simplification. A selected unbranched cross-Web arm supports G1 and G2 full rounds. Selecting the
whole branched cap rejects atomically rather than inventing a four-way blend.

## Verification

The final construction/consumer matrix passes **16 cases / 202 assertions**, including straight,
asymmetric, parallel, arc, inflected spline, open miter and closed rectangular/circular bands,
draft and minimum-thickness rejection. Independent arclength integration checks the spline volume.
Cross-Web → generated-face Hole → G1/G2 full round retains analytic material after a thickness
edit, exact canonical reopen and physical two-Body STEP replay. Ordinary edge Fillet and Shell
consume generated supports. Imported immutable bases, stale digest rejection and unrelated Body
bytes remain intact. Partial first Body termination rejects; explicit farther-face repair succeeds.

Complete Core: **805 / 35,388**. Complete isolated Geometry: **836 / 91,510**. Finishing/construction
regression: **37 / 9,736**. Focused Qt: **13 passed / 1 skipped / 314 assertions**. All five targets
build. Logs and canonical runtime manifest: `build/verification/block181/`. Frozen runtime:
**1,272 files**, SHA-256 `eb460d08c4225d438c060c10902a199b8a2c8abc8e1b16f70ee43addfc3f813d`.
These checks close 181. [Native authoring and integrated F03/UI01](gui-sketch-driven-features-mvp9.md)
are owned by 184.
