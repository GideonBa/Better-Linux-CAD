#include "blcad/gui/main_window.hpp"

#include "blcad/geometry/viewport_scene.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTabBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>

#include <array>
#include <sstream>

namespace blcad::gui {
namespace {

constexpr std::array<GuiWorkspace, 4> kWorkspaces{GuiWorkspace::Part, GuiWorkspace::Assembly,
                                                  GuiWorkspace::Inspect, GuiWorkspace::Exchange};

int workspace_index(GuiWorkspace workspace) {
  for (std::size_t index = 0; index < kWorkspaces.size(); ++index) {
    if (kWorkspaces[index] == workspace) {
      return static_cast<int>(index);
    }
  }
  return 0;
}

constexpr int kSemanticIdRole = Qt::UserRole;
constexpr int kSelectionKindRole = Qt::UserRole + 1;
constexpr int kPropertyKeyRole = Qt::UserRole + 2;
constexpr int kPropertyEditableRole = Qt::UserRole + 3;
constexpr int kViewportSemanticIdRole = Qt::UserRole + 4;

QTreeWidgetItem* append_browser_item(QTreeWidgetItem* parent, QTreeWidget* tree,
                                     const GuiBrowserNode& node) {
  auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
  item->setText(0, QString::fromStdString(node.label));
  item->setData(0, kSemanticIdRole, QString::fromStdString(node.semantic_id));
  item->setData(0, kViewportSemanticIdRole, QString::fromStdString(node.viewport_semantic_id));
  if (node.selection_kind)
    item->setData(0, kSelectionKindRole, static_cast<int>(*node.selection_kind));
  if (node.kind == GuiBrowserNodeKind::Group)
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  for (const auto& child : node.children)
    append_browser_item(item, tree, child)->setExpanded(true);
  return item;
}

QTreeWidgetItem* find_browser_item(QTreeWidgetItem* item, QStringView id) {
  if (item->data(0, kSemanticIdRole).toString() == id ||
      item->data(0, kViewportSemanticIdRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_browser_item(item->child(index), id))
      return found;
  return nullptr;
}

QString document_id_from_name(const QString& name, QStringView prefix) {
  QString id = prefix.toString();
  for (const QChar character : name.toLower()) {
    if (character.isLetterOrNumber())
      id += character;
    else if (!id.endsWith(QLatin1Char('.')) && !id.endsWith(QLatin1Char('_')))
      id += QLatin1Char('_');
  }
  while (id.endsWith(QLatin1Char('_')))
    id.chop(1);
  return id;
}

bool sketch_idle_stage(GuiSketchInteractionStage stage) noexcept {
  return stage == GuiSketchInteractionStage::Idle || stage == GuiSketchInteractionStage::Hover;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setObjectName(QStringLiteral("blcad.main_window"));
  setWindowTitle(QStringLiteral("BLCAD"));
  resize(1280, 800);
  create_default_commands();
  create_shell();
  viewport_manipulator_shell_.set_candidate_callback(
      [this](const GuiViewportManipulatorCandidate& candidate) {
        if (interactive_assembly_.active())
          interactive_assembly_.on_candidate(candidate);
        else
          interactive_features_.on_candidate(candidate);
      });
  refresh_command_state();
}

GuiDocumentSession& MainWindow::session() noexcept {
  return session_;
}

const GuiDocumentSession& MainWindow::session() const noexcept {
  return session_;
}

GuiCommandRegistry& MainWindow::command_registry() noexcept {
  return command_registry_;
}

const GuiCommandRegistry& MainWindow::command_registry() const noexcept {
  return command_registry_;
}

GuiSketchWorkbench& MainWindow::sketch_workbench() noexcept { return sketch_workbench_; }

GuiSketchWorkspace& MainWindow::sketch_workspace() noexcept { return sketch_workspace_; }

const GuiSketchWorkspace& MainWindow::sketch_workspace() const noexcept { return sketch_workspace_; }

GuiPartFoundationWorkbench& MainWindow::part_foundation_workbench() noexcept {
  return part_foundation_workbench_;
}

GuiPartOperationsWorkbench& MainWindow::part_operations_workbench() noexcept {
  return part_operations_workbench_;
}

GuiSpatialSurfaceWorkbench& MainWindow::spatial_surface_workbench() noexcept {
  return spatial_surface_workbench_;
}

GuiAssemblyWorkbench& MainWindow::assembly_workbench() noexcept { return assembly_workbench_; }

GuiAnalysisExportWorkbench& MainWindow::analysis_export_workbench() noexcept {
  return analysis_export_workbench_;
}

const std::optional<SketchId>& MainWindow::active_sketch() const noexcept { return active_sketch_; }

bool MainWindow::request_workspace(GuiWorkspace workspace) noexcept {
  if (sketch_workspace_.active() || workspace == GuiWorkspace::Sketch ||
      !session_.set_workspace(workspace)) {
    synchronize_workspace_tab();
    return false;
  }
  synchronize_workspace_tab();
  refresh_command_state();
  return true;
}

void MainWindow::refresh_command_state() {
  const auto context = session_.command_context();
  const bool idle = !context.task_active();
  const bool sketch_active = sketch_workspace_.active();
  const bool sketch_idle = sketch_active && idle && sketch_idle_stage(sketch_workspace_.stage());
  new_part_action_->setEnabled(idle && !sketch_active);
  new_project_action_->setEnabled(idle && !sketch_active);
  open_action_->setEnabled(idle && !sketch_active);
  save_action_->setEnabled(command_registry_.is_enabled("document.save", context));
  save_as_action_->setEnabled(context.has_document() && idle);
  undo_action_->setEnabled(session_.can_undo() && !sketch_active);
  redo_action_->setEnabled(session_.can_redo() && !sketch_active);
  recompute_action_->setEnabled(context.has_document() && idle);
  const bool part = context.document_kind == GuiDocumentKind::Part;
  const bool part_workspace_idle = part && idle && !sketch_active;
  for (QAction* action : {create_datum_action_, create_axis_action_, create_sketch_action_,
                          edit_sketch_action_})
    if (action)
      action->setEnabled(part_workspace_idle);
  if (finish_sketch_action_)
    finish_sketch_action_->setEnabled(sketch_idle);
  if (add_line_action_)
    add_line_action_->setEnabled(part && sketch_idle);
  for (QAction* action : sketch_create_actions_)
    if (action)
      action->setEnabled(part && sketch_idle);
  if (repeat_sketch_action_)
    repeat_sketch_action_->setEnabled(sketch_idle && !sketch_workspace_.last_repeatable_command().empty());
  for (QAction* action : {inspect_sketch_action_, repair_sketch_action_})
    if (action)
      action->setEnabled(part && idle);

  refresh_sketch_workspace_ui();
  statusBar()->showMessage(sketch_active
                               ? QString::fromStdString(sketch_workspace_.status().tool_hint)
                               : session_.task().active()
                                     ? QStringLiteral("Task active — Apply or Cancel")
                                     : QStringLiteral("Ready"));
  refresh_document_presentation();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (sketch_workspace_.active()) {
    QMessageBox::warning(this, QStringLiteral("Sketch workspace"),
                         QStringLiteral("Finish Sketch before closing the document."));
    event->ignore();
    return;
  }
  if (session_.task().active()) {
    QMessageBox::warning(this, QStringLiteral("Active task"),
                         QStringLiteral("Apply or cancel the active task before closing."));
    event->ignore();
    return;
  }
  if (!confirm_discard_dirty()) {
    event->ignore();
    return;
  }
  event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape && sketch_workspace_.active() &&
      sketch_workspace_.escape(session_)) {
    refresh_command_state();
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

void MainWindow::create_shell() {
  create_menus();
  create_command_bar();
  create_docks();

  viewport_ = new OcctViewport(this);
  viewport_->set_selection_callback([this](std::optional<GuiSelection> selection) {
    if (synchronizing_selection_)
      return;
    session_.selection().clear();
    if (selection.has_value())
      (void)session_.selection().add(*selection);
    synchronize_tree_selection(selection ? selection->semantic_id : std::string_view{});
    refresh_command_state();
  });
  viewport_->set_context_menu_callback([this](QPoint global_position) {
    if (!sketch_workspace_.active())
      return;
    QMenu menu(this);
    QAction* repeat = menu.addAction(QStringLiteral("Repeat last Sketch command"));
    repeat->setEnabled(!sketch_workspace_.last_repeatable_command().empty() &&
                       sketch_idle_stage(sketch_workspace_.stage()) && !session_.task().active());
    QAction* selected = menu.exec(global_position);
    if (selected == repeat)
      repeat_last_sketch_command();
  });
  setCentralWidget(viewport_);

  statusBar()->setObjectName(QStringLiteral("blcad.status_bar"));
  statusBar()->setSizeGripEnabled(true);
  sketch_cursor_status_ = new QLabel(QStringLiteral("Cursor: —"), this);
  sketch_snap_status_ = new QLabel(QStringLiteral("Snap: —"), this);
  sketch_dof_status_ = new QLabel(QStringLiteral("DOF: —"), this);
  sketch_solve_status_ = new QLabel(QStringLiteral("Solve: Not evaluated"), this);
  sketch_cursor_status_->setObjectName(QStringLiteral("blcad.sketch.cursor_status"));
  sketch_snap_status_->setObjectName(QStringLiteral("blcad.sketch.snap_status"));
  sketch_dof_status_->setObjectName(QStringLiteral("blcad.sketch.dof_status"));
  sketch_solve_status_->setObjectName(QStringLiteral("blcad.sketch.solve_status"));
  statusBar()->addPermanentWidget(sketch_cursor_status_);
  statusBar()->addPermanentWidget(sketch_snap_status_);
  statusBar()->addPermanentWidget(sketch_dof_status_);
  statusBar()->addPermanentWidget(sketch_solve_status_);
}

void MainWindow::create_menus() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));
  auto* new_menu = file_menu->addMenu(QStringLiteral("New"));
  new_part_action_ = new_menu->addAction(QStringLiteral("Part"));
  new_part_action_->setObjectName(QStringLiteral("blcad.action.new_part"));
  new_project_action_ = new_menu->addAction(QStringLiteral("Project"));
  new_project_action_->setObjectName(QStringLiteral("blcad.action.new_project"));
  open_action_ = file_menu->addAction(QStringLiteral("Open…"));
  open_action_->setObjectName(QStringLiteral("blcad.action.open"));
  save_action_ = file_menu->addAction(QStringLiteral("Save"));
  save_action_->setObjectName(QStringLiteral("blcad.action.save"));
  save_as_action_ = file_menu->addAction(QStringLiteral("Save As…"));
  save_as_action_->setObjectName(QStringLiteral("blcad.action.save_as"));
  file_menu->addSeparator();
  file_menu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

