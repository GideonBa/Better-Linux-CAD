# Checked face finishing — Block 178

Construction is accepted within the capability table; `mvp-plan.md` owns numbered status. Typed inputs and persistence are
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
normal angle and the complete signed normal-curvature tensor. For G1 on an actual analytic
sphere, the exact radial normal remains defined at its coordinate poles; G2 continues to require
a regular native D2 chart. Circular G1 caps deliberately fail
the G2 check against a plane. Native operation labels are insufficient evidence.

Owned hold lines must coincide with the complete actual contact on their declared side, checked
in both directions by native overlap length. Full-round must replace the complete middle region.
All operations use isolated copies or non-destructive Boolean operations; failed construction or
verification cannot publish a candidate or mutate the cached input.

## Analytic setback corner

The initial setback policy is an orthogonal convex three-edge vertex, three explicit straight
constant-radius groups sharing r, setback S > r, and sufficient remaining edge length. A local
tetrahedral tool has axis intercept `T = S + (2 − √3) r`. Its cut edges and the three truncated
original groups are all rounded with r. The resulting network consists of a retained planar
center, three cap-edge blends and three spherical junctions, meeting the primary edge blends.
This is a defined tangent relief; it does not claim a single freeform patch or G2 corner.

In inward orthonormal coordinates the junction centers are `(r,r,S)` and axis permutations.
Their distance to both original support planes and the cut plane is exactly r. Thus S is the
physical station at which the primary constant-radius edge blend meets the corner junction.
S > r also leaves a positive central patch; collapsed cases reject. Every actual corner-network
contact, including the planar center and junctions, passes the same oriented G1 verifier used
for the other supported combinations. Source history identifies primary blends, the center and
the corner face set; kernel traversal ordinals never become persistent identities.

The tool removes only the selected local corner. Candidate validity, unchanged solid count and
self-interference checks remain mandatory. Ordinary native rolling corners retain their original
policy and independent spherical proof. No fit or solver label bypasses the contact checks.

## Capability boundary

| Mode / supports | Supported continuity and construction |
| --- | --- |
| Shared native sharp contours between declared face sets | G1 circular face blend; tangent expansion must stay on the selected sets. |
| Orthogonal convex planar pair | Exact quintic G2 contact-setback blend. |
| Opposed planar sides, one perpendicular middle | G1 semicircle or exact quintic G2 full-round. |
| Coaxial cylindrical side sets, annular planar middle | G1 revolved semicircle or exact quintic G2 full-round. |
| Ordinary checked three-edge rolling corner | Native G1; independent center/radius proof. |
| Orthogonal convex three-edge setback, equal constant r, S > r | Analytic G1 relief network defined above. |
| Spline/spline, concave G2, branched middle sets, nonorthogonal setbacks or G2 corners | Explicit unsupported-capability error. |

Hold lines constrain the complete actual contact of their declared side; unsupported or
inconsistent curves fail. Tiny middle faces remain subject to physical resolution and side
coverage checks. Native authoring of these capabilities belongs to 179.

## Verification status

All five targets built. Focused face/variable/contact/exchange regression: **48 cases / 10,434
assertions**. Complete Core: **798 / 35,311**. Complete Geometry: **815 / 91,195**, run without
concurrent builds or other CAD processes. Qt documentation audit: **5 / 751**. Logs and the frozen
runtime manifest are retained in `build/verification/block178/`.

Nine face-finishing cases cover independent removed volumes, middle replacement, a G1 cap rejected
as G2, imported coaxial supports, owned hold-line agreement and failed radius edits, tiny widths,
insufficient height, an upstream Hole edit, normal history/reopening, physical STEP material and
normal Stitch/closed-shell/Thicken consumers with retained source Bodies. Two corner cases measure
the ordinary rolling sphere and analytic setback sphere centers/radii, original BRep bytes, far
edge sections, setback edits, reopening, physical STEP and atomic rejection. Reader coverage adds
an independent spherical-octant volume and displaced-vertex rejection.

Copy-map face orientation is restored from the original target. Periodic analytic carriers retain
seam equivalence through rectangular trims. STEP-rounded seam parameters may agree within 0.1%
of the physical incidence budget; every admissible jet still undergoes full contact checks.
G1 spherical pole normals and the existing rejection of singular G2 charts have separate proof.

Runtime: **1,244 files**, SHA-256
`f590ef7bfea79674bd30ef001da27dc3eb979ded6fdeb13c0e86e3fbd462b79d`.
Block 178 is accepted within the capability table. Native UI acceptance remains 179.
