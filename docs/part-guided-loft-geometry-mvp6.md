# Part Guided and Continuity-controlled Loft Geometry MVP-6

[Blocks 168–169](inventor-parity-sequence.md) add authored bindings and independently verified
boundary G2 within the capability limits below. Historical global longitudinal G2 retains its
diagnostic; support-face and longitudinal continuity stay distinct. Advanced native controls
remain planned in Block 173.
Status: implemented in Block 87.

Block 87 executes the persistent primary-path, ordered-guide, and continuity intent introduced in
Block 84. It extends the same transactional solid/surface and Body-result pipeline used by Blocks
85–86.

## Center-path control

An optional open `PathCurve` is resolved through the shared planar/spatial path resolver. It must
pass through every authored section center in section order. Between authored sections the adapter
inserts deterministic sampled control sections whose centers follow the path. The authored profile
sections themselves remain exact Loft sections.

Section `i` of `n` binds to normalized path-length station `i / (n - 1)` (three sections:
0, 0.5, 1; five sections: 0, 0.25, 0.5, 0.75, 1). Placement is not inferred by intersecting
arbitrary section planes with the trajectory. Planes may have distinct normals and in-plane roll,
but their profile centers must meet these stations within the binding tolerance (1e-5 mm).
Unequal edge counts are first refined by BLCAD's exact compatibility layer; controlled interpolation
still requires matching edge kinds at corresponding refined positions. Different sizes and polygon
outlines are supported within that correspondence. Authored section centers remain the original
vertex averages; added subdivision vertices do not redefine them. Intermediate center translation
uses interpolation of those original centers, preventing refinement density from moving a section.

The control sampler retains resolved lines and circular arcs. Arc lengths and sampled positions
use the actual circle, including reversed arcs; replacing an arc with two chords changes the section
stations and is incorrect. Historical splines retain the shared resolver's deterministic 32 line
spans. Explicitly upgraded paths retain native spline/equation curves and checked Helix geometry;
the sampler queries their actual arc length and native curve position. Modeling tolerance belongs
to that source construction. Four
subintervals per authored section interval (three intermediate control sections) constrain the Loft.
The resulting interpolated surface is not an exact analytic sweep of the entire trajectory.

This supports planar and model-space Sketch3D line, polyline, arc, sampled spline, and sampled helix
sources already accepted by the shared path contract. Degenerate, disconnected, closed, or
misbound paths fail before cache publication.

## Ordered guide control

Guides are resolved as ordered open `PathCurve` values. Their order distributes them
deterministically over seam-relative boundary points; two guides on a four-point profile therefore
bind the seam point and the opposite point, suitable for leading/trailing-edge control. Every guide
must pass through its bound point on every authored section at the corresponding normalized section
station.

Intermediate control sections interpolate matching refined edge kinds, then bind their selected
boundary points to sampled guide positions. Mismatched edge kinds, incompatible anchors, excessive
guides, or a guide that misses an authored station fail closed. OCCT compatibility rewriting remains
disabled. For automatically refined sections, each sampled guide station must identify one
unambiguous original boundary vertex. The original-to-refined mapping must give the same anchor on
every section, and different guides must bind distinct anchors. This preserves leading/trailing
anchors even when their authored indices differ. Equal-count sections retain the historical ordinal
rule. Automatic refinement does not guess a guide binding inside an authored edge.

## Continuity contract

- `c0` executes positional Loft continuity.
- `g1` requests the stronger OCCT C1 approximation and verifies that every resulting face surface
  reports at least C1 continuity before publication.
- `g2` remains persisted but fails explicitly because this implementation cannot yet guarantee and
  verify curvature continuity. It is never silently degraded to G1.

The continuity claim concerns the longitudinal Loft surface through section stations. Profile
corners remain intentional boundaries between side faces.

## Persistence and transaction

Block 87 adds no JSON fields. Block-84 `path_curve`, ordered `guide_curves`, and `continuity` values
are already persistent dependency authority. Resolved paths, sampled control sections, and OCCT
surfaces remain derived Geometry/cache products. Any control, continuity, Boolean, or shape failure
leaves prior feature and Body products unchanged.

## Focused proof

```text
./build/dev-gui/blcad_geometry_tests "[geometry][guided-loft]"
```

