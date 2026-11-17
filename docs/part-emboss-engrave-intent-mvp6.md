# Emboss and Engrave intent

Block 182 extends the normal immutable `SketchDrivenFeature` authority introduced for Rib/Web.
The [main plan](mvp-plan.md) owns numbered acceptance; checked construction belongs to 183 and
native authoring to 184. Core contains no font rasterizer or kernel dependency.

## Owned source and target

A source is exactly one existing Sketch profile-region reference or an embedded existing
`SketchText` attached to an owned Sketch. The latter preserves template, explicit token bindings,
height/rotation bindings, anchor and requested font. This is a feature-owned use of the existing
Sketch text contract; it does not invent a global Part text registry. Generated glyph BRep is
never persisted.

The target is one owned Solid Body. An ordered, unique set of semantic face references identifies
its support. Foreign semantic, captured-result or imported support ownership rejects. Captured
supports also retain Body availability dependencies. Normal historical target snapshots prevent
self-cycles; update, suppression, reorder and source edits use ordinary replay and atomic rollback.
Reference-mode Parts retain their existing editing restriction.

Raise and Recess are explicit operations of the same definition. Depth requires a positive Length;
orientation requires an Angle. Text height and token bindings use existing SketchText resolution.
Missing source, support, parameter or font diagnostics retain authored identities. Geometry checks
the actual carrier capability and material-normal direction; Core does not infer a surface from IDs.

## Mapping units and reproducibility

| Mapping | Authored chart |
| --- | --- |
| Planar | Project resolved Sketch coordinates into the actual target plane. Anchor is in target-plane millimeters; orientation rotates the source before projection. There is no periodic seam. |
| Cylindrical | `u = (rotated x + anchor.x) / R + seam`, `v = rotated y + anchor.y`; seam is in degrees in intent and radians during construction. |
| Conical | `u = (rotated x + anchor.x) / Rref + seam`, `v = rotated y + anchor.y`; v is meridian arclength and `Rref = R0 + anchor.y sin(beta)`. Hoop scale varies along the cone. |

Raw finite anchors and seam literals round-trip exactly, including tiny values and multiple turns.
No normalization on save changes seam identity. Physical depth is measured along the oriented
material normal, including reversed bore faces. Arbitrary doubly curved wrapping requires a later
specified extension; unsupported carriers do not switch to planar mapping.

Text persists an asset name and exact SHA-256 for bundled Barlow or IBM Plex Mono. Requested family,
asset and digest must agree. Unknown ambient fonts and mismatched bytes reject; there is no system
font substitution in material geometry. Existing licensed assets and their manifests remain the
authority. Geometry 183 owns deterministic vector outline decoding and missing-asset diagnostics.

## Persistence and verification

The necessary Part V15 envelope remains shared with Rib/Web. Each Emboss/Engrave record has a strict
11-field shape, discriminated source, explicit mapping, ordered supports, bindings and literal
anchors/seam. Embedded text reuses the existing text serializer/parser with strict field counts.
Unknown modes, extra fields and malformed font/source records reject. Removing the last new-family
record restores the legacy envelope; existing Rib/Web and advanced Fillet records coexist normally.

Block 182 is accepted. Focused Core **5 cases / 53 assertions** and full Core **810 / 35,441** pass.
Rib/Web Geometry regression **16 / 202** and finishing Qt **13 passed / 1 native skip / 314** pass;
all five targets build. The one test correction replaces an invalid Quantity factory call with an
actual formula-driven negative parameter edit, preserving the intended rollback assertion.
Runtime **1,279 files**, SHA-256
`1d52862658bfa810127d135198669e7d9f691e445be3893e2d3a6c29edbae3bd`.
Logs and manifest: `build/verification/block182/`. Block 183 owns the
independent planar volume, cylindrical/conical coordinates, text counters and checked material proof;
184 owns [native creation/editing, grips and combined F03/UI01](gui-sketch-driven-features-mvp9.md).
