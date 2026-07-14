# Part Mirror Geometry MVP-6

Status: implemented in Block 67.

Block 67 executes persistent `MirrorFeature` intent as deterministic OCCT geometry. The result
contains reflected source geometry; it does not implicitly retain an original source instance.

## Geometry contract

- Feature and Body sources resolve from `ShapeCache` in persistent source order.
- Datum planes, construction planes, and supported semantic generated planar faces resolve to a
  model-space origin and normal.
- Cached associative Body transforms are applied to referenced plane origins and normals.
- `MirrorAdapter` creates one reflected copy per source with OCCT plane reflection.
- Multiple reflected sources are fused deterministically before Body-result semantics are applied.
- `NewBody` publishes the reflected union; `Join`, `Cut`, and `Intersect` combine that union with
  the preceding target-Body result.
- Cache products are published only after all work succeeds.

The plane normal must be finite and non-zero, every source shape must exist, and Boolean results
must remain valid. Failure preserves the caller's prior cache state and reports the stable failing
source or feature identity.

## Recompute and proof boundary

Dependency order executes plane, source, and prior target-Body producers before the mirror.
Parameter edits rebuild semantic planes and reflected geometry instead of reusing stale shapes.

```text
./build/blcad_geometry_tests "[geometry][mirror-feature]"
```

The suite covers ordered output, Feature and Body sources, a non-axis-aligned construction plane,
a semantic generated top face, all four Body-result modes, parameter-driven recompute, and
transactional missing-reference failure.

Blocks 72–74 ShellFeature Geometry and DraftFeature intent/JSON plus Geometry are implemented.
Block 75 Basic 3D Sketch Core intent and Block 76 richer 3D curve intent are implemented; Block 77 3D Sketch JSON and semantic references is implemented; Block 78 3D Sketch Geometry conversion is implemented; Block 79 connected PathCurve Core intent, JSON, and validation is implemented; Block 80 Sweep feature Core intent and JSON is implemented; Block 81 Basic Sweep Geometry is implemented; Block 82 3D path, twist, and guide-controlled Sweep is implemented; Block 83 Path-following Extrude and Extruded Cut is implemented; Block 84 ProfileSectionReference and Loft Core intent plus JSON is implemented; Block 85 Two-section Loft Geometry on arbitrary planes is implemented; Block 86 Multi-section Loft is implemented; Block 87 Guided and continuity-controlled Loft is implemented; Block 88 Surface feature Core intent and JSON is implemented; Block 89 Boundary and Fill Surface Geometry is implemented; Block 90 Trim and Extend Surface Geometry is implemented; Block 91 Stitch/Knit/Sew shell Geometry is implemented; Block 92 Closed shell to solid conversion is implemented; Block 93 multi-body STEP export is implemented; Block 94 integrated Part Construction MVP acceptance is implemented; Block 95 Qt application shell, GUI document session, and command architecture is implemented; Block 96 document lifecycle, transactions, recompute, and diagnostics is implemented; Block 97 OCCT viewport, navigation, display, and semantic picking is implemented; Block 98 browser, property editor, and selection synchronization is next.
