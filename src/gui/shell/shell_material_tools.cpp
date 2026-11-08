#include "blcad/gui/shell/shell_material_tools.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>
#include <cmath>

namespace blcad::gui {
namespace {
QString text(const std::string& value) {
  return QString::fromStdString(value);
}
Error error(std::string value) {
  return Error::validation("gui.shell.material", std::move(value));
}
Result<std::size_t> fail(std::string value) {
  return Result<std::size_t>::failure(error(std::move(value)));
}
QString key_text(const EngineeringResourceKey& key) {
  return text(key.id().value()) + " @ " + text(key.revision());
}
QString provenance_text(const EngineeringProvenance& p) {
  const auto origin = p.origin() == EngineeringDataOrigin::Normative      ? "Normative"
                      : p.origin() == EngineeringDataOrigin::Manufacturer ? "Manufacturer"
                                                                          : "User-defined";
  return QString::fromLatin1(origin) + ": " + text(p.source()) +
         (p.reference() ? " | " + text(*p.reference()) : QString{}) +
         (p.uri() ? " | " + text(*p.uri()) : QString{}) +
         (p.license() ? " | " + text(*p.license()) : QString{});
}
std::optional<std::string> optional_text(QLineEdit* edit) {
  const auto value = edit->text().trimmed().toStdString();
  return value.empty() ? std::nullopt : std::optional(value);
}
} // namespace

struct ShellMaterialTools::Impl {
  explicit Impl(ShellWindow& owner) : window(owner) {}
  ShellWindow& window;
  GuiMaterialController controller;
  QAction* action{};
  QDockWidget* dock{};
  QComboBox *materials{}, *parts{}, *bodies{}, *assignment{}, *origin{};
  QLineEdit *id{}, *revision{}, *name{}, *source{}, *reference{}, *uri{}, *license{};
  std::array<QLineEdit*, 4> values{};
  QLabel *summary{}, *details{}, *effective{};
  QListWidget* members{};
  QPushButton *assign{}, *snapshot{}, *adopt{}, *remove{}, *apply{};
  std::vector<EngineeringResourceKey> keys;
  std::optional<EngineeringMaterial> loaded;
  bool populating = false, editor_dirty = false, prefix_child = false;

