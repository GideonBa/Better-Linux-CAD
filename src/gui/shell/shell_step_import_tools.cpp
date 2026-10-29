#include "blcad/gui/shell/shell_step_import_tools.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/gui_step_import_workbench.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOption>
#include <QTableWidget>
#include <QTextDocument>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace blcad::gui {
namespace {
constexpr std::string_view task_id = "step.sources";
Error error(std::string message) {
  return Error::validation(std::string(task_id), std::move(message));
}
QString text(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
Result<std::string> source_json(const GuiDocumentSession& session) {
  if (session.part_document())
    return serialize_part_document_to_json(*session.part_document());
  if (session.project())
    return serialize_project_to_json(*session.project());
  return Result<std::string>::failure(error("No document open"));
}
std::filesystem::path document_path(const GuiDocumentSession& session) {
  return session.file_path().empty()
             ? std::filesystem::path{}
             : std::filesystem::absolute(session.file_path()).lexically_normal();
}
const std::vector<StepImportSource>& sources(const GuiDocumentSession& session) {
  return session.part_document() ? session.part_document()->step_import_sources()
                                 : session.project()->step_import_sources();
}
QString mode_text(StepImportMode mode) {
  return mode == StepImportMode::Reference ? QStringLiteral("Reference")
                                           : QStringLiteral("Editable Body");
}
QString state_text(StepImportFreshness state) {
  switch (state) {
  case StepImportFreshness::Available:
    return QStringLiteral("Current");
  case StepImportFreshness::Missing:
    return QStringLiteral("File Missing");
  case StepImportFreshness::Changed:
    return QStringLiteral("File Changed");
  case StepImportFreshness::Unreadable:
    return QStringLiteral("Unreadable");
  }
  return {};
}
QString shape_kind_text(geometry::StepImportedShapeKind kind) {
  switch (kind) {
  case geometry::StepImportedShapeKind::Solid:
    return QStringLiteral("Solid");
  case geometry::StepImportedShapeKind::Shell:
    return QStringLiteral("Shell / Surface");
  case geometry::StepImportedShapeKind::Face:
    return QStringLiteral("Face / Surface");
  case geometry::StepImportedShapeKind::Compound:
    return QStringLiteral("Compound");
  }
  return {};
}
} // namespace

struct ShellStepImportTools::Impl {
  ShellWindow& window;
  GuiStepImportWorkbench workbench;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox* source{};
  QComboBox* operation{};
  QLineEdit* name{};
  QLineEdit* path{};
  QComboBox* mode{};
  QPushButton* browse{};
  QPushButton* apply{};
  QTableWidget* catalog{};
  QLabel* status{};
  QPlainTextEdit* expected{};
  QPlainTextEdit* observed{};
  QLabel* summary{};
  QLabel* geometry_summary{};
  QLabel* geometry_units{};
  QTreeWidget* geometry_products{};
  QPlainTextEdit* geometry_diagnostics{};
  QFileSystemWatcher* watcher{};
  bool active{};
  bool loading{};
  bool undo_available{};
  bool redo_available{};
  std::size_t revision{};
  std::filesystem::path original_path;
  std::string original_json;
  std::optional<GuiStepImportPreview> candidate;
  std::optional<GuiStepImportGeometryPreview> inspection;
  std::optional<GuiViewportCameraBookmark> inspection_camera;

