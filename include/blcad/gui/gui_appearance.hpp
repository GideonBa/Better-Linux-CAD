#pragma once

class QApplication;

namespace blcad::gui {

// Establish the desktop's English, square, black-and-white widget contract.
// Call immediately after constructing QApplication, before creating widgets.
void configure_gui_appearance(QApplication& application);

} // namespace blcad::gui