  const PartDocument* selected_part() const {
    if (const auto* part = controller.part())
      return part;
    return controller.project() ? controller.project()->find_part_document(
                                      DocumentId(parts->currentData().toString().toStdString()))
                                : nullptr;
  }
  std::optional<BodyId> selected_body() const {
    const auto value = bodies->currentData().toString().toStdString();
    return value.empty() ? std::nullopt : std::optional(BodyId(value));
  }
  std::optional<EngineeringResourceKey> selected_key(QComboBox* combo) const {
    const auto index = combo->currentData().toInt();
    return index >= 0 && static_cast<std::size_t>(index) < keys.size() ? std::optional(keys[index])
                                                                       : std::nullopt;
  }
  std::vector<DocumentId> selected_members() const {
    std::vector<DocumentId> selected;
    for (int i = 0; i < members->count(); ++i) {
      const auto* item = members->item(i);
      if (item->checkState() == Qt::Checked)
        selected.emplace_back(item->data(Qt::UserRole).toString().toStdString());
    }
    return selected;
  }
  bool available() const {
    const auto& session = window.session();
    return !window.active_sketch() && !session.task().active() &&
           ((session.part_modeling_ready() &&
             (!session.part_workflow_active() || session.history_prefix_count())) ||
            (session.project() && session.derived_results_fresh()));
  }
  void controls() {
    const auto* part = selected_part();
    const bool writable = controller.active() && part && !part->is_reference_part();
    const bool catalog_writable = controller.active() && (controller.project() || writable);
    assign->setEnabled(writable);
    snapshot->setEnabled(catalog_writable);
    adopt->setEnabled(catalog_writable && loaded.has_value());
    remove->setEnabled(catalog_writable && loaded.has_value());
    apply->setEnabled(controller.active() && !editor_dirty);
    for (auto* edit : {id, revision, name, source, reference, uri, license})
      edit->setReadOnly(!catalog_writable);
    for (auto* edit : values)
      edit->setReadOnly(!catalog_writable);
    origin->setEnabled(catalog_writable);
  }
  void load() {
    populating = true;
    loaded.reset();
    if (const auto key = selected_key(materials))
      if (const auto* material = controller.catalog()->find_material(*key))
        loaded = *material;
    for (auto* edit : {id, revision, name, source, reference, uri, license})
      edit->clear();
    for (auto* edit : values)
      edit->clear();
    origin->setCurrentIndex(0);
    QString disclosure;
    if (loaded) {
      id->setText(text(loaded->key().id().value()));
      revision->setText(text(loaded->key().revision()));
      name->setText(text(loaded->name()));
      const auto& provenance = loaded->provenance();
      origin->setCurrentIndex(origin->findData(static_cast<int>(provenance.origin())));
      source->setText(text(provenance.source()));
      reference->setText(text(provenance.reference().value_or("")));
      uri->setText(text(provenance.uri().value_or("")));
      license->setText(text(provenance.license().value_or("")));
      disclosure = provenance_text(provenance);
      const auto& p = loaded->properties();
      if (p.density) {
        values[0]->setText(QString::number(p.density->value.kg_per_m3(), 'g', 17));
        disclosure += "\nDensity: " + provenance_text(p.density->provenance);
      }
      const std::array pressures{p.young_modulus, p.yield_strength, p.tensile_strength};
      const std::array labels{"Young modulus", "Yield strength", "Tensile strength"};
      for (std::size_t i = 0; i < pressures.size(); ++i)
        if (pressures[i]) {
          values[i + 1]->setText(QString::number(pressures[i]->value.megapascals(), 'g', 17));
          disclosure += "\n" + QString::fromLatin1(labels[i]) + ": " +
                        provenance_text(pressures[i]->provenance);
        }
    }
    details->setText(disclosure);
    details->setToolTip(disclosure);
    editor_dirty = false;
    populating = false;
    controls();
  }
  void targets(bool rebuild_parts) {
    const QSignalBlocker block_parts(parts), block_bodies(bodies), block_assignment(assignment);
    const auto old_part = parts->currentData();
    const auto old_body = bodies->currentData();
    if (rebuild_parts) {
      parts->clear();
      if (const auto* p = controller.part())
        parts->addItem(text(p->name()), text(p->id().value()));
      if (const auto* root = controller.project())
        for (const auto& p : root->part_documents()) {
          const auto qualified = text(p.name()) + " — " + text(p.id().value());
          parts->addItem(qualified, text(p.id().value()));
          parts->setItemData(parts->count() - 1, qualified, Qt::ToolTipRole);
        }
      const auto index = parts->findData(old_part);
      if (index >= 0)
        parts->setCurrentIndex(index);
    }
    bodies->clear();
    bodies->addItem("Part default", QString{});
    const auto* part = selected_part();
    if (part)
      for (const auto& body : part->bodies())
        bodies->addItem(text(body.name()), text(body.id().value()));
    const auto index = bodies->findData(old_body);
    if (index >= 0)
      bodies->setCurrentIndex(index);
    show_assignment();
  }
  void show_assignment() {
    parts->setToolTip(parts->currentText());
    const QSignalBlocker blocker(assignment);
    assignment->clear();
    assignment->addItem(selected_body() ? "Inherit Part default" : "Unassigned", -1);
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const auto* material = controller.catalog()->find_material(keys[i]);
      assignment->addItem(text(material->name()) + " — " + key_text(keys[i]), static_cast<int>(i));
    }
    const auto* part = selected_part();
    std::optional<EngineeringResourceKey> pin;
    const EngineeringMaterial* material = nullptr;
    if (part) {
      pin = part->default_material();
      if (const auto body = selected_body()) {
        pin.reset();
        for (const auto& assignment : part->body_materials())
          if (assignment.body == *body)
            pin = assignment.material;
        material = part->effective_material_for_body(*body);
      } else if (pin)
        material = part->engineering_resources().find_material(*pin);
    }
    if (pin)
      for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == *pin)
          assignment->setCurrentIndex(assignment->findData(static_cast<int>(i)));
    QString description = material ? text(material->name()) + " — " + key_text(material->key())
                                   : QStringLiteral("Unassigned");
    if (selected_body())
      description += pin ? " (Body override)" : " (inherited)";
    if (part && part->is_reference_part())
      description += "\nReference Part: read-only";
    if (controller.project())
      description += "\nPart definition: applies to all its occurrences.";
    effective->setText(description);
    effective->setToolTip(description);
    controls();
  }
  void populate(std::optional<EngineeringResourceKey> select = {}) {
    const QSignalBlocker block(materials);
    materials->clear();
    materials->addItem("New material", -1);
    keys.clear();
    if (controller.catalog())
      for (const auto& resource : controller.catalog()->entries())
        if (const auto* material = std::get_if<EngineeringMaterial>(&resource)) {
          materials->addItem(text(material->name()) + " — " + key_text(material->key()),
                             static_cast<int>(keys.size()));
          keys.push_back(material->key());
        }
    if (select)
      for (std::size_t i = 0; i < keys.size(); ++i)
        if (keys[i] == *select)
          materials->setCurrentIndex(static_cast<int>(i + 1));
    targets(true);
    load();
  }
  Result<EngineeringMaterial> editor_material() const {
    const auto key =
        EngineeringResourceKey::create(EngineeringResourceId(id->text().trimmed().toStdString()),
                                       revision->text().trimmed().toStdString());
    if (!key)
      return Result<EngineeringMaterial>::failure(key.error());
    const auto provenance = EngineeringProvenance::create(
        static_cast<EngineeringDataOrigin>(origin->currentData().toInt()),
        source->text().trimmed().toStdString(), optional_text(reference), optional_text(uri),
        optional_text(license));
    if (!provenance)
      return Result<EngineeringMaterial>::failure(provenance.error());
    EngineeringMaterialProperties p;
    for (std::size_t i = 0; i < values.size(); ++i) {
      const auto input = values[i]->text().trimmed();
      if (input.isEmpty())
        continue;
      bool ok = false;
      const double value = QLocale::c().toDouble(input, &ok);
      if (!ok || !std::isfinite(value))
        return Result<EngineeringMaterial>::failure(
            error("Enter finite numeric properties using a decimal point"));
      if (i == 0) {
        const auto density = MassDensity::from_kg_per_m3(value);
        if (!density)
          return Result<EngineeringMaterial>::failure(density.error());
        p.density = SourcedEngineeringValue{density.value(), provenance.value()};
        if (loaded && loaded->properties().density &&
            loaded->properties().density->value == density.value())
          p.density = loaded->properties().density;
      } else {
        const auto pressure = EngineeringPressure::from_mpa(value);
        if (!pressure)
          return Result<EngineeringMaterial>::failure(pressure.error());
        auto* target = i == 1 ? &p.young_modulus : i == 2 ? &p.yield_strength : &p.tensile_strength;
        *target = SourcedEngineeringValue{pressure.value(), provenance.value()};
        if (loaded) {
          const auto& old = i == 1   ? loaded->properties().young_modulus
                            : i == 2 ? loaded->properties().yield_strength
                                     : loaded->properties().tensile_strength;
          if (old && input == QString::number(old->value.megapascals(), 'g', 17))
            *target = old;
        }
      }
    }
    return EngineeringMaterial::create(key.value(), name->text().trimmed().toStdString(),
                                       provenance.value(), p);
  }
};

