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

Blocks 48–94 are implemented. Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.

## Authored boundary and guide constraints

`LoftFeature::with_constraints` opts into separately persisted `LoftConstraintIntent`. Existing
`LoftContinuity` remains longitudinal: neither `g1` nor historical unsupported `g2` means a smooth
join to a neighboring face. A boundary condition separately requests G1 or G2 to an explicit
`FaceReference` carrying `LoftBoundarySupport`. Start/end bind the corresponding logical terminal
section identity; reordering sections requires reauthoring those bindings. No implicit upgrade is
performed. The Geometry execution gate currently diagnoses constrained execution as pending 169.

The identity is a length-framed source key: Sketch + Profile for closed sections, PathCurve for
open sections. Alignment, rotation, flip, geometric parameter values and compatibility subdivisions
never enter that key. References to original boundary entities therefore survive transient edge
refinement. Ambiguous or missing sources fail; no kernel traversal index is persistent authority.

A condition stores a positive dimensionless `tangent_scale` (at most 1e6), oriented support normal
reversal and boundary continuity. Scale controls the authored transverse derivative magnitude;
capability checking must reject unsupported scales rather than ignore them. Curvature uses the
oriented support normal after `reverse_normal`; reversing this normal also reverses signed normal
curvature. Neighboring surfaces must have regular differential frames. Point/pole sections are
outside the current section types: collapsing a section does not implicitly create a pole, and
singular frames cannot receive a G2 success label.

Every authored guide has exactly one ordered binding and an anchor at every logical section. Each
anchor stores a section identity, optional original outer-boundary Sketch entity, boundary fraction
and guide fraction. Fractions are normalized **native true length**, in [0,1], rather than native
curve parameter or display-polyline distance. A boundary entity fraction follows that original
entity's authored direction before section rotation/flip. With no entity, the fraction follows the
section's canonical seam-aligned outer wire (or open path) before rotation/flip. Rectangle/circle
profiles use their existing canonical seam; entity names from unrelated contours are rejected.
Geometry must transform the resulting anchor with the section and verify incidence with the guide.
A guide fraction is measured in its original PathCurve direction. `reversed` changes traversal;
`1-fraction` must then increase strictly in section order. Duplicate authored anchors across rails
are rejected; coincident geometry or intersecting rails require Geometry validation. Intermediate
samples and compatibility-generated sections are derived products and never serialized.

Linear verification tolerance is an existing Length parameter in [1e-9,1] mm. Angular tolerance is
an explicit finite radian value in (0,0.1]; signed normal-curvature error tolerance is finite in
(0,1] inverse mm. These are modeling criteria, separate from connection/solver/display tolerances.
Parameter changes invalidate the Loft through the ordinary dependency graph. Tolerance edits and
dependent formula changes outside the allowed range roll back the complete Part transaction. Support
producers and explicit imported base Bodies participate in ordering and removal protection.
Support producers are mandatory typed history inputs: suppression blocks the Loft, including when
a modifier otherwise bypasses its Body. In-place Body operations retain the original predecessor
rather than introducing a self-cycle through the result Body alias.
Generic result-face validation permits Surface as well as Solid producers; the existing planar
workplane validation keeps its Solid-only contract. An imported support must belong to the selected
base Body's immutable import output; placed supports use producer-anchored result-face identity. Source loss/ambiguity cannot choose another
face or a later Body pose.

Constraints require actual authored support or guide intent. Removing them restores the historical
record shape. Core accepts no unknown continuity, inactive source Body, invalid fractions, unordered
stations, wrong role, missing tolerance or foreign boundary entity. Strict Part V12 and both Project
resource adapters preserve exactly this intent, including empty/local resource subsets and pins.
The existing JSON-based feature editor preserves the complete record; advanced native authoring is
planned in 173. Focused tests use `[loft-constraints]`; complete execution acceptance remains 169.
