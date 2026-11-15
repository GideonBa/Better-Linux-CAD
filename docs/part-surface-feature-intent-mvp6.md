# Part Surface Feature Intent MVP-6

Status: implemented in Block 88.

Block 88 activates `BodyKind::Surface` as a persistent construction-result authority. It freezes
the Core and JSON boundary for the first six Surface features; OCCT execution remains owned by
Blocks 89–92.

## Persistent intent

The first feature family consists of:

- `BoundarySurfaceFeature`: two to four unique boundary-curve references, producing a Surface
  Body;
- `FillSurfaceFeature`: one or more unique boundary-curve references, producing a Surface Body;
- `TrimSurfaceFeature`: a Surface target plus a boundary-curve or sketch-profile trimming
  reference, producing a Surface Body;
- `ExtendSurfaceFeature`: a Surface target, boundary curve, positive Length distance parameter,
  and Surface result;
- `SurfaceStitchFeature`: at least two unique Surface references, a positive Length tolerance,
  and Surface result;
- `ClosedShellToSolidFeature`: one Surface shell reference and a Solid result.

Every feature owns a stable `FeatureId`, name, and explicit result `BodyId`. Results are separate
history outputs: an input Body cannot simultaneously be the result Body. A result Body may have
only one producing feature.

## Semantic references

`BoundaryCurveReference` selects either a persistent `PathCurveId` or a semantic generated edge.
This makes planar-sketch, 3D-sketch, construction/reference-curve, and supported generated-edge
sources available without persisting OCCT edge identity.

`SurfaceReference` selects either a `BodyKind::Surface` Body or a semantic generated face.
`TrimmingReference` selects either a `BoundaryCurveReference` or a persistent sketch/profile
region. References expose deterministic source graph nodes and stable duplicate-detection keys.

## Document graph and validation

`PartDocument::add_surface_feature` validates all referenced paths, profiles, semantic producers,
parameters, input Body kinds, and result Body kinds. It adds input-to-feature and
feature-to-result-Body dependency edges, rejects duplicate feature IDs, multiple result producers,
and cycles, and synchronizes invalidation state transactionally.

Removing a referenced PathCurve or Body is rejected by the dependency graph. Removing a Surface
feature is allowed only while its result has no dependents. Parameter changes propagate through
Extend/Stitch intent to all downstream Surface/Solid results.

## JSON contract

Part JSON always emits a top-level `surface_features` array; historical files may omit it and
restore zero Surface features. Each strict record always emits the complete field set:

```text
id, name, type, boundaries, target, trimming, boundary,
distance_parameter, surfaces, tolerance_parameter, shell, result_body
```

Inactive scalar/reference slots are `null` and inactive collections are empty arrays. Boundary,
Surface, and trimming references persist an explicit `source_kind` plus mutually exclusive source
slots. Unknown fields, missing mandatory fields, inconsistent reference variants, unsupported
types, and invalid document references fail loading. Derived OCCT faces, shells, tolerances,
topology maps, and geometry caches are never serialized.

## Focused proof

```bash
./build/blcad_core_tests "[core][surface-feature]"
```

The focused tests cover constructor invariants, all six document-integrated feature types,
dependency/invalidation propagation, input and feature removal protection, strict JSON round-trip,
malformed records, and compatibility with historical files lacking `surface_features`.

## Handoff

Blocks 48–94 are implemented. Boundary/Fill, Trim/Extend, Stitch, and Closed-shell-to-solid
execution are canonical in `docs/part-boundary-fill-surface-geometry-mvp6.md`,
`docs/part-trim-extend-surface-geometry-mvp6.md`,
`docs/part-surface-stitch-geometry-mvp6.md`, and
`docs/part-closed-shell-to-solid-geometry-mvp6.md`. Block 93 multi-body STEP export and deterministic body naming is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is the current next technical step. Network surfaces and arbitrary
patch-network intent remain deferred beyond the first Part Construction MVP.


## OffsetSurface and Thicken intent — 170

The extension retains the six historical record shapes and their planar replay. Core intent,
strict persistence and atomic updates are implemented; native construction is the separate 171–172
gate. Numbered status stays in `docs/mvp-plan.md`.

- Add separate OffsetSurface and Thicken kinds. Offset produces no material and always creates a
  Surface Body. Thicken closes offset skins and their free boundaries into a Solid, then reuses
  New/Join/Cut/Intersect context, in-place identity, history and atomic feature updates. Existing
  Shell removes faces from a solid; it is a distinct operation.
