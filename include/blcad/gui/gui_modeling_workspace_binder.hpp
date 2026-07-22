#pragma once

#include "blcad/gui/gui_modeling_workspace.hpp"

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>
#include <QPointer>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace blcad::gui {

class GuiModelingWorkspaceShellBinder final : public QObject {
public:
  GuiModelingWorkspaceShellBinder(QMainWindow* window, GuiDocumentSession* session,
                                  GuiModelingWorkspace* workspace,
                                  std::function<void()> finish_sketch_handoff,
                                  std::function<void()> refresh_commands)
      : window_(window), session_(session), workspace_(workspace),
        finish_sketch_handoff_(std::move(finish_sketch_handoff)),
        refresh_commands_(std::move(refresh_commands)) {
    if (window_)
      QTimer::singleShot(0, window_, [this] { ensure_installed(); });
  }

  void ensure_installed() {
    if (installed_ || window_.isNull() || session_ == nullptr || workspace_ == nullptr)
      return;
    viewport_ = window_->findChild<OcctViewport*>(QStringLiteral("blcad.occt_viewport"));
    if (viewport_.isNull())
      return;

    create_context_toolbar();
    create_mini_toolbar();
    rewire_finish_sketch();

    window_->installEventFilter(this);
    viewport_->installEventFilter(this);
    if (auto* browser = window_->findChild<QObject*>(QStringLiteral("blcad.model_browser"))) {
      browser_ = browser;
      browser->installEventFilter(this);
    }
    connect(window_, &QWidget::windowTitleChanged, this,
            [this](const QString&) { refresh(); });

    workspace_->capture_home_view(*viewport_);
    workspace_->apply_selection_filter(*session_, *viewport_, workspace_->selection_filter());
    installed_ = true;
    refresh();
  }

  void refresh() {
    if (!installed_) {
      ensure_installed();
      if (!installed_)
        return;
    }

    synchronize_area_tabs();
    synchronize_filter();
    synchronize_bookmarks();
    const bool context_matches = synchronize_preselection();
    repeat_button_->setEnabled(!workspace_->last_repeatable_command_id().empty() &&
                               !session_->task().active());
    render_mini_toolbar(context_matches);
  }

protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    if (!installed_)
      return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress && !workspace_->active_command_id().empty()) {
      const auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape && workspace_->cancel_command(*session_)) {
        window_->statusBar()->showMessage(QStringLiteral("Modeling command cancelled"));
        refresh_and_publish();
        return true;
      }
    }

    if (watched == viewport_.data() && event->type() == QEvent::MouseButtonRelease) {
      const auto* mouse = static_cast<QMouseEvent*>(event);
      mini_toolbar_anchor_ = mouse->position().toPoint() + QPoint(12, 12);
      schedule_refresh();
    } else if ((watched == viewport_.data() || watched == browser_.data()) &&
               (event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::KeyRelease || event->type() == QEvent::FocusIn)) {
      schedule_refresh();
    }
    return QObject::eventFilter(watched, event);
  }

