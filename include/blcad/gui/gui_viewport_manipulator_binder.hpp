#pragma once

#include "blcad/gui/gui_viewport_manipulator.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>

namespace blcad::gui {

class GuiViewportManipulatorOverlay final : public QWidget {
public:
  GuiViewportManipulatorOverlay(QWidget* parent, GuiViewportManipulatorLayer* layer)
      : QWidget(parent), layer_(layer) {
    setObjectName(QStringLiteral("blcad.viewport_manipulator_overlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    QWidget::paintEvent(event);
    if (layer_ == nullptr)
      return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto presentations = layer_->presentations();
    for (const auto& presentation : presentations) {
      QPen pen(presentation.active ? palette().highlight().color()
                                   : palette().windowText().color());
      pen.setWidthF(presentation.active ? 2.5 : 1.5);
      painter.setPen(pen);
      painter.setBrush(presentation.active ? palette().highlight()
                                           : palette().windowText());

      const QPointF anchor(presentation.anchor.x, presentation.anchor.y);
      const QPointF endpoint(presentation.endpoint.x, presentation.endpoint.y);
      if (presentation.ring_radius_dip > 0.0) {
        const double radius = presentation.ring_radius_dip;
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(anchor, radius, radius);
        draw_ring_marker(painter, anchor, endpoint);
      } else {
        painter.drawLine(anchor, endpoint);
        draw_arrow_head(painter, anchor, endpoint);
        painter.drawEllipse(endpoint, 4.0, 4.0);
      }

      if (presentation.kind == GuiViewportManipulatorKind::PatternCount)
        painter.drawText(endpoint + QPointF(7.0, -7.0), QStringLiteral("#"));
      if (!presentation.value_text.empty())
        painter.drawText(endpoint + QPointF(9.0, 14.0),
                         QString::fromStdString(presentation.value_text));
    }
  }

private:
  static void draw_arrow_head(QPainter& painter, QPointF anchor, QPointF endpoint) {
    const QPointF vector = endpoint - anchor;
    const double magnitude = std::hypot(vector.x(), vector.y());
    if (magnitude <= 1.0e-9)
      return;
    const QPointF direction(vector.x() / magnitude, vector.y() / magnitude);
    const QPointF normal(-direction.y(), direction.x());
    const QPointF base = endpoint - direction * 10.0;
    QPolygonF arrow;
    arrow << endpoint << base + normal * 4.5 << base - normal * 4.5;
    painter.drawPolygon(arrow);
  }

  static void draw_ring_marker(QPainter& painter, QPointF anchor, QPointF endpoint) {
    const QPointF vector = endpoint - anchor;
    const double magnitude = std::hypot(vector.x(), vector.y());
    if (magnitude <= 1.0e-9)
      return;
    const QPointF direction(vector.x() / magnitude, vector.y() / magnitude);
    const QPointF tangent(-direction.y(), direction.x());
    const QPointF marker = anchor + direction * magnitude;
    painter.setBrush(painter.pen().color());
    QPolygonF arrow;
    arrow << marker + tangent * 6.0
          << marker - direction * 9.0 + tangent * 2.0
          << marker - direction * 3.0 - tangent * 5.0;
    painter.drawPolygon(arrow);
  }

  GuiViewportManipulatorLayer* layer_{nullptr};
};

class GuiViewportManipulatorShellBinder final : public QObject {
public:
  using CandidateCallback =
      std::function<void(const GuiViewportManipulatorCandidate&)>;

  GuiViewportManipulatorShellBinder(QMainWindow* window,
                                    GuiViewportManipulatorLayer* layer,
                                    CandidateCallback candidate_callback = {})
      : window_(window), layer_(layer),
        candidate_callback_(std::move(candidate_callback)) {
    if (window_)
      QTimer::singleShot(0, window_, [this] { ensure_installed(); });
  }

  void ensure_installed() {
    if (installed_ || window_.isNull() || layer_ == nullptr)
      return;
    viewport_ = window_->findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
    if (viewport_.isNull())
      return;

    toolbar_ = window_->findChild<QToolBar*>(QStringLiteral("blcad.modeling_toolbar"));
    if (toolbar_.isNull()) {
      toolbar_ = window_->addToolBar(QStringLiteral("Viewport Manipulators"));
      toolbar_->setObjectName(QStringLiteral("blcad.manipulator_toolbar"));
      toolbar_->setMovable(false);
    }

    auto* label = new QLabel(QStringLiteral("Value"), toolbar_);
    label->setObjectName(QStringLiteral("blcad.manipulator_numeric_label"));
    toolbar_->addWidget(label);
    numeric_hud_ = new QLineEdit(toolbar_);
    numeric_hud_->setObjectName(QStringLiteral("blcad.manipulator_numeric_hud"));
    numeric_hud_->setPlaceholderText(QStringLiteral("Drag a handle or type a value"));
    numeric_hud_->setMaximumWidth(180);
    toolbar_->addWidget(numeric_hud_);
    connect(numeric_hud_, &QLineEdit::textEdited, this, [this](const QString& text) {
      if (layer_->set_numeric_input(text.toStdString())) {
        publish_candidate();
        synchronize_from_layer();
      }
    });

    overlay_ = new GuiViewportManipulatorOverlay(viewport_, layer_);
    overlay_->setGeometry(viewport_->rect());
    overlay_->raise();

    viewport_->installEventFilter(this);
    window_->installEventFilter(this);
    update_mapping();

    revision_timer_ = new QTimer(window_);
    revision_timer_->setInterval(33);
    connect(revision_timer_, &QTimer::timeout, this,
            [this] { synchronize_from_layer(); });
    revision_timer_->start();

    installed_ = true;
    synchronize_from_layer();
  }

  void refresh() {
    ensure_installed();
    if (!installed_)
      return;
    update_mapping();
    synchronize_from_layer(true);
  }

  void set_candidate_callback(CandidateCallback callback) {
    candidate_callback_ = std::move(callback);
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (!installed_ || viewport_.isNull())
      return QObject::eventFilter(watched, event);

    if (watched == viewport_) {
      switch (event->type()) {
      case QEvent::Resize:
        overlay_->setGeometry(viewport_->rect());
        update_mapping();
        synchronize_from_layer(true);
        break;
      case QEvent::Wheel:
        QTimer::singleShot(0, viewport_, [this] {
          update_mapping();
          synchronize_from_layer(true);
        });
        break;
      case QEvent::MouseButtonPress: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
          update_mapping();
          if (layer_->begin_drag(to_screen(mouse->position()))) {
            viewport_->grabMouse();
            synchronize_from_layer(true);
            return true;
          }
        }
        break;
      }
      case QEvent::MouseMove: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (!layer_->active_handle_id().empty() &&
            layer_->update_drag(to_screen(mouse->position()))) {
          publish_candidate();
          synchronize_from_layer(true);
          return true;
        }
        break;
      }
      case QEvent::MouseButtonRelease: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton &&
            !layer_->active_handle_id().empty()) {
          const auto candidate = layer_->end_drag(to_screen(mouse->position()));
          viewport_->releaseMouse();
          if (candidate && candidate_callback_)
            candidate_callback_(*candidate);
          synchronize_from_layer(true);
          return true;
        }
        break;
      }
      default: break;
      }
    }

    if (event->type() == QEvent::KeyPress &&
        !layer_->active_handle_id().empty()) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape) {
        layer_->cancel();
        if (viewport_->mouseGrabber() == viewport_)
          viewport_->releaseMouse();
        window_->statusBar()->showMessage(QStringLiteral("Manipulator cancelled"));
        synchronize_from_layer(true);
        return true;
      }
    } else if (event->type() == QEvent::WindowDeactivate &&
               !layer_->active_handle_id().empty()) {
      layer_->cancel();
      if (viewport_->mouseGrabber() == viewport_)
        viewport_->releaseMouse();
      synchronize_from_layer(true);
    }

    return QObject::eventFilter(watched, event);
  }

