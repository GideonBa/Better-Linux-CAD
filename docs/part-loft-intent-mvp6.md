# Part Loft Core Intent MVP-6

Status: implemented in Block 84.

Block 84 introduces persistent ordered Loft intent without executing OCCT geometry. It freezes the
section, seam/alignment, path/guide, continuity, and Body-result contracts consumed by Block 85.

## Ordered profile sections

Every `LoftFeature`, `LoftCutFeature`, or `LoftSurfaceFeature` owns at least two explicitly ordered
`ProfileSectionReference` values. A section is either:

- a closed `ProfileRegionReference` with the `LoftSection` role; or
- an open `PathCurve` section for surface lofts.

Sections within one feature use a homogeneous kind and distinct sources. Solid Loft and LoftCut
require closed sections. LoftSurface accepts closed or open sections. Incidental sketch, entity,
or kernel traversal order never determines section order.

Closed sections persist an optional boundary `SketchEntityId` as their seam/alignment authority,
`flip_normal`, and an optional Angle parameter as `rotation_offset`. The alignment entity must
belong to the referenced profile boundary. Rectangle and circle sections therefore use the
implicit canonical seam until a later geometry contract adds a suitable explicit point identity.

## Path, guides, continuity, and Body result

An optional open `path_curve` and ordered, unique open `guide_curves` participate in validation,
dependency invalidation, recompute ordering, and removal protection. Guides must differ from the
primary path. Continuity is explicit as `c0`, `g1`, or `g2` intent; Geometry support for those
requests is staged in later blocks.

Solid variants use the established `FeatureBodyResultContext` for `NewBody`, `Join`, `Cut`, or
`Intersect`. LoftCut requires `Cut`. LoftSurface requires `NewBody` and a Surface result Body.

## Persistence

The optional-on-read, always-emitted top-level `loft_features` array stores strict records with
feature identity, ordered sections, nullable path, ordered guides, continuity, and Body-result
fields. Section records always emit all source slots and use null for the inactive source kind.
Historical files without the array restore no Loft features.

## Focused proof

```text
./build/blcad_core_tests "[core][loft-feature]"
```

Coverage proves three feature kinds, minimum and deterministic section order, closed/open section
compatibility, dependencies, Body-kind validation, strict JSON round-trip, malformed-record
rejection, and historical compatibility.

Blocks 48–91 are implemented. Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is next.
