# Trim and Extend Surface Geometry MVP-6

Status: implemented in Block 90.

## Scope

Block 90 executes the persistent `TrimSurfaceFeature` and `ExtendSurfaceFeature` contracts from
Block 88. Targets may be `BodyKind::Surface` results or supported semantic planar faces. Boundary
and profile references remain model authority; OCCT face and edge identities remain cache-only.

## Trim contract

Trim resolves exactly one target face and exactly one closed planar trimming contour. The contour
may come from a closed PathCurve or a supported sketch `ProfileRegionReference` (rectangle,
circle, line-based closed profile, or arc/spline closed profile). The deterministic result is the
single connected intersection inside that contour.

There is intentionally no inferred keep-side choice. Open contours, non-planar contours, missing
targets, zero/multiple target faces, and empty, disconnected, or multi-face intersections fail
closed as ambiguous.

## Extend contract

Extend resolves one linear PathCurve or semantic-edge boundary that uniquely matches an outer edge
of a single geometrically planar Surface face. The positive Length parameter moves that edge
outward in the face plane and rebuilds one polygonal face. The current Block-90 execution rejects
curved boundaries, inner loops, multi-face targets, and non-planar targets rather than inventing
topology or an extension direction.

## Recompute and recovery

Semantic planar-face targets are recovered from current producer geometry on every execution.
PathCurve and profile inputs are likewise re-resolved after parameter changes. Target resolution,
trim/extend execution, validation, and feature/Surface-Body cache publication are transactional.
Successful results contain faces and no solid and never become the legacy single-solid
`final_shape`.

## Persistence

Block 90 adds no JSON fields. Existing `surface_features`, semantic Surface/trimming references,
PathCurves, profile regions, distance parameters, and result Body ids contain all persistent
intent. Generated intersection, boundary, and output faces are never serialized.

## Focused proof

```bash
./build/blcad_geometry_tests "[geometry][surface-trim-extend]"
```

The proof covers semantic-face trimming and recovery after an upstream dimension change,
parameter-driven boundary recovery for Extend, Surface-Body publication, and closed failure for an
ambiguous multi-face target.

## Handoff

Blocks 48–94 are implemented. Stitch/Knit/Sew shell and Closed-shell-to-solid execution are
canonical in `docs/part-surface-stitch-geometry-mvp6.md` and
`docs/part-closed-shell-to-solid-geometry-mvp6.md`. Block 93 multi-body STEP export and deterministic body naming is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is the current next technical step.

## Block-136 manipulator frame

`GeometryRecomputeExecutor::surface_extension_frame` resolves the same target and boundary references
as actual Extend. `SurfaceAdapter::extension_frame` computes their boundary midpoint and outward
perpendicular from the target face's center of mass. Actual extension reuses that calculation, so
native drag direction and numeric distance share one Geometry authority. Preview still validates
boundary membership, planarity and topology before exposing an applicable result. The frame is
transient, follows reversed boundary orientation and source-parameter recompute, and adds no persisted
intent. Focused proof: `[geometry][surface-trim-extend][block136-surface]`.


## Curved boundary construction — 172

The V13 options frozen in [Surface intent](part-surface-feature-intent-mvp6.md) now have typed
persistence and native construction. Historical records without options retain their Block-90
replay. Numbered status remains in `mvp-plan.md`: complete G04 combination and atomic repair are
still required before accepting 172.

- Modern Trim has explicit KeepInside/RemoveInside. OnSurface retains finite planar contours on
  coplanar targets. AlongDirection projects a finite closed planar contour through every forward
  intersection on the selected curved faces/quilt; direction is an authored nonzero vector.
  Multiple forward intersections are not silently reduced to the first. Disconnected selected
  output regions reject and require separate explicit selections. Existing inner holes survive.
- Exact native contour curves and Boolean intersections own the cut. Candidate construction uses
  private topology and authored physical tolerance; ineffective, invalid, disconnected and
  self-intersecting results reject. New logical trim roles include side and contour identity;
  changing side can lose an existing captured support and requires explicit repair.
