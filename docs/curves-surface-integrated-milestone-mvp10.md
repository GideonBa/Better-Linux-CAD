# Connected curves and Surface milestone — Block 174

The acceptance model combines one connected result before Pattern, rather than independent
feature examples. The final visible Body contains two translated copies of that solid.

The source model retains an unequal, rolled three-section airfoil with explicit G2 support
patches, two native 3D rails and a center path. All rail poles have fixed spatial constraints.
A real Smooth constraint catalog joins adjacent root Sketch spans. An affine equation in the
middle-section Sketch drives an exact curve-preserving circular Sweep. Projected cylindrical
Trim, curved Natural Extend, planar Tangent Extend and Stitch feed an exact Offset and symmetric
Thicken, followed by Hole. Body Boolean combines the airfoil, duct and thickened quilt;
Linear Pattern and a later circular rim Fillet consume that common result. Only the final
pattern Body is visible for export; all source intent remains in the document.

`tests/support/curve_surface_milestone_fixture.hpp` builds the graph. The public test support
header and its OCCT implementation keep native kernel headers out of the GUI test target.

## Changes, failures and history

Change the middle section's shared leading point and the corresponding constrained rail pole
in one transaction. Update the explicit middle guide anchor using its actual normalized curve
length; its parameter is not assumed to be the midpoint. The rail's terminal jets stay unchanged.
A second transaction changes the quilt window height and recomputes all Surface and Body consumers.
The exposed cylindrical quilt still contributes material after the Boolean.

An inconsistent rail anchor, unsupported global longitudinal G2 and an impossible inward Offset
must preserve the committed Part, last-valid Body and Undo boundary. Assert a valid initial edit
and the actual failing Offset identity; a disabled Apply button alone is insufficient. Failed
numeric candidates restore the previous scalar, while changed direction/support roles can still
require reference repair. The real numeric editor publishes the failure diagnosis. Preview errors
also remain visible in the status bar when their panel explanation has scrolled out of view.
The shared native Loft and
Surface editors exercise the latter failures against historical, hidden source geometry.
Undo both accepted changes to the exact baseline, then Redo, save and reopen. Snapshot comparison
includes the Part and every Sketch constraint/dimension catalog, including the authored Smooth
constraint. Reopening independently recomputes the complete final geometry.

## Independent physical checks

The oracle reads actual trimmed faces and native derivatives, independently of feature feedback:

- At 16 root/tip boundary stations per copy, compare Bernstein section positions, outward normals
  and normal curvature in contour, span and mixed directions against the adjacent extruded support.
  Limits are 1e-5 mm position, 1e-4 rad angle and 1e-4 /mm curvature. This is bounded boundary G2
  evidence; it does not claim global longitudinal G2. Separately sample all three analytic section
  curves on the pre-Boolean Loft, including the edited middle contour.
- At the exposed cylindrical quilt, duct and airfoil, classify independently specified material
  and void points in each actual solid. Check native BRep validity and the two final solids.
- Before Boolean, the thickened quilt volume is `(21 * acos(0.8) + 3) * (height + 3)` mm³:
  radii 10/11 mm, angle `2 * acos(0.8)`, a 3 mm tangent strip and a 1 mm total wall. The normal
  planar bore removes `0.04 * pi` mm³. The equation duct volume is `20 * pi` mm³.
  These are component measurements; the fused final Body has no asserted uniform thickness.
- The first duct rim has radius 0.2 mm on a 1 mm cylinder. Its removed volume is
  `pi * ((2 - pi/2) * R * r^2 + (pi/2 - 5/3) * r^3)` mm³. Integrate only the isolated terminal
  region, avoiding cancellation between large Loft masses. Forty independently parameterized
  contact/blend points must lie within 1e-5 mm of the actual trimmed Fillet faces.

STEP reimport decomposes the Pattern into two placed solids. Identify each by its physical
position, without assuming import ordering, and repeat the boundary differential, material,
Fillet-contact and volume checks. Compare the summed adaptive volumes to the committed result
within 5e-5 mm³. The bounded toroidal seam decoder in
[the STEP contract](step-import-sequence-mvp10.md) retains all 3D and tolerance evidence and
rejects a deliberately displaced seam vertex.

## Verification status

All five targets passed. Geometry milestone **1 / 133**, combined curved Surface/model regression
**4 / 234**, semantic Sweep/periodic STEP **6 / 193** and reader regression **14 / 751** passed.
Final focused Qt **7 passed / 2 native skips / 600 assertions** and native **1 / 203** at both
100% and compact 150% passed. Reviewed captures show reachable controls/footers, explicit global
G2 rejection and the real singular-offset diagnosis, with the last-valid geometry preserved.
Native runs repeat exact sidecar history, reopening and physical STEP checks.

The unchanged runtime manifest covers **1,219 files**:
`fc2855b392f94ebf6666548fabb1f9b90483783a1fc8534821760757a5cc684f`.
Block 174 is accepted. Continue the complete 175–184 finishing and sketch-driven feature phase;
all 185–204 planning remains in the sequence.
