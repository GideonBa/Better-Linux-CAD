#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/gui_appearance.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include <QFontMetrics>
#include <QPainter>
#include <QWidget>
#include <cmath>
namespace blcad::gui {
namespace {
constexpr double kEpsilon = 1e-9;
class ShellManipulatorOverlay final : public QWidget {
public:
  ShellManipulatorOverlay(QWidget* parent, const GuiViewportManipulatorLayer& layer)
      : QWidget(parent), layer_(layer) {
    setObjectName(QStringLiteral("blcad.viewport_manipulator_overlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& presentation : layer_.presentations()) {
      const QColor color = presentation.active ? QColor(0, 122, 204) : QColor(245, 170, 35);
      QPen pen(color);
      pen.setWidthF(presentation.active ? 3.0 : 2.0);
      painter.setPen(pen);
      painter.setBrush(color);
      const QPointF anchor(presentation.anchor.x, presentation.anchor.y);
      const QPointF endpoint(presentation.endpoint.x, presentation.endpoint.y);
      if (presentation.ring_radius_dip > 0.0) {
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(anchor, presentation.ring_radius_dip, presentation.ring_radius_dip);
        draw_ring_marker(painter, anchor, endpoint);
      } else {
        painter.drawLine(anchor, endpoint);
        draw_arrow_head(painter, anchor, endpoint);
        painter.drawEllipse(endpoint, 4.5, 4.5);
      }
      painter.setFont(appearance::numeric_font());
      painter.setPen(QColor::fromRgb(appearance::text));
      if (!presentation.value_text.empty()) {
        const QString text = QString::fromStdString(presentation.value_text);
        const QPointF origin = endpoint + QPointF(9.0, 16.0);
        const QRectF badge = QFontMetricsF(painter.font())
                                 .boundingRect(text)
                                 .translated(origin)
                                 .adjusted(-4.0, -2.0, 4.0, 2.0);
        painter.fillRect(badge, QColor::fromRgb(appearance::panel));
        painter.setPen(QColor::fromRgb(appearance::accent));
        painter.drawRect(badge);
        painter.setPen(QColor::fromRgb(appearance::text));
        painter.drawText(origin, text);
      }
      painter.setPen(QColor::fromRgb(appearance::text));
      if (presentation.handle_id.ends_with(".x") || presentation.handle_id.ends_with(".y") ||
          presentation.handle_id.ends_with(".z"))
        painter.drawText(endpoint + QPointF(8, -8),
                         QString(QChar(presentation.handle_id.back())).toUpper());
    }
  }

private:
  static void draw_arrow_head(QPainter& painter, QPointF anchor, QPointF endpoint) {
    const QPointF vector = endpoint - anchor;
    const double magnitude = std::hypot(vector.x(), vector.y());
    if (magnitude <= kEpsilon)
      return;
    const QPointF direction(vector.x() / magnitude, vector.y() / magnitude);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF base = endpoint - direction * 11.0;
    painter.drawPolygon(QPolygonF{endpoint, base + normal * 5.0, base - normal * 5.0});
  }

  static void draw_ring_marker(QPainter& painter, QPointF anchor, QPointF endpoint) {
    const QPointF vector = endpoint - anchor;
    const double magnitude = std::hypot(vector.x(), vector.y());
    if (magnitude <= kEpsilon)
      return;
    const QPointF direction(vector.x() / magnitude, vector.y() / magnitude);
    const QPointF tangent(-direction.y(), direction.x());
    painter.setBrush(painter.pen().color());
    painter.drawPolygon(QPolygonF{endpoint + tangent * 6.0,
                                  endpoint - direction * 9.0 + tangent * 2.0,
                                  endpoint - direction * 3.0 - tangent * 5.0});
  }

  const GuiViewportManipulatorLayer& layer_;
};

} // namespace
QWidget* make_shell_manipulator_overlay(QWidget* parent, const GuiViewportManipulatorLayer& layer) {
  return new ShellManipulatorOverlay(parent, layer);
}
} // namespace blcad::gui
