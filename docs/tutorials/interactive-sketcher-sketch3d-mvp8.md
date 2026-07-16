# Interactive Sketch3D Tutorial MVP-8

This tutorial is the deterministic Block-120/121 acceptance scenario for direct 3D Sketch interaction.

## Document

- document id: `tutorial.sketch3d`
- Sketch3D id: `sketch3d.tutorial`

## Steps

1. `sketch3d.001.origin`: create `point.origin` at `(0,0,0)` using typed XYZ input.
2. `sketch3d.002.axis-lock`: create `point.x` with X-axis lock through `point.origin` and candidate `(30,12,8)`; expect `(30,0,0)`.
3. `sketch3d.003.plane-lock`: create `point.xy` with XY-plane lock and candidate `(30,20,15)`; expect `(30,20,0)`.
4. `sketch3d.004.angular-input`: create `point.directed` from the origin with distance `50 mm`, azimuth `30 deg`, and elevation `20 deg`.
5. `sketch3d.005.line`: create `line.path` from `point.origin` to `point.directed` and assign guide role `SweepPath` with guide id `line.path.guide`.
6. `sketch3d.006.handle`: move `point.directed` to `(45,25,10)`; expect `line.path` and the guide record to preserve identity.
7. `sketch3d.007.project`: project a planar Sketch line-start target into local point `point.projected`; expect explicit source Sketch and target intent.
8. `sketch3d.008.invalid`: attempt a negative-distance placement and a move of a missing point; both must fail without source mutation.
9. `sketch3d.009.persistence`: save, reopen, and verify point coordinates, line references, guide role, and projected-point source intent.
10. `sketch3d.010.history`: undo and redo the accepted handle move and compare exact persistent state.

## Acceptance

Pointer placement and direct command input must resolve to the same model-space coordinates and persistent ids. This tutorial does not imply curved 3D creation handles or a variational 3D constraint solver; those remain explicitly deferred.
