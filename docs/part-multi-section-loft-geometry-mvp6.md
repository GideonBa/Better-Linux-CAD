# Part Multi-section Loft Geometry MVP-6

Status: implemented in Block 86.

Block 86 extends the Block-85 Loft executor from exactly two to any ordered set of at least two
sections. Three-or-more-section `Loft`, `LoftCut`, and `LoftSurface` values use the same workplane,
Body-result, validation, and transactional cache boundaries as two-section lofts.

## Ordered multi-section execution

Every section is independently resolved from persistent `ProfileSectionReference` intent into its
own model-space wire. Wires are passed to OCCT strictly in authored section order. A middle section
therefore shapes the transition and is not treated as metadata or reduced to an endpoint blend.

Section correspondence remains semantic and deterministic:

- explicit alignment entities or the canonical profile seam choose each section start;
- flip and rotation controls are applied before wire creation;
- equal authored edge counts retain their existing correspondence unchanged;
- unequal counts receive deterministic, exact derived subdivision into matching edge counts;
- OCCT automatic compatibility rewriting is disabled.

Correspondence is owned by BLCAD rather than incidental kernel wire traversal. Composite profiles
remain deferred; automatic subdivision does not add/remove holes or change open/closed section kind.

## Automatic section refinement — 2026-10-26

`make_loft_sections_compatible` operates on resolved, oriented Geometry copies before both preview
and recompute. It leaves Sketch entity counts, ids, control points and serialized Loft intent intact.
All-equal edge counts bypass refinement, preserving existing saved models' correspondence.

For unequal counts, each section is partitioned into ordered runs of the same curve kind with
codirectional endpoint tangents. The authored seam remains the first run boundary; a smooth join has
normalized tangent dot product at least `1 - 1e-10`. If all sections have the same run count, run
junctions correspond. Thus differently subdivided upper/lower airfoil chains preserve the leading and
trailing edge correspondence. If run counts differ, the complete seam-relative contour is one run.

Within each run, an authored edge occupies one equal parameter interval. The union of all exact
rational edge boundaries defines the common subdivision. Original vertices remain in order, with a
mapping to refined vertices for guide binding. Refinement is deterministic and does not use
LCM-sized tessellations. It uses linear interpolation for lines, true circular parameters for arcs,
and exact OCCT Bézier restriction for polynomial/rational cubics. No polyline sampling or curve
refitting changes the authored section shapes. The resulting longitudinal blend still depends on
the explicit section order, seam, flip and parameter correspondence.

Automatic refinement accepts at most 4,096 input and derived edges per section and 65,536 input and
derived edges across sections. Invalid/non-finite/disconnected/degenerate input or budget overflow
fails before publishing a shape. These are refinement limits; equal-count legacy inputs retain the
existing validation path. Arbitrary shape intersections, unsupported continuity and invalid OCCT
products still fail transactionally.

## Recompute and products

Parameter edits on any intermediate section invalidate the Loft dependency and rebuild the whole
ordered product. Solid and surface variants, arbitrary section workplanes, and `NewBody`, `Join`,
`Cut`, and `Intersect` retain the Block-85 execution rules. Results are published only after the
complete loft and optional Body Boolean succeeds.

Block 86 adds no persistent fields: Block-84 JSON already stores an arbitrary ordered section
array. Resolved wires, correspondence, and OCCT products remain transient Geometry/cache state.

## Explicit deferrals

Block 87 owns primary-path and guide-curve control plus G1/G2 continuity execution. Block 86
continues to execute C0 only and fails staged requests closed.

## Focused proof

```text
./build/blcad_geometry_tests "[geometry][multi-section-loft]"
```

Coverage proves three-section execution, deterministic four-section solid and open-surface lofts,
parameter-driven intermediate-section recompute, and square/triangle/square refinement without
kernel compatibility rewriting. `[geometry][loft-section-compatibility]` additionally covers the
8/9/6-edge fitted-outline regression, unchanged source JSON, original-curve boundary oracles, exact
circular/rational sections, open sections, center/guide binding and bounded malformed-input failure.

Blocks 48–94 are implemented. Block 87 Guided and continuity-controlled Loft is implemented;
Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
