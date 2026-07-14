# GUI Feature Validation Manual Smoke Checks

Status: Block-105 acceptance companion for interactions that are unreliable offscreen.

1. Start `./build/dev-gui/blcad` and verify the Archimedes splash hands off to the CAD shell.
2. Open `examples/bolt_circle_plate.blcad.json`; orbit, pan, zoom, fit, and switch
   shaded/wireframe plus orthographic/perspective modes.
3. Select one Body, datum, sketch entity, edge, and face in both browser and viewport; verify the
   same semantic item and property record becomes active in the other surface.
4. Enter a planar Sketch and verify the camera becomes normal to the workplane; enter one numeric
   line and verify pointer/model coordinates remain in the same plane.
5. Open `examples/nested_subassembly.blcad.project.json`; expand rooted occurrence paths and verify
   repeated components remain distinct while selecting them in the viewport.
6. Start an Apply-capable task, inspect Preview, then Cancel; verify neither browser nor viewport
   publishes candidate state. Repeat and Apply, then Undo/Redo.
7. Start a STEP export, cancel before writing, then export to a new path and verify the completion
   diagnostic and file presence.
