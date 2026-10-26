#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>

namespace blcad::gui {
namespace {
using K = geometry::MeasureSelectionKind;
constexpr std::string_view task_id = "inspect.measure";
QString number(double value) {
  return QString::number(value, 'g', 12);
}
Result<std::size_t> fail(std::string text) {
  return Result<std::size_t>::failure(Error::validation("gui.shell.measure", std::move(text)));
}
std::string owner_label(const std::vector<GuiBrowserNode>& nodes, const std::string& identity) {
  for (const auto& node : nodes) {
    if (node.viewport_semantic_id == identity)
      return node.label;
    auto nested = owner_label(node.children, identity);
    if (!nested.empty())
      return nested;
  }
  return {};
}
geometry::ViewportSceneKind scene_kind(K kind) {
  return kind == K::Point                       ? geometry::ViewportSceneKind::Vertex
         : kind == K::Line || kind == K::Circle ? geometry::ViewportSceneKind::Edge
                                                : geometry::ViewportSceneKind::Face;
}
class MeasureOverlay final : public QWidget {
public:
  explicit MeasureOverlay(OcctViewport& viewport) : QWidget(&viewport), viewport_(viewport) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
  }
  std::vector<std::pair<QString, Point3>> markers;
  std::vector<Point3> witnesses;
  QString value;

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(255, 195, 55), 2));
    std::vector<QPointF> projected;
    for (const auto& point : witnesses) {
      auto screen = viewport_.model_to_screen(point);
      if (!screen)
        return;
      projected.emplace_back(screen.value().x, screen.value().y);
    }
    if (projected.size() == 2) {
      painter.drawLine(projected[0], projected[1]);
      for (auto point : projected)
        painter.drawEllipse(point, 4, 4);
      const auto middle = (projected[0] + projected[1]) / 2;
      const QRectF label(middle + QPointF(8, -26), QSizeF(210, 24));
      painter.fillRect(label, Qt::black);
      painter.setPen(Qt::white);
      painter.drawRect(label);
      painter.drawText(label, Qt::AlignLeft | Qt::AlignVCenter, value);
    }
    for (const auto& [label, point] : markers) {
      auto screen = viewport_.model_to_screen(point);
      if (!screen)
        continue;
      const QPointF p(screen.value().x, screen.value().y);
      painter.drawEllipse(p, 6, 6);
      painter.setPen(Qt::white);
      painter.drawText(p + QPointF(8, 16), label);
    }
  }