private:
  static std::vector<std::string> capabilities_for(const GuiSelection& selection) {
    switch (selection.kind) {
    case GuiSelectionKind::Body:
      return {"Body"};
    case GuiSelectionKind::Face:
      return {"Face"};
    case GuiSelectionKind::Edge:
      return {"Edge"};
    case GuiSelectionKind::Datum:
      return {"Datum"};
    case GuiSelectionKind::Component:
      return {"Component"};
    case GuiSelectionKind::AssemblyTarget:
      return {"AssemblyTarget"};
    case GuiSelectionKind::SketchEntity:
    case GuiSelectionKind::Vertex:
      return {};
    }
    return {};
  }

  void refresh_and_publish() {
    refresh();
    if (refresh_commands_)
      refresh_commands_();
  }

  void schedule_refresh() {
    if (refresh_scheduled_ || window_.isNull())
      return;
    refresh_scheduled_ = true;
    QTimer::singleShot(0, window_, [this] {
      refresh_scheduled_ = false;
      refresh();
    });
  }

  void create_context_toolbar() {
    context_toolbar_ = window_->addToolBar(QStringLiteral("Modeling Context"));
    context_toolbar_->setObjectName(QStringLiteral("blcad.modeling_toolbar"));
    context_toolbar_->setMovable(false);
    context_toolbar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    area_tabs_ = new QTabBar(context_toolbar_);
    area_tabs_->setObjectName(QStringLiteral("blcad.modeling_tabs"));
    area_tabs_->setExpanding(false);
    for (const auto tab : GuiModelingWorkspace::tabs())
      area_tabs_->addTab(QString::fromUtf8(tab.data(), static_cast<qsizetype>(tab.size())));
    context_toolbar_->addWidget(area_tabs_);
    connect(area_tabs_, &QTabBar::currentChanged, this, [this](int index) {
      if (synchronizing_controls_ || index < 0 || index > 2 || viewport_.isNull())
        return;
      const auto area = static_cast<GuiModelingArea>(index);
      if (!workspace_->set_area(*session_, area)) {
        synchronize_area_tabs();
        return;
      }
      workspace_->apply_selection_filter(*session_, *viewport_, GuiModelingSelectionFilter::All);
      window_->statusBar()->showMessage(
          QStringLiteral("%1 modeling workspace").arg(area_tabs_->tabText(index)));
      refresh_and_publish();
    });

    filter_combo_ = new QComboBox(context_toolbar_);
    filter_combo_->setObjectName(QStringLiteral("blcad.modeling_selection_filter"));
    const std::array<std::pair<const char*, GuiModelingSelectionFilter>, 8> filters{{
        {"All", GuiModelingSelectionFilter::All},
        {"Profiles", GuiModelingSelectionFilter::Profiles},
        {"Datums", GuiModelingSelectionFilter::Datums},
        {"Faces", GuiModelingSelectionFilter::Faces},
        {"Edges", GuiModelingSelectionFilter::Edges},
        {"Bodies", GuiModelingSelectionFilter::Bodies},
        {"Components", GuiModelingSelectionFilter::Components},
        {"Assembly targets", GuiModelingSelectionFilter::AssemblyTargets},
    }};
    for (const auto& [label, filter] : filters)
      filter_combo_->addItem(QString::fromUtf8(label), static_cast<int>(filter));
    context_toolbar_->addWidget(filter_combo_);
    connect(filter_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
              if (synchronizing_controls_ || index < 0 || viewport_.isNull())
                return;
              const auto filter = static_cast<GuiModelingSelectionFilter>(
                  filter_combo_->itemData(index).toInt());
              workspace_->apply_selection_filter(*session_, *viewport_, filter);
              refresh_and_publish();
            });

    repeat_button_ = new QToolButton(context_toolbar_);
    repeat_button_->setObjectName(QStringLiteral("blcad.modeling_repeat"));
    repeat_button_->setText(QStringLiteral("Repeat"));
    repeat_button_->setToolTip(QStringLiteral("Repeat the last modeling command"));
    context_toolbar_->addWidget(repeat_button_);
    connect(repeat_button_, &QToolButton::clicked, this, [this] {
      if (!workspace_->repeat_last_command(*session_))
        return;
      window_->statusBar()->showMessage(
          QStringLiteral("Repeated %1 — select command inputs or press Esc")
              .arg(QString::fromUtf8(workspace_->active_command_id().data(),
                                     static_cast<qsizetype>(
                                         workspace_->active_command_id().size()))));
      refresh_and_publish();
    });

    view_cube_button_ = new QToolButton(context_toolbar_);
    view_cube_button_->setObjectName(QStringLiteral("blcad.view_cube"));
    view_cube_button_->setText(QStringLiteral("ViewCube"));
    view_cube_button_->setPopupMode(QToolButton::InstantPopup);
    auto* view_menu = new QMenu(view_cube_button_);
    const auto add_view = [this, view_menu](QString label, GuiViewCubeTarget target) {
      QAction* action = view_menu->addAction(std::move(label));
      connect(action, &QAction::triggered, this, [this, target] {
        if (!viewport_.isNull())
          (void)workspace_->activate_view_cube_target(*viewport_, target);
      });
    };
    add_view(QStringLiteral("Home"), GuiViewCubeTarget::Home);
    QAction* set_home = view_menu->addAction(QStringLiteral("Set current as Home"));
    connect(set_home, &QAction::triggered, this, [this] {
      if (!viewport_.isNull())
        workspace_->capture_home_view(*viewport_);
    });
    view_menu->addSeparator();
    add_view(QStringLiteral("Isometric"), GuiViewCubeTarget::Isometric);
    add_view(QStringLiteral("Front"), GuiViewCubeTarget::Front);
    add_view(QStringLiteral("Back"), GuiViewCubeTarget::Back);
    add_view(QStringLiteral("Left"), GuiViewCubeTarget::Left);
    add_view(QStringLiteral("Right"), GuiViewCubeTarget::Right);
    add_view(QStringLiteral("Top"), GuiViewCubeTarget::Top);
    add_view(QStringLiteral("Bottom"), GuiViewCubeTarget::Bottom);
    view_cube_button_->setMenu(view_menu);
    context_toolbar_->addWidget(view_cube_button_);

    bookmark_combo_ = new QComboBox(context_toolbar_);
    bookmark_combo_->setObjectName(QStringLiteral("blcad.camera_bookmarks"));
    bookmark_combo_->setMinimumContentsLength(16);
    context_toolbar_->addWidget(bookmark_combo_);
    connect(bookmark_combo_, qOverload<int>(&QComboBox::activated), this, [this](int index) {
      if (index <= 0 || viewport_.isNull())
        return;
      (void)workspace_->restore_camera_bookmark(bookmark_combo_->itemText(index).toStdString(),
                                                *viewport_);
    });

    auto* save_bookmark = new QToolButton(context_toolbar_);
    save_bookmark->setObjectName(QStringLiteral("blcad.camera_bookmark_save"));
    save_bookmark->setText(QStringLiteral("Save View"));
    context_toolbar_->addWidget(save_bookmark);
    connect(save_bookmark, &QToolButton::clicked, this, [this] {
      if (viewport_.isNull())
        return;
      bool accepted = false;
      const QString name = QInputDialog::getText(window_, QStringLiteral("Camera bookmark"),
                                                  QStringLiteral("Name:"), QLineEdit::Normal,
                                                  QString{}, &accepted)
                               .trimmed();
      if (accepted && !name.isEmpty() &&
          workspace_->save_camera_bookmark(name.toStdString(), *viewport_))
        synchronize_bookmarks(name);
    });

    auto* remove_bookmark = new QToolButton(context_toolbar_);
    remove_bookmark->setObjectName(QStringLiteral("blcad.camera_bookmark_remove"));
    remove_bookmark->setText(QStringLiteral("Remove View"));
    context_toolbar_->addWidget(remove_bookmark);
    connect(remove_bookmark, &QToolButton::clicked, this, [this] {
      const int index = bookmark_combo_->currentIndex();
      if (index > 0 &&
          workspace_->remove_camera_bookmark(bookmark_combo_->itemText(index).toStdString()))
        synchronize_bookmarks();
    });
  }

  void create_mini_toolbar() {
    mini_toolbar_ = new QFrame(viewport_);
    mini_toolbar_->setObjectName(QStringLiteral("blcad.modeling_mini_toolbar"));
    mini_toolbar_->setFrameShape(QFrame::StyledPanel);
    mini_toolbar_->setAutoFillBackground(true);
    mini_toolbar_layout_ = new QHBoxLayout(mini_toolbar_);
    mini_toolbar_layout_->setContentsMargins(4, 4, 4, 4);
    mini_toolbar_layout_->setSpacing(4);
    mini_toolbar_->hide();
    mini_toolbar_anchor_ = QPoint(24, 24);
  }

  void rewire_finish_sketch() {
    if (!finish_sketch_handoff_)
      return;
    auto* finish = window_->findChild<QAction*>(QStringLiteral("blcad.action.finish_sketch"));
    if (!finish)
      return;
    QObject::disconnect(finish, nullptr, window_, nullptr);
    connect(finish, &QAction::triggered, this, [this] {
      finish_sketch_handoff_();
      refresh_and_publish();
    });
  }

  void synchronize_area_tabs() {
    if (session_->document_kind() == GuiDocumentKind::Part &&
        workspace_->area() == GuiModelingArea::Assembly && !session_->task().active()) {
      (void)workspace_->set_area(*session_, GuiModelingArea::Part);
      if (!viewport_.isNull())
        workspace_->apply_selection_filter(*session_, *viewport_, GuiModelingSelectionFilter::All);
    } else if (session_->document_kind() == GuiDocumentKind::Project &&
               workspace_->area() != GuiModelingArea::Assembly && !session_->task().active()) {
      (void)workspace_->set_area(*session_, GuiModelingArea::Assembly);
      if (!viewport_.isNull())
        workspace_->apply_selection_filter(*session_, *viewport_, GuiModelingSelectionFilter::All);
    }

    const QSignalBlocker blocker(area_tabs_);
    synchronizing_controls_ = true;
    const bool part = session_->document_kind() == GuiDocumentKind::Part;
    const bool project = session_->document_kind() == GuiDocumentKind::Project;
    area_tabs_->setTabEnabled(0, part);
    area_tabs_->setTabEnabled(1, part);
    area_tabs_->setTabEnabled(2, project);
    area_tabs_->setCurrentIndex(static_cast<int>(workspace_->area()));
    synchronizing_controls_ = false;
  }

  void synchronize_filter() {
    const QSignalBlocker blocker(filter_combo_);
    synchronizing_controls_ = true;
    for (int index = 0; index < filter_combo_->count(); ++index) {
      if (filter_combo_->itemData(index).toInt() ==
          static_cast<int>(workspace_->selection_filter())) {
        filter_combo_->setCurrentIndex(index);
        break;
      }
    }
    synchronizing_controls_ = false;
  }

  void synchronize_bookmarks(const QString& selected = {}) {
    const QSignalBlocker blocker(bookmark_combo_);
    const QString retained = selected.isEmpty() ? bookmark_combo_->currentText() : selected;
    bookmark_combo_->clear();
    bookmark_combo_->addItem(QStringLiteral("Camera bookmarks"));
    for (const auto& bookmark : workspace_->camera_bookmarks())
      bookmark_combo_->addItem(QString::fromStdString(bookmark.name));
    const int retained_index = bookmark_combo_->findText(retained);
    bookmark_combo_->setCurrentIndex(std::max(0, retained_index));
  }

  bool synchronize_preselection() {
    if (!workspace_->active_command_id().empty())
      return true;
    const auto& entries = session_->selection().entries();
    if (entries.empty()) {
      workspace_->clear_preselection_context();
      return false;
    }

    const GuiSelection& selected = entries.front();
    if (workspace_->preselection().has_value() &&
        workspace_->preselection()->selection == selected)
      return true;

    auto capabilities = capabilities_for(selected);
    if (capabilities.empty()) {
      workspace_->clear_preselection_context();
      return false;
    }
    return workspace_->set_preselection(*session_, {selected, std::move(capabilities)});
  }

  void clear_mini_toolbar() {
    while (auto* item = mini_toolbar_layout_->takeAt(0)) {
      delete item->widget();
      delete item;
    }
  }

  void render_mini_toolbar(bool /*context_matches*/) {
    // The floating contextual mini-toolbar is disabled: it popped up at the
    // cursor on every viewport click, which reads as erratic behaviour. Modeling
    // commands live in the persistent ribbon/menus instead. The frame stays in
    // the widget tree (kept hidden) so its stable object name remains
    // discoverable for acceptance tests.
    clear_mini_toolbar();
    if (mini_toolbar_) mini_toolbar_->hide();
  }

  QPointer<QMainWindow> window_;
  GuiDocumentSession* session_{nullptr};
  GuiModelingWorkspace* workspace_{nullptr};
  std::function<void()> finish_sketch_handoff_;
  std::function<void()> refresh_commands_;
  QPointer<OcctViewport> viewport_;
  QPointer<QObject> browser_;
  QPointer<QToolBar> context_toolbar_;
  QPointer<QTabBar> area_tabs_;
  QPointer<QComboBox> filter_combo_;
  QPointer<QToolButton> repeat_button_;
  QPointer<QToolButton> view_cube_button_;
  QPointer<QComboBox> bookmark_combo_;
  QPointer<QFrame> mini_toolbar_;
  QHBoxLayout* mini_toolbar_layout_{nullptr};
  QPoint mini_toolbar_anchor_;
  bool installed_{false};
  bool synchronizing_controls_{false};
  bool refresh_scheduled_{false};
};

} // namespace blcad::gui
