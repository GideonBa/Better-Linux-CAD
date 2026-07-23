#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_origin.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <string>
#include <utility>

namespace blcad::gui {
namespace {

constexpr int kSemanticIdRole = Qt::UserRole;
constexpr int kViewportIdRole = Qt::UserRole + 1;

QString sanitized_document_id(const QString& name, QStringView prefix) {
  QString id;
  for (const QChar character : name.trimmed().toLower()) {
    if (character.isLetterOrNumber())
      id.append(character);
    else if (!id.isEmpty() && !id.endsWith(QLatin1Char('.')))
      id.append(QLatin1Char('.'));
  }
  while (id.endsWith(QLatin1Char('.')))
    id.chop(1);
  if (id.isEmpty())
    id = QStringLiteral("dokument");
  return prefix.toString() + id;
}

QString diagnostic_line(const Error& error) {
  return QStringLiteral("[%1] %2: %3")
      .arg(QString::fromStdString(std::string(to_string(error.category()))),
           QString::fromStdString(error.object_id()),
           QString::fromStdString(error.message()));
}

QTreeWidgetItem* append_browser_item(QTreeWidgetItem* parent, QTreeWidget* tree,
                                     const GuiBrowserNode& node) {
  auto* item = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
  item->setText(0, QString::fromStdString(node.label));
  item->setData(0, kSemanticIdRole, QString::fromStdString(node.semantic_id));
  item->setData(0, kViewportIdRole, QString::fromStdString(node.viewport_semantic_id));
  for (const auto& child : node.children)
    append_browser_item(item, tree, child);
  return item;
}

QTreeWidgetItem* find_item_by_viewport_id(QTreeWidgetItem* item, const QString& id) {
  if (item->data(0, kViewportIdRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item_by_viewport_id(item->child(index), id))
      return found;
  return nullptr;
}

} // namespace

ShellWindow::ShellWindow() {
  setWindowTitle(QStringLiteral("BLCAD"));
  resize(1440, 900);

  auto* central = new QWidget(this);
  auto* column = new QVBoxLayout(central);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  ribbon_ = new ShellRibbon(central);
  viewport_ = new OcctViewport(central);
  column->addWidget(ribbon_);
  column->addWidget(viewport_, 1);
  setCentralWidget(central);

  build_ribbon();
  build_docks();
  wire_viewport();
  sketch_tools_ = std::make_unique<ShellSketchTools>(*this, session_, sketch_workbench_,
                                                     sketch_workspace_, *viewport_, *ribbon_,
                                                     tab_sketch_);
  statusBar()->showMessage(QStringLiteral("Bereit"));
  refresh();
}

ShellWindow::~ShellWindow() = default;

void ShellWindow::build_ribbon() {
  tab_model_ = ribbon_->add_tab(QStringLiteral("3D-Modell"));
  tab_sketch_ = ribbon_->add_tab(QStringLiteral("Skizze"));
  tab_view_ = ribbon_->add_tab(QStringLiteral("Ansicht"));

  // 3D-Modell → Dokument
  auto* document_group = ribbon_->add_group(tab_model_, QStringLiteral("Dokument"));
  action_new_part_ = new QAction(QStringLiteral("Neu"), this);
  connect(action_new_part_, &QAction::triggered, this, [this] {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Neues Bauteil"),
                                               QStringLiteral("Name:"), QLineEdit::Normal,
                                               QStringLiteral("Bauteil"), &accepted)
                             .trimmed();
    if (accepted && !name.isEmpty())
      (void)new_part(name);
  });
  action_open_ = new QAction(QStringLiteral("Öffnen"), this);
  connect(action_open_, &QAction::triggered, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("BLCAD-Dokument öffnen"), {},
        QStringLiteral("BLCAD-Dokumente (*.blcad.json *.blcad.project.json);;JSON (*.json)"));
    if (!path.isEmpty())
      (void)open_path(path);
  });
  action_save_ = new QAction(QStringLiteral("Speichern"), this);
  connect(action_save_, &QAction::triggered, this, [this] { (void)save_to({}); });
  action_save_as_ = new QAction(QStringLiteral("Speichern unter"), this);
  connect(action_save_as_, &QAction::triggered, this, [this] {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("BLCAD-Dokument speichern"), {},
        session_.document_kind() == GuiDocumentKind::Project
            ? QStringLiteral("BLCAD-Projekt (*.blcad.project.json)")
            : QStringLiteral("BLCAD-Bauteil (*.blcad.json)"));
    if (!path.isEmpty())
      (void)save_to(path);
  });
  ribbon_->add_button(document_group, action_new_part_);
  ribbon_->add_button(document_group, action_open_);
  ribbon_->add_button(document_group, action_save_);
  ribbon_->add_button(document_group, action_save_as_);

  // 3D-Modell → Bearbeiten (undo/redo, always available via shortcut)
  auto* edit_group = ribbon_->add_group(tab_model_, QStringLiteral("Bearbeiten"));
  action_undo_ = new QAction(QStringLiteral("Rückgängig"), this);
  action_undo_->setShortcut(QKeySequence::Undo);
  action_undo_->setShortcutContext(Qt::WindowShortcut);
  connect(action_undo_, &QAction::triggered, this, [this] { (void)undo(); });
  action_redo_ = new QAction(QStringLiteral("Wiederholen"), this);
  action_redo_->setShortcut(QKeySequence::Redo);
  action_redo_->setShortcutContext(Qt::WindowShortcut);
  connect(action_redo_, &QAction::triggered, this, [this] { (void)redo(); });
  ribbon_->add_button(edit_group, action_undo_);
  ribbon_->add_button(edit_group, action_redo_);
  // Shortcuts must fire regardless of which ribbon tab is visible.
  addAction(action_undo_);
  addAction(action_redo_);

  // 3D-Modell → Skizze
  auto* sketch_group = ribbon_->add_group(tab_model_, QStringLiteral("Skizze"));
  action_create_sketch_ = new QAction(QStringLiteral("2D-Skizze erstellen"), this);
  action_create_sketch_->setToolTip(
      QStringLiteral("Ursprungsebene wählen, dann Skizze auf dieser Ebene beginnen"));
  connect(action_create_sketch_, &QAction::triggered, this,
          [this] { (void)create_sketch_on_selection(); });
  ribbon_->add_button(sketch_group, action_create_sketch_);

  // Skizze → Beenden (draw/constrain/dimension groups arrive with Block 134)
  auto* finish_group = ribbon_->add_group(tab_sketch_, QStringLiteral("Beenden"));
  action_finish_sketch_ = new QAction(QStringLiteral("Skizze fertig stellen"), this);
  connect(action_finish_sketch_, &QAction::triggered, this,
          [this] { (void)finish_active_sketch(); });
  ribbon_->add_button(finish_group, action_finish_sketch_);
  ribbon_->set_tab_enabled(tab_sketch_, false);

  // Ansicht → Navigation
  auto* navigate_group = ribbon_->add_group(tab_view_, QStringLiteral("Navigation"));
  action_fit_ = new QAction(QStringLiteral("Einpassen"), this);
  connect(action_fit_, &QAction::triggered, this, [this] { viewport_->fit_all(); });
  ribbon_->add_button(navigate_group, action_fit_);
  const struct {
    const char* label;
    GuiStandardView view;
  } standard_views[] = {{"Isometrisch", GuiStandardView::Isometric},
                        {"Vorne", GuiStandardView::Front},
                        {"Oben", GuiStandardView::Top},
                        {"Rechts", GuiStandardView::Right}};
  for (const auto& definition : standard_views) {
    auto* action = new QAction(QString::fromUtf8(definition.label), this);
    const GuiStandardView view = definition.view;
    connect(action, &QAction::triggered, this, [this, view] { viewport_->set_standard_view(view); });
    ribbon_->add_button(navigate_group, action);
  }

  // Ansicht → Darstellung
  auto* display_group = ribbon_->add_group(tab_view_, QStringLiteral("Darstellung"));
  action_perspective_ = new QAction(QStringLiteral("Perspektive"), this);
  action_perspective_->setCheckable(true);
  action_perspective_->setChecked(true);
  connect(action_perspective_, &QAction::toggled, this, [this](bool checked) {
    viewport_->set_projection(checked ? GuiViewportProjection::Perspective
                                      : GuiViewportProjection::Orthographic);
  });
  ribbon_->add_button(display_group, action_perspective_);
  auto* display_modes = new QActionGroup(this);
  display_modes->setExclusive(true);
  const struct {
    const char* label;
    GuiViewportDisplayMode mode;
    QAction** slot;
  } modes[] = {{"Schattiert", GuiViewportDisplayMode::Shaded, &action_shaded_},
               {"Kanten", GuiViewportDisplayMode::ShadedWithEdges, &action_shaded_edges_},
               {"Drahtgitter", GuiViewportDisplayMode::Wireframe, &action_wireframe_}};
  for (const auto& definition : modes) {
    auto* action = new QAction(QString::fromUtf8(definition.label), this);
    action->setCheckable(true);
    display_modes->addAction(action);
    const GuiViewportDisplayMode mode = definition.mode;
    connect(action, &QAction::triggered, this, [this, mode] { viewport_->set_display_mode(mode); });
    ribbon_->add_button(display_group, action);
    *definition.slot = action;
  }
  action_shaded_edges_->setChecked(true);

  ribbon_->set_current_tab(tab_model_);
}

