#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/core/sketch_3d_interaction.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_viewport_manipulator.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <array>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "sketch3d.edit";
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
bool has_entity(const Sketch3D& sketch, const SketchEntityId& id) {
  return sketch.find_point(id) || sketch.find_line(id) || sketch.find_polyline(id) ||
         sketch.find_arc(id) || sketch.find_spline(id) || sketch.find_helix(id) ||
         sketch.find_guide_curve(id);
}
SketchEntityId next_entity(const Sketch3D& sketch, const char* prefix) {
  for (std::size_t index = 1;; ++index) {
    SketchEntityId id(std::string(prefix) + "." + std::to_string(index));
    if (!has_entity(sketch, id))
      return id;
  }
}
class PlacementOverlay final : public QWidget {
public:
  explicit PlacementOverlay(OcctViewport& viewport) : QWidget(&viewport), viewport_(viewport) {
    setObjectName("blcad.sketch3d.placement_overlay");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
  }
  void present(Point3 anchor, std::optional<Point3> candidate) {
    anchor_ = anchor;
    candidate_ = candidate;
    setGeometry(viewport_.rect());
    setVisible(candidate_.has_value());
    if (candidate_) {
      raise();
      update();
    }
  }

protected:
  void paintEvent(QPaintEvent*) override {
    if (!candidate_)
      return;
    const auto screen = viewport_.model_to_screen(*candidate_);
    const auto anchor = viewport_.model_to_screen(anchor_);
    if (!screen || !anchor)
      return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF p(screen.value().x, screen.value().y);
    const QPointF a(anchor.value().x, anchor.value().y);
    painter.setPen(QPen(QColor(245, 170, 35), 1.5, Qt::DashLine));
    painter.drawLine(a, p);
    painter.setPen(QPen(QColor(245, 170, 35), 2));
    painter.drawEllipse(p, 5, 5);
    painter.drawLine(p - QPointF(9, 0), p + QPointF(9, 0));
    painter.drawLine(p - QPointF(0, 9), p + QPointF(0, 9));
  }

private:
  OcctViewport& viewport_;
  Point3 anchor_{};
  std::optional<Point3> candidate_;
};
} // namespace

struct ShellSketch3DTools::Impl {
  ShellWindow& window;
  ShellSketch3DTools& owner;
  QAction* action{};
  QDockWidget* dock{};
  QLabel* title{};
  QComboBox* point{};
  std::array<QDoubleSpinBox*, 3> coordinates{};
  QCheckBox* detach{};
  QComboBox* start{};
  QComboBox* end{};
  QLabel* summary{};
  QPushButton* apply{};
  QComboBox* pick{};
  QComboBox* lock{};
  QComboBox* placement_anchor{};
  QCheckBox* grips{};
  QLineEdit* numeric{};
  QWidget* overlay{};
  PlacementOverlay* placement_overlay{};
  Point3 placement_origin{};
  std::optional<Point3> placement_hover;
  bool placement_pressed{false};
  GuiViewportManipulatorLayer layer;
  std::optional<GuiViewportCameraBookmark> camera;
  std::array<int, 2> viewport_size{};
  struct Drag {
    Sketch3D source;
    SketchEntityId point;
    Point3 origin;
    bool changed, previewed;
  };
  std::optional<Drag> drag;
  bool numeric_invalid{false};
  std::optional<Sketch3D> draft;
  bool editing{false};
  bool changed{false};
  bool pending_point{false};
  bool previewed{false};
  std::size_t revision{};

