# Checked Emboss and Engrave Geometry

[Typed intent](part-emboss-engrave-intent-mvp6.md) owns persistence and units. The [main plan](mvp-plan.md)
owns numbered status. Block 183 is accepted with text, combined consumer and complete Geometry regression proof.
Native authoring belongs to 184.

## Bounded native construction

Planar projection and explicit cylinder/cone charts use native analytic carriers. Rotated source
curves map into bounded UV contours. Native region intersection and containment establish nesting;
partial overlap, coincident contours, touching ambiguous loops and invalid/self-intersecting regions
reject. Material alternates with nesting depth, preserving counters and separate glyph components.

Base and normal-depth caps retain the analytic plane/cylinder/cone. Corresponding edges bound ruled
side faces. Spatial boundary curves require independent curve-on-surface maximum error ≤1e−7 mm.
Only newly constructed tools are sewn at 1e−7 mm. Closed-solid orientation, validity and
self-interference checks precede the material operation; target geometry is never healed.

The complete base-patch area must lie on the union of the selected **actual trimmed support faces**.
A support hole or uncovered region rejects. An infinite analytic carrier never substitutes for a
finite selected support. Multiple supports require a common compatible carrier and material normal.

Depth follows the material normal, accounting for face orientation and carrier handedness. Outer
cylinder Raise increases radius; bore Raise decreases it. For a cone, signed normal depth δ changes
reference radius by `δ cos(beta)` and shifts its axis origin by `−δ sin(beta)`; meridian parameter v
is unchanged. Collapse, apex crossing and a complete overlapping circumference reject explicitly.

The transient periodic chart is rotated so the complete contour bounds lie within one period.
Its pcurves receive the opposite phase shift, leaving every physical point and the authored seam
unchanged. This preserves exact cylinder/cone geometry and avoids ambiguous parameter unwrapping
when a contour crosses the original carrier seam. Core literals remain untouched.

Material publication deep-copies the target and uses nondestructive Fuse/Cut. Every raised component
must join the target; a recess must retain positive connected material. Unchanged solid count,
positive added/removed volume, native validity and self-interference are mandatory. Normal executor
transactions retain immutable imported bases, source freshness and the last accepted cache.
Generated cap/side origins derive from authored profile curves or pinned text/glyph/contour order.
Physical fragment witnesses use the shared lineage algorithm with a separate Emboss origin prefix.

## Reproducible text outlines

The Geometry-only FreeType dependency reads exact pinned bundled bytes. Source/install asset roots
are explicit; an optional `BLCAD_FONT_DIR` override still requires the authored SHA-256. Font reads
are size-bounded before allocation. Missing bytes, digest mismatch, missing glyphs and malformed
UTF-8 reject. Material geometry has no ambient font fallback.

Unscaled, unhinted vector outlines use authored em height and unscaled kerning. Quadratic segments
convert exactly to cubic Bézier curves; rasterization never defines material. Explicit contours
retain holes and dots. C callbacks contain exceptions and rethrow them in the C++ boundary.
This is deterministic glyph sequencing, not a general complex-script shaping engine.
Core-only builds do not discover or link FreeType. Geometry installation includes fonts, licenses
and the existing source manifest under `share/blcad/fonts`.

## Verification

Foundation **5 cases / 148 assertions** pass: independent ±6 mm³ planar material and depth edit,
cylinder `w h (d ± d²/(2R))`, cone `w h d ± w h cos(beta) d²/(2Rref)`, reversed bore normals,
canonical reopen and physical seam-crossing points before/after STEP. Imported base bytes remain
unchanged. Wrong analytic mapping, uncovered finite supports and an actual whole-Body erasing
recess preserve the last cache. Logs: `build/verification/block183/geometry-foundation.log`.

All five targets build. Core **810 / 35,441**, Rib/Web regression **16 / 202** and focused Qt
**13 passed / 1 native skip / 314** pass. Foundation runtime **1,287 files**, SHA-256
`2ea725df6b4ccca9afd077769e25b739369684328669c7fee6b916a7ef635d68`.

The complete matrix passes **12 cases / 239 assertions**. An independent Green-integral oracle
uses font outline polynomials without OCCT curves, nesting or solids; it verifies O/B counters,
the separate i dot, literal and parameter-driven text, changed height, physical mass and STEP.
Composite profile holes retain analytic area. Complete-circumference wrapping, the conical apex,
actual finite support holes, missing/corrupt pinned font bytes and erasing recesses reject atomically.
Emboss/Engrave after Shell or Fillet retains independently known material increments after upstream
size changes and canonical reopening.

Complete isolated Geometry: **848 / 91,749**. Core **810 / 35,441** and focused Qt **13 passed /
1 native skip / 314** retain the foundation result; subsequent changes affect only Geometry tests.
All five targets build. A separate Core-only configure with FreeType discovery disabled succeeds.
Final runtime **1,289 files**, SHA-256
`9115edaf90a878def3d761e0b513cfd6a3dae46d35a636a8f944b6db6402ff14`.
Final logs and manifest: `build/verification/block183/`. These checks close 183; combined native
creation/editing, grips, reference repair and F03/UI01 remain 184.