- Extend Surface references with explicitly owned, unique typed face selections, including curved
  result/imported faces, alongside existing quilt Bodies and semantic planar faces. Preserve all
  producer/Body dependencies and import ownership; no native topology index becomes intent.
- Persist positive Length distance and modeling tolerance, explicit normal/opposite/symmetric side,
  and an exact-analytic or explicitly verified-approximation policy. Distance is total thickness;
  symmetry uses half on each side. Symmetric Offset intentionally creates two material-free skins.
  Approximation requires its own maximum-deviation Length budget; derived measurements stay transient.
- Define bounded positive distance/tolerance and relative error limits before construction. Validate
  parameter and dependent-formula edits transactionally. Geometry checks finite normal frames,
  curvature/offset singularities, physical normal distances, collisions, closure and manifoldness;
  kernel success or a display mesh cannot prove thickness. Invalid/inconclusive candidates reject.
- A feature constructs one connected selected quilt. Disconnected or non-manifold selections need
  an explicit diagnostic; additional regions can be authored separately. Preserve inner holes and
  give derived offset skins stable face origins for downstream sketches, Hole and finishing.
- Necessary Part V13 owns the new kinds, selected-face references and later advanced Trim/Extend
  options. Older records retain their existing spelling/version. Both Project resource directions
  must preserve strict records, local subsets and pins; no derived geometry is serialized.
- G03 independently checks planes, cylinders, curved smooth quilts, holes, symmetry, all Body
  operations, excessive inward offsets, concavity/collision and disconnected inputs. G04 follows
  with explicit curved trim keep/remove, bounded natural/tangent extension and shared derived
  free-edge/gap/non-manifold reports. Authored Stitch tolerance never conceals a larger gap.

Public selection, intent and query types should be completed together before the full rebuild.
Geometry capability limits and approximation/error evidence must be documented before native
advanced controls are enabled in 173. All deferred arbitrary-network/healing scope above is retained.

### Core policy and evidence

`SurfaceOffsetFeature` has distinct OffsetSurface/Thicken kinds and one typed parameter policy.
Distance is **1e-6..1e6 mm**. Tolerance is **1e-9..1 mm**, at most **1% of distance**.
Verified approximation requires an explicit maximum deviation between tolerance and 1% of distance;
exact analytic intent rejects an inactive deviation budget. These limits apply on insertion,
feature replacement, parameter changes and dependent formula reevaluation. Failed changes restore
the complete Part, including graph, history, values and invalidation state.

Selected-face references own a Body and **1..64** unique `SurfaceSourceFace` references. Result-face
owners must agree; imported faces retain the exact immutable base Body/catalog ownership. Dependency
inputs include the owner and every producer, not merely the first face. Body/quilt and historical
semantic-planar references keep their original spelling. Offset always creates a new Surface Body;
Thicken uses the existing New/Join/Cut/Intersect context and restores the predecessor producer when
an unreferenced in-place feature is removed. Existing update commands preserve result identity.

Advanced Trim/Extend options share the necessary V13 envelope: explicit keep/remove and projection
policy for Trim; natural/tangent mode, tolerance and tangent-deviation budget for Extend. Their
absence retains historical replay. Native construction follows the checked capability below; the Core contract alone does not
claim kernel support or native authoring. Advanced Trim/Extend execution remains separate.

Proof: [Core cases](../tests/core/surface_offset_intent_tests.cpp), tags
`[surface-offset-intent],[surface-feature],[loft-constraints]`: **16 cases / 494 assertions**.
Full Core: **782 / 34,928**. Existing Surface Geometry: **16 / 850**, including the separate
`[surface-to-solid]` filter. Existing Qt Surface/edit regression: **4 / 74** offscreen.
All five build targets completed after the public variant/layout change. Runtime manifest:
**1,167 files**, SHA256 `e59f8fd7dca53347bf2b6a43bbbfab106d7001b29366e63bd40282d4913ffae2`.
No native advanced-authoring claim is made here; that remains the 173 gate. Detailed run chronology
stays in Git/PR history.

### Checked native construction foundation

`SurfaceSelectionGeometry` resolves explicitly owned faces against the actual cached quilt or
in-place Body predecessor. Every captured contributor must recover exactly one descendant; split,
lost, duplicate and disconnected selections reject. Sewing receives private copies with the
original face-use orientations. Boundary/Fill's intrinsic single patch and authored extrusion/Loft
origins survive selection, Stitch and closed-shell conversion. No traversal index becomes a role.

