#pragma once
#include "blcad/core/part_document.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <array>
#include <map>
namespace blcad::gui::detail {
struct SketchDrivenControls {
  struct Binding {
    QComboBox* parameter{};
    QLineEdit* value{};
    ParameterType type;
    QString local, initial;
    bool was_local = true;
  };
  std::map<QString, QWidget*> fields;
  QWidget *panel{}, *content{};
  QScrollArea* scroll{};
  QComboBox *mode{}, *body{}, *sketch{}, *source_kind{}, *profile{}, *curve{}, *plane{}, *side{},
      *extent{}, *limit_body{}, *limit_face{}, *crossings{}, *mapping{}, *font{}, *face{};
  QLineEdit *name{}, *text{}, *anchor_x{}, *anchor_y{}, *seam{};
  std::array<QLineEdit*, 3> direction{};
  Binding size, second, distance, draft, height, orientation;
  QCheckBox *draft_enabled{}, *orientation_enabled{};
  QListWidget *curves{}, *supports{};
  QTableWidget* tokens{};
  QGroupBox *rib{}, *emboss{}, *curve_group{}, *text_group{}, *support_group{};
  QPushButton *preview{}, *apply{}, *cancel{}, *source_edit{}, *breadcrumb{}, *curve_add{},
      *face_add{}, *token_add{};
  QLabel* status{};
  explicit SketchDrivenControls(QWidget* parent) : panel(new QWidget(parent)) {
    auto* column = new QVBoxLayout(panel);
    scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    content = new QWidget(scroll);
    auto* rows = new QVBoxLayout(content);
    rows->setContentsMargins(8, 8, 8, 8);
    auto* common = new QFormLayout;
    common->setRowWrapPolicy(QFormLayout::WrapLongRows);
    rows->addLayout(common);
    const auto group = [&](const QString& title) {
      auto* g = new QGroupBox(title, content);
      rows->addWidget(g);
      return g;
    };
    const auto form = [](QWidget* parent) {
      auto* f = new QFormLayout(parent);
      f->setRowWrapPolicy(QFormLayout::WrapLongRows);
      return f;
    };
    mode = combo("mode", {"Rib", "Web", "Emboss", "Engrave"});
    common->addRow("Feature", mode);
    name = line("name");
    common->addRow("Name", name);
    body = combo("body");
    common->addRow("Target Body", body);
    sketch = combo("sketch");
    common->addRow("Source Sketch", sketch);
    source_kind = combo("source_kind", {"Open curves", "Closed profile", "Text"});
    common->addRow("Source", source_kind);
    profile = combo("profile");
    common->addRow("Profile", profile);
    source_edit = button("source_edit", "Edit source Sketch");
    common->addRow(source_edit);
    curve_group = group("Ordered source curves");
    auto* source_rows = new QVBoxLayout(curve_group);
    curve = combo("curve");
    source_rows->addWidget(curve);
    curve_add = button("curve_add", "Add curve");
    source_rows->addWidget(curve_add);
    curves = list("curves");
    source_rows->addWidget(curves);
    list_buttons(source_rows, curves, "curves");
    auto* size_group = group("Size and binding");
    auto* sizes = form(size_group);
    size = binding(sizes, "size", "Thickness / Depth", ParameterType::Length, "1");
    rib = group("Rib / Web");
    auto* ribs = form(rib);
    plane = combo("plane", {"Perpendicular to Sketch", "Parallel to Sketch"});
    ribs->addRow("Extrusion", plane);
    auto* directions = new QWidget(content);
    auto* axes = new QHBoxLayout(directions);
    axes->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
      direction[i] = line(QString("direction_%1").arg(i));
      direction[i]->setText(i == 2 ? "1" : "0");
      direction[i]->setPlaceholderText(i == 0 ? "X" : i == 1 ? "Y" : "Z");
      axes->addWidget(direction[i]);
    }
    ribs->addRow("Sketch direction X / Y / Z", directions);
    side = combo("side", {"Symmetric", "First side", "Second side", "Asymmetric"});
    ribs->addRow("Thickness side", side);
    second = binding(ribs, "second", "Second thickness", ParameterType::Length, "1");
    draft_enabled = new QCheckBox("Draft", content);
    register_field("draft_enabled", draft_enabled);
    ribs->addRow(draft_enabled);
    draft = binding(ribs, "draft", "Draft angle", ParameterType::Angle, "0");
    extent = combo("extent", {"Distance", "To face", "To Body"});
    ribs->addRow("Termination", extent);
    distance = binding(ribs, "distance", "Distance", ParameterType::Length, "12");
    limit_face = combo("limit_face");
    ribs->addRow("Limit face", limit_face);
    limit_body = combo("limit_body");
    ribs->addRow("Limit Body", limit_body);
    crossings = combo("crossings", {"Union crossings", "Reject crossings"});
    ribs->addRow("Crossings", crossings);
    emboss = group("Emboss / Engrave");
    auto* mapped = form(emboss);
    mapping = combo("mapping", {"Planar projection", "Cylindrical wrap", "Conical wrap"});
    mapped->addRow("Mapping", mapping);
    anchor_x = line("anchor_x");
    anchor_y = line("anchor_y");
    seam = line("seam");
    anchor_x->setText("0");
    anchor_y->setText("0");
    seam->setText("0");
    mapped->addRow("Anchor X (mm)", anchor_x);
    mapped->addRow("Anchor Y (mm)", anchor_y);
    mapped->addRow("Wrap seam (deg)", seam);
    orientation_enabled = new QCheckBox("Orientation", content);
    register_field("orientation_enabled", orientation_enabled);
    mapped->addRow(orientation_enabled);
    orientation = binding(mapped, "orientation", "Orientation", ParameterType::Angle, "0");
    support_group = group("Ordered target supports");
    auto* support_rows = new QVBoxLayout(support_group);
    face = combo("face");
    support_rows->addWidget(face);
    face_add = button("face_add", "Add support face");
    support_rows->addWidget(face_add);
    supports = list("supports");
    support_rows->addWidget(supports);
    list_buttons(support_rows, supports, "supports");
    text_group = group("Sketch text source");
    auto* text_rows = new QVBoxLayout(text_group);
    auto* texts = new QFormLayout;
    texts->setRowWrapPolicy(QFormLayout::WrapLongRows);
    text_rows->addLayout(texts);
    text = line("text");
    text->setText("Label");
    texts->addRow("Text / ${token}", text);
    font = combo("font", {"Barlow", "IBM Plex Mono"});
    texts->addRow("Outline font", font);
    height = binding(texts, "height", "Text height", ParameterType::Length, "4");
    tokens = new QTableWidget(0, 3, content);
    register_field("tokens", tokens);
    tokens->setHorizontalHeaderLabels({"Token", "Parameter", "Digits"});
    tokens->setMinimumHeight(100);
    tokens->setMaximumHeight(175);
    text_rows->addWidget(tokens);
    auto* token_buttons = new QHBoxLayout;
    token_add = button("token_add", "Add binding");
    auto* token_remove = button("token_remove", "Remove binding");
    token_buttons->addWidget(token_add);
    token_buttons->addWidget(token_remove);
    text_rows->addLayout(token_buttons);
    QObject::connect(token_remove, &QPushButton::clicked, panel, [this] {
      if (tokens->currentRow() >= 0)
        tokens->removeRow(tokens->currentRow());
    });
    status = new QLabel(content);
    register_field("status", status);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rows->addWidget(status);
    rows->addStretch();
    scroll->setWidget(content);
    column->addWidget(scroll);
    breadcrumb = button("breadcrumb", "Finish Sketch → return to feature");
    breadcrumb->hide();
    column->addWidget(breadcrumb);
    auto* buttons = new QHBoxLayout;
    preview = button("preview", "Preview");
    apply = button("apply", "Apply");
    cancel = button("cancel", "Cancel");
    apply->setProperty("guiRole", "primary");
    buttons->addWidget(preview);
    buttons->addWidget(apply);
    buttons->addWidget(cancel);
    column->addLayout(buttons);
  }
  void register_field(const QString& key, QWidget* widget) {
    widget->setObjectName("blcad.sketch_driven." + key);
    fields[key] = widget;
  }
  QLineEdit* line(const QString& key) {
    auto* w = new QLineEdit(content);
    register_field(key, w);
    return w;
  }
  QComboBox* combo(const QString& key, const QStringList& labels = {}) {
    auto* w = new QComboBox(content);
    w->addItems(labels);
    w->setMinimumContentsLength(14);
    w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    w->setMaxVisibleItems(12);
    register_field(key, w);
    return w;
  }
  QPushButton* button(const QString& key, const QString& label) {
    auto* w = new QPushButton(label, content);
    register_field(key, w);
    return w;
  }
  QListWidget* list(const QString& key) {
    auto* w = new QListWidget(content);
    register_field(key, w);
    w->setMinimumHeight(72);
    w->setMaximumHeight(132);
    return w;
  }
  Binding binding(QFormLayout* form, const QString& key, const QString& label, ParameterType type,
                  const QString& initial) {
    Binding out{combo(key + "_binding"), line(key), type, initial, initial};
    out.value->setText(initial);
    form->addRow(label + " binding", out.parameter);
    form->addRow(type == ParameterType::Angle ? "Angle (deg)" : "Value (mm)", out.value);
    return out;
  }
  void list_buttons(QVBoxLayout* rows, QListWidget* list, const QString& prefix) {
    auto* buttons = new QHBoxLayout;
    auto* remove = button(prefix + "_remove", "Remove");
    auto* up = button(prefix + "_up", "Up");
    auto* down = button(prefix + "_down", "Down");
    buttons->addWidget(remove);
    buttons->addWidget(up);
    buttons->addWidget(down);
    rows->addLayout(buttons);
    QObject::connect(remove, &QPushButton::clicked, panel,
                     [list] { delete list->takeItem(list->currentRow()); });
    QObject::connect(up, &QPushButton::clicked, panel, [list] {
      int row = list->currentRow();
      if (row > 0) {
        auto* item = list->takeItem(row);
        list->insertItem(row - 1, item);
        list->setCurrentRow(row - 1);
      }
    });
    QObject::connect(down, &QPushButton::clicked, panel, [list] {
      int row = list->currentRow();
      if (row >= 0 && row + 1 < list->count()) {
        auto* item = list->takeItem(row);
        list->insertItem(row + 1, item);
        list->setCurrentRow(row + 1);
      }
    });
  }
  std::array<Binding*, 6> bindings() {
    return {&size, &second, &distance, &draft, &height, &orientation};
  }
  void bind_catalog(const PartDocument& p, Binding& binding,
                    const ParameterId& selected = ParameterId("")) {
    binding.parameter->clear();
    binding.parameter->addItem("New value", QString());
    for (const auto& parameter : p.parameters())
      if (parameter.type() == binding.type)
        binding.parameter->addItem(QString::fromStdString(parameter.name()),
                                   QString::fromStdString(parameter.id().value()));
    int at = binding.parameter->findData(QString::fromStdString(selected.value()));
    binding.parameter->setCurrentIndex(at < 0 ? 0 : at);
    select_binding(p, binding);
  }
  void select_binding(const PartDocument& p, Binding& b) {
    if (b.was_local)
      b.local = b.value->text();
    const auto* parameter =
        p.find_parameter(ParameterId(b.parameter->currentData().toString().toStdString()));
    b.was_local = !parameter;
    b.value->setReadOnly(parameter && parameter->is_expression());
    const QString value = parameter ? QString::number(parameter->type() == ParameterType::Angle
                                                          ? parameter->value().degrees()
                                                          : parameter->value().millimeters(),
                                                      'g', 17)
                                    : b.local;
    b.value->setText(value);
    b.initial = value;
    b.value->setToolTip(parameter && parameter->formula()
                            ? "Formula: " + QString::fromStdString(*parameter->formula())
                            : QString());
  }
  void render() {
    const bool is_rib = mode->currentIndex() < 2;
    rib->setVisible(is_rib);
    emboss->setVisible(!is_rib);
    support_group->setVisible(!is_rib);
    curve_group->setVisible(is_rib && source_kind->currentIndex() == 0);
    profile->setEnabled(source_kind->currentIndex() == 1);
    text_group->setVisible(!is_rib && source_kind->currentIndex() == 2);
    second.parameter->setEnabled(is_rib && side->currentIndex() == 3);
    second.value->setEnabled(is_rib && side->currentIndex() == 3);
    distance.parameter->setEnabled(is_rib && extent->currentIndex() == 0);
    distance.value->setEnabled(is_rib && extent->currentIndex() == 0);
    limit_face->setEnabled(is_rib && extent->currentIndex() == 1);
    limit_body->setEnabled(is_rib && extent->currentIndex() == 2);
    draft.parameter->setEnabled(draft_enabled->isChecked());
    draft.value->setEnabled(draft_enabled->isChecked());
    seam->setEnabled(mapping->currentIndex() != 0);
    orientation.parameter->setEnabled(orientation_enabled->isChecked());
    orientation.value->setEnabled(orientation_enabled->isChecked());
  }
};
} // namespace blcad::gui::detail