- Natural Extend initially supports complete native isoparametric sides of rectangular analytic
  plane/cylinder patches. One selected free side is extended per feature within a single- or
  multi-face quilt. Distance is measured on the actual native isocurves; periodic overlap, partial
  split sides, arbitrary trimmed boundaries and unsupported surfaces reject.
- Tangent Extend uses the exact first-order continuation. A cylinder's axial generator gains a
  planar G1 strip; axial continuation and planar patches retain their analytic continuation.
  It verifies the oriented join and actual departure from the authored tangent extrapolation
  against the explicit deviation budget. This mode does not claim G2 or approximation to natural
  cylinder curvature. Native direction frames share the same boundary resolver as execution.
- The derived free-edge/gap/non-manifold query supplies Stitch/Thicken diagnostics and later native
  overlays. Physical whole-edge maxima, authored tolerance and actual shell closure remain the
  authorities; no overlay or topology index is serialized and no larger gap is automatically healed.

The remaining complete G04 combination will prove a trimmed multi-face cylindrical quilt across its seam, explicit opposite sides,
curved boundary extension, preserved holes, parameter-driven contours, physical area/volume,
Stitch/solid conversion and atomic downstream failure/repair. Advanced native authoring remains
173, and combined S04/G04/X01 remains 174. General arbitrary patch extension/healing stays deferred.


### Native boundary and persistence foundation proof

`SurfaceTrimAdapter` uses private topology throughout, validates its planar region tool and actual
Boolean/quilt result, and never selects the first disconnected region. Native profile Beziers keep
their exact curves in modern Trim; referenced PathCurves retain their explicitly authored execution
policy. Trim supports **64 input faces / 256 contour segments**. All new side/contour/extension roles
name authored inputs; imported roots use the canonical owned catalog-Face spelling.

`surface_extension_selection` is the one shared boundary authority for construction and direction
frames. It checks both complete native curves, face-use counts, native UV-side incidence and actual
whole-side length. Natural distance is checked on native isocurves at 17 boundary stations. Tangent
strips pass actual oriented G1 verification and physical extrapolation-error checks. A read-only
imported-face frame uses the current verified Body placement, with source freshness checked before
and after the query. Imported Path bindings still refer to the immutable base; a transformed Body
boundary can instead use its actual producer's complete current result-edge witness. No placement
or provenance is relabeled to make a reference resolve.

Shared boundary queries now participate in Stitch/Thicken validation. Stitch retains legacy replay
when an optional diagnostic exceeds its query budget, and augments failed sewing with actionable
free-boundary context. Closed-shell conversion copies input topology before changing closed flags
or orientation and maps copied semantic face descendants into the Solid.

[Core option tests](../tests/core/advanced_surface_intent_tests.cpp) isolate necessary V13 from new
selection types, cover both Project resource adapters, strict malformed/inactive intent and atomic
numeric/formula rollback. [Native geometry tests](../tests/geometry/curved_surface_geometry_tests.cpp)
check seam-crossing Keep/Remove against analytic cylindrical areas, preserved signed normals through
actual thickness volumes, positive contour edits, exact reopening, forward-direction failure,
measured natural/tangent endpoints, shared frames and transformed imported boundary references.

Foundation checks: focused Core **8 / 415**; Surface Geometry regression **25 / 1,312**; offscreen Qt
**10 passed / 1 native-only skip / 4,192 assertions**. The skipped Surface pick/drag/release case
passed natively at **100% and 150%**, **1 / 593** each, with both captures reviewed after CAD exited.
All five build targets passed. Runtime: **1,187 files**, SHA256
`b410d00632de292edc49d2d888fba8e5b6fef72da6a8e4dc33569cd8eb1f5e11`.
These focused proofs do not establish complete G04 or advanced native authoring; the remaining
combination/repair gate stays open, and the new authoring controls remain 173.
