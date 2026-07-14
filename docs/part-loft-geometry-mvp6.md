# Part Two-section Loft Geometry MVP-6

Status: implemented in Block 85.

Block 85 executes the persistent Block-84 `Loft`, `LoftCut`, and `LoftSurface` intent for exactly
two ordered sections. Each section is resolved through its own workplane frame before a shared
OCCT through-sections adapter creates the transient shape.

## Supported sections and placement

Closed solid sections support rectangle, circle, line-closed, and arc/spline-closed profile
regions. Open `LoftSurface` sections support connected planar line/arc `PathCurve` values. The two
sections may lie on parallel, non-parallel, or arbitrarily oriented datum/construction planes.
Composite regions with holes remain outside this execution boundary.

Section order comes exclusively from `LoftFeature::sections()`. Closed explicit alignment entities
rotate the authored profile boundary to the selected seam; otherwise the profile resolver's
canonical seam applies. `flip_normal` reverses section orientation, and `rotation_offset` rotates a
closed section about its own workplane normal. Matching authored edge counts are required so OCCT
wire traversal cannot silently decide correspondence.

## Products and Body operations

Closed `Loft` and `LoftCut` sections create checked solids. Open `LoftSurface` sections create
faces without a solid. `new_body`, `add`, `cut`, and `intersect` use the established Body Boolean,
history, and cache-publication paths; `LoftCut` remains restricted to `cut` by the Core contract.

Execution is transactional. Invalid workplanes, profiles, correspondence, OCCT products, or Body
operations leave existing feature and Body cache products unchanged. Resolved sections and OCCT
shapes remain derived Geometry products, so Block 85 adds no persistent JSON fields.

## Explicit deferrals

Block 86 owns three-or-more-section loft execution. Block 87 owns primary-path and guide-curve
execution plus G1/G2 continuity. Block 85 therefore fails those staged requests closed rather than
approximating them; C0 is the supported continuity boundary.

## Focused proof

```text
./build/blcad_geometry_tests "[geometry][loft-feature]"
```

Coverage proves parallel, tilted, and arbitrarily oriented sections; closed solid and open surface
products; `NewBody`, `Join`, `Cut`/`LoftCut`, and `Intersect`; deterministic seam/flip/rotation
execution; and transactional rejection of staged continuity intent.

Blocks 48–91 are implemented. Block 86 Multi-section Loft is implemented; Block 87 Guided and
continuity-controlled Loft is next.