void ShellWindow::build_docks() {
  auto* browser_dock = new QDockWidget(QStringLiteral("Modell"), this);
  browser_dock->setFeatures(QDockWidget::DockWidgetMovable);
  browser_tree_ = new QTreeWidget(browser_dock);
  browser_tree_->setHeaderLabel(QStringLiteral("Kein Dokument"));
  browser_tree_->setMinimumWidth(220);
  browser_tree_->header()->setStretchLastSection(true);
  browser_dock->setWidget(browser_tree_);
  addDockWidget(Qt::LeftDockWidgetArea, browser_dock);
  resizeDocks({browser_dock}, {300}, Qt::Horizontal);
  connect(browser_tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem*) { handle_tree_selection(current); });

  auto* diagnostics_dock = new QDockWidget(QStringLiteral("Diagnose"), this);
  diagnostics_dock->setFeatures(QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetClosable);
  diagnostics_ = new QPlainTextEdit(diagnostics_dock);
  diagnostics_->setReadOnly(true);
  diagnostics_->setPlaceholderText(QStringLiteral("Diagnosen erscheinen hier"));
  diagnostics_dock->setWidget(diagnostics_);
  addDockWidget(Qt::BottomDockWidgetArea, diagnostics_dock);
}

void ShellWindow::wire_viewport() {
  viewport_->set_selection_callback(
      [this](std::optional<GuiSelection> selection) { apply_viewport_selection(selection); });
}