private:
  [[nodiscard]] static GuiSketchScreenPoint to_screen(QPointF point) noexcept {
    return {point.x(), point.y()};
  }

  void update_mapping() {
    if (viewport_.isNull() || layer_ == nullptr || viewport_->width() <= 0 ||
        viewport_->height() <= 0)
      return;
    auto mapping = GuiViewportManipulatorMapping::create_camera(
        viewport_->camera_bookmark(), static_cast<double>(viewport_->width()),
        static_cast<double>(viewport_->height()));
    if (!mapping.has_error())
      (void)layer_->set_mapping(std::move(mapping.value()));
  }

  void synchronize_from_layer(bool force = false) {
    if (!installed_ || layer_ == nullptr || overlay_.isNull() ||
        numeric_hud_.isNull())
      return;
    if (!force && last_revision_ == layer_->revision())
      return;
    last_revision_ = layer_->revision();

    const bool has_handles = !layer_->handles().empty();
    overlay_->setVisible(has_handles);
    numeric_hud_->setVisible(has_handles);
    if (has_handles) {
      overlay_->setGeometry(viewport_->rect());
      overlay_->raise();
      overlay_->update();
    }

    const QSignalBlocker blocker(numeric_hud_);
    numeric_hud_->setText(QString::fromUtf8(
        layer_->numeric_text().data(),
        static_cast<qsizetype>(layer_->numeric_text().size())));
    numeric_hud_->setEnabled(!layer_->active_handle_id().empty());
  }

  void publish_candidate() {
    if (!layer_->candidate())
      return;
    if (candidate_callback_)
      candidate_callback_(*layer_->candidate());
    if (!window_.isNull())
      window_->statusBar()->showMessage(
          QStringLiteral("Manipulator candidate: %1")
              .arg(QString::fromUtf8(layer_->numeric_text().data(),
                                     static_cast<qsizetype>(
                                         layer_->numeric_text().size()))));
  }

  QPointer<QMainWindow> window_;
  GuiViewportManipulatorLayer* layer_{nullptr};
  CandidateCallback candidate_callback_;
  QPointer<OcctViewport> viewport_;
  QPointer<QToolBar> toolbar_;
  QPointer<QLineEdit> numeric_hud_;
  QPointer<GuiViewportManipulatorOverlay> overlay_;
  QPointer<QTimer> revision_timer_;
  std::size_t last_revision_{std::numeric_limits<std::size_t>::max()};
  bool installed_{false};
};

} // namespace blcad::gui