private:
  OcctViewport& viewport_;
};
} // namespace
ShellMeasureTools::ShellMeasureTools(ShellWindow& window, GuiDocumentSession& session,
                                     OcctViewport& viewport, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), window_(window), session_(session), viewport_(viewport) {
  auto* group = ribbon.add_group(model_tab, QStringLiteral("Inspect"));
  for (const auto& [kind, label, id] :
       {std::tuple{GuiMeasureKind::Distance, "Distance", "measure_distance"},
        std::tuple{GuiMeasureKind::Angle, "Angle", "measure_angle"},
        std::tuple{GuiMeasureKind::RadiusDiameter, "Radius / Diameter", "measure_radius"},
        std::tuple{GuiMeasureKind::BodySummary, "Body Summary", "measure_body"}}) {
    auto* action = new QAction(QString::fromUtf8(label), this);
    action->setObjectName(QStringLiteral("blcad.action.") + id);
    connect(action, &QAction::triggered, this, [this, kind] {
      auto begun = begin(kind);
      if (!begun)
        window_.report_error(begun.error());
    });
    ribbon.add_small_button(group, action);
    actions_.emplace_back(kind, action);
  }
  dock_ = new QDockWidget(QStringLiteral("Measure"), &window);
  dock_->setObjectName("blcad.shell.measure_task");
  dock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
  auto* panel = new QWidget(dock_);
  panel->setMinimumWidth(380);
  auto* column = new QVBoxLayout(panel);
  form_ = new QFormLayout;
  auto* form = form_;
  kind_ = new QComboBox(panel);
  kind_->setObjectName("blcad.measure.kind");
  kind_->addItems({QStringLiteral("Distance"), QStringLiteral("Angle"),
                   QStringLiteral("Radius / Diameter"), QStringLiteral("Body Summary")});
  form->addRow(QStringLiteral("Measurement"), kind_);
  pick_ = new QComboBox(panel);
  pick_->setObjectName("blcad.measure.pick_role");
  pick_->addItems({QStringLiteral("Element A"), QStringLiteral("Element B")});
  form->addRow(QStringLiteral("Viewport Click Selects"), pick_);
  filter_ = new QComboBox(panel);
  filter_->setObjectName("blcad.measure.filter");
  filter_->addItems({QStringLiteral("All Compatible Elements"), QStringLiteral("Points"),
                     QStringLiteral("Edges"), QStringLiteral("Faces")});
  form->addRow(QStringLiteral("Selection Filter"), filter_);
  first_ = new QComboBox(panel);
  second_ = new QComboBox(panel);
  first_->setObjectName("blcad.measure.first");
  second_->setObjectName("blcad.measure.second");
  form->addRow(QStringLiteral("Element A"), first_);
  form->addRow(QStringLiteral("Element B"), second_);
  column->addLayout(form);
  hint_ = new QLabel(panel);
  hint_->setWordWrap(true);
  column->addWidget(hint_);
  readout_ = new QLabel(panel);
  readout_->setObjectName("blcad.measure.readout");
  readout_->setWordWrap(true);
  readout_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  column->addWidget(readout_);
  column->addStretch();
  auto* buttons = new QHBoxLayout;
  auto* clear = new QPushButton(QStringLiteral("Select Again"), panel);
  clear->setObjectName("blcad.measure.clear");
  copy_ = new QPushButton(QStringLiteral("Copy"), panel);
  copy_->setObjectName("blcad.measure.copy");
  auto* close = new QPushButton(QStringLiteral("Close"), panel);
  close->setObjectName("blcad.measure.close");
  for (auto* button : {clear, copy_, close})
    buttons->addWidget(button);
  column->addLayout(buttons);
  dock_->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, dock_);
  dock_->hide();
  dock_->installEventFilter(this);
  viewport.installEventFilter(this);
  window.installEventFilter(this);
  overlay_ = new MeasureOverlay(viewport);
  overlay_->setObjectName("blcad.measure.overlay");
  connect(close, &QPushButton::clicked, this, [this] { cancel(); });
  connect(clear, &QPushButton::clicked, this, [this] { clear_inputs(); });
  connect(copy_, &QPushButton::clicked, this, [this] {
    if (active() && result_ && session_.derived_results_fresh() &&
        revision_ == session_.model_revision())
      QApplication::clipboard()->setText(readout_->text());
  });
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
    if (!active())
      return;
    (void)controller_.begin(session_, static_cast<GuiMeasureKind>(kind_->currentIndex()));
    populate_inputs();
  });
  for (auto* input : {first_, second_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (active())
        update_result();
    });
  for (auto* input : {pick_, filter_})
    connect(input, &QComboBox::currentIndexChanged, this, [this] {
      if (active())
        update_pick_scope();
    });
}
ShellMeasureTools::~ShellMeasureTools() {
  delete overlay_;
}