bool ShellWindow::new_part(const QString& name) {
  if (session_.dirty()) {
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Ungespeicherte Änderungen"),
        QStringLiteral("Änderungen in %1 verwerfen?")
            .arg(QString::fromStdString(std::string(session_.display_name()))),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Discard)
      return false;
  }
  const QString id = sanitized_document_id(name, u"part.");
  const auto created = session_.create_part(DocumentId(id.toStdString()), name.toStdString(), true);
  if (created.has_error()) {
    show_error(created.error());
    refresh();
    return false;
  }
  const auto seeded = seed_origin_geometry(session_);
  if (seeded.has_error())
    show_error(seeded.error());
  refresh();
  viewport_->fit_all();
  return !seeded.has_error();
}

bool ShellWindow::open_path(const QString& path) {
  const auto opened = session_.open_file(path.toStdString(), true);
  if (opened.has_error()) {
    show_error(opened.error());
    refresh();
    return false;
  }
  refresh();
  viewport_->fit_all();
  return true;
}

bool ShellWindow::save_to(const QString& path) {
  const auto saved =
      path.isEmpty() ? session_.save() : session_.save_as(path.toStdString());
  if (saved.has_error()) {
    show_error(saved.error());
    refresh();
    return false;
  }
  refresh();
  return true;
}

std::optional<DatumPlaneId> ShellWindow::selected_datum_plane() const {
  constexpr std::string_view kPrefix = "datum-plane/";
  if (selected_semantic_id_.rfind(kPrefix, 0) != 0)
    return std::nullopt;
  return DatumPlaneId(selected_semantic_id_.substr(kPrefix.size()));
}

bool ShellWindow::create_sketch_on_selection() {
  const auto* part = session_.part_document();
  const auto plane = selected_datum_plane();
  if (active_sketch_ || part == nullptr || !plane) {
    statusBar()->showMessage(
        QStringLiteral("Zuerst eine Ursprungsebene wählen, dann 2D-Skizze erstellen"));
    return false;
  }
  std::size_t number = 1;
  while (part->find_sketch(SketchId("sketch.skizze" + std::to_string(number))) != nullptr)
    ++number;
  auto sketch = Sketch::create(SketchId("sketch.skizze" + std::to_string(number)),
                               "Skizze" + std::to_string(number), *plane);
  if (sketch.has_error()) {
    show_error(sketch.error());
    return false;
  }
  const SketchId sketch_id = sketch.value().id();
  const auto created = sketch_workbench_.create_sketch(session_, std::move(sketch.value()));
  if (created.has_error()) {
    show_error(created.error());
    refresh();
    return false;
  }
  return enter_sketch(sketch_id);
}