  auto* edit_menu = menuBar()->addMenu(QStringLiteral("&Edit"));
  undo_action_ = edit_menu->addAction(QStringLiteral("Undo"));
  undo_action_->setObjectName(QStringLiteral("blcad.action.undo"));
  redo_action_ = edit_menu->addAction(QStringLiteral("Redo"));
  redo_action_->setObjectName(QStringLiteral("blcad.action.redo"));

  auto* view_menu = menuBar()->addMenu(QStringLiteral("&View"));
  recompute_action_ = view_menu->addAction(QStringLiteral("Recompute"));
  recompute_action_->setObjectName(QStringLiteral("blcad.action.recompute"));
  view_menu->addSeparator();
  auto* display_menu = view_menu->addMenu(QStringLiteral("Display mode"));
  auto* display_group = new QActionGroup(this);
  display_group->setExclusive(true);
  shaded_action_ = display_menu->addAction(QStringLiteral("Shaded"));
  shaded_edges_action_ = display_menu->addAction(QStringLiteral("Shaded with edges"));
  wireframe_action_ = display_menu->addAction(QStringLiteral("Wireframe"));
  for (QAction* action : {shaded_action_, shaded_edges_action_, wireframe_action_}) {
    action->setCheckable(true);
    display_group->addAction(action);
  }
  shaded_action_->setObjectName(QStringLiteral("blcad.action.display_shaded"));
  shaded_edges_action_->setObjectName(QStringLiteral("blcad.action.display_shaded_edges"));
  wireframe_action_->setObjectName(QStringLiteral("blcad.action.display_wireframe"));
  shaded_edges_action_->setChecked(true);
  perspective_action_ = view_menu->addAction(QStringLiteral("Perspective"));
  perspective_action_->setObjectName(QStringLiteral("blcad.action.perspective"));
  perspective_action_->setCheckable(true);
  perspective_action_->setChecked(true);
  view_menu->addAction(QStringLiteral("Fit all"), this, [this] { viewport_->fit_all(); })
      ->setObjectName(QStringLiteral("blcad.action.fit_all"));
  auto* standard_views = view_menu->addMenu(QStringLiteral("Standard views"));
  const auto add_standard_view = [this, standard_views](QString label, QString object_name,
                                                        GuiStandardView view) {
    QAction* action = standard_views->addAction(std::move(label));
    action->setObjectName(std::move(object_name));
    connect(action, &QAction::triggered, this,
            [this, view] { viewport_->set_standard_view(view); });
  };
  add_standard_view(QStringLiteral("Isometric"), QStringLiteral("blcad.action.view_isometric"),
                    GuiStandardView::Isometric);
  add_standard_view(QStringLiteral("Front"), QStringLiteral("blcad.action.view_front"),
                    GuiStandardView::Front);
  add_standard_view(QStringLiteral("Back"), QStringLiteral("blcad.action.view_back"),
                    GuiStandardView::Back);
  add_standard_view(QStringLiteral("Left"), QStringLiteral("blcad.action.view_left"),
                    GuiStandardView::Left);
  add_standard_view(QStringLiteral("Right"), QStringLiteral("blcad.action.view_right"),
                    GuiStandardView::Right);
  add_standard_view(QStringLiteral("Top"), QStringLiteral("blcad.action.view_top"),
                    GuiStandardView::Top);
  add_standard_view(QStringLiteral("Bottom"), QStringLiteral("blcad.action.view_bottom"),
                    GuiStandardView::Bottom);

