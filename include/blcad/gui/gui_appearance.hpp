#pragma once

#include <QColor>
#include <QFont>

class QApplication;

namespace blcad::gui {

namespace appearance {
inline constexpr QRgb panel = 0xff15011a;
inline constexpr QRgb base = 0xff0d0010;
inline constexpr QRgb chrome = 0xff1d0124;
inline constexpr QRgb selection = 0xff26012f;
inline constexpr QRgb hover = 0xff22012a;
inline constexpr QRgb disabled_surface = 0xff19011f;
inline constexpr QRgb canvas = 0xff181a1b;
inline constexpr QRgb tooltip = 0xff040005;
inline constexpr QRgb text = 0xfff5f1f7;
inline constexpr QRgb muted = 0xffb9a9c2;
inline constexpr QRgb accent = 0xffd7a8ed;
inline constexpr QRgb line = 0xff3f2948;
inline constexpr QRgb edge = 0xff887292;

[[nodiscard]] QFont heading_font(int pixel_size = 22);
[[nodiscard]] QFont numeric_font(int pixel_size = 13);
} // namespace appearance

// Establish the English desktop's shared color, typography and control roles.
// Call immediately after constructing QApplication, before creating widgets.
void configure_gui_appearance(QApplication& application);

} // namespace blcad::gui