Result<std::size_t> ShellMeasureTools::begin(GuiMeasureKind kind) {
  if (active() || session_.task().active() || window_.active_sketch() || !session_.has_document() ||
      !session_.derived_results_fresh())
    return fail("Measure requires a document with current geometry without an active command");
  auto built = rebuild_catalogue();
  if (!built)
    return built;
  if (std::none_of(targets_.begin(), targets_.end(), [&](const auto& target) {
        switch (kind) {
        case GuiMeasureKind::Distance:
          return target.kind == K::Point || target.kind == K::Line || target.kind == K::Plane;
        case GuiMeasureKind::Angle:
          return target.kind == K::Line || target.kind == K::Plane;
        case GuiMeasureKind::RadiusDiameter:
          return target.kind == K::Circle || target.kind == K::Cylinder;
        case GuiMeasureKind::BodySummary:
          return target.body.has_value();
        }
        return false;
      }))
    return fail("No compatible measurable elements available");
  if (!session_.task().begin(std::string(task_id)) || !session_.task().begin_parameter_editing() ||
      !session_.task().show_preview())
    return fail("Could not start measure task");
  auto begun = controller_.begin(session_, kind);
  if (!begun) {
    (void)session_.task().cancel();
    return begun;
  }
  previous_mask_ = viewport_.selection_filter_mask();
  previous_scope_ = viewport_.selection_semantic_scope();
  {
    const QSignalBlocker block(kind_);
    kind_->setCurrentIndex(static_cast<int>(kind));
  }
  {
    const QSignalBlocker block(filter_);
    filter_->setCurrentIndex(0);
  }
  populate_inputs();
  dock_->show();
  dock_->raise();
  window_.refresh_feature_selection_scene();
  window_.refresh();
  return begun;
}
Result<std::size_t> ShellMeasureTools::rebuild_catalogue() {
  targets_.clear();
  revision_ = session_.model_revision();
  fresh_ = session_.derived_results_fresh();
  if (!fresh_)
    return fail("Geometry is stale. Recompute and select again.");
  if (session_.project()) {
    auto catalogue =
        geometry::MeasureSelectionBuilder::build_project(*session_.project(), ++generation_);
    if (!catalogue)
      return Result<std::size_t>::failure(catalogue.error());
    targets_ = std::move(catalogue.value());
    return Result<std::size_t>::success(targets_.size());
  }
  geometry::ViewportSceneBuilder builder;
  auto scene = session_.part_document() && session_.part_shape_cache()
                   ? builder.build_part(*session_.part_document(), *session_.part_shape_cache())
               : session_.project() ? builder.build_project(*session_.project())
                                    : Result<std::vector<geometry::ViewportSceneItem>>::success({});
  if (!scene)
    return Result<std::size_t>::failure(scene.error());
  std::vector<geometry::MeasureSelectionOwner> owners;
  for (const auto& item : scene.value()) {
    if (item.kind != geometry::ViewportSceneKind::SolidBody &&
        item.kind != geometry::ViewportSceneKind::SurfaceBody &&
        item.kind != geometry::ViewportSceneKind::Component)
      continue;
    std::optional<BodyId> body;
    std::string label = owner_label(window_.browser_model().roots(), item.semantic_id);
    if (label.empty())
      label = item.semantic_id;
    if (session_.part_document() && item.semantic_id.starts_with("body/")) {
      body = BodyId(item.semantic_id.substr(5));
      if (const auto* record = session_.part_document()->find_body(*body))
        label = record->name();
    }
    owners.push_back({item.semantic_id, label, item.shape, body});
  }
  auto catalogue = geometry::MeasureSelectionBuilder::build(owners, ++generation_);
  if (!catalogue)
    return Result<std::size_t>::failure(catalogue.error());
  targets_ = std::move(catalogue.value());
  return Result<std::size_t>::success(targets_.size());
}
bool ShellMeasureTools::accepts(K kind) const {
  switch (controller_.kind()) {
  case GuiMeasureKind::Distance:
    return kind == K::Point || kind == K::Line || kind == K::Plane;
  case GuiMeasureKind::Angle:
    return kind == K::Line || kind == K::Plane;
  case GuiMeasureKind::RadiusDiameter:
    return kind == K::Circle || kind == K::Cylinder;
  case GuiMeasureKind::BodySummary:
    return kind == K::Body;
  }
  return false;
}
void ShellMeasureTools::populate_inputs() {
  {
    const QSignalBlocker first(first_), second(second_), pick(pick_);
    first_->clear();
    second_->clear();
    first_->addItem(QStringLiteral("Select"), -1);
    second_->addItem(QStringLiteral("Select"), -1);
    for (std::size_t i = 0; i < targets_.size(); ++i)
      if (accepts(targets_[i].kind)) {
        first_->addItem(QString::fromStdString(targets_[i].label), static_cast<int>(i));
        second_->addItem(QString::fromStdString(targets_[i].label), static_cast<int>(i));
      }
    pick_->setCurrentIndex(0);
  }
  const bool pair =
      controller_.kind() == GuiMeasureKind::Distance || controller_.kind() == GuiMeasureKind::Angle;
  auto* form = form_;
  form->setRowVisible(second_, pair);
  form->setRowVisible(pick_, pair);
  form->setRowVisible(filter_, controller_.kind() != GuiMeasureKind::BodySummary);
  hint_->setText(
      controller_.kind() == GuiMeasureKind::Distance
          ? QStringLiteral(
                "Select two points, straight edges, or planar faces. Measurement uses the "
                "underlying lines or planes, including beyond the bounded elements.")
      : controller_.kind() == GuiMeasureKind::Angle
          ? QStringLiteral("Select two straight edges or planar faces. The smaller angle "
                           "from 0° to 90° is displayed.")
      : controller_.kind() == GuiMeasureKind::RadiusDiameter
          ? QStringLiteral("Select a circular edge or cylindrical face.")
          : QStringLiteral("Select a part body. Measurement does not change the model."));
  update_pick_scope();
  update_result();
}
const geometry::MeasureSelectionTarget* ShellMeasureTools::selected(QComboBox* combo) const {
  const int index = combo->currentData().toInt();
  return index >= 0 && static_cast<std::size_t>(index) < targets_.size() ? &targets_[index]
                                                                         : nullptr;
}
void ShellMeasureTools::update_pick_scope() {
  std::vector<std::string> scope;
  for (const auto& target : targets_) {
    if (!accepts(target.kind))
      continue;
    const int filter = filter_->currentIndex();
    const bool allowed = controller_.kind() == GuiMeasureKind::BodySummary || filter == 0 ||
                         (filter == 1 && target.kind == K::Point) ||
                         (filter == 2 && (target.kind == K::Line || target.kind == K::Circle)) ||
                         (filter == 3 && (target.kind == K::Plane || target.kind == K::Cylinder));
    if (allowed)
      scope.push_back(target.semantic_id);
  }
  viewport_.set_selection_filter_mask(
      selection_kind_bit(GuiSelectionKind::Vertex) | selection_kind_bit(GuiSelectionKind::Edge) |
      selection_kind_bit(GuiSelectionKind::Face) | selection_kind_bit(GuiSelectionKind::Body));
  viewport_.set_selection_semantic_scope(std::move(scope));
}
void ShellMeasureTools::update_result() {
  window_.statusBar()->clearMessage();
  result_.reset();
  copy_->setEnabled(false);
  readout_->setText(QStringLiteral("Select measurement elements."));
  const auto kind = static_cast<GuiMeasureKind>(kind_->currentIndex());
  (void)controller_.begin(session_, kind);
  if (!session_.derived_results_fresh() || revision_ != session_.model_revision()) {
    readout_->setText(QStringLiteral("Geometry changed or is stale. Select again."));
    refresh_presentation();
    return;
  }
  const auto* first = selected(first_);
  const auto* second = selected(second_);
  const bool pair = kind == GuiMeasureKind::Distance || kind == GuiMeasureKind::Angle;
  if (!first || (pair && !second)) {
    refresh_presentation();
    return;
  }
  if (pair && first == second) {
    readout_->setText(QStringLiteral("Select two distinct elements."));
    refresh_presentation();
    return;
  }
  if (kind == GuiMeasureKind::BodySummary)
    (void)controller_.select_body(*first->body);
  else {
    (void)controller_.add_target(*first->descriptor);
    if (pair)
      (void)controller_.add_target(*second->descriptor);
  }
  auto measured = controller_.preview(session_);
  if (!measured)
    readout_->setText(QString::fromStdString(measured.error().message()));
  else {
    result_ = std::move(measured.value());
    readout_->setText(
        kind == GuiMeasureKind::Distance
            ? QStringLiteral("Distance: %1 mm").arg(number(result_->primary_value))
        : kind == GuiMeasureKind::Angle
            ? QStringLiteral("Angle: %1°").arg(number(result_->primary_value))
        : kind == GuiMeasureKind::RadiusDiameter
            ? QStringLiteral("Radius: %1 mm\nDiameter: %2 mm")
                  .arg(number(result_->primary_value), number(*result_->secondary_value))
            : QStringLiteral("Volume: %1 mm³\nSolids: %2")
                  .arg(number(result_->volume_mm3))
                  .arg(result_->solid_count));
    copy_->setEnabled(true);
    window_.statusBar()->showMessage(readout_->text());
  }
  refresh_presentation();
}
void ShellMeasureTools::clear_inputs() {
  const QSignalBlocker first(first_), second(second_), pick(pick_);
  first_->setCurrentIndex(0);
  second_->setCurrentIndex(0);
  pick_->setCurrentIndex(0);
  update_result();
}
bool ShellMeasureTools::accept_selection(const GuiSelection& selection) {
  if (!active() || !session_.derived_results_fresh() || revision_ != session_.model_revision())
    return false;
  auto* input = pick_->currentIndex() == 1 ? second_ : first_;
  for (int row = 1; row < input->count(); ++row) {
    const int index = input->itemData(row).toInt();
    if (targets_[index].semantic_id != selection.semantic_id)
      continue;
    input->setCurrentIndex(row);
    if (input == first_ && (controller_.kind() == GuiMeasureKind::Distance ||
                            controller_.kind() == GuiMeasureKind::Angle))
      pick_->setCurrentIndex(1);
    return true;
  }
  return false;
}
std::vector<geometry::ViewportSceneItem> ShellMeasureTools::selection_items() const {
  std::vector<geometry::ViewportSceneItem> items;
  if (active())
    for (const auto& target : targets_)
      if (target.descriptor)
        items.push_back({scene_kind(target.kind), target.semantic_id, target.shape,
                         SketchLineRole::Geometry,
                         target.body ? target.body->value() : std::string{}});
  return items;
}
void ShellMeasureTools::refresh_presentation() {
  auto* overlay = static_cast<MeasureOverlay*>(overlay_);
  overlay->markers.clear();
  overlay->witnesses.clear();
  overlay->value.clear();
  const bool fresh =
      active() && session_.derived_results_fresh() && revision_ == session_.model_revision();
  if (fresh) {
    for (auto* input : {first_, second_})
      if (const auto* target = selected(input); target && target->descriptor)
        overlay->markers.emplace_back(input == first_ ? QStringLiteral("A") : QStringLiteral("B"),
                                      target->anchor);
    if (result_) {
      overlay->witnesses = result_->overlay_points;
      overlay->value = readout_->text();
    }
  }
  overlay->setVisible(fresh && (!overlay->markers.empty() || !overlay->witnesses.empty()));
  overlay->setGeometry(viewport_.rect());
  overlay->raise();
  overlay->update();
}
void ShellMeasureTools::refresh() {
  const bool available = !active() && !session_.task().active() && !window_.active_sketch() &&
                         session_.derived_results_fresh();
  const bool part_shapes =
      session_.part_shape_cache() && (session_.part_shape_cache()->body_shape_count() ||
                                      session_.part_shape_cache()->has_final_shape());
  for (const auto& [kind, action] : actions_)
    action->setEnabled(available &&
                       (kind == GuiMeasureKind::BodySummary
                            ? part_shapes && session_.part_shape_cache()->body_shape_count() > 0
                            : part_shapes || session_.project()));
  if (!active())
    return;
  if (!session_.has_document() || session_.task().command_id() != task_id ||
      window_.active_sketch()) {
    finish(false);
    return;
  }
  if (revision_ != session_.model_revision() || fresh_ != session_.derived_results_fresh()) {
    auto rebuilt = rebuild_catalogue();
    populate_inputs();
    if (!rebuilt)
      readout_->setText(QString::fromStdString(rebuilt.error().message()));
    else
      readout_->setText(QStringLiteral("Model changed. Select measurement elements again."));
    window_.refresh_feature_selection_scene();
  }
  refresh_presentation();
}
void ShellMeasureTools::finish(bool refresh_window) {
  if (!active())
    return;
  controller_.cancel();
  window_.statusBar()->clearMessage();
  result_.reset();
  targets_.clear();
  if (session_.task().command_id() == task_id)
    (void)session_.task().cancel();
  viewport_.set_selection_semantic_scope(previous_scope_);
  viewport_.set_selection_filter_mask(previous_mask_);
  dock_->hide();
  overlay_->hide();
  copy_->setEnabled(false);
  readout_->clear();
  window_.refresh_feature_selection_scene();
  if (refresh_window)
    window_.refresh();
}
void ShellMeasureTools::cancel() {
  finish(true);
}
bool ShellMeasureTools::eventFilter(QObject* watched, QEvent* event) {
  if (watched == dock_ && event->type() == QEvent::Close)
    cancel();
  if (event->type() == QEvent::KeyPress && active() &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (watched == &viewport_ && event->type() == QEvent::Resize)
    refresh_presentation();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