  connect(new_part_action_, &QAction::triggered, this,
          [this] { create_new_document(GuiDocumentKind::Part); });
  connect(new_project_action_, &QAction::triggered, this,
          [this] { create_new_document(GuiDocumentKind::Project); });
  connect(open_action_, &QAction::triggered, this, [this] { open_document(); });
  connect(save_action_, &QAction::triggered, this, [this] { save_document(false); });
  connect(save_as_action_, &QAction::triggered, this, [this] { save_document(true); });
  connect(undo_action_, &QAction::triggered, this, [this] {
    const auto result = session_.undo();
    if (result.has_error())
      show_error(result.error());
    refresh_command_state();
  });
  connect(redo_action_, &QAction::triggered, this, [this] {
    const auto result = session_.redo();
    if (result.has_error())
      show_error(result.error());
    refresh_command_state();
  });
  connect(recompute_action_, &QAction::triggered, this, [this] {
    const auto result = session_.recompute();
    if (result.has_error())
      show_error(result.error());
    refresh_command_state();
  });
  connect(shaded_action_, &QAction::triggered, this,
          [this] { viewport_->set_display_mode(GuiViewportDisplayMode::Shaded); });
  connect(shaded_edges_action_, &QAction::triggered, this,
          [this] { viewport_->set_display_mode(GuiViewportDisplayMode::ShadedWithEdges); });
  connect(wireframe_action_, &QAction::triggered, this,
          [this] { viewport_->set_display_mode(GuiViewportDisplayMode::Wireframe); });
  connect(perspective_action_, &QAction::toggled, this, [this](bool perspective) {
    viewport_->set_projection(perspective ? GuiViewportProjection::Perspective
                                          : GuiViewportProjection::Orthographic);
  });

