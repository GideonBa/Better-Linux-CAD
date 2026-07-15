#pragma once

namespace blcad::gui {

class MainWindow;

// Installs the transient Block-107 Sketch interaction bridge on an already-constructed shell.
// The binder owns no persistent CAD intent; it projects the active Core Sketch into the viewport
// and feeds pointer/selection results back into the existing Block-106 workspace/session surfaces.
void install_sketch_interaction_binder(MainWindow& window);

} // namespace blcad::gui
