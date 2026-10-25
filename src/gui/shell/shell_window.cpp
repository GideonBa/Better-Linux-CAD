#include "blcad/gui/shell/shell_window.hpp"

#include "blcad/geometry/sketch_finish.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/gui_interactive_profile_materialization.hpp"
#include "blcad/gui/gui_sketch_intent_remap.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_construction_tools.hpp"
#include "blcad/gui/shell/shell_context_tools.hpp"
#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/gui/shell/shell_feature_tools.hpp"
#include "blcad/gui/shell/shell_hole_tools.hpp"
#include "blcad/gui/shell/shell_inspect_tools.hpp"
#include "blcad/gui/shell/shell_measure_tools.hpp"
#include "blcad/gui/shell/shell_navigation_tools.hpp"
#include "blcad/gui/shell/shell_origin.hpp"
#include "blcad/gui/shell/shell_parameter_tools.hpp"
#include "blcad/gui/shell/shell_reference_repair_tools.hpp"
#include "blcad/gui/shell/shell_repeat_tools.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_sketch_3d_tools.hpp"
#include "blcad/gui/shell/shell_sketch_diagnostics_tools.hpp"
#include "blcad/gui/shell/shell_sketch_tools.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

QTreeWidgetItem* find_item_by_semantic_id(QTreeWidgetItem* item, const QString& id) {
  if (item->data(0, kSemanticIdRole).toString() == id)
    return item;
  for (int index = 0; index < item->childCount(); ++index)
    if (auto* found = find_item_by_semantic_id(item->child(index), id))
      return found;
  return nullptr;
}

std::optional<ProfileId> unique_profile(const Sketch& sketch) {
  std::optional<ProfileId> result;
  std::size_t count = 0U;
  const auto visit = [&result, &count](const auto& profiles) {
    for (const auto& profile : profiles) {
      result = profile.id();
      ++count;
    }
  };
  visit(sketch.rectangle_profiles());
  visit(sketch.circle_profiles());
  visit(sketch.closed_profiles());
  visit(sketch.arc_closed_profiles());
  visit(sketch.composite_closed_profiles());
  return count == 1U ? result : std::nullopt;
}

std::optional<ProfileId>
recoverable_profile(const PartDocument& part, const Sketch& sketch,
                    std::optional<SketchEntityId> selected_entity = std::nullopt) {
  // Analyze only curves that are not owned by an existing materialized
  // profile. This lets a later raw contour coexist with older feature-ready
  // geometry without ever duplicating the older profile.
  auto analysis = analyze_interactive_raw_profiles(part, sketch);
  if (analysis.has_error() || !analysis.value().diagnostics.empty())
    return std::nullopt;
  std::vector<ProfileId> matches;
  for (const auto& region : analysis.value().regions) {
    if (!selected_entity.has_value() ||
        std::find(region.source_entities().begin(), region.source_entities().end(),
                  *selected_entity) != region.source_entities().end())
      matches.push_back(region.id);
  }
  return matches.size() == 1U ? std::optional(matches.front()) : std::nullopt;
}

std::optional<std::pair<SketchId, ProfileId>> browser_profile(std::string_view id) {
  constexpr std::string_view prefix = "sketch/";
  if (!id.starts_with(prefix))
    return std::nullopt;
  id.remove_prefix(prefix.size());
  const auto sketch_end = id.find('/');
  if (sketch_end == std::string_view::npos || sketch_end == 0U)
    return std::nullopt;
  const SketchId sketch(std::string(id.substr(0U, sketch_end)));
  id.remove_prefix(sketch_end + 1U);
  constexpr std::string_view families[]{"profile/", "rectangle-profile/", "circle-profile/",
                                        "closed-profile/", "arc-profile/",
                                        "composite-profile/", "circular-hole-pattern/"};
  for (const auto family : families)
    if (id.starts_with(family) && id.size() > family.size())
      return std::pair{sketch, ProfileId(std::string(id.substr(family.size())))};
  return std::nullopt;
}

std::optional<ProfileId> profile_containing_entity(const Sketch& sketch,
                                                   const SketchEntityId& entity) {
  std::vector<ProfileId> matches;
  const auto contains = [&entity](const auto& entities) {
    return std::find(entities.begin(), entities.end(), entity) != entities.end();
  };
  for (const auto& profile : sketch.closed_profiles())
    if (contains(profile.line_segments()))
      matches.push_back(profile.id());
  for (const auto& profile : sketch.arc_closed_profiles())
    if (contains(profile.curve_segments()))
      matches.push_back(profile.id());
  for (const auto& profile : sketch.composite_closed_profiles()) {
    bool found = contains(profile.outer_contour());
    for (const auto& contour : profile.inner_contours())
      found = found || contains(contour);
    if (found)
      matches.push_back(profile.id());
  }
  std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
    return left.value() < right.value();
  });
  matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
  return matches.size() == 1U ? std::optional(matches.front()) : std::nullopt;
}

