#pragma once

#include <QColor>
#include <QFont>

class QApplication;

namespace blcad::gui {

namespace appearance {
inline constexpr QRgb blue = 0xff0000f2;
inline constexpr QRgb paper = 0xfff5f4ef;
inline constexpr QRgb ink = 0xff191638;
inline constexpr QRgb muted = 0xff625e7a;
inline constexpr QRgb line = 0xffd7d5e3;
inline constexpr QRgb edge = 0xff8b85a3;
inline constexpr QRgb wash = 0xffe9e8ff;
inline constexpr QRgb canvas = 0xff101426;

[[nodiscard]] QFont heading_font(int pixel_size = 22);
[[nodiscard]] QFont numeric_font(int pixel_size = 13);
} // namespace appearance

// Establish the English desktop's shared color, typography and control roles.
// Call immediately after constructing QApplication, before creating widgets.
void configure_gui_appearance(QApplication& application);

} // namespace blcad::gui