ShellMaterialTools::ShellMaterialTools(ShellWindow& window, ShellRibbon& ribbon, int manage_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(window)) {
  auto& ui = *impl_;
  ui.action = new QAction("Material", this);
  ui.action->setObjectName("blcad.action.material");
  ribbon.add_button(ribbon.add_group(manage_tab, "Materials"), ui.action);
  connect(ui.action, &QAction::triggered, this, [this] {
    const auto result = begin();
    if (!result)
      impl_->window.report_error(result.error());
  });
  ui.dock = new QDockWidget("Material", &window);
  ui.dock->setObjectName("blcad.shell.material_task");
  ui.dock->setMinimumWidth(300);
  auto* panel = new QWidget(ui.dock);
  auto* layout = new QVBoxLayout(panel);
  auto* scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* column = new QVBoxLayout(content);
  auto label = [](QWidget* parent) {
    auto* result = new QLabel(parent);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    result->setMinimumWidth(0);
    result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return result;
  };
  auto group = [&](const char* id, const char* title) {
    auto* result = new ShellPropertyGroup(QString("blcad.material.") + id, title, content);
    column->addWidget(result);
    return result;
  };
  auto combo = [&](QFormLayout* form, QWidget* parent, const char* id, const char* title) {
    auto* result = new QComboBox(parent);
    result->setObjectName(QString("blcad.material.") + id);
    result->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    result->setMinimumContentsLength(10);
    form->addRow(title, result);
    return result;
  };
  auto edit = [&](QFormLayout* form, QWidget* parent, const char* id, const char* title) {
    auto* result = new QLineEdit(parent);
    result->setObjectName(QString("blcad.material.") + id);
    form->addRow(title, result);
    return result;
  };
  auto button = [&](QWidget* parent, QLayout* box, const char* id, const char* title) {
    auto* result = new QPushButton(title, parent);
    result->setObjectName(QString("blcad.material.") + id);
    box->addWidget(result);
    return result;
  };
  auto* target = group("target", "Assignment");
  auto* target_form = new QFormLayout;
  target_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  target->content_layout().addLayout(target_form);
  ui.parts = combo(target_form, &target->content(), "part", "Part definition");
  ui.bodies = combo(target_form, &target->content(), "body", "Target");
  ui.assignment = combo(target_form, &target->content(), "assignment", "Material revision");
  ui.effective = label(&target->content());
  ui.effective->setMaximumHeight(ui.effective->fontMetrics().lineSpacing() * 3);
  ui.effective->setObjectName("blcad.material.effective");
  target->content_layout().addWidget(ui.effective);
  ui.assign = button(&target->content(), &target->content_layout(), "assign", "Stage Assignment");
  auto* catalog = group("catalog", "Material library");
  auto* catalog_form = new QFormLayout;
  catalog_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  catalog->content_layout().addLayout(catalog_form);
  ui.materials = combo(catalog_form, &catalog->content(), "material", "Snapshot");
  ui.details = label(&catalog->content());
  ui.details->setMaximumHeight(ui.details->fontMetrics().lineSpacing() * 8);
  ui.details->setObjectName("blcad.material.details");
  catalog->content_layout().addWidget(ui.details);
  auto* editor = group("editor", "New immutable snapshot");
  auto* hint = label(&editor->content());
  hint->setText("Existing revisions stay unchanged. Change the revision before staging new values. "
                "Blank properties remain unknown. Unchanged property values retain their source; "
                "changed values use the source below. Density uses kg/m³; pressures use MPa.");
  editor->content_layout().addWidget(hint);
  auto* form = new QFormLayout;
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  editor->content_layout().addLayout(form);
  ui.id = edit(form, &editor->content(), "id", "Material ID");
  ui.revision = edit(form, &editor->content(), "revision", "Revision");
  ui.name = edit(form, &editor->content(), "name", "Name");
  ui.origin = combo(form, &editor->content(), "origin", "Origin");
  ui.origin->addItem("User-defined", static_cast<int>(EngineeringDataOrigin::UserDefined));
  ui.origin->addItem("Manufacturer", static_cast<int>(EngineeringDataOrigin::Manufacturer));
  ui.origin->addItem("Normative", static_cast<int>(EngineeringDataOrigin::Normative));
  ui.source = edit(form, &editor->content(), "source", "Source");
  ui.reference = edit(form, &editor->content(), "reference", "Reference");
  ui.uri = edit(form, &editor->content(), "uri", "Source URI");
  ui.license = edit(form, &editor->content(), "license", "License");
  const std::array ids{"density", "young", "yield", "tensile"};
  const std::array titles{"Density (kg/m³)", "Young modulus (MPa)", "Yield strength (MPa)",
                          "Tensile strength (MPa)"};
  for (std::size_t i = 0; i < ids.size(); ++i)
    ui.values[i] = edit(form, &editor->content(), ids[i], titles[i]);
  ui.snapshot = button(&editor->content(), &editor->content_layout(), "snapshot", "Stage Snapshot");
  auto* revisions = group("updates", "Revision updates / removal");
  auto* update_hint = label(&revisions->content());
  update_hint->setText(
      "Adopt replaces pins to the selected library snapshot with the new revision "
      "above. In a Project, check each Part to update. Unchecked Parts retain their revision. "
      "Removal requires all references to be cleared first.");
  revisions->content_layout().addWidget(update_hint);
  ui.members = new QListWidget(&revisions->content());
  ui.members->setObjectName("blcad.material.members");
  ui.members->setMaximumHeight(120);
  revisions->content_layout().addWidget(ui.members);
  ui.adopt = button(&revisions->content(), &revisions->content_layout(), "adopt",
                    "Stage Revision Adoption");
  ui.remove = button(&revisions->content(), &revisions->content_layout(), "remove",
                     "Stage Snapshot Removal");
  column->addStretch();
  scroll->setWidget(content);
  layout->addWidget(scroll);
  ui.summary = label(panel);
  ui.summary->setObjectName("blcad.material.summary");
  layout->addWidget(ui.summary);
  auto* footer = new QHBoxLayout;
  ui.apply = button(panel, footer, "apply", "Apply");
  auto* cancel_button = button(panel, footer, "cancel", "Cancel");
  layout->addLayout(footer);
  ui.dock->setWidget(panel);
  window.addDockWidget(Qt::RightDockWidgetArea, ui.dock);
  ui.dock->hide();
  auto report = [this](const Result<std::size_t>& result) {
    impl_->summary->setText(result ? "Changes staged; Apply publishes them together."
                                   : text(result.error().message()));
  };
  connect(ui.assign, &QPushButton::clicked, this, [this, report] { report(stage_assignment()); });
  connect(ui.snapshot, &QPushButton::clicked, this, [this, report] { report(stage_snapshot()); });
  connect(ui.adopt, &QPushButton::clicked, this, [this, report] { report(stage_snapshot(true)); });
  connect(ui.remove, &QPushButton::clicked, this, [this, report] { report(remove_snapshot()); });
  connect(ui.apply, &QPushButton::clicked, this, [this] {
    const auto result = apply();
    if (!result)
      impl_->summary->setText(text(result.error().message()));
  });
  connect(cancel_button, &QPushButton::clicked, this, [this] { cancel(); });
  connect(ui.parts, &QComboBox::currentIndexChanged, this, [this] { impl_->targets(false); });
  connect(ui.bodies, &QComboBox::currentIndexChanged, this, [this] { impl_->show_assignment(); });
  connect(ui.materials, &QComboBox::currentIndexChanged, this, [this] { impl_->load(); });
  auto edited = [this] {
    if (!impl_->populating) {
      impl_->editor_dirty = true;
      impl_->summary->setText(
          "Stage the snapshot or select a library entry to discard its editor changes.");
      impl_->controls();
    }
  };
  for (auto* field : {ui.id, ui.revision, ui.name, ui.source, ui.reference, ui.uri, ui.license})
    connect(field, &QLineEdit::textChanged, this, edited);
  for (auto* field : ui.values)
    connect(field, &QLineEdit::textChanged, this, edited);
  connect(ui.origin, &QComboBox::currentIndexChanged, this, edited);
  qApp->installEventFilter(this);
  ui.dock->installEventFilter(this);
}
ShellMaterialTools::~ShellMaterialTools() = default;
bool ShellMaterialTools::active() const noexcept {
  return impl_->controller.active();
}
QAction& ShellMaterialTools::action() noexcept {
  return *impl_->action;
}