  auto* sketch_menu = menuBar()->addMenu(QStringLiteral("&Sketch"));
  sketch_menu->setObjectName(QStringLiteral("blcad.menu.sketch"));
  create_datum_action_ = sketch_menu->addAction(QStringLiteral("New XY datum plane…"));
  create_axis_action_ = sketch_menu->addAction(QStringLiteral("New Z datum axis…"));
  create_sketch_action_ = sketch_menu->addAction(QStringLiteral("New sketch on selection…"));
  edit_sketch_action_ = sketch_menu->addAction(QStringLiteral("Enter Sketch"));
  finish_sketch_action_ = sketch_menu->addAction(QStringLiteral("Finish Sketch"));
  sketch_menu->addSeparator();
  auto* create_menu = sketch_menu->addMenu(QStringLiteral("Create"));
  create_menu->setObjectName(QStringLiteral("blcad.menu.sketch_create"));
  const std::array<std::pair<const char*, const char*>, 9> create_tools{{
      {"blcad.action.sketch_create_point", "Point"},
      {"blcad.action.sketch_create_line", "Line"},
      {"blcad.action.sketch_create_polyline", "Polyline"},
      {"blcad.action.sketch_create_corner_rectangle", "Corner rectangle"},
      {"blcad.action.sketch_create_center_rectangle", "Center rectangle"},
      {"blcad.action.sketch_create_three_point_rectangle", "Three-point rectangle"},
      {"blcad.action.sketch_create_parallelogram", "Parallelogram"},
      {"blcad.action.sketch_create_polygon", "Regular polygon"},
      {"blcad.action.sketch_create_centerline", "Centerline"},
  }};
  sketch_create_actions_.reserve(create_tools.size());
  for (const auto& [object_name, label] : create_tools) {
    QAction* action = create_menu->addAction(QString::fromUtf8(label));
    action->setObjectName(QString::fromUtf8(object_name));
    sketch_create_actions_.push_back(action);
  }
  add_line_action_ = sketch_menu->addAction(QStringLiteral("Add line"));
  repeat_sketch_action_ = sketch_menu->addAction(QStringLiteral("Repeat last Sketch command"));
  inspect_sketch_action_ = sketch_menu->addAction(QStringLiteral("Inspect selected sketch"));
  repair_sketch_action_ = sketch_menu->addAction(QStringLiteral("Preview/repair selected sketch…"));
  create_datum_action_->setObjectName(QStringLiteral("blcad.action.create_datum_plane"));
  create_axis_action_->setObjectName(QStringLiteral("blcad.action.create_datum_axis"));
  create_sketch_action_->setObjectName(QStringLiteral("blcad.action.create_sketch"));
  edit_sketch_action_->setObjectName(QStringLiteral("blcad.action.edit_sketch"));
  finish_sketch_action_->setObjectName(QStringLiteral("blcad.action.finish_sketch"));
  add_line_action_->setObjectName(QStringLiteral("blcad.action.sketch_line"));
  repeat_sketch_action_->setObjectName(QStringLiteral("blcad.action.sketch_repeat"));
  inspect_sketch_action_->setObjectName(QStringLiteral("blcad.action.inspect_sketch"));
  repair_sketch_action_->setObjectName(QStringLiteral("blcad.action.repair_sketch"));
  connect(create_datum_action_, &QAction::triggered, this, [this] { create_xy_datum(); });
  connect(create_axis_action_, &QAction::triggered, this, [this] { create_default_datum_axis(); });
  connect(create_sketch_action_, &QAction::triggered, this, [this] { create_sketch_on_selection(); });
  connect(edit_sketch_action_, &QAction::triggered, this, [this] { edit_selected_sketch(); });
  connect(finish_sketch_action_, &QAction::triggered, this, [this] { finish_sketch(); });
  connect(add_line_action_, &QAction::triggered, this, [this] { add_numeric_sketch_line(); });
  connect(repeat_sketch_action_, &QAction::triggered, this, [this] { repeat_last_sketch_command(); });
  connect(inspect_sketch_action_, &QAction::triggered, this, [this] { inspect_selected_sketch(); });
  connect(repair_sketch_action_, &QAction::triggered, this, [this] { repair_selected_sketch(); });
}

void MainWindow::create_command_bar() {
  auto* command_bar = addToolBar(QStringLiteral("Workspaces"));
  command_bar->setObjectName(QStringLiteral("blcad.command_bar"));
  command_bar->setMovable(false);
  command_bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  workspace_tabs_ = new QTabBar(command_bar);
  workspace_tabs_->setObjectName(QStringLiteral("blcad.workspace_tabs"));
  workspace_tabs_->setExpanding(false);
  workspace_tabs_->addTab(QStringLiteral("Part"));
  workspace_tabs_->addTab(QStringLiteral("Assembly"));
  workspace_tabs_->addTab(QStringLiteral("Inspect"));
  workspace_tabs_->addTab(QStringLiteral("Exchange"));
  command_bar->addWidget(workspace_tabs_);

  sketch_command_groups_ = new QWidget(command_bar);
  sketch_command_groups_->setObjectName(QStringLiteral("blcad.sketch.command_groups"));
  auto* groups_layout = new QHBoxLayout(sketch_command_groups_);
  groups_layout->setContentsMargins(12, 0, 0, 0);
  groups_layout->setSpacing(10);
  for (const auto group : GuiSketchWorkspace::command_groups()) {
    auto* label = new QLabel(QString::fromUtf8(group.data(), static_cast<qsizetype>(group.size())),
                             sketch_command_groups_);
    label->setObjectName(QStringLiteral("blcad.sketch.command_group"));
    groups_layout->addWidget(label);
  }
  sketch_command_groups_action_ = command_bar->addWidget(sketch_command_groups_);

  sketch_numeric_hud_ = new QLineEdit(command_bar);
  sketch_numeric_hud_->setObjectName(QStringLiteral("blcad.sketch.numeric_hud"));
  sketch_numeric_hud_->setPlaceholderText(QStringLiteral("id, x1, y1, x2, y2 (mm)"));
  sketch_numeric_hud_->setMaximumWidth(280);
  sketch_numeric_hud_action_ = command_bar->addWidget(sketch_numeric_hud_);
  connect(sketch_numeric_hud_, &QLineEdit::textChanged, this, [this](const QString& text) {
    if (sketch_workspace_.stage() == GuiSketchInteractionStage::NumericInput)
      (void)sketch_workspace_.set_numeric_input(text.toStdString());
  });
  connect(sketch_numeric_hud_, &QLineEdit::returnPressed, this,
          [this] { commit_numeric_sketch_line(); });

  connect(workspace_tabs_, &QTabBar::currentChanged, this, [this](int index) {
    if (index >= 0 && index < static_cast<int>(kWorkspaces.size())) {
      (void)request_workspace(kWorkspaces[static_cast<std::size_t>(index)]);
    }
  });
  synchronize_workspace_tab();
}

void MainWindow::create_docks() {
  auto* browser_dock = new QDockWidget(QStringLiteral("Model / Assembly"), this);
  browser_dock->setObjectName(QStringLiteral("blcad.model_browser_dock"));
  model_browser_ = new QTreeWidget(browser_dock);
  model_browser_->setObjectName(QStringLiteral("blcad.model_browser"));
  model_browser_->setHeaderLabel(QStringLiteral("No active document"));
  browser_dock->setWidget(model_browser_);
  addDockWidget(Qt::LeftDockWidgetArea, browser_dock);

  auto* task_dock = new QDockWidget(QStringLiteral("Properties / Tasks"), this);
  task_dock->setObjectName(QStringLiteral("blcad.task_panel_dock"));
  auto* task_panel = new QWidget(task_dock);
  task_panel->setObjectName(QStringLiteral("blcad.task_panel"));
  auto* task_layout = new QVBoxLayout(task_panel);
  property_table_ = new QTableWidget(0, 2, task_panel);
  property_table_->setObjectName(QStringLiteral("blcad.property_table"));
  property_table_->setHorizontalHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
  property_table_->horizontalHeader()->setStretchLastSection(true);
  property_validation_ = new QLabel(QStringLiteral("Select a model item"), task_panel);
  property_validation_->setObjectName(QStringLiteral("blcad.property_validation"));
  property_validation_->setWordWrap(true);
  auto* buttons = new QWidget(task_panel);
  auto* button_layout = new QHBoxLayout(buttons);
  button_layout->setContentsMargins(0, 0, 0, 0);
  preview_button_ = new QPushButton(QStringLiteral("Preview"), buttons);
  apply_button_ = new QPushButton(QStringLiteral("Apply"), buttons);
  cancel_button_ = new QPushButton(QStringLiteral("Cancel"), buttons);
  preview_button_->setObjectName(QStringLiteral("blcad.property_preview"));
  apply_button_->setObjectName(QStringLiteral("blcad.property_apply"));
  cancel_button_->setObjectName(QStringLiteral("blcad.property_cancel"));
  button_layout->addWidget(preview_button_);
  button_layout->addWidget(apply_button_);
  button_layout->addWidget(cancel_button_);
  task_layout->addWidget(property_table_);
  task_layout->addWidget(property_validation_);
  task_layout->addWidget(buttons);
  task_dock->setWidget(task_panel);
  addDockWidget(Qt::LeftDockWidgetArea, task_dock);
  splitDockWidget(browser_dock, task_dock, Qt::Vertical);

  auto* diagnostics_dock = new QDockWidget(QStringLiteral("Diagnostics"), this);
  diagnostics_dock->setObjectName(QStringLiteral("blcad.diagnostics_dock"));
  diagnostics_ = new QTextEdit(diagnostics_dock);
  diagnostics_->setObjectName(QStringLiteral("blcad.diagnostics"));
  diagnostics_->setReadOnly(true);
  diagnostics_->setPlaceholderText(QStringLiteral("Diagnostics will appear here"));
  diagnostics_dock->setWidget(diagnostics_);
  addDockWidget(Qt::BottomDockWidgetArea, diagnostics_dock);

  connect(model_browser_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current) {
            if (synchronizing_selection_)
              return;
            const std::string id = current ? current->data(0, kSemanticIdRole).toString().toStdString() : std::string{};
            selected_browser_id_ = id;
            const auto* node = browser_model_.find(id);
            show_node_properties(node);
            session_.selection().clear();
            synchronizing_selection_ = true;
            if (node && node->selection_kind && !id.empty()) {
              const std::string selection_id = node->viewport_semantic_id.empty()
                                                   ? id
                                                   : node->viewport_semantic_id;
              (void)session_.selection().add({*node->selection_kind, selection_id});
              (void)viewport_->select_semantic(selection_id);
            } else if (viewport_) {
              viewport_->clear_selection();
            }
            synchronizing_selection_ = false;
            refresh_command_state();
          });
  connect(preview_button_, &QPushButton::clicked, this, [this] { preview_property_edit(); });
  connect(apply_button_, &QPushButton::clicked, this, [this] { apply_property_edit(); });
  connect(cancel_button_, &QPushButton::clicked, this, [this] { cancel_property_edit(); });
}