std::optional<std::pair<SketchId, ProfileId>>
feature_profile_for_selection(const PartDocument& part, std::string_view semantic_id) {
  if (const auto explicit_profile = browser_profile(semantic_id))
    return explicit_profile;

  const SketchId direct_sketch{std::string(semantic_id)};
  if (const auto* sketch = part.find_sketch(direct_sketch)) {
    if (const auto profile = unique_profile(*sketch))
      return std::pair{direct_sketch, *profile};
    if (const auto profile = recoverable_profile(part, *sketch))
      return std::pair{direct_sketch, *profile};
  }

  constexpr std::string_view prefix = "sketch/";
  if (!semantic_id.starts_with(prefix))
    return std::nullopt;
  semantic_id.remove_prefix(prefix.size());
  const auto sketch_end = semantic_id.find('/');
  if (sketch_end == std::string_view::npos || sketch_end == 0U)
    return std::nullopt;
  const SketchId sketch_id(std::string(semantic_id.substr(0U, sketch_end)));
  const auto* sketch = part.find_sketch(sketch_id);
  if (sketch == nullptr)
    return std::nullopt;
  semantic_id.remove_prefix(sketch_end + 1U);

  constexpr std::string_view entity_families[]{"entity/", "line/", "arc/", "spline/"};
  for (const auto family : entity_families) {
    if (!semantic_id.starts_with(family) || semantic_id.size() <= family.size())
      continue;
    const SketchEntityId entity(std::string(semantic_id.substr(family.size())));
    if (const auto profile = profile_containing_entity(*sketch, entity))
      return std::pair{sketch_id, *profile};
    if (const auto profile = recoverable_profile(part, *sketch, entity))
      return std::pair{sketch_id, *profile};
    // An explicitly selected entity must belong to the profile we hand to a
    // feature. Do not reinterpret an unrelated raw entity as the Sketch's
    // otherwise unique profile.
    return std::nullopt;
  }

  if (const auto profile = unique_profile(*sketch))
    return std::pair{sketch_id, *profile};
  if (const auto profile = recoverable_profile(part, *sketch))
    return std::pair{sketch_id, *profile};
  return std::nullopt;
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
  statusBar()->addPermanentWidget(new QLabel(QStringLiteral("Auswahl:"), this));
  selection_filter_ = new QComboBox(this);
  selection_filter_->setObjectName(QStringLiteral("blcad.selection.filter"));
  selection_filter_->setAccessibleName(QStringLiteral("Auswahlfilter"));
  selection_filter_->setToolTip(QStringLiteral("Auswahl im Modell und Browser begrenzen"));
  const std::pair<const char*, GuiModelingSelectionFilter> filters[] = {
      {"Alles", GuiModelingSelectionFilter::All},
      {"Profile", GuiModelingSelectionFilter::Profiles},
      {"Bezugselemente", GuiModelingSelectionFilter::Datums},
      {"Flächen", GuiModelingSelectionFilter::Faces},
      {"Kanten", GuiModelingSelectionFilter::Edges},
      {"Körper", GuiModelingSelectionFilter::Bodies},
      {"Komponenten", GuiModelingSelectionFilter::Components},
      {"Baugruppen-Ziele", GuiModelingSelectionFilter::AssemblyTargets}};
  for (const auto& [label, filter] : filters)
    selection_filter_->addItem(QString::fromUtf8(label),
                               GuiModelingWorkspace::selection_filter_mask(filter));
  statusBar()->addPermanentWidget(selection_filter_);
  connect(selection_filter_, &QComboBox::currentIndexChanged, this,
          [this] { change_selection_filter(); });
  wire_viewport();
  sketch_tools_ = std::make_unique<ShellSketchTools>(*this, session_, sketch_workbench_,
                                                     sketch_workspace_, *viewport_, *ribbon_,
                                                     tab_sketch_);
  feature_tools_ = std::make_unique<ShellFeatureTools>(
      *this, session_, sketch_workbench_, *viewport_, *ribbon_, tab_model_);
  measure_tools_ =
      std::make_unique<ShellMeasureTools>(*this, session_, *viewport_, *ribbon_, tab_model_);
  assembly_tools_ =
      std::make_unique<ShellAssemblyTools>(*this, session_, *viewport_, *ribbon_, tab_assembly_);
  feature_edit_tools_ = std::make_unique<ShellFeatureEditTools>(*this);
  inspect_tools_ = std::make_unique<ShellInspectTools>(*this, *ribbon_, tab_model_, tab_assembly_);
  construction_tools_ = std::make_unique<ShellConstructionTools>(*this, *ribbon_, tab_model_);
  reference_repair_tools_ =
      std::make_unique<ShellReferenceRepairTools>(*this, *ribbon_, tab_model_);
  sketch_diagnostics_tools_ =
      std::make_unique<ShellSketchDiagnosticsTools>(*this, *ribbon_, tab_model_, tab_sketch_);
  sketch_3d_tools_ = std::make_unique<ShellSketch3DTools>(*this, *ribbon_, tab_model_);
  parameter_tools_ = std::make_unique<ShellParameterTools>(*this, *ribbon_, tab_model_);
  hole_tools_ = std::make_unique<ShellHoleTools>(*this, *ribbon_, tab_model_);
  context_tools_ = std::make_unique<ShellContextTools>(*this);
  statusBar()->showMessage(QStringLiteral("Bereit"));
  refresh();
}

ShellWindow::~ShellWindow() {
  // Release the viewer while its native top-level drawable and the session
  // still exist. QMainWindow/QWidget teardown otherwise destroys that drawable
  // before deleting the central viewport and its OCCT GL resources.
  viewport_->set_view_changed_callback({});
  navigation_tools_.reset();
  repeat_tools_.reset();
  context_tools_.reset();
  sketch_3d_tools_.reset();
  parameter_tools_.reset();
  hole_tools_.reset();
  sketch_diagnostics_tools_.reset();
  reference_repair_tools_.reset();
  construction_tools_.reset();
  inspect_tools_.reset();
  feature_edit_tools_.reset();
  assembly_tools_.reset();
  measure_tools_.reset();
  feature_tools_.reset();
  sketch_tools_.reset();
  delete centralWidget();
  viewport_ = nullptr;
  ribbon_ = nullptr;
}

