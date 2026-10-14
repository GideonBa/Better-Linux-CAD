# Interactive Sketcher MVP-8 Manual Smoke Checks

Status: Block-121 acceptance companion for pointer feel and visual behavior that is unreliable in
offscreen automation.

1. Start `./build/dev-gui/blcad`, open `examples/triangle_prism.blcad.json`, enter its Sketch, and
   verify normal camera, dimmed surroundings, contextual command groups, cursor coordinates, solve
   state, and remaining DOF. Resize the shell to 1280×800, scroll the Sketch ribbon to both
   ends, and verify every command and caption remains reachable without forcing a wider window.
   Draw a three-point semicircle and a closing line with the same authored start/end direction.
   Verify its interior is a selectable exact region, Finish creates a curved profile, and Extrude
   previews it. Check Cancel, one-step Undo/Redo and Save/Open. Repeat with two semicircles and a
   raw four-span rational ellipse. A spline crossing another edge or itself must reject Finish
   without changing the document or history.
2. At 100%, 150%, and 200% desktop scale, select endpoints, curves, dimensions, and constraint
   glyphs. Verify hit priority, hover stability, box/crossing selection, grid spacing, snap glyphs,
   and that the model-space cursor position does not change with scale.
3. Create a polyline, rectangle, polygon, circle, arc, exact ellipse, and straight slot.
   Exercise numeric input, `Esc` backtracking, command repeat, Apply, Cancel, and Undo/Redo.
   For tangent arcs, pick a curve endpoint then the new endpoint. For tangent circles, pick a line
   contact or arc/spline endpoint then a rim point; verify the preview is tangent and the relation
   persists after dragging the source and reopening the file.
4. Drag an under-constrained endpoint and a connected constrained chain. Verify live solve, one undo
   record on release, no mutation on cancel, and an explanatory refusal for fixed geometry.
5. Add and delete representative coincident, horizontal, tangent, concentric, and symmetry
   constraints. Add driving and reference dimensions, then edit a parameter expression in-canvas.
   Drag a dimension label, save/reopen, and verify its placement; cancel a second label drag with
   Escape. Verify Enter accepts and Escape cancels the inline dimension value editor.
6. Exercise trim, extend, split, fillet, and chamfer on loose curves and ordered profiles. Include
   line/arc and spline corners, reversed edge selection, oversized corner rejection, and cubic
   continuation. Verify one undo removes a corner together with its constraints and size.
   Select an unordered/reversed line chain and run **Versatz** with positive and negative distances;
   include collinear joins, a closed loop, one arc, and a rejected branched selection.
   Run **Verschieben**, **Drehen**, **Skalieren**, **Kopieren**, **Spiegeln**, **Rechteckmuster**,
   and **Kreismuster** from **Transformieren**. Change numeric fields and check the dashed preview,
   including separate paths for disconnected copies. Cancel/Escape must restore the original view
   and history. Apply must be undoable in one step. Check a 360° pattern has no duplicate seed,
   mirrored ellipses retain their exact shape/profile, and fixed/driving geometry rejects conflicting
   transforms. Select a line plus a parameter-backed circle and verify the whole transform is
   unavailable/rejected with no partial move. Free points and projected references must not enable
   ordinary curve transforms. Copy reports identify relationships that are not copied to instances.
   Use **Geometrie projizieren** to choose a construction source or semantic model edge/vertex.
   Select the projected line and **Verknüpfung lösen**, set its segment length, Apply, Undo, and reopen
   the file. Verify source identity before detachment and ordinary editable geometry afterward.
7. Open `examples/projected_sketch_references.blcad.json`, invalidate a reference, and verify lost or
   ambiguous state is presented before repair; cancelling repair must retain the original intent.
8. Select and finish a bounded line region, then Undo/Redo. Finish an open contour and re-enter its
   Sketch; verify its geometry remains editable and no artificial closed profile was created.
   Self-intersecting and branched closed region candidates must still fail with diagnostics.
9. Create and move Sketch3D points/lines with axis and plane locks. Curved Sketch3D handles remain
   unavailable according to the accepted deferral.