  bool available() const {
    const auto& session = window.session();
    return session.has_document() && !session.file_path().empty() && !session.task().active() &&
           !window.active_sketch() && !session.reference_repair_mode();
  }
  bool current() {
    const auto& session = window.session();
    if (!active || !session.has_document() || session.task().command_id() != task_id ||
        window.active_sketch() || session.reference_repair_mode() ||
        document_path(session) != original_path)
      return false;
    if (revision == session.presentation_revision())
      return true;
    const auto json = source_json(session);
    if (!json || json.value() != original_json)
      return false;
    revision = session.presentation_revision();
    return true;
  }
  const StepImportSource* selected() const {
    if (!window.session().has_document())
      return nullptr;
    const auto id = source->currentData().toString().toStdString();
    for (const auto& entry : sources(window.session()))
      if (entry.id().value() == id)
        return &entry;
    return nullptr;
  }
  void invalidate() {
    candidate.reset();
    inspection.reset();
    if (watcher) {
      const auto files = watcher->files();
      const auto directories = watcher->directories();
      if (!files.empty())
        watcher->removePaths(files);
      if (!directories.empty())
        watcher->removePaths(directories);
    }
    window.viewport().clear_feature_preview();
    if (inspection_camera) {
      (void)window.viewport().restore_camera_bookmark(*inspection_camera);
      inspection_camera.reset();
    }
    if (geometry_products) {
      geometry_products->clear();
      geometry_units->clear();
      geometry_diagnostics->clear();
      geometry_summary->setText(QStringLiteral("No geometry inspection prepared."));
    }
    apply->setEnabled(false);
    if (window.session().task().command_id() == task_id)
      (void)window.session().task().return_to_editing();
  }
  void edited() {
    if (loading || !active)
      return;
    invalidate();
    status->setProperty("freshness", -1);
    status->setText(QStringLiteral("Not Inspected"));
    expected->clear();
    observed->clear();
    summary->setText(QStringLiteral("Inspect the file and prepare the desired change."));
  }
  void select() {
    if (loading || !active)
      return;
    loading = true;
    invalidate();
    const auto* entry = selected();
    name->setText(entry ? text(entry->name()) : QString());
    path->setText(entry ? text(entry->project_relative_asset_path()) : QString());
    mode->setCurrentIndex(entry ? mode->findData(static_cast<int>(entry->mode())) : -1);
    name->setReadOnly(entry);
    path->setReadOnly(entry);
    mode->setEnabled(!entry);
    browse->setEnabled(!entry);
    operation->setCurrentIndex(entry ? 1 : 0);
    auto* model = static_cast<QStandardItemModel*>(operation->model());
    for (int row = 0; row < operation->count(); ++row)
      model->item(row)->setEnabled(entry ? row != 0 : row == 0);
    loading = false;
    edited();
    if (entry)
      expected->setPlainText(text(entry->sha256_digest()));
  }
  void report(const StepImportFreshnessReport& value) {
    status->setProperty("freshness", static_cast<int>(value.state()));
    status->setText(state_text(value.state()));
    expected->setPlainText(text(value.source().sha256_digest()));
    observed->setPlainText(text(value.observed_sha256_digest().value_or("")));
  }
  void show_geometry(const GuiStepImportGeometryPreview& value) {
    const auto& imported = value.geometry;
    QStringList units;
    for (const auto& unit : imported.units)
      units.push_back(QStringLiteral("%1 × %2 → mm")
                          .arg(text(unit.declared_length_unit))
                          .arg(unit.length_to_mm, 0, 'g', 12));
    geometry_units->setText(units.join(QStringLiteral("; ")));
    auto* definitions = new QTreeWidgetItem(geometry_products, {QStringLiteral("Definitions")});
    for (const auto& product : imported.products) {
      const auto kind = product.is_assembly ? QStringLiteral("Assembly")
                        : product.source_kind
                            ? QStringLiteral("Part / ") + shape_kind_text(*product.source_kind)
                            : QStringLiteral("Part");
      auto* item = new QTreeWidgetItem(
          definitions,
          {product.name.empty() ? QStringLiteral("Unnamed Product") : text(product.name), kind,
           QStringLiteral("%1 bodies").arg(product.bodies.size())});
      item->setToolTip(0, text(product.name) + "\n" + text(product.definition_key));
      for (const auto& body : product.bodies) {
        const auto& bounds = body.bounds_mm;
        auto* row = new QTreeWidgetItem(
            item, {body.name.empty() ? QStringLiteral("Unnamed Body") : text(body.name),
                   shape_kind_text(body.source_kind),
                   QStringLiteral("%1 × %2 × %3 mm")
                       .arg(bounds[3] - bounds[0], 0, 'g', 8)
                       .arg(bounds[4] - bounds[1], 0, 'g', 8)
                       .arg(bounds[5] - bounds[2], 0, 'g', 8)});
        row->setToolTip(0, QStringLiteral("%1\n%2\nVolume: %3 mm³\nArea: %4 mm²\nFaces: %5")
                               .arg(text(body.name))
                               .arg(text(body.definition_key))
                               .arg(body.volume_mm3, 0, 'g', 12)
                               .arg(body.area_mm2, 0, 'g', 12)
                               .arg(body.face_count));
      }
    }
    auto* occurrences = new QTreeWidgetItem(geometry_products, {QStringLiteral("Occurrences")});
    const auto append = [&](auto&& self, QTreeWidgetItem* parent,
                            std::optional<std::size_t> definition,
                            std::vector<std::size_t> ancestors) -> void {
      for (const auto& occurrence : imported.occurrences) {
        if (occurrence.parent_definition != definition ||
            occurrence.product_definition >= imported.products.size())
          continue;
        const auto& product = imported.products[occurrence.product_definition];
        auto* item = new QTreeWidgetItem(
            parent, {occurrence.name.empty() ? text(product.name) : text(occurrence.name),
                     product.is_assembly ? QStringLiteral("Assembly occurrence")
                                         : QStringLiteral("Part occurrence"),
                     text(product.name)});
        const auto& m = occurrence.transform.matrix;
        item->setToolTip(
            0, QStringLiteral("%1\n%2\nDirect translation: %3, %4, %5 mm")
                   .arg(occurrence.name.empty() ? text(product.name) : text(occurrence.name))
                   .arg(text(product.name))
                   .arg(m[3], 0, 'g', 12)
                   .arg(m[7], 0, 'g', 12)
                   .arg(m[11], 0, 'g', 12));
        if (std::find(ancestors.begin(), ancestors.end(), occurrence.product_definition) ==
            ancestors.end()) {
          auto chain = ancestors;
          chain.push_back(occurrence.product_definition);
          self(self, item, occurrence.product_definition, std::move(chain));
        }
      }
    };
    append(append, occurrences, std::nullopt, {});
    geometry_products->expandToDepth(1);
    QStringList diagnostics;
    for (const auto& diagnostic : imported.diagnostics)
      diagnostics.push_back(text(diagnostic));
    geometry_diagnostics->setPlainText(diagnostics.join('\n'));
    geometry_summary->setText(
        QStringLiteral(
            "%1 product definitions, %2 body definitions, %3 occurrences. "
            "%4 posed bodies are previewed. Inspection does not import or change the document.")
            .arg(imported.products.size())
            .arg(imported.body_count())
            .arg(imported.occurrences.size())
            .arg(value.preview_shapes.size()));
  }
  void watch_inspection() {
    if (!inspection)
      return;
    const auto root = inspection->document_path.parent_path();
    const auto asset = root / inspection->source.project_relative_asset_path();
    QStringList paths{QString::fromStdString(asset.string())};
    for (auto parent = asset.parent_path(); !parent.empty(); parent = parent.parent_path()) {
      paths.push_back(QString::fromStdString(parent.string()));
      if (parent == root)
        break;
    }
    watcher->addPaths(paths);
  }
  Result<std::size_t> catalog_status() {
    const auto reports = workbench.inspect_sources(window.session());
    if (!reports)
      return Result<std::size_t>::failure(reports.error());
    const QSignalBlocker blocker(catalog);
    catalog->setRowCount(0);
    const auto& session = window.session();
    for (const auto& value : reports.value()) {
      const auto& entry = value.report.source();
      const bool embedded = value.embedded_part;
      const auto* part =
          embedded ? session.project()->find_part_document(value.owner_document) : nullptr;
      const auto label = embedded
                             ? text(part ? part->name() : value.owner_document.value()) + " / " +
                                   text(entry.name()) + QStringLiteral(" (Display Only)")
                             : text(entry.name());
      const int row = catalog->rowCount();
      catalog->insertRow(row);
      const QStringList cells{label, mode_text(entry.mode()),
                              text(entry.project_relative_asset_path()),
                              state_text(value.report.state())};
      for (int column = 0; column < cells.size(); ++column) {
        auto* item = new QTableWidgetItem(cells[column]);
        if (embedded)
          item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setData(Qt::UserRole, text(entry.id().value()));
        item->setData(Qt::UserRole + 1, text(value.owner_document.value()));
        item->setData(Qt::UserRole + 2, embedded);
        item->setToolTip(text(value.owner_document.value()) + " / " + text(entry.id().value()) +
                         "\n" + text(entry.project_relative_asset_path()));
        catalog->setItem(row, column, item);
      }
    }
    return Result<std::size_t>::success(reports.value().size());
  }
};

ShellStepImportTools::ShellStepImportTools(ShellWindow& window, ShellRibbon& ribbon, int model_tab,
                                           int assembly_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction(QStringLiteral("STEP Sources"), this);
  ui.action->setObjectName("blcad.action.step_sources");
  ui.action->setToolTip(
      QStringLiteral("Manage source files and import mode; save the document first."));
  for (const auto tab : {model_tab, assembly_tab})
    ribbon.add_button(ribbon.add_group(tab, QStringLiteral("STEP Sources")), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget(QStringLiteral("Manage STEP Sources"), &window);
  ui.dock->setObjectName("blcad.step_import.task");
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setObjectName("blcad.step_import.scroll");
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* content_layout = new QVBoxLayout(content);
  auto* hint = new QLabel(
      QStringLiteral(
          "Link a STEP file in the document directory or a subdirectory. Choose the import mode "
          "explicitly. "
          "Inspect Geometry previews the file without importing it. Source files are preserved. "
          "Inspect changed files first and explicitly accept their new checksum."),
      content);
  hint->setWordWrap(true);
  hint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  hint->setMinimumWidth(0);
  content_layout->addWidget(hint);
  ui.catalog = new QTableWidget(content);
  ui.catalog->setObjectName("blcad.step_import.catalog");
  ui.catalog->setColumnCount(4);
  ui.catalog->setHorizontalHeaderLabels({"Source", "Mode", "Relative Path", "Status"});
  ui.catalog->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui.catalog->setSelectionBehavior(QAbstractItemView::SelectRows);
  ui.catalog->setSelectionMode(QAbstractItemView::SingleSelection);
  ui.catalog->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  ui.catalog->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  ui.catalog->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  ui.catalog->setTextElideMode(Qt::ElideMiddle);
  ui.catalog->setMinimumHeight(100);
  ui.catalog->setMaximumHeight(160);
  content_layout->addWidget(ui.catalog);
  auto* form = new QFormLayout;
  ui.source = new QComboBox(content);
  ui.source->setObjectName("blcad.step_import.source");
  ui.source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  ui.source->setMinimumContentsLength(14);
  form->addRow(QStringLiteral("Source"), ui.source);
  ui.operation = new QComboBox(content);
  ui.operation->setObjectName("blcad.step_import.operation");
  ui.operation->addItems({QStringLiteral("Register"), QStringLiteral("Update Checksum"),
                          QStringLiteral("Remove Source")});
  apply_gui_choice_icons(*ui.operation);
  form->addRow(QStringLiteral("Change"), ui.operation);
  ui.name = new QLineEdit(content);
  ui.name->setObjectName("blcad.step_import.name");
  form->addRow(QStringLiteral("Name"), ui.name);
  auto* path_row = new QWidget(content);
  auto* path_layout = new QHBoxLayout(path_row);
  path_layout->setContentsMargins(0, 0, 0, 0);
  ui.path = new QLineEdit(path_row);
  ui.path->setObjectName("blcad.step_import.path");
  ui.path->setPlaceholderText("assets/bauteil.step");
  path_layout->addWidget(ui.path);
  ui.browse = make_gui_button(QStringLiteral("Choose File…"), path_row);
  ui.browse->setObjectName("blcad.step_import.browse");
  path_layout->addWidget(ui.browse);
  form->addRow(QStringLiteral("Relative Path"), path_row);
  ui.mode = new QComboBox(content);
  ui.mode->setObjectName("blcad.step_import.mode");
  for (auto mode : {StepImportMode::Reference, StepImportMode::EditableBody})
    ui.mode->addItem(mode_text(mode), static_cast<int>(mode));
  ui.mode->setPlaceholderText(QStringLiteral("Choose Import Mode"));
  ui.mode->setCurrentIndex(-1);
  form->addRow(QStringLiteral("Import Mode"), ui.mode);
  content_layout->addLayout(form);
  const auto label = [content, content_layout](const char* name) {
    auto* value = new QLabel(content);
    value->setObjectName(QStringLiteral("blcad.step_import.") + name);
    value->setWordWrap(true);
    value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    value->setMinimumWidth(0);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content_layout->addWidget(value);
    return value;
  };
  content_layout->addWidget(new QLabel(QStringLiteral("File Status"), content));
  ui.status = label("status");
  for (auto [target, name, caption] :
       {std::tuple{&ui.expected, "expected_digest", "Saved SHA-256"},
        std::tuple{&ui.observed, "observed_digest", "Read SHA-256"}}) {
    content_layout->addWidget(new QLabel(QString::fromUtf8(caption), content));
    auto* value = new QPlainTextEdit(content);
    value->setObjectName(QStringLiteral("blcad.step_import.") + name);
    value->setReadOnly(true);
    value->setWordWrapMode(QTextOption::WrapAnywhere);
    value->setFrameShape(QFrame::NoFrame);
    value->ensurePolished();
    // Reserve three wrapped lines inside the styled frame and padding, using
    // the final font and document margins rather than assuming native chrome.
    QStyleOptionFrame frame;
    frame.initFrom(value);
    frame.frameShape = value->frameShape();
    const QRect styled_contents =
        value->style()->subElementRect(QStyle::SE_ShapedFrameContents, &frame, value);
    const int chrome_height = frame.rect.height() - styled_contents.height();
    const int document_margins =
        static_cast<int>(std::ceil(2.0 * value->document()->documentMargin()));
    value->setFixedHeight(3 * value->fontMetrics().lineSpacing() + document_margins +
                          chrome_height);
    value->setMinimumWidth(0);
    value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    content_layout->addWidget(value);
    *target = value;
  }
  ui.summary = label("summary");
  auto* geometry_button = make_gui_button(QStringLiteral("Inspect Geometry"), content);
  geometry_button->setObjectName("blcad.step_import.inspect_geometry");
  content_layout->addWidget(geometry_button);
  ui.geometry_summary = label("geometry_summary");
  ui.geometry_units = label("geometry_units");
  ui.geometry_products = new QTreeWidget(content);
  ui.geometry_products->setObjectName("blcad.step_import.geometry_products");
  ui.geometry_products->setColumnCount(3);
  ui.geometry_products->setHeaderLabels({"Product / Body", "Kind", "Dimensions / Definition"});
  ui.geometry_products->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui.geometry_products->setSelectionMode(QAbstractItemView::NoSelection);
  ui.geometry_products->header()->setSectionResizeMode(QHeaderView::Stretch);
  ui.geometry_products->setMinimumWidth(0);
  ui.geometry_products->setMinimumHeight(160);
  ui.geometry_products->setMaximumHeight(220);
  ui.geometry_products->setTextElideMode(Qt::ElideMiddle);
  content_layout->addWidget(ui.geometry_products);
  ui.geometry_diagnostics = new QPlainTextEdit(content);
  ui.geometry_diagnostics->setObjectName("blcad.step_import.geometry_diagnostics");
  ui.geometry_diagnostics->setReadOnly(true);
  ui.geometry_diagnostics->setMinimumWidth(0);
  ui.geometry_diagnostics->setMaximumHeight(100);
  ui.geometry_diagnostics->setPlaceholderText(QStringLiteral("Geometry inspection diagnostics"));
  content_layout->addWidget(ui.geometry_diagnostics);
  content_layout->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll);
  auto* buttons = new QHBoxLayout;
  const auto button = [panel, buttons](const char* id, const QString& label) {
    auto* value = make_gui_button(label, panel);
    value->setObjectName(QStringLiteral("blcad.step_import.") + id);
    if (QString::fromLatin1(id) == QStringLiteral("apply"))
      value->setProperty("guiRole", "primary");
    buttons->addWidget(value);
    return value;
  };
  auto* inspect_button = button("inspect", QStringLiteral("Inspect File"));
  auto* preview_button = button("preview", QStringLiteral("Prepare"));
  ui.apply = button("apply", QStringLiteral("Apply"));
  auto* cancel_button = button("cancel", QStringLiteral("Close"));
  layout->addLayout(buttons);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  ui.watcher = new QFileSystemWatcher(this);
  const auto source_changed = [this] {
    auto& ui = *impl_;
    if (!ui.active || !ui.inspection)
      return;
    if (!ui.current()) {
      cancel();
      return;
    }
    auto current = ui.workbench.validate_inspection(ui.window.session(), *ui.inspection);
    if (!current) {
      const auto report =
          inspect_step_import_source(ui.inspection->source, ui.original_path.parent_path());
      ui.invalidate();
      if (report)
        ui.report(report.value());
      ui.summary->setText(text(current.error().message()));
    } else {
      // An atomic replacement can keep accepted bytes while removing the old inode's watch.
      // Continue watching the replacement so its later in-place edits invalidate this preview.
      ui.watch_inspection();
    }
  };
  connect(ui.watcher, &QFileSystemWatcher::fileChanged, this, source_changed);
  connect(ui.watcher, &QFileSystemWatcher::directoryChanged, this, source_changed);
  qApp->installEventFilter(this);
  ui.dock->installEventFilter(this);
  connect(ui.source, &QComboBox::currentIndexChanged, this, [this] { impl_->select(); });
  connect(ui.operation, &QComboBox::currentIndexChanged, this, [this] { impl_->edited(); });
  connect(ui.mode, &QComboBox::currentIndexChanged, this, [this] { impl_->edited(); });
  for (auto* input : {ui.name, ui.path})
    connect(input, &QLineEdit::textChanged, this, [this] { impl_->edited(); });
  connect(ui.path, &QLineEdit::textChanged, ui.path, &QWidget::setToolTip);
  connect(ui.catalog, &QTableWidget::itemSelectionChanged, this, [this] {
    const auto items = impl_->catalog->selectedItems();
    if (items.empty() || !impl_->current())
      return;
    impl_->invalidate();
    const auto& session = impl_->window.session();
    const auto owner =
        session.part_document() ? session.part_document()->id() : session.project()->id();
    if (!items.front()->data(Qt::UserRole + 2).toBool() &&
        items.front()->data(Qt::UserRole + 1).toString() == text(owner.value()))
      impl_->source->setCurrentIndex(impl_->source->findData(items.front()->data(Qt::UserRole)));
  });
  connect(ui.catalog, &QTableWidget::itemClicked, this, [this](QTableWidgetItem* item) {
    if (!item || !impl_->current() || (item->flags() & Qt::ItemIsSelectable))
      return;
    impl_->invalidate();
    impl_->summary->setText(
        QStringLiteral("This source belongs to an embedded part and is displayed here only. "
                       "The prepared change was discarded."));
  });
  connect(ui.browse, &QPushButton::clicked, this, [this] {
    auto& ui = *impl_;
    if (!ui.current() || ui.selected())
      return;
    const QDir root(QString::fromStdString(ui.original_path.parent_path().string()));
    const auto selected = QFileDialog::getOpenFileName(
        &ui.window, QStringLiteral("Choose STEP Source File"), root.path(),
        QStringLiteral("STEP (*.step *.stp);;All Files (*)"));
    if (selected.isEmpty() || !ui.current())
      return;
    ui.path->setText(root.relativeFilePath(selected));
    if (ui.name->text().isEmpty())
      ui.name->setText(QFileInfo(selected).completeBaseName());
  });
  const auto report = [this](const Result<std::size_t>& result) {
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  };
  connect(inspect_button, &QPushButton::clicked, this, [this, report] { report(inspect()); });
  connect(geometry_button, &QPushButton::clicked, this,
          [this, report] { report(inspect_geometry()); });
  connect(preview_button, &QPushButton::clicked, this, [this, report] { report(preview()); });
  connect(ui.apply, &QPushButton::clicked, this, [this, report] { report(apply()); });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
}
ShellStepImportTools::~ShellStepImportTools() = default;
bool ShellStepImportTools::active() const noexcept {
  return impl_->active;
}
bool ShellStepImportTools::can_undo_after_cancel() const noexcept {
  return impl_->active && impl_->undo_available;
}
bool ShellStepImportTools::can_redo_after_cancel() const noexcept {
  return impl_->active && impl_->redo_available;
}
Result<std::size_t> ShellStepImportTools::begin() {
  auto& ui = *impl_;
  if (!ui.available())
    return Result<std::size_t>::failure(error("Save the document first and close active editing"));
  const auto json = source_json(ui.window.session());
  if (!json)
    return Result<std::size_t>::failure(json.error());
  ui.original_json = json.value();
  ui.original_path = document_path(ui.window.session());
  ui.revision = ui.window.session().presentation_revision();
  ui.undo_available = ui.window.session().can_undo();
  ui.redo_available = ui.window.session().can_redo();
  if (!ui.window.session().task().begin(std::string(task_id)) ||
      !ui.window.session().task().begin_parameter_editing())
    return Result<std::size_t>::failure(error("Could not start source management"));
  ui.active = true;
  ui.loading = true;
  ui.source->clear();
  ui.source->addItem(QStringLiteral("New Source"), QString());
  for (const auto& entry : sources(ui.window.session()))
    ui.source->addItem(text(entry.name()), text(entry.id().value()));
  ui.loading = false;
  ui.select();
  const auto catalog = ui.catalog_status();
  if (!catalog)
    ui.summary->setText(text(catalog.error().message()));
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellStepImportTools::inspect() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Document changed. Reopen source management."));
  }
  ui.invalidate();
  const auto catalog = ui.catalog_status();
  if (!catalog)
    return catalog;
  if (const auto* source = ui.selected()) {
    const auto report = ui.workbench.inspect(ui.window.session(), source->id());
    if (!report)
      return Result<std::size_t>::failure(report.error());
    ui.report(report.value());
    ui.summary->setText(QStringLiteral("File status inspected; saved checksum unchanged."));
    return Result<std::size_t>::success(1);
  }
  if (ui.mode->currentIndex() < 0)
    return Result<std::size_t>::failure(error("Choose the import mode explicitly"));
  const auto result = ui.workbench.prepare_register(
      ui.window.session(), ui.name->text().trimmed().toStdString(), ui.path->text().toStdString(),
      static_cast<StepImportMode>(ui.mode->currentData().toInt()));
  if (!result)
    return Result<std::size_t>::failure(result.error());
  ui.report(result.value().report);
  ui.expected->clear();
  ui.summary->setText(
      QStringLiteral("File readable. Prepare Register to save the source information."));
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellStepImportTools::inspect_geometry() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Document changed. Reopen source management."));
  }
  ui.invalidate();
  const auto inspect = [&]() -> Result<GuiStepImportGeometryPreview> {
    if (const auto* source = ui.selected()) {
      const auto report = ui.workbench.inspect(ui.window.session(), source->id());
      if (report)
        ui.report(report.value());
      return ui.workbench.inspect_geometry(ui.window.session(), source->id());
    }
    if (ui.mode->currentIndex() < 0)
      return Result<GuiStepImportGeometryPreview>::failure(
          error("Choose the import mode explicitly"));
    return ui.workbench.inspect_geometry(
        ui.window.session(), ui.name->text().trimmed().toStdString(), ui.path->text().toStdString(),
        static_cast<StepImportMode>(ui.mode->currentData().toInt()));
  };
  auto result = inspect();
  if (!result)
    return Result<std::size_t>::failure(result.error());
  const auto camera = ui.window.viewport().camera_bookmark();
  auto shown = ui.window.viewport().set_feature_previews(result.value().preview_shapes);
  if (!shown)
    return shown;
  ui.inspection_camera = camera;
  ui.inspection = std::move(result.value());
  ui.report(ui.inspection->report);
  if (!ui.inspection->registered_source)
    ui.expected->clear();
  ui.show_geometry(*ui.inspection);
  ui.watch_inspection();
  auto valid = ui.workbench.validate_inspection(ui.window.session(), *ui.inspection);
  if (!valid) {
    ui.invalidate();
    return valid;
  }
  (void)ui.window.session().task().show_preview();
  ui.window.viewport().fit_feature_preview();
  ui.summary->setText(QStringLiteral("Geometry inspection is read-only. Prepare a source change "
                                     "separately to register or update its checksum."));
  return shown;
}
Result<std::size_t> ShellStepImportTools::preview() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Document changed. Reopen source management."));
  }
  ui.invalidate();
  const auto prepare = [&]() -> Result<GuiStepImportPreview> {
    const auto* source = ui.selected();
    if (!source) {
      if (ui.operation->currentIndex() != 0 || ui.mode->currentIndex() < 0)
        return Result<GuiStepImportPreview>::failure(error("Choose the import mode explicitly"));
      return ui.workbench.prepare_register(
          ui.window.session(), ui.name->text().trimmed().toStdString(),
          ui.path->text().toStdString(),
          static_cast<StepImportMode>(ui.mode->currentData().toInt()));
    }
    if (ui.operation->currentIndex() == 1)
      return ui.workbench.prepare_refresh(ui.window.session(), source->id());
    if (ui.operation->currentIndex() == 2)
      return ui.workbench.prepare_remove(ui.window.session(), source->id());
    return Result<GuiStepImportPreview>::failure(error("Choose Update or Remove"));
  };
  const auto result = prepare();
  if (!result)
    return Result<std::size_t>::failure(result.error());
  ui.candidate = result.value();
  ui.report(result.value().report);
  ui.apply->setEnabled(true);
  (void)ui.window.session().task().show_preview();
  ui.summary->setText(
      ui.operation->currentIndex() == 2
          ? QStringLiteral("Apply removes only the reference; the source file is preserved.")
          : QStringLiteral("Inspection successful. Apply accepts the displayed source and "
                           "checksum in one step."));
  return Result<std::size_t>::success(1);
}
Result<std::size_t> ShellStepImportTools::apply() {
  auto& ui = *impl_;
  if (!ui.current()) {
    cancel();
    return Result<std::size_t>::failure(error("Document changed. Reopen source management."));
  }
  if (!ui.candidate)
    return Result<std::size_t>::failure(error("Prepare the change first"));
  const auto result = ui.workbench.apply(ui.window.session(), *ui.candidate);
  if (!result) {
    ui.invalidate();
    return result;
  }
  (void)ui.window.session().task().apply();
  cancel();
  return result;
}
void ShellStepImportTools::cancel() {
  auto& ui = *impl_;
  if (!ui.active)
    return;
  ui.active = false;
  ui.invalidate();
  if (ui.window.session().task().command_id() == task_id)
    (void)ui.window.session().task().cancel();
  ui.dock->hide();
  ui.original_json.clear();
  ui.original_path.clear();
  ui.window.refresh();
}
void ShellStepImportTools::refresh() {
  if (impl_->active && !impl_->current()) {
    cancel();
    return;
  }
  if (impl_->active && impl_->inspection) {
    const auto current =
        impl_->workbench.validate_inspection(impl_->window.session(), *impl_->inspection);
    if (!current) {
      const auto report =
          inspect_step_import_source(impl_->inspection->source, impl_->original_path.parent_path());
      impl_->invalidate();
      if (report)
        impl_->report(report.value());
      impl_->summary->setText(text(current.error().message()));
    }
  }
  impl_->action->setEnabled(impl_->available());
}
bool ShellStepImportTools::eventFilter(QObject* watched, QEvent* event) {
  const auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && !QApplication::activeModalWidget() && event->type() == QEvent::KeyPress &&
      widget && (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
