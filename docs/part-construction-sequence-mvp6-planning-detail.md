# Part Construction MVP-6 — retained planning boundaries

Blocks 48–94 are implemented. The [completed block map](part-construction-sequence-mvp6.md)
links their current behavior, persistence, failure policy and focused tests. This companion keeps
the original deferred scope and combined acceptance requirements; it is not current status or a
second implementation queue. Some original deferrals have since been implemented: consult
[mvp-plan.md](mvp-plan.md) and [the advanced sequence](inventor-parity-sequence.md).

Repeated completed-block recipes were removed. The full historical snapshot is recoverable with:

```bash
git show 46ab49c:docs/part-construction-sequence-mvp6-planning-detail.md
```

## Original feature bounds and deferred work (verbatim)

Non-uniform scale and arbitrary matrix authoring remain deferred.

First MVP intentionally excludes arbitrary face-only patterning and raw topology copies.

The existing `CircularHolePattern` remains a specialized compatibility fast path until explicitly migrated.

Variable-radius fillets and setback/corner-management systems remain deferred.

First implementation supports explicit coordinates and typed parameter-driven coordinates. No general 3D constraint solver yet.

Spline points may reference:

```text
explicit 3D points
construction points
semantic points from planar sketches on different planes
later recovered generated vertices/edge points
```

Freeze fit/control-point representation, degree/continuity constraints for the first seed, helix axis/pitch/turn semantics, dependencies, and failure behavior.

Supported segment sources include:

```text
planar sketch line/arc/spline
projected/reference-driven curve
construction line
3D sketch line/polyline/arc/spline/helix
semantic generated edge when Block-35/36 recovery supports it
```

Validation requires deterministic ordered connectivity within explicit tolerance. Branch points and self-intersecting paths may be rejected in the first MVP.

Continuity contract:

```text
C0 Position
G1 Tangent
G2 Curvature
```

C0 and G1 must be fully proved. G2 must either be implemented with explicit OCCT/geometry guarantees or fail closed as an unsupported persisted option until a dedicated Geometry increment; do not silently degrade G2 to G1.

Required fixtures include duct transition and airfoil/blade-like root/mid/tip sections with leading/trailing guide curves.

`NetworkSurface` and arbitrary patch-network intent remain deferred after the first Part Construction MVP.

No automatic topology healing or arbitrary gap closing.

## What "Part Construction MVP complete" means

After Block 94 BLCAD should have a serious first parametric Part kernel for common mechanical and first freeform workflows.

It does not mean SolidWorks Part or Inventor Part product parity.

Still deferred beyond Block 94:

```text
production GUI modeling/picking/previews
full Class-A surfacing
arbitrary NURBS control-cage editing
variable-radius fillets
advanced fillet corner/setback systems
face-only and sketch-driven general pattern breadth beyond the frozen first pattern source model
advanced topology healing and automatic gap repair
direct modeling / push-pull
sheet-metal feature system
weldments / structural members
mold/core/cavity toolsets
full thread/emboss/rib/web specialized features
feature configurations/design tables
full history rollback/UI feature suppression workflows
part-level finite-element or manufacturing modules
```

The next phase after Block 94 must be planned from measured gaps in the integrated Part Construction MVP rather than by adding several unrelated feature families to Block 94.

## Original combined acceptance requirements

## Block 94 — Integrated Part Construction MVP acceptance and headless workflows

Primary boundary: end-to-end Part Construction MVP proof.

This block introduces no new feature family unless an integration defect requires a narrowly scoped correction.

Add representative headless fixtures proving:

### Multi-body mechanical part

```text
Body A from Extrude
Body B from Revolve
BodyTransform on B
A - B through BodyBooleanFeature
Fillet semantic edges
Chamfer semantic edges
Shell selected semantic faces
Draft selected faces
Linear Pattern of a feature
Circular Pattern of another feature
Mirror Feature across DatumPlane
```

### Routed/swept part

```text
3D Sketch
PathCurve
Sweep solid
SweepCut
path-following Extrude/Cut
```

### Lofted freeform part

```text
root/mid/tip profile sections on arbitrary planes
3D guide curves
multi-section guided Loft
surface Boundary/Fill support fixture
surface Stitch
closed shell -> Solid
```

Prove:

```text
full JSON roundtrip for every new persistent family
stable dependency/invalidation/recompute
incremental recompute of affected bodies only
semantic reference recovery/fail-closed behavior
no raw OCCT identity in Core JSON
multi-body STEP export
source model immutability for Geometry queries
headless reproducibility/insertion-order independence where ordering is declared deterministic
```

Focused tag:

```text
[integration][part-construction-mvp]
```

After Block 94, the first Part Construction MVP is considered complete.