Coverage proves a center-path-controlled duct transition, a root/mid/tip blade-like Loft with
leading and trailing guides plus verified G1/C1 surfaces, and transactional G2 rejection.
`tests/geometry/mixed_path_loft_tests.cpp` (`[mixed-path-loft]`) adds three/five unequal rectangle and
trapezoid profiles on differently oriented and rolled planes, following a line–cubic spline–reversed
quarter-circle trajectory. An independent formula-based oracle checks section vertices and edge
midpoints against actual OCCT boundary faces, plus an intermediate arc station (distance <2e-5 mm).
It checks valid solids/surfaces, open-section center control, closed-section guide control, canonical
JSON/recompute, middle-profile edits, rejected upstream path/plane/G2 edits retaining the previous
native shape, invalid circular control geometry, and a constant-section Sweep comparison.
The first three cases failed against the old chord-based arc sampler before the correction.

Blocks 48–94 are implemented. Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.

## Native boundary-continuity verification

The private Geometry verifier `surface_join_verification` supplies numerical evidence for constrained
construction. Feature integration and bounded G02 acceptance are
recorded in `docs/mvp-plan.md`.

It evaluates native located trimmed faces at native true-length boundary stations. Incidence uses
actual 3D distance to the finite trim edges, in millimeters, followed by their native pcurves.
A millimeter tolerance is never applied directly to UV coordinates: anisotropic parameter scaling
can otherwise accept a physically distant trim. Pcurve/surface/edge points must agree within the
model tolerance. Distinct native projection branches are ambiguous;
only equivalent periodic parameters may refer to the same branch. Missing incidence, unsupported
orientation, nonfinite values, collapsed boundaries and singular differential frames/poles reject.
The caller validates candidate topology before accepting a construction.

Position error is measured in mm and oriented normal error in radians. Face orientation and the
authored support-normal reversal determine the signed normal. For G2, native D2 derivatives define
the first and signed second fundamental forms. Orthonormal tangent frames are identified by the
minimal rotation between the candidate and support normals. Tangent, transverse and diagonal
curvatures recover the complete symmetric difference tensor; its maximum absolute eigenvalue is
the error in inverse mm. The criterion therefore bounds every unit direction in that tangent frame:

```text
m = (delta_xx + delta_yy) / 2
error = abs(m) + hypot((delta_xx - delta_yy) / 2, delta_xy)
```

Checking the three sampled directions independently would miss some extrema between them. The
[analytic tensor counterexample](../tests/geometry/surface_join_verification_tests.cpp) uses
`z = q*x*x/2 + q*x*y - q*y*y/2`: each axial/diagonal discrepancy is at most `q`, but the actual
extremum is `sqrt(2)*q`. Independent plane/cylinder and polynomial fixtures also separate G1-only
joins, nearly failing curvature, positional/normal tolerances, normal reversal and rigid placement.

The bounded verification grids contain 33, 65 and 129 true-length stations. Every grid must meet
all authored criteria; the change in measured maxima from 65 to 129 stations must be at most 5%
of the corresponding budget. The report records the actual final station count: 129 uniform stations plus mandatory native
knot crossings. This is a
controlled numerical boundary check, not a global continuum or approximation error bound.
Construction capability limits, complete section/rail adherence and candidate refinement remain
separate obligations. No `GeomAbs_C2` flag or filling-builder success replaces these checks.

Focused numerical proof: `[surface-join-verification]`; the `[surface-join-spectrum]` subset owns the
complete-tensor counterexample. Whole-169 execution acceptance remains in `docs/mvp-plan.md`.


## Native constrained patch construction

The private `constrained_loft_patch` / `native_loft_tensor` builder constructs a bounded rational
BSpline tensor patch from four explicitly ordered native boundaries and up to nine interior
sections. Exact source curves are copied, converted and degree/knot refined into common bases;
no display chords or altered source BReps enter construction. A Hermite solve interpolates all
section stations and the rail-derived first/second endpoint derivative fields. Positive matching
section weight functions support rational profiles, including exact circular arcs.

Nearly identical converted knots may share a canonical value only within 64 machine-epsilon
units and a conservative geometric budget. With knot displacement `delta`, minimum positive knot
span `h`, degree `p`, centered control radius `r` and positive weight ratio `M`, corresponding-span
de Boor interpolation bounds geometric displacement by `4*p*delta*r*(3*M-1)/h`. A snap exceeding
5% of the authored linear tolerance rejects. Distinct knots within one curve never collapse;
remaining ill-conditioned knot spans reject. Original input BReps remain unchanged, and actual
section/rail adherence and differential checks still decide the constructed result.