void MainWindow::create_default_commands() {
  const auto idle = [](const GuiCommandContext& context) { return !context.task_active(); };
  (void)command_registry_.register_command({"workspace.part", "Part", idle});
  (void)command_registry_.register_command(
      {"workspace.sketch", "Sketch", [](const GuiCommandContext& context) {
         return context.document_kind == GuiDocumentKind::Part && !context.task_active();
       }});
  (void)command_registry_.register_command({"workspace.assembly", "Assembly", idle});
  (void)command_registry_.register_command({"workspace.inspect", "Inspect", idle});
  (void)command_registry_.register_command({"workspace.exchange", "Exchange", idle});
  (void)command_registry_.register_command(
      {"document.save", "Save", [](const GuiCommandContext& context) {
         return context.has_document() && !context.task_active();
       }});
}

void MainWindow::synchronize_workspace_tab() {
  if (!workspace_tabs_) {
    return;
  }
  const QSignalBlocker blocker(workspace_tabs_);
  workspace_tabs_->setCurrentIndex(workspace_index(session_.workspace()));
}

void MainWindow::refresh_document_presentation() {
  const QString name = session_.has_document()
                           ? QString::fromStdString(std::string(session_.display_name()))
                           : QStringLiteral("No active document");
  model_browser_->setHeaderLabel(
      sketch_workspace_.active() && active_sketch_
          ? QStringLiteral("Sketch — %1").arg(QString::fromStdString(active_sketch_->value()))
          : name);
  setWindowTitle(session_.has_document()
                     ? QStringLiteral("%1%2 — BLCAD")
                           .arg(name, session_.dirty() ? QStringLiteral(" *") : QString{})
                     : QStringLiteral("BLCAD"));

  QStringList lines;
  for (const auto& diagnostic : session_.diagnostics()) {
    lines.push_back(QStringLiteral("[%1] %2: %3")
                        .arg(QString::fromStdString(std::string(to_string(diagnostic.category))),
                             QString::fromStdString(diagnostic.object_id),
                             QString::fromStdString(diagnostic.message)));
  }
  diagnostics_->setPlainText(lines.join(QLatin1Char('\n')));
  refresh_model_browser();
  refresh_viewport_scene();
}

void MainWindow::refresh_model_browser() {
  if (browser_revision_ == session_.presentation_revision())
    return;
  browser_revision_ = session_.presentation_revision();
  const std::string retained = selected_browser_id_;
  browser_model_ = GuiDocumentBrowser::build(session_);
  const QSignalBlocker blocker(model_browser_);
  model_browser_->clear();
  for (const auto& root : browser_model_.roots())
    append_browser_item(nullptr, model_browser_, root)->setExpanded(true);
  for (int index = 0; index < model_browser_->topLevelItemCount(); ++index) {
    auto* root = model_browser_->topLevelItem(index);
    for (int child = 0; child < root->childCount(); ++child)
      root->child(child)->setExpanded(true);
  }
  selected_browser_id_.clear();
  if (!retained.empty())
    synchronize_tree_selection(retained);
  else
    show_node_properties(nullptr);
}

void MainWindow::show_node_properties(const GuiBrowserNode* node) {
  const QSignalBlocker blocker(property_table_);
  property_table_->setRowCount(0);
  if (!node) {
    property_validation_->setText(sketch_workspace_.active()
                                      ? QString::fromStdString(sketch_workspace_.status().tool_hint)
                                      : QStringLiteral("Select a model item"));
    preview_button_->setEnabled(false);
    apply_button_->setEnabled(false);
    cancel_button_->setEnabled(session_.task().active());
    return;
  }
  for (const auto& field : node->properties) {
    const int row = property_table_->rowCount();
    property_table_->insertRow(row);
    auto* label = new QTableWidgetItem(QString::fromStdString(field.label));
    auto* value = new QTableWidgetItem(QString::fromStdString(field.value));
    label->setFlags(label->flags() & ~Qt::ItemIsEditable);
    value->setData(kPropertyKeyRole, QString::fromStdString(field.key));
    value->setData(kPropertyEditableRole, field.editable);
    if (!field.editable) {
      value->setFlags(value->flags() & ~Qt::ItemIsEditable);
      value->setToolTip(QString::fromStdString(field.read_only_reason));
      value->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    }
    property_table_->setItem(row, 0, label);
    property_table_->setItem(row, 1, value);
  }
  property_validation_->setText(QStringLiteral("Read-only fields are grey; editable fields use Preview, then Apply."));
  preview_button_->setEnabled(true);
  apply_button_->setEnabled(false);
  cancel_button_->setEnabled(session_.task().active());
}

void MainWindow::synchronize_tree_selection(std::string_view semantic_id) {
  synchronizing_selection_ = true;
  QTreeWidgetItem* found = nullptr;
  const QString id = QString::fromUtf8(semantic_id.data(), static_cast<qsizetype>(semantic_id.size()));
  for (int index = 0; index < model_browser_->topLevelItemCount() && !found; ++index)
    found = find_browser_item(model_browser_->topLevelItem(index), id);
  model_browser_->setCurrentItem(found);
  selected_browser_id_ = found ? std::string(semantic_id) : std::string{};
  show_node_properties(browser_model_.find(selected_browser_id_));
  synchronizing_selection_ = false;
}