bool ShellWindow::enter_sketch(SketchId sketch) {
  sketch_camera_bookmark_ = viewport_->camera_bookmark();
  model_selection_filter_mask_ = viewport_->selection_filter_mask();
  const auto plane = sketch_workspace_.enter(session_, sketch_workbench_, sketch);
  if (plane.has_error()) {
    show_error(plane.error());
    sketch_camera_bookmark_.reset();
    refresh();
    return false;
  }
  if (!viewport_->set_plane_camera(plane.value().origin, plane.value().normal,
                                   plane.value().y_axis)) {
    (void)sketch_workspace_.finish(session_, sketch_workbench_);
    sketch_camera_bookmark_.reset();
    show_error(Error::geometry(sketch.value(), "could not orient camera normal to sketch plane"));
    refresh();
    return false;
  }
  active_sketch_ = sketch_workspace_.active_sketch();
  viewport_->set_projection(GuiViewportProjection::Orthographic);
  action_perspective_->setChecked(false);
  viewport_->set_selection_filter_mask(GuiSketchWorkspace::selection_filter_mask());
  viewport_->set_sketch_focus(active_sketch_->value(), GuiSketchSurroundingsMode::Dim);
  // Full sketch tool surface (Block 134): scene with topology, glyphs, and
  // annotations, drag handles, and armed ribbon tools.
  sketch_tools_->activate();
  ribbon_->set_tab_enabled(tab_sketch_, true);
  ribbon_->set_current_tab(tab_sketch_);
  refresh();
  return true;
}

bool ShellWindow::finish_active_sketch() {
  if (!active_sketch_)
    return false;
  const auto finished = sketch_workspace_.finish(session_, sketch_workbench_);
  if (finished.has_error()) {
    show_error(finished.error());
    return false;
  }
  sketch_tools_->deactivate();
  viewport_->clear_sketch_interaction();
  viewport_->clear_sketch_focus();
  if (sketch_camera_bookmark_ && !viewport_->restore_camera_bookmark(*sketch_camera_bookmark_))
    append_diagnostic(Error::geometry("gui.shell", "could not restore the pre-sketch camera"));
  viewport_->set_selection_filter_mask(model_selection_filter_mask_);
  viewport_->unsetCursor();
  action_perspective_->setChecked(viewport_->projection() == GuiViewportProjection::Perspective);
  active_sketch_.reset();
  sketch_camera_bookmark_.reset();
  ribbon_->set_tab_enabled(tab_sketch_, false);
  ribbon_->set_current_tab(tab_model_);
  refresh();
  return true;
}

bool ShellWindow::undo() {
  if (!session_.can_undo())
    return false;
  const auto result = session_.undo();
  if (result.has_error()) {
    show_error(result.error());
    return false;
  }
  // The document changed underneath any active sketch tools; resync the scene
  // and drag handles.
  if (active_sketch_ && sketch_tools_)
    sketch_tools_->activate();
  refresh();
  return true;
}

bool ShellWindow::redo() {
  if (!session_.can_redo())
    return false;
  const auto result = session_.redo();
  if (result.has_error()) {
    show_error(result.error());
    return false;
  }
  if (active_sketch_ && sketch_tools_)
    sketch_tools_->activate();
  refresh();
  return true;
}

void ShellWindow::apply_viewport_selection(const std::optional<GuiSelection>& selection) {
  if (synchronizing_selection_)
    return;
  session_.selection().clear();
  selected_semantic_id_.clear();
  if (selection) {
    (void)session_.selection().add(*selection);
    selected_semantic_id_ = selection->semantic_id;
  }
  synchronize_tree_selection(selected_semantic_id_);
  refresh();
}

void ShellWindow::handle_tree_selection(QTreeWidgetItem* current) {
  if (synchronizing_selection_)
    return;
  session_.selection().clear();
  selected_semantic_id_.clear();
  if (current != nullptr) {
    const std::string viewport_id = current->data(0, kViewportIdRole).toString().toStdString();
    selected_semantic_id_ = viewport_id;
    synchronizing_selection_ = true;
    if (viewport_id.empty() || !viewport_->select_semantic(viewport_id))
      viewport_->clear_selection();
    synchronizing_selection_ = false;
    if (const auto& selected = viewport_->selected_semantic())
      (void)session_.selection().add(*selected);
  } else {
    synchronizing_selection_ = true;
    viewport_->clear_selection();
    synchronizing_selection_ = false;
  }
  refresh();
}

