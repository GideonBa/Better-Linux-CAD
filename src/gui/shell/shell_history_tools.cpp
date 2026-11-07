#include "blcad/gui/shell/shell_history_tools.hpp"

#include "blcad/gui/gui_feature_edit.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_part_history.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDataStream>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace blcad::gui {
namespace {
constexpr auto mime_type = "application/x-blcad-part-history-v1";
Error error(std::string message) {
  return Error::validation("gui.shell.history", std::move(message));
}
QString text(const std::string& value) {
  return QString::fromStdString(value);
}
QTreeWidgetItem* browser_item(QTreeWidgetItem* item, const QString& id, const QString& owner) {
  if (item->data(0, Qt::UserRole).toString() == id &&
      item->data(0, ShellHistoryBrowser::owner_role).toString() == owner)
    return item;
  for (int i = 0; i < item->childCount(); ++i)
    if (auto* found = browser_item(item->child(i), id, owner))
      return found;
  return nullptr;
}
class HistoryOrderList final : public QListWidget {
public:
  explicit HistoryOrderList(QWidget* parent) : QListWidget(parent) {}
  bool dragging = false;

protected:
  void startDrag(Qt::DropActions actions) override {
    dragging = true;
    QListWidget::startDrag(actions);
    dragging = false;
  }
};
} // namespace