Current capability is deliberately bounded: the two rails must share endpoint derivative fields
and compatible section parameters. An exact shared-V-basis correction interpolates polynomial
B-spline rails. A rational U partition with Greville coefficients retains the matching profile
weight function and endpoint values; corrections vanish at authored sections and through second
order at terminal stations. Interior V knots must retain C2 polynomial continuity; rational rail
weight functions remain outside this capability.
A section may reverse only when its first endpoint has no rail incidence; an ambiguous incidence
never selects an arbitrary branch. Singular/ill-conditioned stations, incompatible weights,
missing incidence, invalid topology and exhausted degree/knot/pole budgets reject explicitly.
The same candidate's two orientations are tested against the authored support normals. Every
specified G1/G2 join must pass the independent differential verifier; all four boundaries and
interior native sections must meet the positional tolerance. G2 never falls back to G1.

[Patch oracles](../tests/geometry/constrained_loft_patch_tests.cpp) independently check a sextic
arch between planar G2 supports, exact cylindrical area/radius/principal curvature and rolled
Airfoil sections with unequal source subdivision. Source BRep snapshots remain identical after
successful and failed construction. These private-kernel oracles are supplemented by the feature
acceptance below. General varying support jets and incompatible rational weight functions remain
unsupported until separately implemented and
verified. Native editing is planned in Block 173.


## Constrained feature integration

Explicit constraints select the native patch builder; historical unconstrained Loft replay is
unchanged. Historical global `LoftContinuity::G2` remains a diagnosed unsupported longitudinal
request. Independently authored boundary G2 is checked against each selected neighboring face.
The supported new network has 2–32 explicitly curve-preserving, ordered rails and 2–11 homogeneous
sections. Sections may use a consistent reversal; mixed reversals reject as ambiguous. Closed
sections form one cyclic traversal; open sections require endpoint rails without wrap. Rational
section weight functions must match after exact common-basis refinement. Singular/pole sections,
nonunit tangent scale and incompatible endpoint derivative fields reject explicitly.

Anchors use native true length on original section entities, in their authored directions, or the
canonical whole contour. Section rotation moves the physical anchor; subdivision does not rebind
it. Every guide anchor must coincide in model units. Native rail subcurves retain authored traversal
and terminal fractions. Missing or multiple rail stations never select the first match. For closed
profiles, an optional center path must cross every finite planar section interior exactly once in
order. It controls these stations; it neither becomes a skin constraint nor silently relocates an
explicitly anchored section. Open-section center paths and nonzero open-section rotation remain
outside the present construction capability.

Patch roles use length-framed adjacent authored guide IDs; cap roles use logical terminal section
identity. Scoped neighboring support conditions resolve exact current semantic/result/imported
faces. The completed quilt must preserve every patch/cap origin, form one connected manifold
quilt (a single face is valid for an open LoftSurface), pass native self-interference checks and retain all prescribed joins after sewing and solid
orientation. Solid capping and positive-volume validation precede ordinary New/Join/Cut/Intersect
Body publication. Boundary continuity describes the constructed Loft tool; a subsequent Boolean
may trim it or reverse a subtractive material face.

Differential sampling includes native boundary C2 intervals and intersections of trim pcurves
with candidate/support spline knot lines. At knots, actual one-sided D1/D2 limits are evaluated on
all admissible native spans; a narrow curvature spike between uniform stations is rejected.
Each candidate polynomial knot cell also receives a bounded 9x9 regularity grid, with a maximum
1,024 cells. These are stated numerical witnesses together with native topology/interference
validation, not global continuum certificates. All constraints retain their authored mm/radian/
inverse-mm budgets; failure publishes no candidate and no G1 fallback. Complete feature/history/
exchange proof is recorded only when it passes in `docs/mvp-plan.md`. Advanced native controls and
Surface-face picking remain planned in Block 173.


Feature-level proof uses `[constrained-loft-feature]`: independent smoothstep volumes for all
four Body operations, actual boundary derivatives and physical STEP sections/curvature, plus
unequal rolled Airfoils, distinct patch supports, open single-face surfaces, repaired failures,
positive middle-section/guide edits and unchanged logical identities after section reversal.
The Core V12 codec also roundtrips a 257-condition intent collection: Geometry's construction
limits are explicit diagnostics, never hidden persistence limits. See the acceptance table in
`docs/mvp-plan.md` for exact build/runtime evidence and native regression scope.