  bool available() const {
    const auto& session = window.session();
    return session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() && !session.task().active();
  }
  bool current() const {
    const auto& session = window.session();
    return draft && session.part_document() && session.derived_results_fresh() &&
           !session.reference_repair_mode() && !window.active_sketch() &&
           session.task().command_id() == task_id && session.presentation_revision() == revision;
  }
  void invalidate() {
    previewed = false;
    apply->setEnabled(false);
    window.viewport().clear_feature_preview();
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void select_point() {
    if (!current())
      return;
    cancel_drag();
    cancel_placement();
    invalidate();
    const auto* selected =
        draft->find_point(SketchEntityId(point->currentData().toString().toStdString()));
    const std::array<const SketchCoordinate3D*, 3> values{selected ? &selected->x() : nullptr,
                                                          selected ? &selected->y() : nullptr,
                                                          selected ? &selected->z() : nullptr};
    for (int axis = 0; axis < 3; ++axis) {
      QSignalBlocker blocker(coordinates[axis]);
      double value = 0;
      if (values[axis])
        value = values[axis]->explicit_coordinate()
                    ? values[axis]->explicit_coordinate()->millimeters()
                    : window.session()
                          .part_document()
                          ->find_parameter(*values[axis]->parameter())
                          ->value()
                          .millimeters();
      coordinates[axis]->setValue(value);
    }
    QSignalBlocker blocker(detach);
    detach->setChecked(false);
    detach->setVisible(selected && !selected->parameter_dependencies().empty());
    pending_point = false;
    summary->setText(
        QStringLiteral("Punkte/Linien lokal übernehmen, dann die gesamte Vorschau prüfen."));
    refresh_handles();
  }
  void fill_points(const SketchEntityId& selected) {
    const auto previous_start = start->currentData();
    const auto previous_end = end->currentData();
    const auto previous_anchor = placement_anchor->currentData();
    QSignalBlocker point_blocker(point), start_blocker(start), end_blocker(end),
        anchor_blocker(placement_anchor);
    point->clear();
    start->clear();
    end->clear();
    placement_anchor->clear();
    placement_anchor->addItem(QStringLiteral("Ursprung (0, 0, 0)"), QString());
    point->addItem(QStringLiteral("Neuer Punkt"), QString());
    for (const auto& item : draft->points()) {
      const auto id = text(item.id().value());
      point->addItem(id, id);
      start->addItem(id, id);
      end->addItem(id, id);
      placement_anchor->addItem(id, id);
    }
    point->setCurrentIndex(std::max(0, point->findData(text(selected.value()))));
    const auto restore = [](QComboBox* combo, const QVariant& previous, int fallback) {
      const int row = combo->findData(previous);
      combo->setCurrentIndex(row >= 0 ? row : fallback);
    };
    restore(start, previous_start, start->count() ? 0 : -1);
    restore(end, previous_end, end->count() - 1);
    restore(placement_anchor, previous_anchor, 0);
    select_point();
  }
  Result<std::size_t> stage_point() {
    if (!current())
      return Result<std::size_t>::failure(
          error("3D-Skizze neu öffnen: Modell oder Befehl geändert."));
    if (drag || placement_pressed)
      return Result<std::size_t>::failure(error("Griffbewegung zuerst beenden."));
    for (auto* spin : coordinates)
      spin->interpretText();
    const SketchPoint3DValue value{coordinates[0]->value(), coordinates[1]->value(),
                                   coordinates[2]->value()};
    auto id = SketchEntityId(point->currentData().toString().toStdString());
    const auto* selected = draft->find_point(id);
    if (selected && !selected->parameter_dependencies().empty() && !detach->isChecked())
      return Result<std::size_t>::failure(
          error("Parameterbindung nur mit ausdrücklicher Zustimmung durch XYZ-Werte ersetzen."));
    if (!selected)
      id = next_entity(*draft, "point");
    auto candidate = selected ? Sketch3DInteractionService::move_point(*draft, id, value)
                              : Sketch3DInteractionService::add_point(*draft, id, value);
    if (!candidate)
      return Result<std::size_t>::failure(candidate.error());
    draft = std::move(candidate.value().sketch);
    changed = true;
    fill_points(id);
    summary->setText(
        QStringLiteral("Punkt lokal übernommen. %1 Punkte, %2 Linien. Noch nicht gespeichert.")
            .arg(draft->points().size())
            .arg(draft->lines().size()));
    return Result<std::size_t>::success(draft->entity_count());
  }
  Result<std::size_t> stage_line() {
    if (!current() || pending_point || drag || placement_pressed)
      return Result<std::size_t>::failure(
          error("Punktänderung zuerst lokal übernehmen oder neu auswählen."));
    invalidate();
    auto line = SketchLine3D::create(next_entity(*draft, "line"),
                                     SketchEntityId(start->currentData().toString().toStdString()),
                                     SketchEntityId(end->currentData().toString().toStdString()));
    if (!line)
      return Result<std::size_t>::failure(line.error());
    auto candidate = *draft;
    auto added = candidate.add_line(std::move(line.value()));
    if (!added)
      return added;
    draft = std::move(candidate);
    changed = true;
    invalidate();
    summary->setText(
        QStringLiteral("Linie lokal übernommen. %1 Punkte, %2 Linien. Vorschau prüfen.")
            .arg(draft->points().size())
            .arg(draft->lines().size()));
    return added;
  }
  Point3 resolved_point(const SketchPoint3D& point) const {
    const auto coordinate = [&](const SketchCoordinate3D& value) {
      return value.explicit_coordinate() ? value.explicit_coordinate()->millimeters()
                                         : window.session()
                                               .part_document()
                                               ->find_parameter(*value.parameter())
                                               ->value()
                                               .millimeters();
    };
    return {coordinate(point.x()), coordinate(point.y()), coordinate(point.z())};
  }
  const SketchPoint3D* selected_point() const {
    return draft ? draft->find_point(SketchEntityId(point->currentData().toString().toStdString()))
                 : nullptr;
  }
  void display_point(Point3 value) {
    const std::array values{value.x, value.y, value.z};
    for (int i = 0; i < 3; ++i) {
      QSignalBlocker blocker(coordinates[i]);
      coordinates[i]->setValue(values[i]);
    }
  }
  void synchronize_overlay() {
    overlay->setGeometry(window.viewport().rect());
    overlay->setVisible(current() && !layer.handles().empty());
    if (overlay->isVisible()) {
      overlay->raise();
      overlay->update();
    }
    if (placement_overlay)
      placement_overlay->present(placement_origin, current() ? placement_hover : std::nullopt);
  }
  void refresh_handles() {
    if (!overlay)
      return;
    if (drag) {
      synchronize_overlay();
      return;
    }
    std::vector<GuiViewportManipulatorHandle> handles;
    const auto* selected = selected_point();
    if (current()) {
      QSignalBlocker blocker(detach);
      const bool bound = selected && !selected->parameter_dependencies().empty();
      detach->setVisible(bound);
      if (!bound)
        detach->setChecked(false);
    }
    if (current() && grips->isChecked() && pick->currentIndex() == 0 && !pending_point &&
        selected && (selected->parameter_dependencies().empty() || detach->isChecked())) {
      const auto value = resolved_point(*selected);
      const std::array values{value.x, value.y, value.z};
      const int restriction = lock->currentIndex();
      auto all = GuiViewportManipulatorLayer::translation_triad("sketch3d.position", value);
      for (int i = 0; i < 3; ++i) {
        const bool allowed = restriction == 0 || restriction == i + 1 ||
                             (restriction == 4 && i != 2) || (restriction == 5 && i != 0) ||
                             (restriction == 6 && i != 1);
        if (!allowed)
          continue;
        auto handle = all[i];
        handle.reference_value = values[i];
        handle.minimum_value = -1e9;
        handle.maximum_value = 1e9;
        handles.push_back(handle);
      }
    }
    (void)layer.set_handles(std::move(handles));
    numeric->setReadOnly(true);
    synchronize_overlay();
  }
  void refresh_mapping() {
    auto& viewport = window.viewport();
    const auto next = viewport.camera_bookmark();
    const std::array size{viewport.width(), viewport.height()};
    const bool changed_camera =
        !camera || size != viewport_size || next.eye != camera->eye ||
        next.target != camera->target || next.up_direction != camera->up_direction ||
        next.scale != camera->scale || next.projection != camera->projection;
    camera = next;
    viewport_size = size;
    if (changed_camera) {
      cancel_drag();
      cancel_placement();
    }
    auto mapping = GuiViewportManipulatorMapping::create(
        [&viewport](GuiSketchScreenPoint p) { return viewport.screen_to_view_ray(p); },
        [&viewport](Point3 p) { return viewport.model_to_screen(p); });
    if (mapping)
      (void)layer.set_mapping(std::move(mapping.value()));
    refresh_handles();
  }
  void cancel_drag(bool restore_preview = true) {
    if (!drag)
      return;
    auto start = std::move(*drag);
    drag.reset();
    layer.cancel();
    numeric_invalid = false;
    draft = std::move(start.source);
    changed = start.changed;
    pending_point = false;
    if (window.viewport().mouseGrabber() == &window.viewport())
      window.viewport().releaseMouse();
    numeric->clear();
    numeric->setReadOnly(true);
    if (current())
      if (const auto* selected = selected_point())
        display_point(resolved_point(*selected));
    invalidate();
    if (restore_preview && start.previewed && current()) {
      auto result = owner.preview();
      if (!result)
        summary->setText(text(result.error().message()));
    }
    refresh_handles();
  }
  void accept_drag(const GuiViewportManipulatorCandidate& candidate) {
    if (!current() || !drag)
      return;
    auto value = drag->origin;
    if (candidate.handle_id.ends_with(".x"))
      value.x = candidate.scalar_value;
    if (candidate.handle_id.ends_with(".y"))
      value.y = candidate.scalar_value;
    if (candidate.handle_id.ends_with(".z"))
      value.z = candidate.scalar_value;
    auto resolved = Sketch3DInteractionService::resolve_point(
        {drag->origin.x, drag->origin.y, drag->origin.z}, {value.x, value.y, value.z},
        static_cast<Sketch3DLock>(lock->currentIndex()));
    if (!resolved)
      return;
    auto candidate_sketch =
        Sketch3DInteractionService::move_point(drag->source, drag->point, resolved.value());
    if (!candidate_sketch)
      return;
    draft = std::move(candidate_sketch.value().sketch);
    changed = true;
    display_point({resolved.value().x, resolved.value().y, resolved.value().z});
    auto result = owner.preview();
    if (!result)
      summary->setText(text(result.error().message()));
    synchronize_overlay();
  }
  void edit_numeric() {
    if (!drag)
      return;
    numeric_invalid = !layer.set_numeric_input(numeric->text().toStdString());
    if (numeric_invalid) {
      invalidate();
      summary->setText(QStringLiteral(
          "Ungültiger Griffwert. XYZ-Koordinate in mm eingeben oder Escape drücken."));
    } else
      accept_drag(*layer.candidate());
  }
  void cancel_placement() {
    const bool captured = placement_pressed;
    const bool had_candidate = captured || placement_hover.has_value();
    placement_pressed = false;
    placement_hover.reset();
    if (captured && window.viewport().mouseGrabber() == &window.viewport())
      window.viewport().releaseMouse();
    if (!drag)
      apply->setEnabled(current() && previewed);
    if (placement_overlay)
      placement_overlay->present({}, std::nullopt);
    if (had_candidate && current())
      summary->setText(QStringLiteral(
          "Punktplatzierung verworfen. Frühere lokale Änderungen bleiben erhalten."));
  }
  Result<Point3> resolve_placement(GuiSketchScreenPoint pointer) {
    using namespace manipulator_detail;
    auto& viewport = window.viewport();
    if (!finite(pointer) || pointer.x < 0 || pointer.y < 0 || pointer.x >= viewport.width() ||
        pointer.y >= viewport.height())
      return Result<Point3>::failure(error("Punkt innerhalb des Modellfensters platzieren."));
    if (!placement_pressed) {
      placement_origin = {};
      const auto id = placement_anchor->currentData().toString();
      if (!id.isEmpty()) {
        const auto* anchor = draft->find_point(SketchEntityId(id.toStdString()));
        if (!anchor)
          return Result<Point3>::failure(error("Platzierungsanker neu auswählen."));
        placement_origin = resolved_point(*anchor);
      }
    }
    auto ray = viewport.screen_to_view_ray(pointer);
    if (!ray || !finite(ray.value().origin) || !finite(ray.value().direction) ||
        length(ray.value().direction) <= epsilon)
      return Result<Point3>::failure(error("Kein gültiger Sehstrahl für die Punktplatzierung."));
    const auto direction = normalized_or_zero(ray.value().direction);
    const auto delta = subtract(placement_origin, ray.value().origin);
    const auto view = normalized_or_zero(subtract(camera->target, camera->eye));
    const int restriction = lock->currentIndex();
    Point3 value;
    if (restriction >= 1 && restriction <= 3) {
      const Vector3 axis = restriction == 1   ? Vector3{1, 0, 0}
                           : restriction == 2 ? Vector3{0, 1, 0}
                                              : Vector3{0, 0, 1};
      const double b = dot(axis, direction), denominator = 1 - b * b;
      if (denominator < 1e-8)
        return Result<Point3>::failure(error("Achse liegt in Blickrichtung. Ansicht ändern."));
      const double distance = (b * dot(direction, delta) - dot(axis, delta)) / denominator;
      value = add(placement_origin, scale(axis, distance));
    } else {
      const Vector3 normal = restriction == 4   ? Vector3{0, 0, 1}
                             : restriction == 5 ? Vector3{1, 0, 0}
                             : restriction == 6 ? Vector3{0, 1, 0}
                                                : view;
      const double denominator = dot(direction, normal);
      if (length(normal) <= epsilon || std::abs(denominator) < 1e-8)
        return Result<Point3>::failure(
            error("Ebene liegt kantenparallel zur Ansicht. Ansicht ändern."));
      value = add(ray.value().origin, scale(direction, dot(delta, normal) / denominator));
    }
    auto resolved = Sketch3DInteractionService::resolve_point(
        {placement_origin.x, placement_origin.y, placement_origin.z}, {value.x, value.y, value.z},
        static_cast<Sketch3DLock>(restriction));
    if (!resolved)
      return Result<Point3>::failure(resolved.error());
    value = {resolved.value().x, resolved.value().y, resolved.value().z};
    if (!finite(value) || std::abs(value.x) > 1e9 || std::abs(value.y) > 1e9 ||
        std::abs(value.z) > 1e9 ||
        (camera->projection == GuiViewportProjection::Perspective &&
         dot(subtract(value, camera->eye), view) <= epsilon))
      return Result<Point3>::failure(
          error("Platzierung liegt außerhalb des sichtbaren XYZ-Bereichs."));
    return Result<Point3>::success(value);
  }
  void update_placement(GuiSketchScreenPoint pointer) {
    if (pending_point) {
      placement_hover.reset();
      summary->setText(QStringLiteral("Punktänderung zuerst lokal übernehmen oder neu auswählen."));
    } else {
      const auto value = resolve_placement(pointer);
      placement_hover = value ? std::optional(value.value()) : std::nullopt;
      summary->setText(
          value ? QStringLiteral(
                      "Neuer Punkt: X %1, Y %2, Z %3 mm. Linke Maustaste loslassen zum Übernehmen.")
                      .arg(value.value().x, 0, 'g', 12)
                      .arg(value.value().y, 0, 'g', 12)
                      .arg(value.value().z, 0, 'g', 12)
                : text(value.error().message()));
    }
    synchronize_overlay();
  }
  Result<std::size_t> stage_placement(Point3 value) {
    const auto id = next_entity(*draft, "point");
    auto candidate = Sketch3DInteractionService::add_point(*draft, id, {value.x, value.y, value.z});
    if (!candidate)
      return Result<std::size_t>::failure(candidate.error());
    draft = std::move(candidate.value().sketch);
    changed = true;
    fill_points(id);
    summary->setText(
        QStringLiteral("Punkt %1 lokal platziert. Linien ergänzen und Vorschau prüfen.")
            .arg(text(id.value())));
    return Result<std::size_t>::success(draft->entity_count());
  }
  bool pick_at(GuiSketchScreenPoint pointer) {
    const int role = pick->currentIndex();
    if (role == 0 || role > 4 || !current())
      return false;
    double best = 10.0;
    SketchEntityId selected;
    const SketchLine3D* selected_line = nullptr;
    if (role == 4) {
      for (const auto& item : draft->lines()) {
        auto first = window.viewport().model_to_screen(
            resolved_point(*draft->find_point(item.start_point())));
        auto last =
            window.viewport().model_to_screen(resolved_point(*draft->find_point(item.end_point())));
        if (!first || !last)
          continue;
        const double distance =
            manipulator_detail::point_segment_distance(pointer, first.value(), last.value());
        if (distance < best) {
          best = distance;
          selected_line = &item;
        }
      }
      if (selected_line) {
        start->setCurrentIndex(start->findData(text(selected_line->start_point().value())));
        end->setCurrentIndex(end->findData(text(selected_line->end_point().value())));
        summary->setText(QStringLiteral("Linie gewählt: ") + text(selected_line->id().value()));
      }
    } else {
      for (const auto& item : draft->points()) {
        auto screen = window.viewport().model_to_screen(resolved_point(item));
        if (!screen)
          continue;
        const double distance = manipulator_detail::screen_distance(pointer, screen.value());
        if (distance < best) {
          best = distance;
          selected = item.id();
        }
      }
      if (!selected.empty()) {
        auto* input = role == 1 ? point : role == 2 ? start : end;
        input->setCurrentIndex(input->findData(text(selected.value())));
        summary->setText(QStringLiteral("Punkt gewählt: ") + text(selected.value()));
      }
    }
    if (selected.empty() && !selected_line)
      summary->setText(QStringLiteral(
          "Kein passender Punkt bzw. keine Linie der aktiven 3D-Skizze an dieser Stelle."));
    return true;
  }
  bool pointer_event(QObject* watched, QEvent* event) {
    auto& viewport = window.viewport();
    if (watched == &window && event->type() == QEvent::WindowDeactivate) {
      cancel_drag();
      cancel_placement();
    }
    if (watched != &viewport || !current())
      return false;
    if (event->type() == QEvent::UngrabMouse && placement_pressed) {
      cancel_placement();
      return false;
    }
    if (event->type() == QEvent::Resize) {
      refresh_mapping();
      return false;
    }
    if (pick->currentIndex() == 5) {
      if (event->type() == QEvent::Leave && !placement_pressed) {
        cancel_placement();
        return false;
      }
      if (event->type() == QEvent::MouseMove) {
        const auto& mouse = *static_cast<QMouseEvent*>(event);
        if (!placement_pressed && mouse.buttons() != Qt::NoButton)
          return false;
        update_placement({mouse.position().x(), mouse.position().y()});
        return true;
      }
      if (event->type() == QEvent::MouseButtonDblClick)
        return static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
      if (event->type() == QEvent::MouseButtonPress) {
        const auto& mouse = *static_cast<QMouseEvent*>(event);
        if (mouse.button() != Qt::LeftButton) {
          cancel_placement();
          return false;
        }
        for (auto* spin : coordinates)
          spin->interpretText();
        refresh_mapping();
        update_placement({mouse.position().x(), mouse.position().y()});
        if (placement_hover) {
          placement_pressed = true;
          viewport.grabMouse();
          apply->setEnabled(false);
        }
        return true;
      }
      if (event->type() == QEvent::MouseButtonRelease) {
        const auto& mouse = *static_cast<QMouseEvent*>(event);
        if (mouse.button() != Qt::LeftButton)
          return false;
        if (placement_pressed) {
          auto value = resolve_placement({mouse.position().x(), mouse.position().y()});
          cancel_placement();
          auto result =
              value ? stage_placement(value.value()) : Result<std::size_t>::failure(value.error());
          if (!result)
            summary->setText(text(result.error().message()));
        }
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonPress) {
      const auto& mouse = *static_cast<QMouseEvent*>(event);
      if (mouse.button() != Qt::LeftButton)
        return false;
      refresh_mapping();
      const GuiSketchScreenPoint p{mouse.position().x(), mouse.position().y()};
      if (pick_at(p))
        return true;
      if (!layer.begin_drag(p))
        return false;
      const auto* selected = selected_point();
      if (!selected) {
        layer.cancel();
        return false;
      }
      drag = Drag{*draft, selected->id(), resolved_point(*selected), changed, previewed};
      viewport.grabMouse();
      numeric_invalid = false;
      numeric->setReadOnly(false);
      numeric->setText(text(layer.numeric_text()));
      numeric->setFocus();
      numeric->selectAll();
      apply->setEnabled(false);
      synchronize_overlay();
      return true;
    }
    if (event->type() == QEvent::MouseMove && drag) {
      const auto& mouse = *static_cast<QMouseEvent*>(event);
      if (!layer.update_drag({mouse.position().x(), mouse.position().y()}))
        cancel_drag();
      else if (!numeric_invalid) {
        accept_drag(*layer.candidate());
        if (!layer.numeric_override_active())
          numeric->setText(text(layer.numeric_text()));
      }
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
      const auto& mouse = *static_cast<QMouseEvent*>(event);
      if (mouse.button() != Qt::LeftButton)
        return false;
      if (drag) {
        if (numeric_invalid) {
          cancel_drag();
          return true;
        }
        auto candidate = layer.end_drag({mouse.position().x(), mouse.position().y()});
        if (!candidate) {
          cancel_drag();
          return true;
        }
        accept_drag(*candidate);
        drag.reset();
        viewport.releaseMouse();
        numeric->setReadOnly(true);
        apply->setEnabled(previewed);
        refresh_handles();
        return true;
      }
      if (pick->currentIndex() != 0)
        return true;
    }
    return false;
  }
  Result<std::size_t> commit(GuiDocumentSession& session) const {
    return session.commit_part_transaction(editing ? "Edit 3D sketch" : "Create 3D sketch",
                                           [sketch = *draft, update = editing](PartDocument& part) {
                                             return update ? part.update_sketch_3d(sketch)
                                                           : part.add_sketch_3d(sketch);
                                           });
  }
};

ShellSketch3DTools::ShellSketch3DTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window, *this)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("3D-Skizze"), this);
  ui.action->setObjectName("blcad.action.sketch_3d");
  ribbon.add_button(ribbon.add_group(model_tab, QStringLiteral("Räumliche Skizze")), ui.action);
  ui.dock = new QDockWidget(QStringLiteral("3D-Skizze: Punkte und Linien"), &window);
  ui.dock->setObjectName("blcad.shell.sketch3d_task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  auto* inputs = new QWidget(scroll);
  auto* column = new QVBoxLayout(inputs);
  ui.title = new QLabel(inputs);
  ui.title->setWordWrap(true);
  column->addWidget(ui.title);
  auto* hint = new QLabel(
      QStringLiteral("XYZ in Modellkoordinaten (mm). Punkte und Linien werden zunächst nur lokal "
                     "übernommen. Anwenden speichert die geprüfte 3D-Skizze in einem Schritt."),
      inputs);
  hint->setWordWrap(true);
  column->addWidget(hint);
  auto* form = new QFormLayout;
  const auto combo = [&](const char* id, const QString& label) {
    auto* result = new QComboBox(inputs);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    result->setMinimumContentsLength(10);
    form->addRow(label, result);
    return result;
  };
  ui.point = combo("point", QStringLiteral("Punkt bearbeiten"));
  for (int axis = 0; axis < 3; ++axis) {
    auto* spin = new QDoubleSpinBox(inputs);
    spin->setObjectName(QStringLiteral("blcad.sketch3d.") + "xyz"[axis]);
    spin->setDecimals(9);
    spin->setRange(-1e9, 1e9);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setKeyboardTracking(false);
    ui.coordinates[axis] = spin;
    form->addRow(QString("XYZ"[axis]), spin);
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, spin](double value) {
      if (!impl_->current())
        return;
      impl_->cancel_drag();
      impl_->cancel_placement();
      // Rollback restores the old coordinate fields. Keep the user's new
      // typed component as pending input, without retaining the drag's axes.
      {
        QSignalBlocker blocker(spin);
        spin->setValue(value);
      }
      impl_->pending_point = true;
      impl_->invalidate();
      impl_->refresh_handles();
      impl_->summary->setText(
          QStringLiteral("Punktänderung zuerst mit „Punkt übernehmen“ bestätigen."));
    });
  }
  ui.detach = new QCheckBox(QStringLiteral("Parameterbindungen durch XYZ-Werte ersetzen"), inputs);
  ui.detach->setObjectName("blcad.sketch3d.detach");
  form->addRow(ui.detach);
  const auto stage_button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, inputs);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    form->addRow(result);
    return result;
  };
  auto* stage_point = stage_button("stage_point", QStringLiteral("Punkt übernehmen"));
  ui.pick = combo("pick", QStringLiteral("Modellklick wählt"));
  ui.pick->addItems({QStringLiteral("Aus"), QStringLiteral("Punkt bearbeiten"),
                     QStringLiteral("Linienstart"), QStringLiteral("Linienende"),
                     QStringLiteral("Linie (beide Enden)"),
                     QStringLiteral("Neuen Punkt platzieren")});
  ui.placement_anchor = combo("placement_anchor", QStringLiteral("Platzierungsanker"));
  ui.grips = new QCheckBox(QStringLiteral("Punktgriffe anzeigen"), inputs);
  ui.grips->setObjectName("blcad.sketch3d.grips");
  form->addRow(ui.grips);
  ui.lock = combo("lock", QStringLiteral("Zeiger-/Griffsperre"));
  ui.lock->addItems({QStringLiteral("XYZ"), QStringLiteral("X-Achse"), QStringLiteral("Y-Achse"),
                     QStringLiteral("Z-Achse"), QStringLiteral("XY-Ebene"),
                     QStringLiteral("YZ-Ebene"), QStringLiteral("ZX-Ebene")});
  ui.numeric = new QLineEdit(inputs);
  ui.numeric->setObjectName("blcad.sketch3d.grip_numeric");
  ui.numeric->setReadOnly(true);
  ui.numeric->setPlaceholderText(QStringLiteral("Griff ziehen, dann XYZ-Koordinate in mm"));
  form->addRow(QStringLiteral("Griffwert (XYZ)"), ui.numeric);
  ui.start = combo("start", QStringLiteral("Linienstart"));
  ui.end = combo("end", QStringLiteral("Linienende"));
  auto* stage_line = stage_button("stage_line", QStringLiteral("Linie ergänzen"));
  column->addLayout(form);
  auto* placement_hint = new QLabel(
      QStringLiteral(
          "Punkte platzieren: XYZ nutzt die Bildebene durch den Anker; Achsen/Ebenen nutzen "
          "Modellrichtungen durch denselben Anker. Ankerkoordinaten werden kopiert. Escape beendet "
          "die Platzierung und erhält bereits lokal übernommene Punkte."),
      inputs);
  placement_hint->setWordWrap(true);
  column->addWidget(placement_hint);
  column->addStretch();
  scroll->setWidget(inputs);
  layout->addWidget(scroll, 1);
  ui.summary = new QLabel(panel);
  ui.summary->setObjectName("blcad.sketch3d.summary");
  ui.summary->setWordWrap(true);
  layout->addWidget(ui.summary);
  auto* buttons = new QHBoxLayout;
  const auto button = [&](const char* id, const QString& label) {
    auto* result = new QPushButton(label, panel);
    result->setObjectName(QStringLiteral("blcad.sketch3d.") + id);
    buttons->addWidget(result);
    return result;
  };
  auto* preview_button = button("preview", QStringLiteral("Vorschau"));
  ui.apply = button("apply", QStringLiteral("Anwenden"));
  auto* cancel_button = button("cancel", QStringLiteral("Abbrechen"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  ui.overlay = make_shell_manipulator_overlay(&window.viewport(), ui.layer);
  ui.overlay->setObjectName("blcad.sketch3d.manipulator_overlay");
  ui.placement_overlay = new PlacementOverlay(window.viewport());
  const auto report = [this](Result<std::size_t> result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(ui.action, &QAction::triggered, this, [this, report] { report(begin()); });
  connect(stage_point, &QPushButton::clicked, this,
          [this, report] { report(impl_->stage_point()); });
  connect(stage_line, &QPushButton::clicked, this, [this, report] { report(impl_->stage_line()); });
  connect(preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
  connect(ui.point, &QComboBox::currentIndexChanged, this, [this] { impl_->select_point(); });
  connect(ui.detach, &QCheckBox::toggled, this, [this] {
    impl_->cancel_drag();
    impl_->cancel_placement();
    impl_->invalidate();
    impl_->refresh_handles();
  });
  for (auto* combo : {ui.pick, ui.lock, ui.placement_anchor})
    connect(combo, &QComboBox::currentIndexChanged, this, [this] {
      impl_->cancel_drag();
      impl_->cancel_placement();
      impl_->refresh_handles();
    });
  connect(ui.grips, &QCheckBox::toggled, this, [this] {
    impl_->cancel_drag();
    impl_->cancel_placement();
    impl_->refresh_handles();
  });
  connect(ui.numeric, &QLineEdit::textEdited, this, [this] { impl_->edit_numeric(); });
  qApp->installEventFilter(this);
}
ShellSketch3DTools::~ShellSketch3DTools() {
  qApp->removeEventFilter(this);
  cancel();
  delete impl_->overlay;
  delete impl_->placement_overlay;
}
bool ShellSketch3DTools::active() const noexcept {
  return impl_->draft.has_value();
}
Result<std::size_t> ShellSketch3DTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("3D-Skizze benötigt ein aktuelles Bauteil ohne aktiven Befehl."));
  const auto& part = *ui.window.session().part_document();
  for (std::size_t index = 1;; ++index) {
    const std::string id = "sketch3d." + std::to_string(index);
    if (part.dependency_graph().has_node(id))
      continue;
    auto created = Sketch3D::create(Sketch3DId(id), "3D-Skizze " + std::to_string(index));
    if (!created)
      return Result<std::size_t>::failure(created.error());
    return open(std::move(created.value()), false);
  }
}
Result<std::size_t> ShellSketch3DTools::begin_edit(Sketch3DId id) {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(
        error("3D-Skizze benötigt ein aktuelles Bauteil ohne aktiven Befehl."));
  const auto* source = ui.window.session().part_document()->find_sketch_3d(id);
  if (!source)
    return Result<std::size_t>::failure(error("3D-Skizze nicht gefunden."));
  return open(*source, true);
}
Result<std::size_t> ShellSketch3DTools::open(Sketch3D sketch, bool editing) {
  auto& ui = *impl_;
  auto& session = ui.window.session();
  (void)session.task().begin(std::string(task_id));
  (void)session.task().begin_parameter_editing();
  ui.revision = session.presentation_revision();
  ui.draft = std::move(sketch);
  ui.editing = editing;
  ui.changed = false;
  ui.numeric_invalid = false;
  ui.numeric->clear();
  {
    QSignalBlocker pick_blocker(ui.pick), grip_blocker(ui.grips), lock_blocker(ui.lock),
        anchor_blocker(ui.placement_anchor);
    ui.pick->setCurrentIndex(0);
    ui.grips->setChecked(false);
    ui.lock->setCurrentIndex(0);
    ui.placement_anchor->setCurrentIndex(0);
  }
  ui.title->setText(text(ui.draft->name()) + " / " + text(ui.draft->id().value()));
  ui.fill_points(editing && !ui.draft->points().empty() ? ui.draft->points().front().id()
                                                        : SketchEntityId{});
  ui.dock->show();
  ui.dock->raise();
  ui.refresh_mapping();
  ui.window.refresh();
  return Result<std::size_t>::success(ui.draft->entity_count());
}
Result<std::size_t> ShellSketch3DTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(
        error("3D-Skizze neu öffnen: Modell oder Befehl geändert."));
  }
  for (auto* spin : ui.coordinates)
    spin->interpretText();
  if (ui.placement_pressed)
    return Result<std::size_t>::failure(error("Punktplatzierung zuerst beenden."));
  ui.invalidate();
  if (ui.numeric_invalid)
    return Result<std::size_t>::failure(
        error("Ungültigen Griffwert korrigieren oder Escape drücken."));
  if (!ui.changed || ui.pending_point || ui.draft->points().empty())
    return Result<std::size_t>::failure(error("Punkte/Linien zuerst lokal übernehmen."));
  // The complete session copy validates catalogs, reference connectivity and
  // downstream Geometry without publishing history, dirty state or file I/O.
  auto candidate = ui.window.session();
  auto checked = ui.commit(candidate);
  if (!checked)
    return checked;
  auto scene = geometry::ViewportSceneBuilder{}.build_part(*candidate.part_document(),
                                                           *candidate.part_shape_cache());
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  std::vector<geometry::GeometryShape> shapes;
  std::vector<std::string> replaced;
  const auto prefix = "sketch3d/" + ui.draft->id().value() + "/";
  for (auto& item : scene.value())
    if (item.semantic_id.starts_with(prefix) || item.kind == geometry::ViewportSceneKind::Path ||
        item.kind == geometry::ViewportSceneKind::SolidBody ||
        item.kind == geometry::ViewportSceneKind::SurfaceBody) {
      replaced.push_back(item.semantic_id);
      shapes.push_back(std::move(item.shape));
    }
  auto shown = ui.window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
  if (!shown)
    return shown;
  ui.previewed = true;
  ui.apply->setEnabled(!ui.drag);
  (void)ui.window.session().task().show_preview();
  ui.summary->setText(QStringLiteral("Vorschau gültig: %1 Punkte, %2 Linien. Abhängige Pfade und "
                                     "Körper neu berechnet. Anwenden übernimmt einen Schritt.")
                          .arg(ui.draft->points().size())
                          .arg(ui.draft->lines().size()));
  return Result<std::size_t>::success(ui.draft->entity_count());
}
Result<std::size_t> ShellSketch3DTools::apply() {
  auto& ui = *impl_;
  if (!ui.current() || !ui.previewed) {
    if (!ui.current())
      cancel();
    return Result<std::size_t>::failure(error("Zuerst eine aktuelle Vorschau prüfen."));
  }
  if (ui.drag || ui.placement_pressed)
    return Result<std::size_t>::failure(error("Griffbewegung zuerst beenden."));
  auto checked = preview();
  if (!checked)
    return checked;
  auto result = ui.commit(ui.window.session());
  cancel();
  return result;
}
void ShellSketch3DTools::cancel() {
  auto& ui = *impl_;
  if (!ui.draft)
    return;
  ui.cancel_drag(false);
  ui.cancel_placement();
  ui.draft.reset();
  ui.layer.clear_handles();
  ui.numeric_invalid = false;
  ui.synchronize_overlay();
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.dock->hide();
  ui.window.refresh();
}
void ShellSketch3DTools::refresh() {
  auto& ui = *impl_;
  if (ui.draft && !ui.current()) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
void ShellSketch3DTools::refresh_presentation() {
  if (impl_->current())
    impl_->refresh_mapping();
}
const GuiViewportManipulatorLayer& ShellSketch3DTools::manipulators() const noexcept {
  return impl_->layer;
}
bool ShellSketch3DTools::eventFilter(QObject* watched, QEvent* event) {
  if (impl_->pointer_event(watched, event))
    return true;
  auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && event->type() == QEvent::KeyPress && widget &&
      (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    if (impl_->drag)
      impl_->cancel_drag();
    else if (impl_->pick->currentIndex() == 5) {
      impl_->cancel_placement();
      impl_->pick->setCurrentIndex(0);
      impl_->summary->setText(
          QStringLiteral("Punktplatzierung beendet. Lokale Änderungen bleiben erhalten."));
    } else
      cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