Result<std::size_t> ShellMaterialTools::begin(std::optional<DocumentId> part,
                                              std::optional<BodyId> body) {
  auto& ui = *impl_;
  if (!ui.available())
    return fail("Material requires a ready document and an idle command context");
  if (body && !part)
    return fail("A Body target requires its owning Part");
  if (part) {
    const auto* target = ui.window.session().part_document();
    if (const auto* root = ui.window.session().project())
      target = root->find_part_document(*part);
    if (!target || target->id() != *part || (body && !target->find_body(*body)))
      return fail("Choose an existing owned Part or Body");
  }
  ui.prefix_child = ui.window.session().history_prefix_count().has_value();
  if (ui.prefix_child) {
    const auto child =
        ui.window.session().begin_part_history_command(std::string(GuiMaterialController::task_id));
    if (!child) {
      ui.prefix_child = false;
      return child;
    }
  }
  const auto task = ui.window.session().task().begin(std::string(GuiMaterialController::task_id));
  if (!task) {
    if (ui.prefix_child)
      ui.window.session().cancel_part_history_command();
    ui.prefix_child = false;
    return fail("Could not reserve the Material task");
  }
  const auto result = ui.controller.begin(ui.window.session());
  if (!result) {
    (void)ui.window.session().task().cancel();
    if (ui.prefix_child)
      ui.window.session().cancel_part_history_command();
    ui.prefix_child = false;
    return result;
  }
  (void)ui.window.session().task().begin_parameter_editing();
  ui.populate();
  ui.members->clear();
  if (ui.controller.project())
    for (const auto& p : ui.controller.project()->part_documents()) {
      auto* item = new QListWidgetItem(text(p.name()) + " — " + text(p.id().value()), ui.members);
      item->setData(Qt::UserRole, text(p.id().value()));
      item->setFlags(p.is_reference_part() ? Qt::NoItemFlags
                                           : Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
      item->setCheckState(Qt::Unchecked);
    }
  ui.members->setVisible(ui.controller.project() != nullptr);
  if (part)
    ui.parts->setCurrentIndex(ui.parts->findData(text(part->value())));
  if (body)
    ui.bodies->setCurrentIndex(ui.bodies->findData(text(body->value())));
  ui.summary->setText("Assignments and library changes remain pending until Apply.");
  ui.dock->show();
  ui.dock->raise();
  ui.window.refresh();
  return result;
}
Result<std::size_t> ShellMaterialTools::stage_assignment() {
  auto& ui = *impl_;
  const auto* part = ui.selected_part();
  if (!part)
    return fail("Choose a Part definition");
  const auto result = ui.controller.stage_assignment(
      ui.window.session(), part->id(), ui.selected_body(), ui.selected_key(ui.assignment));
  if (result)
    ui.show_assignment();
  return result;
}
Result<std::size_t> ShellMaterialTools::stage_snapshot(bool adopt) {
  auto& ui = *impl_;
  const auto material = ui.editor_material();
  if (!material)
    return Result<std::size_t>::failure(material.error());
  const auto original = ui.selected_key(ui.materials);
  if (adopt && !original)
    return fail("Choose the library snapshot whose pins should be updated");
  const auto result = adopt ? ui.controller.stage_revision(ui.window.session(), *original,
                                                           material.value(), ui.selected_members())
                            : ui.controller.stage_material(ui.window.session(), material.value());
  if (result)
    ui.populate(material.value().key());
  return result;
}
Result<std::size_t> ShellMaterialTools::remove_snapshot() {
  auto& ui = *impl_;
  const auto key = ui.selected_key(ui.materials);
  if (!key)
    return fail("Choose a material snapshot to remove");
  if (ui.editor_dirty)
    return fail("Stage or discard the pending snapshot editor changes first");
  const auto result = ui.controller.stage_remove(ui.window.session(), *key, ui.selected_members());
  if (result)
    ui.populate();
  return result;
}
Result<std::size_t> ShellMaterialTools::apply() {
  auto& ui = *impl_;
  if (ui.editor_dirty)
    return fail("Stage or discard the pending snapshot editor changes first");
  auto accepted = ui.window.session();
  auto candidate = ui.controller;
  const auto result = candidate.apply(accepted, "Apply Material Changes");
  if (!result)
    return result;
  if (ui.prefix_child) {
    const auto published = accepted.publish_part_history_command("Apply Material Changes");
    if (!published)
      return published;
  }
  ui.window.session() = std::move(accepted);
  ui.prefix_child = false;
  cancel();
  return result;
}
void ShellMaterialTools::cancel() {
  auto& ui = *impl_;
  if (!active())
    return;
  ui.controller.cancel();
  if (ui.window.session().task().command_id() == GuiMaterialController::task_id)
    (void)ui.window.session().task().cancel();
  if (ui.prefix_child)
    ui.window.session().cancel_part_history_command();
  ui.prefix_child = false;
  ui.loaded.reset();
  ui.editor_dirty = false;
  ui.dock->hide();
  ui.window.refresh();
}
void ShellMaterialTools::refresh() {
  auto& ui = *impl_;
  if (active() && !ui.controller.current(ui.window.session())) {
    cancel();
    return;
  }
  ui.action->setEnabled(ui.available());
}
bool ShellMaterialTools::eventFilter(QObject* watched, QEvent* event) {
  auto* widget = qobject_cast<QWidget*>(watched);
  if (active() && event->type() == QEvent::KeyPress && widget &&
      (widget == &impl_->window || impl_->window.isAncestorOf(widget)) &&
      static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
    cancel();
    return true;
  }
  if (active() && watched == impl_->dock && event->type() == QEvent::Close)
    cancel();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