ShellHistoryBrowser::ShellHistoryBrowser(QWidget* parent)
    : QTreeWidget(parent), nonce_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
  setDragEnabled(true);
  setAcceptDrops(true);
  viewport()->setAcceptDrops(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::MoveAction);
  setDropIndicatorShown(false);
}
void ShellHistoryBrowser::set_history_callbacks(
    std::function<Result<std::size_t>(const ShellHistoryMove&)> check,
    std::function<void(ShellHistoryMove)> propose, std::function<std::size_t()> revision) {
  check_ = std::move(check);
  propose_ = std::move(propose);
  revision_ = std::move(revision);
}
QStringList ShellHistoryBrowser::mimeTypes() const {
  return {QString::fromLatin1(mime_type)};
}
Qt::DropActions ShellHistoryBrowser::supportedDropActions() const {
  return Qt::MoveAction;
}
QMimeData* ShellHistoryBrowser::mimeData(const QList<QTreeWidgetItem*>& items) const {
  if (!revision_ || items.size() != 1 || !items.front()->data(0, feature_role).toBool())
    return nullptr;
  QByteArray bytes;
  QDataStream output(&bytes, QIODevice::WriteOnly);
  output << nonce_ << quint64(revision_()) << items.front()->data(0, Qt::UserRole).toString()
         << items.front()->data(0, owner_role).toString();
  auto* mime = new QMimeData;
  mime->setData(QString::fromLatin1(mime_type), bytes);
  return mime;
}
std::optional<ShellHistoryMove> ShellHistoryBrowser::decode(const QMimeData* mime,
                                                            QPoint point) const {
  if (!mime || !revision_ || !mime->hasFormat(QString::fromLatin1(mime_type)))
    return std::nullopt;
  QDataStream input(mime->data(QString::fromLatin1(mime_type)));
  QString nonce, id, owner;
  quint64 revision{};
  input >> nonce >> revision >> id >> owner;
  if (input.status() != QDataStream::Ok || nonce != nonce_ || revision != revision_())
    return std::nullopt;
  QTreeWidgetItem* source = nullptr;
  for (int i = 0; i < topLevelItemCount() && !source; ++i)
    source = browser_item(topLevelItem(i), id, owner);
  auto* target = itemAt(point);
  if (!source || !source->data(0, feature_role).toBool() || !source->parent() || !target)
    return std::nullopt;
  ShellHistoryMove move{id.toStdString(), owner.toStdString(), std::nullopt, false,
                        static_cast<std::size_t>(revision)};
  if (target == source->parent())
    return move;
  if (target->parent() != source->parent() || !target->data(0, feature_role).toBool())
    return std::nullopt;
  move.target = target->data(0, Qt::UserRole).toString().toStdString();
  move.after = point.y() >= visualItemRect(target).center().y();
  return move;
}
void ShellHistoryBrowser::dragEnterEvent(QDragEnterEvent* event) {
  const auto move = decode(event->mimeData(), event->position().toPoint());
  if (move && check_)
    event->acceptProposedAction();
  else
    event->ignore();
}
void ShellHistoryBrowser::dragMoveEvent(QDragMoveEvent* event) {
  drop_line_.reset();
  const auto move = decode(event->mimeData(), event->position().toPoint());
  if (move && check_ && check_(*move)) {
    auto* target = itemAt(event->position().toPoint());
    const auto rect = visualItemRect(target);
    drop_line_ = move->after || !move->target ? rect.bottom() : rect.top();
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else
    event->ignore();
  viewport()->update();
}
void ShellHistoryBrowser::dragLeaveEvent(QDragLeaveEvent* event) {
  drop_line_.reset();
  viewport()->update();
  event->accept();
}
void ShellHistoryBrowser::dropEvent(QDropEvent* event) {
  drop_line_.reset();
  viewport()->update();
  const auto move = decode(event->mimeData(), event->position().toPoint());
  if (!move || !check_ || !propose_ || !check_(*move)) {
    event->ignore();
    return;
  }
  event->setDropAction(Qt::MoveAction);
  event->accept();
  // Opening a task can refresh the browser. Return from Qt's drag handling first.
  const auto proposal = *move;
  QMetaObject::invokeMethod(
      this,
      [this, proposal] {
        if (revision_ && revision_() == proposal.revision && propose_)
          propose_(proposal);
      },
      Qt::QueuedConnection);
}
void ShellHistoryBrowser::paintEvent(QPaintEvent* event) {
  QTreeWidget::paintEvent(event);
  if (drop_line_) {
    QPainter painter(viewport());
    painter.setPen(QPen(palette().highlight().color(), 2));
    painter.drawLine(4, *drop_line_, viewport()->width() - 4, *drop_line_);
  }
}

void ShellHistoryBrowser::resizeEvent(QResizeEvent* event) {
  QTreeWidget::resizeEvent(event);
  auto* item = currentItem();
  if (!item)
    return;
  for (auto* parent = item->parent(); parent; parent = parent->parent())
    if (!parent->isExpanded())
      return;
  scrollToItem(item, QAbstractItemView::EnsureVisible);
}

void ShellHistoryBrowser::mousePressEvent(QMouseEvent* event) {
  pressed_current_item_ = event->button() == Qt::LeftButton && currentItem() &&
                          itemAt(event->position().toPoint()) == currentItem();
  QTreeWidget::mousePressEvent(event);
}

struct ShellHistoryTools::Impl {
  ShellWindow& window;
  GuiPartHistoryController edit;
  PartFeatureHistoryIntent initial;
  QAction *suppress{}, *restore{}, *reorder{}, *edit_feature{};
  QDockWidget* dock{};
  QLabel *breadcrumb{}, *status{};
  ShellPropertyGroup *order_group{}, *effects_group{};
  HistoryOrderList* order{};
  QListWidget* affected{};
  QCheckBox* closure{};
  QPushButton *up{}, *down{}, *apply{};
  bool active = false, reordering = false, suppressing = false, loading = false;
  std::string feature;
  std::optional<GuiViewportCameraBookmark> camera;
  std::optional<std::string> selected;
  std::uint32_t filter{};
  std::optional<std::vector<std::string>> scope;
  QString browser_id, browser_owner;
  std::size_t preview_revision = 0;

  explicit Impl(ShellWindow& window_) : window(window_) {}
  ShellHistoryBrowser& browser() {
    return static_cast<ShellHistoryBrowser&>(window.model_browser());
  }
  std::string selected_feature() const {
    const auto* item = window.model_browser().currentItem();
    if (!item)
      return {};
    const auto* node =
        window.browser_model().find(item->data(0, Qt::UserRole).toString().toStdString());
    const auto* part = window.session().part_document();
    return node && part && node->kind == GuiBrowserNodeKind::Feature &&
                   node->owner_document_id == part->id().value()
               ? node->semantic_id
               : std::string{};
  }
  std::string unavailable_reason() const {
    const auto& session = window.session();
    if (active || window.active_sketch() || session.task().active() ||
        session.part_workflow_active())
      return "Apply or cancel the active command before editing history.";
    if (!session.part_document())
      return "Open an editable Part to use feature history.";
    if (session.part_document()->is_reference_part())
      return "Reference Parts are immutable; create an Editable copy to author features.";
    if ((!session.derived_results_fresh() && !session.reference_repair_mode()) ||
        (session.derived_results_fresh() &&
         (!session.part_shape_cache() ||
          !session.part_shape_cache()->validate_complete_history_result(*session.part_document()))))
      return "Recompute the current Part before editing history.";
    return {};
  }
  bool available() const {
    return unavailable_reason().empty();
  }
  std::string label(const std::string& id) const {
    const auto* node = window.browser_model().find(id);
    if (node)
      for (const auto& property : node->properties)
        if (property.key == "name")
          return property.value;
    return id;
  }
  void fill_order() {
    const auto previous =
        order->currentItem() ? order->currentItem()->data(Qt::UserRole).toString() : QString{};
    loading = true;
    order->clear();
    for (const auto& id : edit.intent().order) {
      const bool suppressed =
          std::find(edit.intent().suppressed.begin(), edit.intent().suppressed.end(), id) !=
          edit.intent().suppressed.end();
      auto* item = new QListWidgetItem(
          text(label(id) + " (" + id + ")" + (suppressed ? " [suppressed]" : "")), order);
      item->setData(Qt::UserRole, text(id));
      item->setToolTip(text(label(id)));
      if (text(id) == previous || (previous.isEmpty() && id == feature))
        order->setCurrentItem(item);
    }
    if (!order->currentItem() && order->count())
      order->setCurrentRow(0);
    loading = false;
    order->setFixedHeight(
        std::clamp(order->count() * (order->fontMetrics().height() + 6) + 8, 80, 180));
    up->setEnabled(reordering && order->currentRow() > 0);
    down->setEnabled(reordering && order->currentRow() >= 0 &&
                     order->currentRow() + 1 < order->count());
  }
  Result<std::size_t> preview() {
    preview_revision = window.session().presentation_revision();
    apply->setEnabled(false);
    window.viewport().clear_feature_preview();
    (void)window.session().task().return_to_editing();
    const auto result = edit.preview(window.session());
    if (!result) {
      const auto message = text(result.error().object_id() + ": " + result.error().message());
      status->setText(message);
      order_group->set_diagnostic(reordering ? message : QString{});
      effects_group->set_diagnostic(!reordering ? message : QString{});
      return Result<std::size_t>::failure(result.error());
    }
    std::vector<geometry::GeometryShape> shapes;
    std::vector<std::string> replaced;
    for (const auto& body : window.session().part_document()->bodies())
      replaced.push_back("body/" + body.id().value());
    for (const auto& body : result.value().document.bodies()) {
      if (body.visibility() != BodyVisibility::Visible ||
          !window.session().presentation_visible("body/" + body.id().value()))
        continue;
      if (const auto* shape = result.value().shapes.find_body_shape(body.id()))
        shapes.push_back(*shape);
    }
    if (window.session().part_document()->bodies().empty()) {
      if (const auto* original = window.session().part_shape_cache();
          original && original->has_final_shape())
        replaced.push_back("feature/" + original->final_feature_id().value());
      if (const auto* shape = result.value().shapes.final_shape())
        shapes.push_back(*shape);
    }
    const auto shown =
        window.viewport().set_feature_previews(std::move(shapes), std::move(replaced));
    if (!shown)
      return shown;
    status->setText(
        "Complete history preview valid. Apply commits one change; Cancel restores the model.");
    window.statusBar()->showMessage("History preview valid. Apply or Cancel.");
    order_group->set_diagnostic({});
    effects_group->set_diagnostic({});
    apply->setEnabled(edit.intent() != initial);
    if (edit.intent() == initial)
      status->setText("No history change is staged. Move a feature to a different position.");
    (void)window.session().task().show_preview();
    return Result<std::size_t>::success(edit.intent().order.size());
  }
  void suppression_changed() {
    if (loading || !active || reordering)
      return;
    const auto staged = edit.stage_suppression(window.session(), feature, suppressing, false);
    affected->clear();
    if (staged) {
      const auto blocked = edit.blocked_dependents(window.session());
      closure->setEnabled(blocked && !blocked.value().empty());
      if (blocked)
        for (const auto& id : blocked.value())
          affected->addItem(text(label(id) + " (" + id + ")"));
    }
    affected->setFixedHeight(
        std::clamp(affected->count() * (affected->fontMetrics().height() + 6) + 8, 36, 100));
    affected->setVisible(affected->count() > 0);
    closure->setToolTip(closure->isEnabled() ? "Explicitly include the listed dependent operations."
                                             : "No dependent feature requires suppression.");
    closure->setText(closure->isChecked() ? "Dependents included" : "Also suppress dependents");
    if (closure->isChecked()) {
      const auto included = edit.stage_suppression(window.session(), feature, suppressing, true);
      if (!included)
        status->setText(text(included.error().message()));
    }
    fill_order();
    (void)preview();
  }
  void order_changed() {
    if (loading || !active || !reordering)
      return;
    std::vector<std::string> ids;
    for (int i = 0; i < order->count(); ++i)
      ids.push_back(order->item(i)->data(Qt::UserRole).toString().toStdString());
    (void)edit.stage_order(window.session(), std::move(ids));
    up->setEnabled(order->currentRow() > 0);
    down->setEnabled(order->currentRow() + 1 < order->count());
    (void)preview();
  }
  void move_row(int delta) {
    const int from = order->currentRow(), to = from + delta;
    if (!reordering || from < 0 || to < 0 || to >= order->count())
      return;
    loading = true;
    auto* item = order->takeItem(from);
    order->insertItem(to, item);
    order->setCurrentItem(item);
    loading = false;
    order_changed();
  }
  Result<std::vector<std::string>> move_order(const ShellHistoryMove& move) const {
    const auto& session = window.session();
    const auto* part = session.part_document();
    if ((!available() && !(active && reordering && edit.current(session))) || !part ||
        part->id().value() != move.owner || session.presentation_revision() != move.revision)
      return Result<std::vector<std::string>>::failure(
          error("History source changed or another command is active."));
    const auto history = part->executable_history();
    if (!history)
      return Result<std::vector<std::string>>::failure(history.error());
    std::vector<std::string> ids;
    if (active)
      ids = edit.intent().order;
    else
      for (const auto& entry : history.value())
        ids.push_back(entry.node_id);
    const auto original = ids;
    const auto source = std::find(ids.begin(), ids.end(), move.feature);
    if (source == ids.end() || (move.target && *move.target == move.feature))
      return Result<std::vector<std::string>>::failure(
          error("Choose a different history position."));
    ids.erase(source);
    auto target = move.target ? std::find(ids.begin(), ids.end(), *move.target) : ids.end();
    if (move.target && target == ids.end())
      return Result<std::vector<std::string>>::failure(
          error("The drop target is no longer available."));
    if (move.target && move.after)
      ++target;
    ids.insert(target, move.feature);
    if (ids == original)
      return Result<std::vector<std::string>>::failure(
          error("Choose a different history position."));
    std::vector<std::string> suppressed;
    if (active)
      suppressed = edit.intent().suppressed;
    else
      for (const auto& entry : history.value())
        if (entry.state == PartFeatureState::Suppressed)
          suppressed.push_back(entry.node_id);
    const auto valid = part->with_history_intent({ids, suppressed});
    if (!valid)
      return Result<std::vector<std::string>>::failure(valid.error());
    return Result<std::vector<std::string>>::success(std::move(ids));
  }
  Result<std::size_t> begin(bool reorder_mode, std::string id, bool suppress_value) {
    if (!available())
      return Result<std::size_t>::failure(
          error("Finish the active command before editing history."));
    const auto started = edit.begin(window.session());
    if (!started)
      return started;
    initial = edit.intent();
    auto& task = window.session().task();
    if (!task.begin(std::string(GuiPartHistoryController::task_id)) ||
        !task.begin_parameter_editing()) {
      edit.cancel();
      return Result<std::size_t>::failure(error("Could not reserve the history task."));
    }
    const QSize window_size = window.size();
    active = true;
    reordering = reorder_mode;
    feature = std::move(id);
    suppressing = suppress_value;
    auto& viewport = window.viewport();
    const auto* browser_selection = browser().currentItem();
    browser_id =
        browser_selection ? browser_selection->data(0, Qt::UserRole).toString() : QString{};
    browser_owner = browser_selection
                        ? browser_selection->data(0, ShellHistoryBrowser::owner_role).toString()
                        : QString{};
    camera = viewport.camera_bookmark();
    filter = viewport.selection_filter_mask();
    scope = viewport.selection_semantic_scope();
    selected = viewport.selected_semantic()
                   ? std::optional(viewport.selected_semantic()->semantic_id)
                   : std::nullopt;
    viewport.clear_selection();
    viewport.set_selection_filter_mask(0);
    viewport.set_selection_semantic_scope(std::vector<std::string>{});
    if (!browser_id.isEmpty())
      for (int i = 0; i < browser().topLevelItemCount(); ++i)
        if (auto* retained = browser_item(browser().topLevelItem(i), browser_id, browser_owner)) {
          browser().setCurrentItem(retained);
          browser().scrollToItem(retained);
          break;
        }
    loading = true;
    closure->setChecked(false);
    order->clear();
    closure->setVisible(!reordering && suppressing);
    affected->clear();
    order->setDragDropMode(reordering ? QAbstractItemView::InternalMove
                                      : QAbstractItemView::NoDragDrop);
    up->setVisible(reordering);
    down->setVisible(reordering);
    dock->setWindowTitle(reordering    ? "Reorder Features"
                         : suppressing ? "Suppress Feature"
                                       : "Unsuppress Feature");
    breadcrumb->setText(text(std::string(window.session().display_name()) + " / History" +
                             (feature.empty() ? "" : " / " + label(feature))));
    order_group->setVisible(reordering);
    effects_group->setVisible(!reordering);
    loading = false;
    fill_order();
    dock->show();
    dock->raise();
    window.resizeDocks({dock}, {320}, Qt::Horizontal);
    window.resize(window_size);
    if (reordering)
      (void)preview();
    else
      suppression_changed();
    window.refresh();
    return started;
  }
  void finish(bool applied, bool refresh_window = true) {
    const bool restore_snapshot = applied || edit.current(window.session());
    active = false;
    edit.cancel();
    window.viewport().clear_feature_preview();
    window.viewport().set_selection_filter_mask(filter);
    window.viewport().set_selection_semantic_scope(scope);
    if (applied)
      (void)window.session().task().apply();
    else
      (void)window.session().task().cancel();
    dock->hide();
    if (refresh_window)
      window.refresh();
    if (restore_snapshot && camera)
      (void)window.viewport().restore_camera_bookmark(*camera);
    if (restore_snapshot && !browser_id.isEmpty())
      for (int i = 0; i < browser().topLevelItemCount(); ++i)
        if (auto* retained = browser_item(browser().topLevelItem(i), browser_id, browser_owner)) {
          browser().setCurrentItem(retained);
          browser().scrollToItem(retained);
          break;
        }
    if (restore_snapshot && selected)
      (void)window.viewport().select_semantic(*selected);
  }
};

ShellHistoryTools::ShellHistoryTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& p = *impl_;
  auto* group = ribbon.add_group(model_tab, "History");
  const auto action = [&](const char* id, const char* label) {
    auto* result = new QAction(QString::fromUtf8(label), this);
    result->setObjectName(QStringLiteral("blcad.action.history_") + id);
    apply_gui_action_icon(*result);
    ribbon.add_small_button(group, result);
    return result;
  };
  p.suppress = action("suppress", "Suppress Feature");
  p.restore = action("restore", "Unsuppress Feature");
  p.reorder = action("reorder", "Reorder Features");
  p.edit_feature = action("edit", "Edit Feature");
  const auto invoke = [this](Result<std::size_t> result) {
    if (!result)
      impl_->window.report_error(result.error());
  };
  connect(p.suppress, &QAction::triggered, this,
          [this, invoke] { invoke(begin_suppression(impl_->selected_feature(), true)); });
  connect(p.restore, &QAction::triggered, this,
          [this, invoke] { invoke(begin_suppression(impl_->selected_feature(), false)); });
  connect(p.reorder, &QAction::triggered, this, [this, invoke] { invoke(begin_reorder()); });
  connect(p.edit_feature, &QAction::triggered, this, [this, invoke] {
    invoke(impl_->window.feature_edit_tools().begin(FeatureId(impl_->selected_feature())));
  });
  p.dock = new QDockWidget("History", &window);
  p.dock->setObjectName("blcad.shell.history_task");
  p.dock->setFeatures(QDockWidget::DockWidgetMovable);
  auto* panel = new QWidget(p.dock);
  auto* layout = new QVBoxLayout(panel);
  p.breadcrumb = new QLabel(panel);
  p.breadcrumb->setTextFormat(Qt::PlainText);
  p.breadcrumb->setObjectName("blcad.history.breadcrumb");
  p.breadcrumb->setWordWrap(true);
  layout->addWidget(p.breadcrumb);
  p.status = new QLabel(panel);
  p.status->setTextFormat(Qt::PlainText);
  p.status->setObjectName("blcad.history.status");
  p.status->setWordWrap(true);
  p.status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(p.status);
  auto* scroll = new QScrollArea(panel);
  scroll->setObjectName("blcad.history.inputs_scroll");
  scroll->setWidgetResizable(true);
  scroll->setMinimumWidth(300);
  auto* content = new QWidget(scroll);
  auto* column = new QVBoxLayout(content);
  p.order_group = new ShellPropertyGroup("history.order", "Feature Order", content);
  p.order = new HistoryOrderList(&p.order_group->content());
  p.order->setObjectName("blcad.history.order");
  p.order->setMinimumHeight(130);
  p.order->setDefaultDropAction(Qt::MoveAction);
  p.order_group->content_layout().addWidget(p.order);
  auto* move_buttons = new QHBoxLayout;
  p.up = make_gui_button("Move Up", &p.order_group->content());
  p.up->setObjectName("blcad.history.up");
  p.down = make_gui_button("Move Down", &p.order_group->content());
  p.down->setObjectName("blcad.history.down");
  move_buttons->addWidget(p.up);
  move_buttons->addWidget(p.down);
  p.order_group->content_layout().addLayout(move_buttons);
  column->addWidget(p.order_group);
  p.effects_group = new ShellPropertyGroup("history.effects", "Dependent Features", content);
  p.affected = new QListWidget(&p.effects_group->content());
  p.affected->setObjectName("blcad.history.dependents");
  p.affected->setMaximumHeight(140);
  p.closure = new QCheckBox("Also suppress dependents", &p.effects_group->content());
  p.closure->setObjectName("blcad.history.include_dependents");
  p.effects_group->content_layout().addWidget(p.closure);
  p.effects_group->content_layout().addWidget(p.affected);
  auto* hint = new QLabel("Unsuppress changes only the selected feature. Visibility remains a "
                          "separate presentation setting.",
                          &p.effects_group->content());
  hint->setWordWrap(true);
  p.effects_group->content_layout().addWidget(hint);
  column->addWidget(p.effects_group);
  column->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll, 1);
  auto* footer = new QHBoxLayout;
  p.apply = make_gui_button("Apply", panel);
  p.apply->setObjectName("blcad.history.apply");
  p.apply->setProperty("guiRole", "primary");
  auto* cancel_button = make_gui_button("Cancel", panel);
  cancel_button->setObjectName("blcad.history.cancel");
  footer->addWidget(p.apply);
  footer->addWidget(cancel_button);
  layout->addLayout(footer);
  p.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, p.dock);
  p.dock->hide();
  connect(p.closure, &QCheckBox::toggled, this, [this] { impl_->suppression_changed(); });
  connect(p.up, &QPushButton::clicked, this, [this] { impl_->move_row(-1); });
  connect(p.down, &QPushButton::clicked, this, [this] { impl_->move_row(1); });
  connect(p.order, &QListWidget::currentRowChanged, this, [this] {
    if (!impl_->loading) {
      impl_->up->setEnabled(impl_->reordering && impl_->order->currentRow() > 0);
      impl_->down->setEnabled(impl_->reordering &&
                              impl_->order->currentRow() + 1 < impl_->order->count());
    }
  });
  connect(p.order->model(), &QAbstractItemModel::rowsMoved, this,
          [this] { impl_->order_changed(); });
  connect(p.apply, &QPushButton::clicked, this, [this, invoke] { invoke(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
  connect(&window.model_browser(), &QTreeWidget::currentItemChanged, this, [this] { refresh(); });
  p.browser().set_history_callbacks(
      [this](const ShellHistoryMove& move) -> Result<std::size_t> {
        const auto order = impl_->move_order(move);
        if (!order) {
          impl_->window.statusBar()->showMessage(
              text(order.error().object_id() + ": " + order.error().message()));
          return Result<std::size_t>::failure(order.error());
        }
        return Result<std::size_t>::success(order.value().size());
      },
      [this, invoke](ShellHistoryMove move) {
        const auto order = impl_->move_order(move);
        if (!order) {
          invoke(Result<std::size_t>::failure(order.error()));
          return;
        }
        if (!active()) {
          const auto begun = begin_reorder();
          if (!begun) {
            invoke(begun);
            return;
          }
        }
        (void)impl_->edit.stage_order(impl_->window.session(), order.value());
        impl_->fill_order();
        (void)impl_->preview();
      },
      [&window] { return window.session().presentation_revision(); });
  qApp->installEventFilter(this);
  refresh();
}
ShellHistoryTools::~ShellHistoryTools() {
  impl_->browser().set_history_callbacks({}, {}, {});
}
QAction& ShellHistoryTools::suppress_action() noexcept {
  return *impl_->suppress;
}
QAction& ShellHistoryTools::restore_action() noexcept {
  return *impl_->restore;
}
QAction& ShellHistoryTools::reorder_action() noexcept {
  return *impl_->reorder;
}
QAction& ShellHistoryTools::edit_action() noexcept {
  return *impl_->edit_feature;
}
bool ShellHistoryTools::active() const noexcept {
  return impl_->active;
}
Result<std::size_t> ShellHistoryTools::begin_suppression(std::string feature, bool suppressed) {
  const auto* part = impl_->window.session().part_document();
  const auto history =
      part ? part->executable_history()
           : Result<std::vector<PartFeatureHistoryEntry>>::failure(error("No active Part."));
  if (!history || std::none_of(history.value().begin(), history.value().end(),
                               [&](const auto& row) { return row.node_id == feature; }))
    return Result<std::size_t>::failure(error("Select a feature in the active Part browser."));
  if (part->find_imported_body_feature(FeatureId(feature)))
    return Result<std::size_t>::failure(
        error("Imported Body bases are required and cannot be suppressed."));
  return impl_->begin(false, std::move(feature), suppressed);
}
Result<std::size_t> ShellHistoryTools::begin_reorder() {
  return impl_->begin(true, {}, false);
}
Result<std::size_t> ShellHistoryTools::apply() {
  auto& p = *impl_;
  if (!active() || !p.apply->isEnabled())
    return Result<std::size_t>::failure(error("The history candidate is incomplete."));
  const auto label = p.reordering
                         ? "Reorder features"
                         : (p.suppressing ? "Suppress " : "Unsuppress ") + p.label(p.feature) +
                               (p.closure->isChecked() ? " and dependents" : "");
  const auto committed = p.edit.apply(p.window.session(), label);
  if (committed)
    p.finish(true);
  else {
    p.apply->setEnabled(false);
    p.window.viewport().clear_feature_preview();
    p.status->setText(text(committed.error().object_id() + ": " + committed.error().message()));
  }
  return committed;
}
void ShellHistoryTools::cancel() {
  if (active())
    impl_->finish(false);
}
void ShellHistoryTools::refresh() {
  auto& p = *impl_;
  if (active() && !p.edit.current(p.window.session()))
    p.finish(false, false);
  if (active() && p.preview_revision != p.window.session().presentation_revision())
    (void)p.preview();
  const auto selected = p.selected_feature();
  bool suppressed = false, imported = false;
  if (const auto* part = p.window.session().part_document()) {
    imported = part->find_imported_body_feature(FeatureId(selected)) != nullptr;
    const auto history = part->executable_history();
    if (history)
      for (const auto& row : history.value())
        if (row.node_id == selected)
          suppressed = row.state == PartFeatureState::Suppressed;
  }
  const bool available = p.available();
  p.suppress->setEnabled(available && !selected.empty() && !suppressed && !imported);
  p.restore->setEnabled(available && !selected.empty() && suppressed && !imported);
  const auto reason = p.unavailable_reason();
  p.suppress->setToolTip(!reason.empty()    ? text(reason)
                         : selected.empty() ? "Select a feature in the active Part browser."
                         : imported   ? "Imported Body bases are required and cannot be suppressed."
                         : suppressed ? "The selected feature is already suppressed."
                                      : "Preview suppression of the selected feature.");
  p.restore->setToolTip(
      !reason.empty()    ? text(reason)
      : selected.empty() ? "Select a suppressed feature in the active Part browser."
      : !suppressed      ? "The selected feature is already active."
                    : "Unsuppress only the selected feature; dependent flags remain unchanged.");
  const auto* part = p.window.session().part_document();
  const auto history =
      part ? part->executable_history()
           : Result<std::vector<PartFeatureHistoryEntry>>::failure(error("No active Part."));
  p.reorder->setEnabled(available && history && history.value().size() > 1);
  p.reorder->setToolTip(
      !reason.empty() ? text(reason)
      : !history || history.value().size() < 2
          ? "At least two features are needed to reorder history."
          : "Preview a dependency-checked feature order using drag/drop or Move Up/Down.");
  GuiFeatureEditController editor;
  const auto editable = available && !selected.empty()
                            ? editor.begin(p.window.session(), FeatureId(selected))
                            : Result<std::size_t>::failure(error(
                                  "Select an editable feature without another active command."));
  p.edit_feature->setEnabled(bool(editable));
  p.edit_feature->setToolTip(part && part->find_body_transform(BodyTransformId(selected))
                                 ? "Body Transform editing is not supported by this property task."
                             : editable
                                 ? "Edit the persistent feature using its existing property task."
                                 : text(editable.error().message()));
}
bool ShellHistoryTools::eventFilter(QObject* watched, QEvent* event) {
  auto& p = *impl_;
  if (!active() || event->type() != QEvent::KeyPress)
    return QObject::eventFilter(watched, event);
  auto* widget = qobject_cast<QWidget*>(watched);
  if (!widget || (widget != &p.window && widget != p.dock && widget != &p.window.viewport() &&
                  !p.dock->isAncestorOf(widget) && !p.window.viewport().isAncestorOf(widget)))
    return QObject::eventFilter(watched, event);
  auto* key = static_cast<QKeyEvent*>(event);
  if (p.order->dragging)
    return QObject::eventFilter(watched, event);
  if (key->key() == Qt::Key_Escape) {
    cancel();
    event->accept();
    return true;
  }
  if ((key->modifiers() & Qt::ControlModifier) &&
      (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
    const auto committed = apply();
    if (!committed)
      p.window.report_error(committed.error());
    event->accept();
    return true;
  }
  if ((key->modifiers() & Qt::ControlModifier) &&
      (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) && p.reordering) {
    p.move_row(key->key() == Qt::Key_Up ? -1 : 1);
    event->accept();
    return true;
  }
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
