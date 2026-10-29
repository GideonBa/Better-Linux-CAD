#pragma once

#include "blcad/gui/gui_appearance.hpp"

#include <QFontMetricsF>
#include <QRectF>
#include <QString>
#include <utility>

namespace blcad::gui {

// Logical device-independent pixels, relative to the projected annotation anchor.
// Painting and picking must use the same font, padding and rectangle at every DPI.
struct GuiSketchAnnotationLayout {
  QFont font;
  QRectF bounds;
};

inline GuiSketchAnnotationLayout sketch_annotation_layout(const QString& label, bool dimension) {
  QFont font = appearance::numeric_font(dimension ? 13 : 14);
  const QRectF text = QFontMetricsF(font).boundingRect(label);
  return {std::move(font),
          {-text.width() * 0.5 - 4.0, -text.height() * 0.5 - 2.0, text.width() + 8.0,
           text.height() + 4.0}};
}

} // namespace blcad::gui
