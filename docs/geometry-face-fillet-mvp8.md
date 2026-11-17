# Checked face finishing — Block 178

Construction is in progress; `mvp-plan.md` owns acceptance. Typed inputs and persistence are
specified in [the Core contract](part-face-full-round-intent-mvp6.md). Native controls remain 179.

## Construction and verification

Face G1 consumes shared sharp contours between the two selected face sets. Native tangent
expansion must remain on those sets. G2 currently constructs an exact quintic transition between
orthogonal convex planar supports; the size denotes contact setback. The quintic has collinear
first/last three poles, establishing zero normal curvature at each planar contact. Its independently
integrated removed cross-section is one tenth of the squared setback.

Planar full-round requires opposed parallel side planes and a perpendicular middle face. Width
comes from the actual supports. The operation removes a local top slab minus the derived cap,
leaving all material outside that slab intact. Curved full-round supports coaxial cylindrical side
sets with an annular planar middle. The equivalent cap is revolved around the verified common axis.
Complete native edge/face intersections check coverage of both contact lines/circles by the trimmed
selected sides. A short side or a hole interrupting required contact cannot become an infinite plane.

Circular G1 full-round uses radius equal to half the middle width. G2 full-round uses a quintic
section whose first/last three poles are collinear with the side supports; it reaches the original
middle height exactly. For half-width r and axial length L, removed planar volume is
`(2 − π/2) r² L` for G1 and `2 r² L / 7` for G2. With coaxial major radius R, removed annular volume
is `(4π − π²) R r²` for G1 and `4π R r² / 7` for G2. G2 denotes differential support contact,
not a constant circular radius.

Candidates must retain the original solid count, valid manifold topology, no self-interference
and positive finite remaining/removed volume. Native operation history identifies actual generated
blend faces and both support descendants. Only those generated faces may establish continuity.
The existing oriented trimmed-surface verifier refines true-length stations and checks C0,
normal angle and the complete signed normal-curvature tensor. Circular G1 caps deliberately fail
the G2 check against a plane. Native operation labels are insufficient evidence.

Owned hold lines must coincide with the complete actual contact on their declared side, checked
in both directions by native overlap length. Full-round must replace the complete middle region.
All operations use isolated copies or non-destructive Boolean operations; failed construction or
verification cannot publish a candidate or mutate the cached input.

## Capability boundary

The initial analytic combinations above do not imply support for arbitrary spline/spline blends,
concave G2 profiles, branched full-round middle sets or G2 setback corners. Those inputs require
an explicit supported construction and independent differential proof. Three-edge rolling corners
remain governed by the variable/constant native Fillet contract. Setback patch construction and its
measured approximation policy are under investigation; no solver-only result establishes acceptance.

## Verification status

All five targets built. Focused face/variable/contact regression: **29 cases / 9,555 assertions**
(`build/verification/block178/face-regression.log`). Seven new face-finishing cases cover analytic
removed volumes, actual middle replacement, G1 rejected as G2, imported coaxial supports, owned
hold-line agreement and failed radius edits, tiny middle width, insufficient height, an upstream
Hole edit, normal history/reopening and physical STEP material preservation.

Copy-map face orientation is restored explicitly from the original target. Periodic analytic
carriers retain seam equivalence through rectangular trims. STEP-rounded seam parameters may
agree within 0.1% of the physical incidence budget; every admissible jet still undergoes the full
position/normal/curvature checks. All previous surface-join differential cases pass.

Setback construction, complete regression and the final capability table remain required before
accepting 178. Native UI acceptance remains 179.