void MainWindow::preview_property_edit() {
  const auto* node = browser_model_.find(selected_browser_id_);
  const int row = property_table_->currentRow();
  auto* item = row >= 0 ? property_table_->item(row, 1) : nullptr;
  if (!node || !item || !item->data(kPropertyEditableRole).toBool()) {
    property_validation_->setText(QStringLiteral("Select an editable value first."));
    return;
  }
  const auto result = edit_browser_property(session_, *node,
      item->data(kPropertyKeyRole).toString().toStdString(), item->text().toStdString(), false);
  if (result.has_error()) {
    property_validation_->setText(QString::fromStdString(result.error().message()));
    apply_button_->setEnabled(false);
    return;
  }
  if (!session_.task().active()) {
    (void)session_.task().begin("property.edit");
    (void)session_.task().begin_parameter_editing();
  } else if (session_.task().stage() == GuiTaskStage::Preview) {
    (void)session_.task().return_to_editing();
  }
  (void)session_.task().show_preview();
  property_validation_->setText(QStringLiteral("Preview valid — Apply commits one undoable transaction."));
  apply_button_->setEnabled(true);
  cancel_button_->setEnabled(true);
  refresh_command_state();
}

void MainWindow::apply_property_edit() {
  const auto* node = browser_model_.find(selected_browser_id_);
  const int row = property_table_->currentRow();
  auto* item = row >= 0 ? property_table_->item(row, 1) : nullptr;
  if (!node || !item || session_.task().stage() != GuiTaskStage::Preview)
    return;
  // Transactions require the task guard to be inactive; the preview remains non-persistent.
  (void)session_.task().apply();
  const auto result = edit_browser_property(session_, *node,
      item->data(kPropertyKeyRole).toString().toStdString(), item->text().toStdString(), true);
  if (result.has_error())
    show_error(result.error());
  refresh_command_state();
}

void MainWindow::cancel_property_edit() {
  if (session_.task().active())
    (void)session_.task().cancel();
  show_node_properties(browser_model_.find(selected_browser_id_));
  refresh_command_state();
}

void MainWindow::create_xy_datum() {
  statusBar()->showMessage(QString::fromStdString(
      GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateDatumPlane).text));
  bool accepted = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("New datum plane"),
                                              QStringLiteral("Name:"), QLineEdit::Normal,
                                              QStringLiteral("XY"), &accepted).trimmed();
  if (!accepted || name.isEmpty()) return;
  const DatumPlaneId id(document_id_from_name(name, u"datum.").toStdString());
  const auto result = sketch_workbench_.create_xy_datum(session_, id, name.toStdString());
  if (result.has_error()) show_error(result.error());
  refresh_command_state();
}

