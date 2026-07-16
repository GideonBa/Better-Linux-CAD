#pragma once

namespace blcad::gui {

class MainWindow;

// Installs Block-115 dimension commands on the existing Sketch workspace shell.
// The binder owns only QAction/task-dialog state; Core intent, solve authority,
// persistence, and undo/redo remain in GuiSketchDimensionController/session.
void install_sketch_dimension_binder(MainWindow& window);
void refresh_sketch_dimension_actions(MainWindow& window);

} // namespace blcad::gui
