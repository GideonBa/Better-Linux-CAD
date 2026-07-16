#include "blcad/gui/gui_sketch_interaction_binder.hpp"
#include "blcad/gui/gui_sketch_create_binder.hpp"
#include "blcad/gui/gui_sketch_drag_binder.hpp"

#include "blcad/gui/main_window.hpp"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPaintEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace blcad::gui {
namespace {

constexpr int kSemanticIdRole = Qt::UserRole;
constexpr int kViewportSemanticIdRole = Qt::UserRole + 4;

bool sketch_idle_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::Idle ||
         stage == GuiSketchInteractionStage::Hover;
}

QTreeWidgetItem* find_tree_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, kSemanticIdRole).toString() == id ||
      item->data(0, kViewportSemanticIdRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_tree_item(item->child(index), id))
      return found;
  return nullptr;
}

QTreeWidgetItem* find_tree_item(QTreeWidget* tree, std::string_view semantic_id) {
  if (tree == nullptr || semantic_id.empty())
    return nullptr;
  const QString id = QString::fromUtf8(semantic_id.data(),
                                       static_cast<qsizetype>(semantic_id.size()));
  for (int index = 0; index < tree->topLevelItemCount(); ++index)
    if (auto* found = find_tree_item(tree->topLevelItem(index), id))
      return found;
  return nullptr;
}

QMenu* sketch_menu(MainWindow& window) {
  for (QAction* action : window.menuBar()->actions()) {
    QMenu* menu = action->menu();
    if (menu == nullptr)
      continue;
    QString title = menu->title();
    title.remove(QLatin1Char('&'));
    if (title == QStringLiteral("Sketch"))
      return menu;
  }
  return nullptr;
}