void ShellWindow::build_ribbon() {
  tab_model_ = ribbon_->add_tab(QStringLiteral("3D-Modell"));
  tab_sketch_ = ribbon_->add_tab(QStringLiteral("Skizze"));
  tab_view_ = ribbon_->add_tab(QStringLiteral("Ansicht"));
  tab_assembly_ = ribbon_->add_tab(QStringLiteral("Baugruppe"));

  // 3D-Modell → Dokument
  auto* document_group = ribbon_->add_group(tab_model_, QStringLiteral("Dokument"));
  action_new_part_ = new QAction(QStringLiteral("Neu"), this);
  action_new_part_->setObjectName("blcad.action.new_part");
  action_new_part_->setShortcut(QKeySequence::New);
  action_new_part_->setShortcutContext(Qt::WindowShortcut);
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
  action_open_->setObjectName("blcad.action.open");
  action_open_->setShortcut(QKeySequence::Open);
  action_open_->setShortcutContext(Qt::WindowShortcut);
  connect(action_open_, &QAction::triggered, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("BLCAD-Dokument öffnen"), {},
        QStringLiteral("BLCAD-Dokumente (*.blcad.json *.blcad.project.json);;JSON (*.json)"));
    if (!path.isEmpty())
      (void)open_path(path);
  });
  action_open_repair_ = new QAction(QStringLiteral("Zur Reparatur öffnen"), this);
  action_open_repair_->setObjectName("blcad.action.open_repair");
  connect(action_open_repair_, &QAction::triggered, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Bauteil zur Referenzreparatur öffnen"), {},
        QStringLiteral("BLCAD-Bauteile (*.blcad.json);;JSON (*.json)"));
    if (!path.isEmpty())
      (void)open_path(path, GuiDocumentOpenMode::ReferenceRepair);
  });
  action_save_ = new QAction(QStringLiteral("Speichern"), this);
  action_save_->setObjectName("blcad.action.save");
  action_save_->setShortcut(QKeySequence::Save);
  action_save_->setShortcutContext(Qt::WindowShortcut);
  connect(action_save_, &QAction::triggered, this, [this] { (void)save_to({}); });
  action_save_as_ = new QAction(QStringLiteral("Speichern unter"), this);
  action_save_as_->setObjectName("blcad.action.save_as");
  action_save_as_->setShortcut(QKeySequence::SaveAs);
  action_save_as_->setShortcutContext(Qt::WindowShortcut);
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
  ribbon_->add_small_button(document_group, action_open_repair_);
  ribbon_->add_button(document_group, action_save_);
  ribbon_->add_button(document_group, action_save_as_);
  // Keep the standard document shortcuts active regardless of the visible
  // ribbon tab or the widget that currently owns keyboard focus.
  addAction(action_new_part_);
  addAction(action_open_);
  addAction(action_save_);
  addAction(action_save_as_);

  // 3D-Modell → Bearbeiten (undo/redo, always available via shortcut)
  auto* edit_group = ribbon_->add_group(tab_model_, QStringLiteral("Bearbeiten"));
  action_undo_ = new QAction(QStringLiteral("Rückgängig"), this);
  action_undo_->setObjectName("blcad.action.undo");
  action_undo_->setShortcut(QKeySequence::Undo);
  action_undo_->setShortcutContext(Qt::WindowShortcut);
  connect(action_undo_, &QAction::triggered, this, [this] { (void)undo(); });
  action_redo_ = new QAction(QStringLiteral("Wiederherstellen"), this);
  action_redo_->setObjectName("blcad.action.redo");
  action_redo_->setShortcut(QKeySequence::Redo);
  action_redo_->setShortcutContext(Qt::WindowShortcut);
  connect(action_redo_, &QAction::triggered, this, [this] { (void)redo(); });
  ribbon_->add_button(edit_group, action_undo_);
  ribbon_->add_button(edit_group, action_redo_);
  // Shortcuts must fire regardless of which ribbon tab is visible.
  addAction(action_undo_);
  addAction(action_redo_);
  repeat_tools_ = std::make_unique<ShellRepeatTools>(*this, *ribbon_, edit_group);

  // 3D-Modell → Skizze
  auto* sketch_group = ribbon_->add_group(tab_model_, QStringLiteral("Skizze"));
  action_create_sketch_ = new QAction(QStringLiteral("2D-Skizze erstellen"), this);
  action_create_sketch_->setObjectName("blcad.action.create_sketch");
  action_create_sketch_->setToolTip(
      QStringLiteral("Ursprungsebene wählen, dann Skizze auf dieser Ebene beginnen"));
  connect(action_create_sketch_, &QAction::triggered, this,
          [this] { (void)create_sketch_on_selection(); });
  ribbon_->add_button(sketch_group, action_create_sketch_);

  // Skizze → Beenden (draw/constrain/dimension groups arrive with Block 134)
  auto* finish_group = ribbon_->add_group(tab_sketch_, QStringLiteral("Beenden"));
  action_finish_sketch_ = new QAction(QStringLiteral("Skizze fertig stellen"), this);
  action_finish_sketch_->setObjectName("blcad.action.finish_sketch");
  connect(action_finish_sketch_, &QAction::triggered, this,
          [this] { (void)finish_active_sketch(); });
  ribbon_->add_button(finish_group, action_finish_sketch_);
  ribbon_->set_tab_enabled(tab_sketch_, false);

  navigation_tools_ = std::make_unique<ShellNavigationTools>(*this, *ribbon_, tab_view_);

  // Ansicht → Darstellung
  auto* display_group = ribbon_->add_group(tab_view_, QStringLiteral("Darstellung"));
  action_perspective_ = new QAction(QStringLiteral("Perspektive"), this);
  action_perspective_->setObjectName("blcad.action.perspective");
  action_perspective_->setCheckable(true);
  action_perspective_->setChecked(true);
  connect(action_perspective_, &QAction::toggled, this, [this](bool checked) {
    viewport_->set_projection(checked ? GuiViewportProjection::Perspective
                                      : GuiViewportProjection::Orthographic);
    if (feature_tools_)
      feature_tools_->refresh_presentation();
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
  browser_tree_->setObjectName("blcad.shell.browser");
  browser_tree_->setHeaderLabel(QStringLiteral("Kein Dokument"));
  browser_tree_->setMinimumWidth(220);
  browser_tree_->header()->setStretchLastSection(true);
  browser_dock->setWidget(browser_tree_);
  addDockWidget(Qt::LeftDockWidgetArea, browser_dock);
  resizeDocks({browser_dock}, {300}, Qt::Horizontal);
  connect(browser_tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* current, QTreeWidgetItem*) { handle_tree_selection(current); });
  connect(browser_tree_, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int) { handle_tree_activation(item); });

  // Property inspector below the model tree: read-only for most properties,
  // editable for the ones Core contracts support (parameter value/expression).
  auto* property_dock = new QDockWidget(QStringLiteral("Eigenschaften"), this);
  property_dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
  properties_ = new QTableWidget(property_dock);
  properties_->setObjectName("blcad.shell.properties");
  properties_->setColumnCount(2);
  properties_->setHorizontalHeaderLabels(
      {QStringLiteral("Eigenschaft"), QStringLiteral("Wert")});
  properties_->verticalHeader()->setVisible(false);
  properties_->horizontalHeader()->setStretchLastSection(true);
  properties_->setSelectionMode(QAbstractItemView::SingleSelection);
  properties_->setSelectionBehavior(QAbstractItemView::SelectItems);
  properties_->setEditTriggers(QAbstractItemView::DoubleClicked |
                               QAbstractItemView::SelectedClicked |
                               QAbstractItemView::EditKeyPressed);
  properties_->setMinimumWidth(220);
  property_dock->setWidget(properties_);
  splitDockWidget(browser_dock, property_dock, Qt::Vertical);
  connect(properties_, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem* item) { handle_property_changed(item); });

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
  viewport_->set_view_changed_callback([this] {
    synchronize_navigation();
    synchronize_selection_filter();
    if (repeat_tools_)
      repeat_tools_->refresh();
    if (feature_tools_)
      feature_tools_->refresh_presentation();
    if (measure_tools_)
      measure_tools_->refresh_presentation();
    if (assembly_tools_)
      assembly_tools_->refresh_presentation();
    if (feature_edit_tools_)
      feature_edit_tools_->refresh_presentation();
    if (construction_tools_)
      construction_tools_->refresh_presentation();
    if (sketch_3d_tools_)
      sketch_3d_tools_->refresh_presentation();
    refresh_context_toolbar();
  });
  viewport_->set_selection_callback(
      [this](std::optional<GuiSelection> selection) { apply_viewport_selection(selection); });
}

