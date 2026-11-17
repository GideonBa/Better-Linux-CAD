# Checked variable-radius Fillet geometry — Block 176

The existing `FilletAdapter` dispatches immutable variable intent to a separate checked engine;
constant Fillet keeps its established native construction. Core ownership, ordered groups,
station IDs, strict persistence and parameter dependencies are defined in
[the intent contract](part-variable-fillet-intent-mvp6.md). Whole-block status lives in `mvp-plan.md`.

## Contours and radius meaning

The radius is the radius of circular sections in planes normal to the resolved source spine.
Stations measure true cumulative edge length in millimetres, normalized over the complete contour.
The numerical law uses the authored station basis, with an explicit direction/phase mapping to the
native contour. Reversing the group and its basis preserves physical radius attachments and all
original fraction literals. Result/import edge direction is canonical geometric witness direction;
linear semantic edges follow their semantic endpoints. Semantic circles start at the source seam.

`Explicit` declares the complete native tangent contour in connected, oriented order.
`TangentChain` permits native tangent expansion; stations then span the expanded endpoints.
Closed contours retain the first declared member's oriented seam. The engine checks actual members,
orientation, adjacency, tangent continuity, branch ambiguity and overlaps between expanded groups.
Missing/split/ambiguous source witnesses retain the normal explicit reference-repair failure.

Linear laws are piecewise linear. Monotone cubic laws use weighted harmonic interior slopes and
limited one-sided endpoint slopes; a positive Bernstein control-pole bound prevents negative-radius
overshoot. Endpoint cap construction continues the endpoint tangent in physical length; a nonpositive
or singular continued radius fails. Closed contours require equal endpoint radii, with cyclic cubic
slopes and explicit periodic law evaluation. Station separation must exceed both `1e-7 mm` and the
relative `1e-12` contour resolution; unresolved stations retain their IDs in diagnostics.

## Native acquisition and physical station mapping

The implementation uses OCCT's protected stripe and virtual `AppendElSpine` extension points.
OCCT 7.6.3's [`ChFiDS_FilSpine::SetRadius(Law_Function)`](https://github.com/Open-Cascade-SAS/OCCT/blob/V7_6_3/src/ChFiDS/ChFiDS_FilSpine.cxx)
creates a temporary composite without retaining it. The checked spine instead installs the exact
law when each elementary guide is appended. The native acquisition table still declares all radii
before corner/extent analysis; it does not replace the final interpolation.

An elementary guide can reparameterize even an exact cubic/circular source. The guide's parameter
therefore passes through a physical source projection before radius evaluation. For source `C(v)`
and guide `G(u)`, stationarity is `(C-G)·C'=0`; implicit differentiation supplies `dv/du` and
`d²v/du²`. Integration of `|C'|` supplies cumulative true length, with chain-rule derivatives for
the radius. The source snapshot precedes native endpoint extensions. Outside an open contour,
physical station continuation uses the endpoint tangent and native endpoint speed; arbitrary
polynomial extrapolation of the original curve is not a station definition. Law continuity cuts
are mapped back to guide coordinates for native approximation.

This mapping is required by the independent periodic-rim and nonuniform cubic section oracles;
sampling the radius law in isolation cannot establish the geometric contract.

## Publication and lineage

All members resolve on the current target Body using the normal semantic/import/result recovery.
The candidate uses a deep BRep/geometry copy; source topology/tolerance bytes are checked unchanged
in the curved-chain regression. Candidate construction is
private until native completion, BRep validity, self-interference, finite positive mass and unchanged
solid component count pass. A failed native contour identifies its group; the diagnostic never
invents a maximum valid radius. No failed candidate replaces the published ShapeCache result.

Surviving and split support faces follow native modification history. Generated blend faces add
`fillet.group/<stable group ID>` origins. Unrelated Bodies and unselected components remain in the
normal history. A geometrically valid source can still encounter an unsupported native support or
corner combination; invalid native output is rejected before publication. The curved cubic fixture
uses an exact B-spline representation of its Bernstein polynomial, including split tangent spans.

## Verification

Native foundation verified on 2026-11-17: all five development targets built, and the focused
variable/constant-Fillet/Chamfer run passed **17 cases / 7,476 assertions**. Executable cases in `tests/geometry/variable_fillet_tests.cpp` cover
increasing/decreasing straight-edge sections, nonmonotone multiple stations, exact group reversal,
interpolation bounds/derivatives/intervals, unresolved station and excessive-radius rollback,
periodic rim closure, nonuniform cubic length, explicit/expanded chains, imported construction,
normal history/Save/Open and independent circles after physical STEP export/reimport.

The remaining F01 matrix includes branch ambiguity, Pattern, mirrored/scaled inputs, new Hole edits and explicit
reference repair. Native shared-panel interaction remains Block 179. Logs and preparatory probes
remain in `build/verification/block176/`; experiments are not acceptance evidence.