class SketchInferenceOverlay final : public QWidget {
public:
  explicit SketchInferenceOverlay(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("blcad.sketch.inference_overlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
  }

  void set_guide(std::optional<Qt::Orientation> orientation,
                 std::optional<double> coordinate) {
    orientation_ = std::move(orientation);
    coordinate_ = coordinate;
    setVisible(orientation_.has_value() && coordinate_.has_value());
    update();
  }

protected:
  void paintEvent(QPaintEvent* event) override {
    Q_UNUSED(event)
    if (!orientation_ || !coordinate_)
      return;
    QPainter painter(this);
    QPen pen(QColor(96, 220, 170, 190), 1.2, Qt::DashLine);
    painter.setPen(pen);
    if (*orientation_ == Qt::Horizontal)
      painter.drawLine(QPointF(0.0, *coordinate_), QPointF(width(), *coordinate_));
    else
      painter.drawLine(QPointF(*coordinate_, 0.0), QPointF(*coordinate_, height()));
  }

private:
  std::optional<Qt::Orientation> orientation_;
  std::optional<double> coordinate_;
};

class SketchInteractionBinder final : public QObject {
public:
  explicit SketchInteractionBinder(MainWindow& window)
      : QObject(&window), window_(window),
        viewport_(window.findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"))),
        tree_(window.findChild<QTreeWidget*>(QStringLiteral("blcad.model_browser"))),
        diagnostics_(window.findChild<QTextEdit*>(QStringLiteral("blcad.diagnostics"))),
        cursor_status_(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.cursor_status"))),
        snap_status_(window.findChild<QLabel*>(QStringLiteral("blcad.sketch.snap_status"))),
        numeric_hud_(window.findChild<QLineEdit*>(QStringLiteral("blcad.sketch.numeric_hud"))) {
    setObjectName(QStringLiteral("blcad.sketch.interaction_binder"));
    if (viewport_ != nullptr) {
      inference_overlay_ = new SketchInferenceOverlay(viewport_);
      inference_overlay_->setGeometry(viewport_->rect());
      viewport_->installEventFilter(this);
    }
    create_grid_actions();
    bind_viewport();
    bind_shell_actions();
    window_.installEventFilter(this);
    sync_workspace();
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == &window_ && event->type() == QEvent::KeyPress) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape)
        defer_selection_state();
    } else if (watched == viewport_ && event->type() == QEvent::Resize &&
               inference_overlay_ != nullptr) {
      inference_overlay_->setGeometry(viewport_->rect());
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void create_grid_actions() {
    QMenu* menu = sketch_menu(window_);
    if (menu == nullptr)
      return;
    menu->addSeparator();
    grid_action_ = menu->addAction(QStringLiteral("Show grid"));
    grid_action_->setObjectName(QStringLiteral("blcad.action.sketch_grid"));
    grid_action_->setCheckable(true);
    grid_action_->setChecked(true);
    grid_snap_action_ = menu->addAction(QStringLiteral("Snap to grid"));
    grid_snap_action_->setObjectName(QStringLiteral("blcad.action.sketch_grid_snap"));
    grid_snap_action_->setCheckable(true);
    grid_snap_action_->setChecked(true);
    connect(grid_action_, &QAction::toggled, this,
            [this] { apply_grid_config(); });
    connect(grid_snap_action_, &QAction::toggled, this,
            [this] { apply_grid_config(); });
  }

  void bind_viewport() {
    if (viewport_ == nullptr)
      return;
    viewport_->set_sketch_pointer_callback(
        [this](Point2 raw_point, const GuiSketchSnapResult& snap,
               const std::optional<GuiSketchHit>& hit) {
          if (!window_.sketch_workspace().active())
            return;
          window_.sketch_workspace().set_cursor_coordinates(raw_point);
          window_.sketch_workspace().set_snap_inference(snap.inference);
          if (sketch_idle_stage(window_.sketch_workspace().stage()))
            (void)window_.sketch_workspace().set_hover(hit.has_value());
          update_inference_preview(snap);
          refresh_status_surfaces();
        });
    viewport_->set_sketch_selection_callback(
        [this](const std::vector<GuiSelection>& selections) {
          synchronize_selection(selections);
        });
  }

  void bind_shell_actions() {
    const auto defer_sync_for = [this](QStringView object_name) {
      if (QAction* action = window_.findChild<QAction*>(object_name.toString()))
        connect(action, &QAction::triggered, this,
                [this] { QTimer::singleShot(0, this, [this] { sync_workspace(); }); });
    };
    const auto defer_state_for = [this](QStringView object_name) {
      if (QAction* action = window_.findChild<QAction*>(object_name.toString()))
        connect(action, &QAction::triggered, this,
                [this] { defer_selection_state(); });
    };

    defer_sync_for(u"blcad.action.edit_sketch");
    defer_sync_for(u"blcad.action.finish_sketch");
    defer_sync_for(u"blcad.action.repair_sketch");
    defer_sync_for(u"blcad.action.recompute");
    defer_state_for(u"blcad.action.sketch_line");
    defer_state_for(u"blcad.action.sketch_repeat");

    if (numeric_hud_ != nullptr)
      connect(numeric_hud_, &QLineEdit::returnPressed, this,
              [this] { QTimer::singleShot(0, this, [this] { sync_workspace(); }); });
  }

  void defer_selection_state() {
    QTimer::singleShot(0, this, [this] {
      update_selection_state();
      refresh_status_surfaces();
    });
  }

  GuiSketchGridConfig grid_config() const {
    GuiSketchGridConfig config;
    config.visible = grid_action_ == nullptr || grid_action_->isChecked();
    config.snap_enabled = grid_snap_action_ == nullptr || grid_snap_action_->isChecked();
    config.spacing = 10.0;
    config.major_every = 5U;
    return config;
  }

  void apply_grid_config() {
    if (viewport_ != nullptr && viewport_->sketch_interaction_active())
      viewport_->set_sketch_grid_config(grid_config());
  }

  void update_inference_preview(const GuiSketchSnapResult& snap) {
    if (inference_overlay_ == nullptr || viewport_ == nullptr) return;

    std::optional<Qt::Orientation> orientation;
    switch (snap.kind) {
    case GuiSketchSnapKind::HorizontalInference:
    case GuiSketchSnapKind::AlignmentY:
      orientation = Qt::Horizontal;
      break;
    case GuiSketchSnapKind::VerticalInference:
    case GuiSketchSnapKind::AlignmentX:
      orientation = Qt::Vertical;
      break;
    default:
      inference_overlay_->set_guide(std::nullopt, std::nullopt);
      return;
    }

    const auto screen = viewport_->sketch_plane_to_screen(snap.snapped_point);
    if (screen.has_error()) {
      inference_overlay_->set_guide(std::nullopt, std::nullopt);
      return;
    }
    const double coordinate = *orientation == Qt::Horizontal ? screen.value().y : screen.value().x;
    inference_overlay_->set_guide(orientation, coordinate);
    inference_overlay_->raise();
  }

  void append_diagnostic(const Error& error) const {
    if (diagnostics_ != nullptr)
      diagnostics_->append(
          QStringLiteral("[%1] %2: %3")
              .arg(QString::fromStdString(std::string(to_string(error.category()))),
                   QString::fromStdString(error.object_id()),
                   QString::fromStdString(error.message())));
  }

  void sync_workspace() {
    const bool active = window_.sketch_workspace().active() &&
                        window_.active_sketch().has_value();
    if (grid_action_ != nullptr) {
      grid_action_->setVisible(active);
      grid_action_->setEnabled(active);
    }
    if (grid_snap_action_ != nullptr) {
      grid_snap_action_->setVisible(active);
      grid_snap_action_->setEnabled(active);
    }
    if (!active) {
      if (viewport_ != nullptr)
        viewport_->clear_sketch_interaction();
      if (inference_overlay_ != nullptr)
        inference_overlay_->set_guide(std::nullopt, std::nullopt);
      refresh_status_surfaces();
      return;
    }

    const PartDocument* part = window_.session().part_document();
    if (viewport_ == nullptr || part == nullptr)
      return;
    const Sketch* sketch = part->find_sketch(*window_.active_sketch());
    if (sketch == nullptr)
      return;

    const auto plane = window_.sketch_workbench().plane_view(
        window_.session(), *window_.active_sketch());
    if (plane.has_error()) {
      append_diagnostic(plane.error());
      return;
    }
    const auto scene = window_.session().part_shape_cache() != nullptr
                           ? GuiSketchInteractionSceneBuilder{}.build(
                                 *part, *sketch, *window_.session().part_shape_cache())
                           : GuiSketchInteractionSceneBuilder{}.build(*part, *sketch);
    if (scene.has_error()) {
      append_diagnostic(scene.error());
      return;
    }

    GuiSketchInteractionConfig config;
    config.grid = grid_config();
    const auto activated = viewport_->set_sketch_interaction(
        plane.value(), scene.value(), config);
    if (activated.has_error()) {
      append_diagnostic(activated.error());
      return;
    }
    if (inference_overlay_ != nullptr) {
      inference_overlay_->setGeometry(viewport_->rect());
      inference_overlay_->raise();
    }
    update_selection_state();
    refresh_status_surfaces();
  }

  void update_selection_state() {
    if (viewport_ == nullptr)
      return;
    const bool selectable = window_.sketch_workspace().active() &&
                            !window_.session().task().active() &&
                            sketch_idle_stage(window_.sketch_workspace().stage());
    viewport_->set_sketch_selection_enabled(selectable);
  }

  void synchronize_selection(const std::vector<GuiSelection>& selections) {
    if (synchronizing_selection_ || !window_.sketch_workspace().active())
      return;
    synchronizing_selection_ = true;

    if (tree_ != nullptr) {
      const QSignalBlocker blocker(tree_);
      QTreeWidgetItem* item = selections.size() == 1U
                                  ? find_tree_item(tree_, selections.front().semantic_id)
                                  : nullptr;
      tree_->setCurrentItem(item);
    }

    window_.session().selection().clear();
    for (const auto& selection : selections)
      (void)window_.session().selection().add(selection);
    synchronizing_selection_ = false;
  }

  void refresh_status_surfaces() const {
    const bool active = window_.sketch_workspace().active();
    if (!active)
      return;
    const auto& status = window_.sketch_workspace().status();
    if (cursor_status_ != nullptr) {
      if (status.cursor_coordinates)
        cursor_status_->setText(
            QStringLiteral("Cursor: %1, %2 mm")
                .arg(status.cursor_coordinates->x, 0, 'f', 2)
                .arg(status.cursor_coordinates->y, 0, 'f', 2));
      else
        cursor_status_->setText(QStringLiteral("Cursor: —"));
    }
    if (snap_status_ != nullptr)
      snap_status_->setText(
          status.snap_inference.empty()
              ? QStringLiteral("Snap: —")
              : QStringLiteral("Snap: %1")
                    .arg(QString::fromStdString(status.snap_inference)));
    window_.statusBar()->showMessage(
        QString::fromStdString(status.tool_hint));
  }

  MainWindow& window_;
  OcctViewport* viewport_{nullptr};
  QTreeWidget* tree_{nullptr};
  QTextEdit* diagnostics_{nullptr};
  QLabel* cursor_status_{nullptr};
  QLabel* snap_status_{nullptr};
  QLineEdit* numeric_hud_{nullptr};
  QAction* grid_action_{nullptr};
  QAction* grid_snap_action_{nullptr};
  SketchInferenceOverlay* inference_overlay_{nullptr};
  bool synchronizing_selection_{false};
};

} // namespace

void install_sketch_interaction_binder(MainWindow& window) {
  if (window.findChild<QObject*>(QStringLiteral("blcad.sketch.interaction_binder")) != nullptr)
    return;
  (void)new SketchInteractionBinder(window);
  install_sketch_create_binder(window);
  install_sketch_drag_binder(window);
}

} // namespace blcad::gui