void ShellWindow::synchronize_navigation() {
  if (navigation_tools_)
    navigation_tools_->refresh();
  if (action_perspective_) {
    const QSignalBlocker blocker(action_perspective_);
    action_perspective_->setChecked(viewport_->projection() == GuiViewportProjection::Perspective);
    action_perspective_->setEnabled(session_.has_document() && !active_sketch_ &&
                                    !viewport_->sketch_focus_active() &&
                                    QWidget::mouseGrabber() != viewport_);
  }
}

void ShellWindow::synchronize_selection_filter() {
  if (!selection_filter_)
    return;
  const bool task_owned = session_.task().active() || active_sketch_.has_value();
  const auto previous_mask = session_.selection().filter_mask();
  // Command components own temporary viewport masks/scopes. Let their accepted
  // references reach the session even when the idle model filter excludes them.
  session_.selection().set_filter_mask(task_owned ? 0xFFFFFFFFU : model_selection_filter_mask_);
  selection_filter_->setEnabled(
      session_.has_document() && !task_owned && !session_.reference_repair_mode() &&
      !viewport_->sketch_focus_active() && QWidget::mouseGrabber() != viewport_);
  if (!task_owned && previous_mask != model_selection_filter_mask_ &&
      !session_.selection().allows(GuiSelectionKind::SketchEntity) && feature_tools_)
    feature_tools_->clear_profile_selection();
}

void ShellWindow::change_selection_filter() {
  synchronize_selection_filter();
  if (!selection_filter_->isEnabled()) {
    const QSignalBlocker blocker(selection_filter_);
    selection_filter_->setCurrentIndex(selection_filter_->findData(model_selection_filter_mask_));
    return;
  }
  model_selection_filter_mask_ = selection_filter_->currentData().toUInt();
  session_.selection().set_filter_mask(model_selection_filter_mask_);
  viewport_->set_selection_filter_mask(model_selection_filter_mask_);
  if (!session_.selection().allows(GuiSelectionKind::SketchEntity))
    feature_tools_->clear_profile_selection();
  // A browser-only Finish Sketch handoff has no viewport selection to evict.
  if (session_.selection().empty() &&
      !session_.selection().allows(GuiSelectionKind::SketchEntity)) {
    selected_semantic_id_.clear();
    synchronize_tree_selection({});
  }
  refresh();
}

void ShellWindow::reset_selection_filter() {
  const QSignalBlocker blocker(selection_filter_);
  selection_filter_->setCurrentIndex(0);
  model_selection_filter_mask_ = 0xFFFFFFFFU;
  session_.selection().set_filter_mask(model_selection_filter_mask_);
  viewport_->set_selection_filter_mask(model_selection_filter_mask_);
}

bool ShellWindow::new_part(const QString& name) {
  if (!confirm_document_replacement())
    return false;
  reset_document_interaction();
  const QString id = sanitized_document_id(name, u"part.");
  const auto created = session_.create_part(DocumentId(id.toStdString()), name.toStdString(), true);
  if (created.has_error()) {
    show_error(created.error());
    refresh();
    return false;
  }
  navigation_tools_->reset();
  repeat_tools_->reset();
  context_tools_->reset();
  reset_selection_filter();
  const auto seeded = seed_origin_geometry(session_);
  if (seeded.has_error())
    show_error(seeded.error());
  refresh();
  viewport_->fit_all();
  return !seeded.has_error();
}

bool ShellWindow::new_project(const QString& name) {
  if (!confirm_document_replacement())
    return false;
  reset_document_interaction();
  const auto id = sanitized_document_id(name, u"project.");
  auto created = session_.create_project(DocumentId(id.toStdString()), name.toStdString(), true);
  if (!created) {
    show_error(created.error());
    refresh();
    return false;
  }
  navigation_tools_->reset();
  repeat_tools_->reset();
  context_tools_->reset();
  reset_selection_filter();
  refresh();
  ribbon_->set_current_tab(tab_assembly_);
  return true;
}

bool ShellWindow::open_path(const QString& path, GuiDocumentOpenMode mode) {
  if (!confirm_document_replacement())
    return false;
  reset_document_interaction();
  const auto opened = session_.open_file(path.toStdString(), true, mode);
  if (opened.has_error()) {
    show_error(opened.error());
    refresh();
    return false;
  }
  navigation_tools_->reset();
  repeat_tools_->reset();
  context_tools_->reset();
  reset_selection_filter();
  refresh();
  viewport_->fit_all();
  if (session_.project())
    ribbon_->set_current_tab(tab_assembly_);
  if (mode == GuiDocumentOpenMode::ReferenceRepair) {
    ribbon_->set_current_tab(tab_model_);
    auto begun = reference_repair_tools_->begin();
    if (!begun) {
      show_error(begun.error());
      return false;
    }
    statusBar()->showMessage(
        QStringLiteral("Referenzen prüfen und Reparatur vollständig anwenden."));
  }
  return true;
}

bool ShellWindow::save_to(const QString& path) {
  QString target = path;
  if (target.isEmpty() && session_.file_path().empty()) {
    target = QFileDialog::getSaveFileName(
        this, QStringLiteral("BLCAD-Dokument speichern"), {},
        session_.document_kind() == GuiDocumentKind::Project
            ? QStringLiteral("BLCAD-Projekt (*.blcad.project.json)")
            : QStringLiteral("BLCAD-Bauteil (*.blcad.json)"));
    if (target.isEmpty())
      return false;
  }
  const auto saved = target.isEmpty() ? session_.save()
                                      : session_.save_as(target.toStdString());
  if (saved.has_error()) {
    show_error(saved.error());
    refresh();
    return false;
  }
  refresh();
  return true;
}

