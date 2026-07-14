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
- every section must have the same authored edge count;
- OCCT automatic compatibility rewriting is disabled.

This fails mismatched boundaries closed instead of deriving correspondence from incidental kernel
wire traversal. Composite profiles and topology-changing correspondence remain deferred.

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
parameter-driven intermediate-section recompute, and rejection of mismatched authored section
correspondence.

Blocks 48–89 are implemented. Block 87 Guided and continuity-controlled Loft is implemented;
Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is next.
