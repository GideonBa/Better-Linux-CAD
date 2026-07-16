# Interactive Sketcher MVP-8 Manual Smoke Checks

Status: Block-121 acceptance companion for pointer feel and visual behavior that is unreliable in
offscreen automation.

1. Start `./build/dev-gui/blcad`, open `examples/triangle_prism.blcad.json`, enter its Sketch, and
   verify normal camera, dimmed surroundings, contextual command groups, cursor coordinates, solve
   state, and remaining DOF.
2. At 100%, 150%, and 200% desktop scale, select endpoints, curves, dimensions, and constraint
   glyphs. Verify hit priority, hover stability, box/crossing selection, grid spacing, snap glyphs,
   and that the model-space cursor position does not change with scale.
3. Create a polyline, rectangle, polygon, circle, arc, ellipse approximation, and straight slot.
   Exercise numeric input, `Esc` backtracking, command repeat, Apply, Cancel, and Undo/Redo.
4. Drag an under-constrained endpoint and a connected constrained chain. Verify live solve, one undo
   record on release, no mutation on cancel, and an explanatory refusal for fixed geometry.
5. Add and delete representative coincident, horizontal, tangent, concentric, and symmetry
   constraints. Add driving and reference dimensions, then edit a parameter expression in-canvas.
6. Exercise trim, extend, split, fillet, and chamfer. Run the Core-backed offset/project/break-link
   and transform/pattern commands through their current headless harness; GUI wiring for Blocks 117
   and 118 is explicitly deferred and must not appear as a non-functional button.
7. Open `examples/projected_sketch_references.blcad.json`, invalidate a reference, and verify lost or
   ambiguous state is presented before repair; cancelling repair must retain the original intent.
8. Select and finish a bounded line region, then Undo/Redo. Verify open or self-intersecting contours
   fail without publishing a stale profile preview.
9. Create and move Sketch3D points/lines with axis and plane locks. Curved Sketch3D handles remain
   unavailable according to the accepted deferral.