void ShellWindow::synchronize_tree_selection(const std::string& viewport_semantic_id) {
  synchronizing_selection_ = true;
  QTreeWidgetItem* found = nullptr;
  const QString id = QString::fromStdString(viewport_semantic_id);
  if (!id.isEmpty())
    for (int index = 0; index < browser_tree_->topLevelItemCount() && found == nullptr; ++index)
      found = find_item_by_viewport_id(browser_tree_->topLevelItem(index), id);
  browser_tree_->setCurrentItem(found);
  synchronizing_selection_ = false;
}

void ShellWindow::refresh() {
  const bool has_document = session_.has_document();
  const bool sketch_active = active_sketch_.has_value();
  action_save_->setEnabled(has_document);
  action_save_as_->setEnabled(has_document);
  action_undo_->setEnabled(session_.can_undo());
  action_redo_->setEnabled(session_.can_redo());
  action_create_sketch_->setEnabled(has_document && !sketch_active &&
                                    selected_datum_plane().has_value());
  action_finish_sketch_->setEnabled(sketch_active);

  const QString name = has_document
                           ? QString::fromStdString(std::string(session_.display_name()))
                           : QString();
  setWindowTitle(has_document
                     ? QStringLiteral("%1%2 — BLCAD")
                           .arg(name, session_.dirty() ? QStringLiteral(" *") : QString())
                     : QStringLiteral("BLCAD"));

  QStringList lines;
  for (const auto& diagnostic : session_.diagnostics())
    lines.push_back(QStringLiteral("[%1] %2: %3")
                        .arg(QString::fromStdString(std::string(to_string(diagnostic.category))),
                             QString::fromStdString(diagnostic.object_id),
                             QString::fromStdString(diagnostic.message)));
  diagnostics_->setPlainText(lines.join(QLatin1Char('\n')));

  refresh_browser();
  refresh_viewport_scene();
  if (sketch_tools_)
    sketch_tools_->refresh_enablement();
}

void ShellWindow::report_error(const Error& error) {
  show_error(error);
}

void ShellWindow::refresh_browser() {
  if (browser_revision_ == session_.presentation_revision())
    return;
  browser_revision_ = session_.presentation_revision();
  const std::string retained = selected_semantic_id_;
  browser_model_ = GuiDocumentBrowser::build(session_);
  {
    const QSignalBlocker blocker(browser_tree_);
    browser_tree_->clear();
    browser_tree_->setHeaderLabel(
        session_.has_document()
            ? QString::fromStdString(std::string(session_.display_name()))
            : QStringLiteral("Kein Dokument"));
    for (const auto& root : browser_model_.roots()) {
      auto* item = append_browser_item(nullptr, browser_tree_, root);
      item->setExpanded(true);
      for (int child = 0; child < item->childCount(); ++child)
        item->child(child)->setExpanded(true);
    }
  }
  if (!retained.empty())
    synchronize_tree_selection(retained);
}

void ShellWindow::refresh_viewport_scene() {
  if (viewport_revision_ == session_.presentation_revision())
    return;
  viewport_revision_ = session_.presentation_revision();
  if (!session_.has_document()) {
    viewport_->clear_scene();
    return;
  }
  const geometry::ViewportSceneBuilder builder;
  auto scene = session_.part_document() != nullptr && session_.part_shape_cache() != nullptr
                   ? builder.build_part(*session_.part_document(), *session_.part_shape_cache())
                   : (session_.project() != nullptr
                          ? builder.build_project(*session_.project())
                          : Result<std::vector<geometry::ViewportSceneItem>>::success({}));
  if (scene.has_error()) {
    append_diagnostic(scene.error());
    return;
  }
  const auto published = viewport_->set_scene(std::move(scene.value()));
  if (published.has_error())
    append_diagnostic(published.error());
}

void ShellWindow::append_diagnostic(const Error& error) {
  diagnostics_->appendPlainText(diagnostic_line(error));
}

void ShellWindow::show_error(const Error& error) {
  append_diagnostic(error);
  statusBar()->showMessage(QStringLiteral("%1: %2").arg(
      QString::fromStdString(error.object_id()), QString::fromStdString(error.message())));
}

} // namespace blcad::gui
