#pragma once

namespace blcad::gui {

class MainWindow;

// Installs the transient Block-110 solver-backed drag bridge after the Block-107 interaction binder.
// It owns live drag preview/coalescing only; persistent mutation still uses GuiDocumentSession.
void install_sketch_drag_binder(MainWindow& window);

} // namespace blcad::gui