bool ShellWindow::confirm_document_replacement() {
  if (!session_.dirty())
    return true;
  const auto answer = QMessageBox::warning(
      this, QStringLiteral("Ungespeicherte Änderungen"),
      QStringLiteral("Änderungen in %1 vor dem Fortfahren speichern?")
          .arg(QString::fromStdString(std::string(session_.display_name()))),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (answer == QMessageBox::Save)
    return save_to({});
  return answer == QMessageBox::Discard;
}

void ShellWindow::reset_document_interaction() {
  if (hole_tools_)
    hole_tools_->cancel();
  if (parameter_tools_)
    parameter_tools_->cancel();
  if (sketch_3d_tools_)
    sketch_3d_tools_->cancel();
  if (sketch_diagnostics_tools_)
    sketch_diagnostics_tools_->cancel();
  if (reference_repair_tools_)
    reference_repair_tools_->cancel();
  if (construction_tools_)
    construction_tools_->cancel();
  if (inspect_tools_)
    inspect_tools_->cancel();
  if (feature_edit_tools_)
    feature_edit_tools_->cancel();
  if (assembly_tools_)
    assembly_tools_->cancel();
  if (measure_tools_)
    measure_tools_->cancel();
  if (feature_tools_)
    feature_tools_->reset();
  if (sketch_tools_)
    sketch_tools_->deactivate();
  if (sketch_workspace_.active()) {
    const auto cancelled = sketch_workspace_.cancel(session_);
    if (cancelled.has_error() && session_.task().active())
      (void)session_.task().cancel();
  } else if (session_.task().active()) {
    (void)session_.task().cancel();
  }

  viewport_->clear_sketch_interaction();
  viewport_->clear_sketch_focus();
  if (sketch_camera_bookmark_ && !viewport_->restore_camera_bookmark(*sketch_camera_bookmark_))
    append_diagnostic(Error::geometry("gui.shell", "could not restore the pre-sketch camera"));
  viewport_->set_selection_filter_mask(model_selection_filter_mask_);
  viewport_->unsetCursor();
  if (action_perspective_ != nullptr)
    action_perspective_->setChecked(viewport_->projection() ==
                                    GuiViewportProjection::Perspective);
  active_sketch_.reset();
  sketch_camera_bookmark_.reset();
  selected_semantic_id_.clear();
  property_semantic_id_.clear();
  session_.selection().clear();
  if (ribbon_ != nullptr) {
    ribbon_->set_tab_enabled(tab_sketch_, false);
    ribbon_->set_current_tab(tab_model_);
  }
}

void ShellWindow::reconcile_active_sketch() {
  if (!active_sketch_)
    return;
  const auto* part = session_.part_document();
  if (part != nullptr && part->find_sketch(*active_sketch_) != nullptr) {
    sketch_tools_->activate();
    return;
  }
  reset_document_interaction();
}

void ShellWindow::closeEvent(QCloseEvent* event) {
  if (!confirm_document_replacement()) {
    event->ignore();
    return;
  }
  reset_document_interaction();
  if (!session_.close(true)) {
    event->ignore();
    return;
  }
  event->accept();
}

std::optional<DatumPlaneId> ShellWindow::selected_datum_plane() const {
  const auto* part = session_.part_document();
  if (!part)
    return std::nullopt;
  for (const std::string_view prefix :
       {"datum-plane/", "construction-plane/", "derived-workplane/"}) {
    if (!selected_semantic_id_.starts_with(prefix))
      continue;
    const DatumPlaneId id(selected_semantic_id_.substr(prefix.size()));
    if (geometry::WorkplaneResolver{}.resolve(*part, id))
      return id;
  }
  return std::nullopt;
}

bool ShellWindow::create_sketch_on_selection() {
  const auto* part = session_.part_document();
  const auto plane = selected_datum_plane();
  if (session_.reference_repair_mode() || active_sketch_ || session_.task().active() ||
      part == nullptr || !plane) {
    statusBar()->showMessage(
        QStringLiteral("Zuerst eine Arbeitsebene wählen, dann 2D-Skizze erstellen"));
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
  if (session_.reference_repair_mode())
    return false;
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
    (void)sketch_workspace_.cancel(session_);
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
  if (sketch_diagnostics_tools_)
    sketch_diagnostics_tools_->cancel();
  if (!active_sketch_)
    return false;
  if (!sketch_tools_->finish_tool())
    return false;
  const SketchId finished_sketch = *active_sketch_;
  bool has_open_contours = false;

  // Creation tools such as Line and Point deliberately re-arm themselves
  // after committing geometry. In that state the document already contains
  // the completed contour, while the workspace owns an empty/incomplete next
  // command. Finish Sketch is itself an explicit request to leave the editor,
  // so discard that pending interaction (and any drag preview) before running
  // the document validation. Otherwise a closed contour drawn with Line could
  // never be finished until the user happened to press Escape first.
  sketch_tools_->deactivate();
  while (session_.task().active() && sketch_workspace_.escape(session_)) {
  }
  const auto reject_finish = [this](const Error& error) {
    sketch_tools_->activate();
    show_error(error);
    return false;
  };

  // Validate every workspace/diagnostic precondition before materializing a
  // profile. This keeps a rejected Finish operation free of document edits.
  const auto valid_finish = sketch_workspace_.validate_finish(session_, sketch_workbench_);
  if (valid_finish.has_error())
    return reject_finish(valid_finish.error());

  // Freehand curve chains do not carry a profile record while they are being
  // authored. Persist every unambiguous bounded region before leaving the
  // workspace. A single region is handed directly to the feature tools;
  // multiple regions remain available for an explicit user selection.
  if (const auto* part = session_.part_document()) {
    const auto* sketch = part->find_sketch(finished_sketch);
    if (sketch != nullptr) {
      // Analyze only curves that are not already owned by a profile. This
      // handles pure freehand contours as well as line regions drawn next to
      // an existing circle/rectangle profile.
      auto analysis = geometry::SketchFinishService::analyze_unmaterialized(*part, *sketch);
      if (analysis.has_error())
        return reject_finish(analysis.error());
      for (const auto& diagnostic : analysis.value().diagnostics) {
        const auto kind = diagnostic.kind;
        if (kind == geometry::SketchRegionDiagnosticKind::OpenContour) {
          has_open_contours = true;
          continue;
        }
        std::string message;
        switch (kind) {
        case geometry::SketchRegionDiagnosticKind::OpenContour:
          break;
        case geometry::SketchRegionDiagnosticKind::SelfIntersection:
          message = "Keine geschlossene Fläche: Die Kontur überschneidet sich selbst.";
          break;
        case geometry::SketchRegionDiagnosticKind::AmbiguousTopology:
          message = "Keine geschlossene Fläche: Die Kontur ist verzweigt oder mehrdeutig.";
          break;
        }
        return reject_finish(Error::validation(finished_sketch.value(), std::move(message)));
      }
      if (!analysis.value().regions.empty()) {
        Sketch completed = *sketch;
        geometry::SketchRegionFinder finder;
        for (const auto& region : analysis.value().regions) {
          auto added = finder.materialize_profile(completed, region);
          if (added.has_error())
            return reject_finish(added.error());
        }
        SketchTopologyMigrationReport migration;
        auto topology = SketchTopology::migrate_legacy(completed, &migration);
        if (topology.has_error())
          return reject_finish(topology.error());
        auto committed = session_.commit_part_sketch_intent_transaction(
            "Skizzenprofil materialisieren",
            [completed = std::move(completed), finished_sketch,
             migration = std::move(migration)](
                PartDocument& document,
                std::vector<SketchConstraintCatalog>& constraint_catalogs,
                std::vector<SketchDimensionCatalog>& dimension_catalogs) mutable {
              auto updated = document.update_sketch(std::move(completed));
              if (updated.has_error())
                return updated;
              auto remapped = remap_sketch_intent_catalogs(
                  document.id(), finished_sketch, migration, constraint_catalogs,
                  dimension_catalogs);
              if (remapped.has_error())
                return remapped;
              return Result<std::size_t>::success(updated.value() + remapped.value());
            });
        if (committed.has_error())
          return reject_finish(committed.error());
      }
    }
  }
  const auto finished = sketch_workspace_.complete_validated_finish(session_);
  if (finished.has_error())
    return reject_finish(finished.error());
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
  std::optional<ProfileId> finished_profile;
  if (feature_tools_) {
    const auto* part = session_.part_document();
    const auto* sketch = part != nullptr ? part->find_sketch(finished_sketch) : nullptr;
    if (sketch != nullptr)
      finished_profile = unique_profile(*sketch);
    feature_tools_->clear_profile_selection();
  }
  refresh();
  // A materializing Finish advances the presentation revision. Rebuilding the
  // viewport can briefly replay its prior selection, so publish the feature
  // handoff only after that synchronization has completed.
  if (feature_tools_ && finished_profile.has_value() &&
      feature_tools_->select_profile(finished_sketch, *finished_profile))
    statusBar()->showMessage(QStringLiteral("Profil bereit — Extrusion oder Drehung wählen"));
  else if (feature_tools_ && feature_tools_->select_open_line(finished_sketch))
    statusBar()->showMessage(
        QStringLiteral("Offene Linie bereit — Extrusion mit Wandstärke wählen"));
  else if (has_open_contours)
    statusBar()->showMessage(QStringLiteral(
        "Skizze beendet — offene Konturen bleiben zum Weiterbearbeiten erhalten"));
  return true;
}

bool ShellWindow::undo() {
  if (hole_tools_)
    hole_tools_->cancel();
  if (parameter_tools_)
    parameter_tools_->cancel();
  if (sketch_3d_tools_)
    sketch_3d_tools_->cancel();
  if (sketch_diagnostics_tools_)
    sketch_diagnostics_tools_->cancel();
  if (reference_repair_tools_)
    reference_repair_tools_->cancel();
  if (construction_tools_)
    construction_tools_->cancel();
  if (inspect_tools_)
    inspect_tools_->cancel();
  if (feature_edit_tools_)
    feature_edit_tools_->cancel();
  if (assembly_tools_)
    assembly_tools_->cancel();
  if (measure_tools_)
    measure_tools_->cancel();
  if (feature_tools_ && feature_tools_->active())
    feature_tools_->cancel();
  if (active_sketch_ && sketch_tools_) {
    sketch_tools_->cancel_tool();
    while (session_.task().active() && sketch_workspace_.escape(session_)) {
    }
  }
  if (!session_.can_undo())
    return false;
  const auto result = session_.undo();
  if (result.has_error()) {
    show_error(result.error());
    return false;
  }
  reconcile_active_sketch();
  refresh();
  return true;
}

bool ShellWindow::redo() {
  if (hole_tools_)
    hole_tools_->cancel();
  if (parameter_tools_)
    parameter_tools_->cancel();
  if (sketch_3d_tools_)
    sketch_3d_tools_->cancel();
  if (sketch_diagnostics_tools_)
    sketch_diagnostics_tools_->cancel();
  if (reference_repair_tools_)
    reference_repair_tools_->cancel();
  if (construction_tools_)
    construction_tools_->cancel();
  if (inspect_tools_)
    inspect_tools_->cancel();
  if (feature_edit_tools_)
    feature_edit_tools_->cancel();
  if (assembly_tools_)
    assembly_tools_->cancel();
  if (measure_tools_)
    measure_tools_->cancel();
  if (feature_tools_ && feature_tools_->active())
    feature_tools_->cancel();
  if (active_sketch_ && sketch_tools_) {
    sketch_tools_->cancel_tool();
    while (session_.task().active() && sketch_workspace_.escape(session_)) {
    }
  }
  if (!session_.can_redo())
    return false;
  const auto result = session_.redo();
  if (result.has_error()) {
    show_error(result.error());
    return false;
  }
  reconcile_active_sketch();
  refresh();
  return true;
}

void ShellWindow::apply_viewport_selection(const std::optional<GuiSelection>& selection) {
  if (synchronizing_selection_)
    return;
  synchronize_selection_filter();
  session_.selection().clear();
  selected_semantic_id_.clear();
  bool selected_feature_profile = false;
  if (selection && session_.selection().add(*selection)) {
    if (hole_tools_)
      (void)hole_tools_->accept_selection(*selection);
    if (construction_tools_)
      (void)construction_tools_->accept_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_extrude_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_finishing_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_pattern_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_body_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_spatial_selection(*selection);
    if (feature_tools_)
      (void)feature_tools_->accept_surface_selection(*selection);
    if (measure_tools_)
      (void)measure_tools_->accept_selection(*selection);
    if (assembly_tools_)
      (void)assembly_tools_->accept_selection(*selection);
    selected_semantic_id_ = selection->semantic_id;
    if (feature_tools_) {
      if (const auto* part = session_.part_document()) {
        if (const auto profile = feature_profile_for_selection(*part, selection->semantic_id))
          selected_feature_profile =
              feature_tools_->select_profile(profile->first, profile->second);
        else if (const auto sketch = interactive_thin_selection(*part, selection->semantic_id))
          selected_feature_profile = feature_tools_->select_open_line(*sketch);
      }
    }
  }
  if (feature_tools_ && !selected_feature_profile)
    feature_tools_->clear_profile_selection();
  synchronize_tree_selection(selected_semantic_id_);
  refresh();
}

void ShellWindow::handle_tree_selection(QTreeWidgetItem* current) {
  if (synchronizing_selection_)
    return;
  session_.selection().clear();
  selected_semantic_id_.clear();
  property_semantic_id_ =
      current != nullptr ? current->data(0, kSemanticIdRole).toString().toStdString()
                         : std::string{};
  bool selected_feature_profile = false;
  if (current != nullptr) {
    const std::string viewport_id = current->data(0, kViewportIdRole).toString().toStdString();
    synchronizing_selection_ = true;
    if (viewport_id.empty() || !viewport_->select_semantic(viewport_id))
      viewport_->clear_selection();
    synchronizing_selection_ = false;
    if (const auto& selected = viewport_->selected_semantic())
      if (session_.selection().add(*selected))
        selected_semantic_id_ = selected->semantic_id;
    if (hole_tools_)
      for (const auto& selected : session_.selection().entries())
        (void)hole_tools_->accept_selection(selected);
    if (feature_tools_) {
      // Browser selection suppresses the viewport callback above. Route these
      // picks to the Pattern/Body/Surface owners explicitly; reference acceptance can
      // clear shared selection while restoring Body scope, so keep a snapshot.
      const auto selected = session_.selection().entries();
      for (const auto& selection : selected) {
        (void)feature_tools_->accept_pattern_selection(selection);
        (void)feature_tools_->accept_body_selection(selection);
        (void)feature_tools_->accept_surface_selection(selection);
      }
      if (const auto* part = session_.part_document()) {
        if (const auto profile = feature_profile_for_selection(*part, property_semantic_id_))
          selected_feature_profile =
              feature_tools_->select_profile(profile->first, profile->second);
        else if (const auto sketch = interactive_thin_selection(*part, property_semantic_id_))
          selected_feature_profile = feature_tools_->select_open_line(*sketch);
      }
    }
  } else {
    synchronizing_selection_ = true;
    viewport_->clear_selection();
    synchronizing_selection_ = false;
  }
  if (feature_tools_ && !selected_feature_profile)
    feature_tools_->clear_profile_selection();
  refresh();
}

void ShellWindow::handle_tree_activation(QTreeWidgetItem* item) {
  // Feature activation opens the update task; planar Sketch activation retains
  // its existing editing route. Both are scoped to the current Part context.
  if (item == nullptr || active_sketch_ || session_.task().active() ||
      session_.part_document() == nullptr)
    return;
  const std::string semantic_id = item->data(0, kSemanticIdRole).toString().toStdString();
  if (semantic_id.empty())
    return;
  const auto* node = browser_model_.find(semantic_id);
  if (node && node->kind == GuiBrowserNodeKind::Feature &&
      node->owner_document_id == session_.part_document()->id().value()) {
    auto begun = feature_edit_tools_->begin(FeatureId(semantic_id));
    if (!begun)
      show_error(begun.error());
    return;
  }
  if (node && node->owner_document_id == session_.part_document()->id().value() &&
      (node->kind == GuiBrowserNodeKind::Construction ||
       (node->kind == GuiBrowserNodeKind::Datum &&
        !session_.part_document()->find_datum_plane(DatumPlaneId(semantic_id))))) {
    auto begun = construction_tools_->begin_edit(semantic_id);
    if (!begun)
      show_error(begun.error());
    return;
  }
  if (session_.part_document()->find_sketch_3d(Sketch3DId(semantic_id))) {
    auto begun = sketch_3d_tools_->begin_edit(Sketch3DId(semantic_id));
    if (!begun)
      show_error(begun.error());
    return;
  }
  const SketchId sketch_id(semantic_id);
  if (session_.part_document()->find_sketch(sketch_id) == nullptr)
    return;
  (void)enter_sketch(sketch_id);
}

void ShellWindow::refresh_properties() {
  if (properties_ == nullptr)
    return;
  const QSignalBlocker blocker(properties_);
  properties_->clearContents();
  properties_->setRowCount(0);
  const GuiBrowserNode* node =
      property_semantic_id_.empty() ? nullptr : browser_model_.find(property_semantic_id_);
  if (node == nullptr)
    return;
  properties_->setRowCount(static_cast<int>(node->properties.size()));
  int row = 0;
  for (const auto& property : node->properties) {
    auto* label = new QTableWidgetItem(QString::fromStdString(property.label));
    label->setFlags(Qt::ItemIsEnabled);
    auto* value = new QTableWidgetItem(QString::fromStdString(property.value));
    if (property.editable && !session_.reference_repair_mode()) {
      value->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
      value->setData(kSemanticIdRole, QString::fromStdString(property.key));
    } else {
      value->setFlags(Qt::ItemIsEnabled);
      if (!property.read_only_reason.empty())
        value->setToolTip(QString::fromStdString(property.read_only_reason));
    }
    properties_->setItem(row, 0, label);
    properties_->setItem(row, 1, value);
    ++row;
  }
}

void ShellWindow::handle_property_changed(QTableWidgetItem* item) {
  if (session_.reference_repair_mode() || item == nullptr || item->column() != 1)
    return;
  const std::string key = item->data(kSemanticIdRole).toString().toStdString();
  if (key.empty())
    return;
  const GuiBrowserNode* node =
      property_semantic_id_.empty() ? nullptr : browser_model_.find(property_semantic_id_);
  if (node == nullptr)
    return;
  const auto edited =
      edit_browser_property(session_, *node, key, item->text().toStdString(), true);
  if (edited.has_error())
    show_error(edited.error());
  // Repopulate outside this itemChanged emission: refresh() rebuilds the table
  // and would delete `item` while Qt is still inside its setData() call. The
  // queued refresh repaints the cell from the model, reverting a rejected edit.
  QMetaObject::invokeMethod(this, [this] { refresh(); }, Qt::QueuedConnection);
}

void ShellWindow::synchronize_tree_selection(const std::string& viewport_semantic_id) {
  synchronizing_selection_ = true;
  QTreeWidgetItem* found = nullptr;
  const QString id = QString::fromStdString(viewport_semantic_id);
  if (!id.isEmpty())
    for (int index = 0; index < browser_tree_->topLevelItemCount() && found == nullptr; ++index)
      found = find_item_by_viewport_id(browser_tree_->topLevelItem(index), id);
  browser_tree_->setCurrentItem(found);
  property_semantic_id_ =
      found != nullptr ? found->data(0, kSemanticIdRole).toString().toStdString() : std::string{};
  synchronizing_selection_ = false;
}

void ShellWindow::refresh() {
  synchronize_selection_filter();
  const bool has_document = session_.has_document();
  const bool sketch_active = active_sketch_.has_value();
  action_save_->setEnabled(has_document);
  action_save_as_->setEnabled(has_document);
  action_undo_->setEnabled(session_.can_undo());
  action_redo_->setEnabled(session_.can_redo());
  action_create_sketch_->setEnabled(has_document && !session_.reference_repair_mode() &&
                                    !sketch_active && !session_.task().active() &&
                                    selected_datum_plane().has_value());
  action_finish_sketch_->setEnabled(sketch_active);
  action_perspective_->setEnabled(!sketch_active);

  const QString name = has_document
                           ? QString::fromStdString(std::string(session_.display_name()))
                           : QString();
  setWindowTitle(has_document
                     ? QStringLiteral("%1%2 — BLCAD")
                           .arg(name, session_.dirty() ? QStringLiteral(" *") : QString())
                     : QStringLiteral("BLCAD"));

  if (session_.reference_repair_mode())
    setWindowTitle(windowTitle() + QStringLiteral(" [Reparatur]"));

  QStringList lines;
  for (const auto& diagnostic : session_.diagnostics())
    lines.push_back(QStringLiteral("[%1] %2: %3")
                        .arg(QString::fromStdString(std::string(to_string(diagnostic.category))),
                             QString::fromStdString(diagnostic.object_id),
                             QString::fromStdString(diagnostic.message)));
  diagnostics_->setPlainText(lines.join(QLatin1Char('\n')));

  refresh_browser();
  refresh_properties();
  synchronize_navigation();
  if (hole_tools_)
    hole_tools_->refresh();
  if (parameter_tools_)
    parameter_tools_->refresh();
  if (sketch_3d_tools_)
    sketch_3d_tools_->refresh();
  if (sketch_diagnostics_tools_)
    sketch_diagnostics_tools_->refresh();
  if (reference_repair_tools_)
    reference_repair_tools_->refresh();
  if (construction_tools_)
    construction_tools_->refresh();
  if (inspect_tools_)
    inspect_tools_->refresh();
  if (measure_tools_)
    measure_tools_->refresh();
  if (assembly_tools_)
    assembly_tools_->refresh();
  if (feature_edit_tools_)
    feature_edit_tools_->refresh();
  refresh_viewport_scene();
  if (sketch_tools_)
    sketch_tools_->refresh_enablement();
  if (feature_tools_) {
    feature_tools_->refresh_enablement();
    feature_tools_->refresh_presentation();
  }
  if (repeat_tools_)
    repeat_tools_->refresh();
  refresh_context_toolbar();
}

void ShellWindow::refresh_context_toolbar() {
  if (context_tools_)
    context_tools_->refresh();
}

void ShellWindow::finish_context_task(bool applied) {
  if (context_tools_)
    context_tools_->finish(applied);
}

void ShellWindow::remember_repeatable_command(QAction& command) {
  repeat_tools_->remember(command);
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
  if (!retained.empty()) {
    synchronize_tree_selection(retained);
  } else if (!property_semantic_id_.empty()) {
    // Nodes without a viewport id (e.g. parameters) are restored by semantic id
    // so the property inspector keeps its target across a rebuild (after an edit).
    synchronizing_selection_ = true;
    QTreeWidgetItem* found = nullptr;
    const QString id = QString::fromStdString(property_semantic_id_);
    for (int index = 0; index < browser_tree_->topLevelItemCount() && found == nullptr; ++index)
      found = find_item_by_semantic_id(browser_tree_->topLevelItem(index), id);
    browser_tree_->setCurrentItem(found);
    synchronizing_selection_ = false;
  }
}

void ShellWindow::refresh_feature_selection_scene() {
  viewport_revision_ = std::numeric_limits<std::uint64_t>::max();
  refresh_viewport_scene();
}

void ShellWindow::refresh_viewport_scene() {
  const auto finishing_mask = feature_tools_ ? feature_tools_->idle_finishing_selection_mask() : 0U;
  if (viewport_revision_ == session_.presentation_revision() &&
      viewport_finishing_mask_ == finishing_mask)
    return;
  viewport_revision_ = session_.presentation_revision();
  viewport_finishing_mask_ = finishing_mask;
  if (!session_.has_document()) {
    viewport_->clear_scene();
    return;
  }
  if (session_.reference_repair_mode()) {
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
  if (feature_tools_) {
    auto extrude_targets = feature_tools_->extrude_selection_items();
    scene.value().insert(scene.value().end(), extrude_targets.begin(), extrude_targets.end());
    auto targets = feature_tools_->finishing_selection_items();
    scene.value().insert(scene.value().end(), targets.begin(), targets.end());
  }
  if (measure_tools_) {
    auto targets = measure_tools_->selection_items();
    scene.value().insert(scene.value().end(), targets.begin(), targets.end());
  }
  if (assembly_tools_) {
    auto targets = assembly_tools_->selection_items();
    scene.value().insert(scene.value().end(), targets.begin(), targets.end());
  }
  if (construction_tools_) {
    auto targets = construction_tools_->selection_items();
    scene.value().insert(scene.value().end(), targets.begin(), targets.end());
  }
  const auto published = viewport_->set_scene(std::move(scene.value()));
  if (published.has_error())
    append_diagnostic(published.error());
}

void ShellWindow::append_diagnostic(const Error& error) {
  const auto& diagnostics = session_.diagnostics();
  const bool already_recorded =
      !diagnostics.empty() && diagnostics.back().category == error.category() &&
      diagnostics.back().object_id == error.object_id() &&
      diagnostics.back().message == error.message();
  if (!already_recorded)
    session_.report_error(error);
  diagnostics_->appendPlainText(diagnostic_line(error));
}

void ShellWindow::show_error(const Error& error) {
  append_diagnostic(error);
  statusBar()->showMessage(QStringLiteral("%1: %2").arg(
      QString::fromStdString(error.object_id()), QString::fromStdString(error.message())));
}

} // namespace blcad::gui