`SurfaceOffsetAdapter` constructs material-free skins or a closed Thicken tool on a private deep
copy. Exact analytic mode currently supports planes and cylinders. Verified approximation supports
regular native C2 patches and consistently oriented smooth quilts within the stated budgets.
Sharp joins, singular normals/poles, curvature collapse, non-manifold/disconnected selections,
invalid topology, self-interference and nonpositive Solid volume reject without fallback healing.
Thicken explicitly orients its one closed Solid before validation. Holes remain free boundaries
whose generated walls close the material region; symmetric distance is total skin separation.

Source regularity/curvature checks include native knot cells and their boundaries, with **17**
subdivisions per cell and signed focal factors `1-offset*curvature`. Trimmed/offset surface wrappers
and native extrusion/revolution generating-curve knots retain their actual differential geometry.
The bounded capability is **64 faces / 128 native cells per face**; it is numerical verification,
not a continuum certificate. Shared quilt edges additionally require aligned oriented normals.

Independent normal rays intersect the actual history-mapped trimmed skins using **9, 17, 33**
interior subdivisions per native cell. Refinement must stabilize the maximum error within 5% of the
budget; each skin and the complete separation must meet the authored physical error in mm.
The ray's root-search interval is separate from that error bound: OCCT OffsetSurface intersection
can miss an existing root in a narrowly clipped interval. Multiple/missing roots still reject.
No distance-to-the-whole-Solid shortcut can certify thickness.

`SurfaceDistanceReport` records criterion (offset distance, wall thickness or symmetric skin
separation), requested/measured min/max distance, maximum error, budget, station count and explicit
approximation policy. It is transient and describes the verified construction **before** any Body
Boolean. Join/Cut/Intersect retain that qualified tool report; they do not claim uniform thickness
of the complete final Body. Skin origins name authored source roles and side. Arbitrary generated
side walls receive no guessed semantic identity; existing complete edge witnesses support finishing.

The shared derived `SurfaceBoundaryQuery` counts native edge uses (including periodic seams),
reports free/non-manifold boundaries and measures bidirectional whole-edge gap maxima using true
curve-length stations and native knots. A nearby endpoint or minimum distance cannot label a
larger complete-edge gap as within tolerance. These geometry/overlay products are never persisted.
The complete G03 consumer/import/exchange proof follows below; curved operations remain the separate 172 gate.


### G03 consumers and exchange proof

[Native G03/gap cases](../tests/geometry/surface_offset_geometry_tests.cpp) cover analytic planes,
cylindrical quilts, directions/symmetry, inner holes, all Body modes, exact JSON reopening,
immutable inputs, inward collapse and disconnected selections. The nonplanar `z=.02*x*y` case
checks actual Solid normal sections and an independently integrated Gaussian-curvature tube-volume
formula. Complete-edge gap cases distinguish maxima from nearby endpoints.

[Consumer/import cases](../tests/geometry/surface_offset_consumer_tests.cpp) prove an associative
skin-supported Sketch and Hole, a positive thickness edit, actual finishing, exact reopening and
physical STEP readback (volume and material points). A placed imported cylinder exercises all four
Body modes against analytic volumes, effective in-place predecessors, stable derived skin faces,
supplier freshness and unchanged geometry caches after impossible edits. A separate imported
multi-face cylindrical quilt has an opening crossing its native seam: both signed directions and
symmetric construction retain the opening and match independently derived angular tube volumes
and material-point witnesses. Supplier bytes remain unchanged.

The nonlocal collision counterexample uses a near-mouth cubic with every local curvature radius
larger than the offset. The small construction passes; the larger colliding skin/walls reject,
proving that focal checks alone do not authorize success. Historical semantic source faces retain
their authored frames; imported curved face roots derive from the exact owned catalog entry.

Final full Geometry: **776 cases / 81,143 assertions**. Existing Qt Surface/edit: **4 / 74**
offscreen. Relevant native Qt workflows: **2 / 255** at both **100% and 150%**, no skips; captures
were inspected after CAD processes exited. This is the existing native workflow regression,
not advanced authoring coverage, which remains 173. All five build targets passed; subsequent
changes added Geometry tests only, with the GUI/app/exporter binary hashes verified unchanged.
Frozen runtime: **1,178 files**, SHA256
`8abc03217aefaf37be388530f916e37da977342921dc9a51e34dc0ddfa527c82`.
Numbered acceptance stays in `mvp-plan.md`; detailed run chronology remains in Git/PR history.