void MainWindow::create_default_datum_axis() {
  bool accepted = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("New datum axis"),
                                              QStringLiteral("Name:"), QLineEdit::Normal,
                                              QStringLiteral("Z Axis"), &accepted).trimmed();
  if (!accepted || name.isEmpty()) return;
  auto axis = DatumAxis::create_explicit(
      DatumAxisId(document_id_from_name(name, u"axis.").toStdString()), name.toStdString(),
      {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
  if (axis.has_error()) {
    show_error(axis.error());
    return;
  }
  const auto result = sketch_workbench_.create_datum_axis(session_, std::move(axis.value()));
  if (result.has_error()) show_error(result.error());
  refresh_command_state();
}

void MainWindow::create_sketch_on_selection() {
  const auto* node = browser_model_.find(selected_browser_id_);
  if (!node || (node->kind != GuiBrowserNodeKind::Datum &&
                node->kind != GuiBrowserNodeKind::Construction)) {
    property_validation_->setText(QString::fromStdString(
        GuiSketchWorkbench::prompt_for(GuiSketchCommand::CreateSketch).text));
    return;
  }
  bool accepted = false;
  const QString name = QInputDialog::getText(this, QStringLiteral("New sketch"),
                                              QStringLiteral("Name:"), QLineEdit::Normal,
                                              QStringLiteral("Sketch"), &accepted).trimmed();
  if (!accepted || name.isEmpty()) return;
  auto sketch = Sketch::create(SketchId(document_id_from_name(name, u"sketch.").toStdString()),
                               name.toStdString(), DatumPlaneId(node->semantic_id));
  if (sketch.has_error()) {
    show_error(sketch.error());
    return;
  }
  const SketchId id = sketch.value().id();
  const auto result = sketch_workbench_.create_sketch(session_, std::move(sketch.value()));
  if (result.has_error()) show_error(result.error());
  else active_sketch_ = id;
  refresh_command_state();
}

void MainWindow::edit_selected_sketch() {
  const auto* node = browser_model_.find(selected_browser_id_);
  if (!node || node->kind != GuiBrowserNodeKind::Sketch || !session_.part_document() ||
      session_.part_document()->find_sketch(SketchId(node->semantic_id)) == nullptr) {
    property_validation_->setText(QString::fromStdString(
        GuiSketchWorkbench::prompt_for(GuiSketchCommand::Diagnose).text));
    return;
  }

  sketch_camera_bookmark_ = viewport_->camera_bookmark();
  sketch_selection_filter_mask_ = viewport_->selection_filter_mask();
  const auto plane = sketch_workspace_.enter(session_, sketch_workbench_, SketchId(node->semantic_id));
  if (plane.has_error()) {
    show_error(plane.error());
    sketch_camera_bookmark_.reset();
    return;
  }
  if (!viewport_->set_plane_camera(plane.value().origin, plane.value().normal,
                                   plane.value().y_axis)) {
    (void)sketch_workspace_.finish(session_, sketch_workbench_);
    sketch_camera_bookmark_.reset();
    show_error(Error::geometry(node->semantic_id, "could not orient camera normal to sketch plane"));
    return;
  }
  active_sketch_ = sketch_workspace_.active_sketch();
  viewport_->set_projection(GuiViewportProjection::Orthographic);
  perspective_action_->setChecked(false);
  viewport_->set_selection_filter_mask(GuiSketchWorkspace::selection_filter_mask());
  viewport_->set_sketch_focus(active_sketch_->value(), GuiSketchSurroundingsMode::Dim);
  viewport_->setCursor(Qt::CrossCursor);
  synchronize_workspace_tab();
  refresh_command_state();
}

void MainWindow::finish_sketch() {
  const auto result = sketch_workspace_.finish(session_, sketch_workbench_);
  if (result.has_error()) {
    show_error(result.error());
    return;
  }
  viewport_->clear_sketch_focus();
  if (sketch_camera_bookmark_ && !viewport_->restore_camera_bookmark(*sketch_camera_bookmark_))
    show_error(Error::geometry("gui.sketch_workspace", "could not restore the pre-Sketch camera"));
  viewport_->set_selection_filter_mask(sketch_selection_filter_mask_);
  viewport_->unsetCursor();
  perspective_action_->setChecked(viewport_->projection() == GuiViewportProjection::Perspective);
  active_sketch_.reset();
  sketch_camera_bookmark_.reset();
  synchronize_workspace_tab();
  refresh_command_state();
}

void MainWindow::add_numeric_sketch_line() {
  if (!active_sketch_ || !sketch_workspace_.active()) {
    property_validation_->setText(QStringLiteral("Enter a planar Sketch before adding geometry."));
    return;
  }
  if (!sketch_workspace_.begin_command(session_, "sketch.line", "Pick line points or enter coordinates") ||
      !sketch_workspace_.begin_numeric_input(session_)) {
    property_validation_->setText(QStringLiteral("Finish or cancel the current Sketch command first."));
    return;
  }
  sketch_numeric_hud_->setText(QStringLiteral("line.1, 0, 0, 10, 0"));
  sketch_numeric_hud_->selectAll();
  sketch_numeric_hud_->setFocus(Qt::ShortcutFocusReason);
  refresh_command_state();
}

void MainWindow::commit_numeric_sketch_line() {
  if (!active_sketch_ || sketch_workspace_.stage() != GuiSketchInteractionStage::NumericInput)
    return;
  QString input = sketch_numeric_hud_->text().trimmed();
  input.replace(QLatin1Char(','), QLatin1Char(' '));
  std::istringstream stream(input.toStdString());
  std::string id;
  double x1{}, y1{}, x2{}, y2{};
  if (!(stream >> id >> x1 >> y1 >> x2 >> y2)) {
    property_validation_->setText(QStringLiteral("Expected: id, x1, y1, x2, y2"));
    return;
  }
  auto line = LineSegment::create(SketchEntityId(id), {x1, y1}, {x2, y2});
  if (line.has_error()) {
    property_validation_->setText(QString::fromStdString(line.error().message()));
    return;
  }
  const auto* existing = session_.part_document()->find_sketch(*active_sketch_);
  if (!existing) {
    property_validation_->setText(QStringLiteral("The active Sketch no longer exists."));
    return;
  }
  Sketch candidate = *existing;
  const auto candidate_result = candidate.add_entity(line.value());
  if (candidate_result.has_error()) {
    property_validation_->setText(QString::fromStdString(candidate_result.error().message()));
    return;
  }
  (void)sketch_workspace_.set_numeric_input(sketch_numeric_hud_->text().toStdString());
  if (!sketch_workspace_.show_preview(session_) || !sketch_workspace_.commit_command(session_)) {
    property_validation_->setText(QStringLiteral("Sketch line preview could not be committed."));
    return;
  }
  const auto result = sketch_workbench_.add_line(session_, *active_sketch_, std::move(line.value()));
  if (result.has_error())
    show_error(result.error());
  refresh_command_state();
}

void MainWindow::repeat_last_sketch_command() {
  if (!sketch_workspace_.repeat_last_command(session_))
    return;
  if (sketch_workspace_.active_command() == "sketch.line") {
    if (!sketch_workspace_.begin_numeric_input(session_)) {
      (void)sketch_workspace_.escape(session_);
      return;
    }
    sketch_numeric_hud_->setText(QStringLiteral("line.1, 0, 0, 10, 0"));
    sketch_numeric_hud_->selectAll();
    sketch_numeric_hud_->setFocus(Qt::ShortcutFocusReason);
  }
  refresh_command_state();
}

void MainWindow::inspect_selected_sketch() {
  const auto* node = browser_model_.find(selected_browser_id_);
  const SketchId id = node && node->kind == GuiBrowserNodeKind::Sketch
                          ? SketchId(node->semantic_id)
                          : active_sketch_.value_or(SketchId{});
  const auto inspection = sketch_workbench_.inspect(session_, id);
  if (inspection.has_error()) {
    show_error(inspection.error());
    return;
  }
  QStringList lines;
  lines << QStringLiteral("Sketch %1: %2 entities, %3 references, %4 constraints, %5 dimensions, %6 profiles")
               .arg(QString::fromStdString(id.value()))
               .arg(inspection.value().explicit_entity_count)
               .arg(inspection.value().projected_reference_count)
               .arg(inspection.value().constraint_count)
               .arg(inspection.value().dimension_count)
               .arg(inspection.value().profile_count);
  lines << QStringLiteral("Region: %1").arg(QString::fromStdString(inspection.value().region_message));
  for (const auto& diagnostic : inspection.value().diagnostics.diagnostics())
    lines << QStringLiteral("[%1] %2: %3")
                 .arg(QString::fromStdString(std::string(to_string(diagnostic.severity()))),
                      QString::fromStdString(diagnostic.target()),
                      QString::fromStdString(diagnostic.message()));
  diagnostics_->setPlainText(lines.join(QLatin1Char('\n')));
}

void MainWindow::repair_selected_sketch() {
  const auto* node = browser_model_.find(selected_browser_id_);
  const SketchId id = node && node->kind == GuiBrowserNodeKind::Sketch
                          ? SketchId(node->semantic_id)
                          : active_sketch_.value_or(SketchId{});
  auto inspection = sketch_workbench_.inspect(session_, id);
  if (inspection.has_error()) {
    show_error(inspection.error());
    return;
  }
  if (inspection.value().repairs.empty()) {
    QMessageBox::information(this, QStringLiteral("Sketch repair"),
                             QStringLiteral("No repair suggestion is available."));
    return;
  }
  const auto& suggestion = inspection.value().repairs.suggestions().front();
  auto preview = sketch_workbench_.preview_repair(session_, id, suggestion);
  if (preview.has_error()) {
    show_error(preview.error());
    return;
  }
  const auto answer = QMessageBox::question(
      this, QStringLiteral("Preview sketch repair"),
      QStringLiteral("%1\n\nPreview: %2\nApply as one undoable transaction?")
          .arg(QString::fromStdString(suggestion.message()),
               QString::fromStdString(preview.value().message())),
      QMessageBox::Apply | QMessageBox::Cancel, QMessageBox::Cancel);
  if (answer != QMessageBox::Apply) return;
  const auto applied = sketch_workbench_.apply_repair(session_, id, suggestion);
  if (applied.has_error()) show_error(applied.error());
  refresh_command_state();
}

void MainWindow::refresh_sketch_workspace_ui() {
  const bool active = sketch_workspace_.active();
  if (sketch_command_groups_action_)
    sketch_command_groups_action_->setVisible(active);
  if (sketch_command_groups_)
    sketch_command_groups_->setVisible(active);
  const bool numeric_hud_visible =
      active && sketch_workspace_.stage() == GuiSketchInteractionStage::NumericInput;
  if (sketch_numeric_hud_action_)
    sketch_numeric_hud_action_->setVisible(numeric_hud_visible);
  if (sketch_numeric_hud_)
    sketch_numeric_hud_->setVisible(numeric_hud_visible);
  for (QLabel* label : {sketch_cursor_status_, sketch_snap_status_, sketch_dof_status_, sketch_solve_status_})
    if (label)
      label->setVisible(active);
  if (!active)
    return;

  const auto& status = sketch_workspace_.status();
  if (status.cursor_coordinates) {
    sketch_cursor_status_->setText(
        QStringLiteral("Cursor: %1, %2 mm")
            .arg(status.cursor_coordinates->x, 0, 'f', 2)
            .arg(status.cursor_coordinates->y, 0, 'f', 2));
  } else {
    sketch_cursor_status_->setText(QStringLiteral("Cursor: —"));
  }
  sketch_snap_status_->setText(status.snap_inference.empty()
                                   ? QStringLiteral("Snap: —")
                                   : QStringLiteral("Snap: %1").arg(QString::fromStdString(status.snap_inference)));
  sketch_dof_status_->setText(status.remaining_dof
                                  ? QStringLiteral("DOF: %1").arg(*status.remaining_dof)
                                  : QStringLiteral("DOF: —"));
  sketch_solve_status_->setText(
      QStringLiteral("Solve: %1").arg(QString::fromStdString(status.solve_status)));
  if (sketch_workspace_.stage() == GuiSketchInteractionStage::NumericInput) {
    sketch_numeric_hud_->setFocus(Qt::OtherFocusReason);
    (void)sketch_workspace_.set_numeric_input(sketch_numeric_hud_->text().toStdString());
  }
}

void MainWindow::refresh_viewport_scene() {
  if (viewport_ == nullptr || viewport_revision_ == session_.presentation_revision())
    return;
  viewport_revision_ = session_.presentation_revision();
  if (!session_.has_document()) {
    viewport_->clear_scene();
    return;
  }

  const geometry::ViewportSceneBuilder builder;
  Result<std::vector<geometry::ViewportSceneItem>> scene =
      session_.part_document() != nullptr && session_.part_shape_cache() != nullptr
          ? builder.build_part(*session_.part_document(), *session_.part_shape_cache())
          : builder.build_project(*session_.project());
  if (scene.has_error()) {
    diagnostics_->append(
        QStringLiteral("[%1] %2: %3")
            .arg(QString::fromStdString(std::string(to_string(scene.error().category()))),
                 QString::fromStdString(scene.error().object_id()),
                 QString::fromStdString(scene.error().message())));
    return;
  }
  const auto published = viewport_->set_scene(std::move(scene.value()));
  if (published.has_error()) {
    diagnostics_->append(
        QStringLiteral("[%1] %2: %3")
            .arg(QString::fromStdString(std::string(to_string(published.error().category()))),
                 QString::fromStdString(published.error().object_id()),
                 QString::fromStdString(published.error().message())));
  }
}

bool MainWindow::confirm_discard_dirty() {
  if (!session_.dirty())
    return true;
  const auto answer =
      QMessageBox::question(this, QStringLiteral("Unsaved changes"),
                            QStringLiteral("Discard the unsaved changes in %1?")
                                .arg(QString::fromStdString(std::string(session_.display_name()))),
                            QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
  return answer == QMessageBox::Discard;
}

void MainWindow::create_new_document(GuiDocumentKind kind) {
  if (!confirm_discard_dirty())
    return;
  bool accepted = false;
  const QString name =
      QInputDialog::getText(this, QStringLiteral("New document"), QStringLiteral("Name:"),
                            QLineEdit::Normal, {}, &accepted)
          .trimmed();
  if (!accepted || name.isEmpty())
    return;
  const QString id = document_id_from_name(
      name, kind == GuiDocumentKind::Part ? QStringView(u"part.") : QStringView(u"project."));
  const auto result =
      kind == GuiDocumentKind::Part
          ? session_.create_part(DocumentId(id.toStdString()), name.toStdString(), true)
          : session_.create_project(DocumentId(id.toStdString()), name.toStdString(), true);
  if (result.has_error())
    show_error(result.error());
  synchronize_workspace_tab();
  refresh_command_state();
}

void MainWindow::open_document() {
  if (!confirm_discard_dirty())
    return;
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open BLCAD document"), {},
      QStringLiteral("BLCAD documents (*.blcad.json *.blcad.project.json);;JSON (*.json)"));
  if (path.isEmpty())
    return;
  const auto result = session_.open_file(path.toStdString(), true);
  if (result.has_error())
    show_error(result.error());
  synchronize_workspace_tab();
  refresh_command_state();
}

void MainWindow::save_document(bool force_path) {
  std::filesystem::path path = session_.file_path();
  if (force_path || path.empty()) {
    const QString selected =
        QFileDialog::getSaveFileName(this, QStringLiteral("Save BLCAD document"), {},
                                     session_.document_kind() == GuiDocumentKind::Project
                                         ? QStringLiteral("BLCAD Project (*.blcad.project.json)")
                                         : QStringLiteral("BLCAD Part (*.blcad.json)"));
    if (selected.isEmpty())
      return;
    path = selected.toStdString();
  }
  const auto result =
      force_path || session_.file_path().empty() ? session_.save_as(path) : session_.save();
  if (result.has_error())
    show_error(result.error());
  refresh_command_state();
}

void MainWindow::show_error(const Error& error) {
  QMessageBox::critical(this, QStringLiteral("BLCAD error"),
                        QStringLiteral("%1: %2").arg(QString::fromStdString(error.object_id()),
                                                     QString::fromStdString(error.message())));
}

} // namespace blcad::gui
