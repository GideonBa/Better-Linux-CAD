#include "direct_edit_tools_impl.hpp"
#include <QActionGroup>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QVBoxLayout>
namespace blcad::gui {
void ShellDirectEditTools::Impl::controls() {
  panel = new QWidget(dock);
  auto* layout = new QVBoxLayout(panel);
  auto* toolbar = new QToolBar(panel);
  toolbar->setObjectName("blcad.direct_edit.mini_toolbar");
  auto* actions = new QActionGroup(toolbar);
  const char* labels[]{"Move", "Rotate", "Offset", "Size", "Delete and Heal"};
  mode = new QComboBox(panel);
  fields["mode"] = mode;
  for (int i = 0; i < 5; ++i) {
    mode->addItem(labels[i]);
    auto* a = toolbar->addAction(labels[i]);
    a->setCheckable(true);
    actions->addAction(a);
    QObject::connect(a, &QAction::triggered, &owner, [this, i] { mode->setCurrentIndex(i); });
    QObject::connect(mode, &QComboBox::currentIndexChanged, &owner,
                     [a, i](int at) { a->setChecked(at == i); });
  }
  actions->actions().front()->setChecked(true);
  layout->addWidget(toolbar);
  scroll = new QScrollArea(panel);
  scroll->setWidgetResizable(true);
  auto* content = new QWidget(scroll);
  auto* form = new QFormLayout(content);
  const auto line = [&](QString key, QString value, QFormLayout* at, const QString& label) {
    auto* edit = new QLineEdit(value, content);
    edit->setObjectName("blcad.direct_edit." + key);
    fields[key] = edit;
    at->addRow(label, edit);
    return edit;
  };
  name = line("name", "Direct Edit", form, "Name");
  form->addRow("Operation", mode);
  body = new QComboBox(content);
  fields["body"] = body;
  form->addRow("Target Body", body);
  face = new QComboBox(content);
  face->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  face->setMinimumContentsLength(16);
  fields["face"] = face;
  form->addRow("Available face", face);
  auto* selection_buttons = new QWidget(content);
  auto* selection_layout = new QHBoxLayout(selection_buttons);
  add = new QPushButton("Add face", selection_buttons);
  remove = new QPushButton("Remove selected", selection_buttons);
  fields["add_face"] = add;
  fields["remove_face"] = remove;
  selection_layout->addWidget(add);
  selection_layout->addWidget(remove);
  form->addRow(selection_buttons);
  selected = new QListWidget(content);
  selected->setMaximumHeight(96);
  fields["faces"] = selected;
  form->addRow("Selected faces", selected);
  const auto binding = [&](const QString& key, const QString& label, ParameterType type,
                           QFormLayout* at, const QString& initial) {
    auto* row = new QWidget(content);
    auto* horizontal = new QHBoxLayout(row);
    horizontal->setContentsMargins(0, 0, 0, 0);
    auto* parameters = new QComboBox(row);
    auto* value = new QLineEdit(initial, row);
    value->setMinimumWidth(72);
    horizontal->addWidget(parameters, 1);
    horizontal->addWidget(value, 1);
    fields[key + "_parameter"] = parameters;
    fields[key] = value;
    at->addRow(label, row);
    return Binding{parameters, value, type};
  };
  const char* titles[]{"Move in reference frame (mm)", "Rotate around frame origin",
                       "Offset / Size (mm)", "Reference frame"};
  std::array<QFormLayout*, 4> forms{};
  for (int i = 0; i < 4; ++i) {
    groups[i] = new QGroupBox(titles[i], content);
    forms[i] = new QFormLayout(groups[i]);
    form->addRow(groups[i]);
  }
  const char* axes[]{"x", "y", "z"};
  for (int i = 0; i < 3; ++i)
    move[i] = binding(axes[i], QString(axes[i]).toUpper(), ParameterType::Length, forms[0],
                      i == 2 ? "1" : "0");
  angle = binding("angle", "Angle (deg)", ParameterType::Angle, forms[1], "5");
  for (int i = 0; i < 3; ++i)
    axis[i] =
        line(QString("axis_") + axes[i], i == 0 ? "1" : "0", forms[1], QString("Axis ") + axes[i]);
  size = binding("size", "Distance / size", ParameterType::Length, forms[2], "1");
  measurement = new QComboBox(content);
  measurement->addItems({"Radius", "Diameter"});
  fields["measurement"] = measurement;
  forms[2]->addRow("Size measurement", measurement);
  for (int i = 0; i < 3; ++i) {
    origin[i] = line(QString("origin_") + axes[i], "0", forms[3], QString("Origin ") + axes[i]);
    frame_x[i] = line(QString("frame_x_") + axes[i], i == 0 ? "1" : "0", forms[3],
                      QString("X direction ") + axes[i]);
    frame_y[i] = line(QString("frame_y_") + axes[i], i == 1 ? "1" : "0", forms[3],
                      QString("Y direction ") + axes[i]);
  }
  align = new QPushButton("Align frame to selected face", content);
  world = new QPushButton("Use world frame", content);
  fields["align_frame"] = align;
  fields["world_frame"] = world;
  forms[3]->addRow(align);
  forms[3]->addRow(world);
  tolerance = line("tolerance", "0.000001", form, "Heal tolerance (mm)");
  auto* policy = new QLabel("Extend adjacent supports. Reject affected blends.\nHole/Thread sizes "
                            "use their authored parameters.",
                            content);
  policy->setWordWrap(true);
  form->addRow(policy);
  scroll->setWidget(content);
  layout->addWidget(scroll, 1);
  numeric = new QLineEdit(panel);
  numeric->setPlaceholderText("Active grip: mm or deg");
  numeric->setEnabled(false);
  fields["grip_numeric"] = numeric;
  layout->addWidget(numeric);
  status = new QLabel(panel);
  status->setWordWrap(true);
  fields["status"] = status;
  layout->addWidget(status);
  auto* buttons = new QHBoxLayout;
  preview = new QPushButton("Preview", panel);
  apply = new QPushButton("Apply", panel);
  cancel = new QPushButton("Cancel", panel);
  fields["preview"] = preview;
  fields["apply"] = apply;
  fields["cancel"] = cancel;
  buttons->addWidget(preview);
  buttons->addWidget(apply);
  buttons->addWidget(cancel);
  layout->addLayout(buttons);
  dock->setWidget(panel);
}
void ShellDirectEditTools::Impl::render() {
  const int at = mode->currentIndex();
  groups[0]->setVisible(at == 0);
  groups[1]->setVisible(at == 1);
  groups[2]->setVisible(at == 2 || at == 3);
  groups[3]->setVisible(true);
  measurement->setVisible(at == 3);
  if (auto* label = qobject_cast<QFormLayout*>(groups[2]->layout())->labelForField(measurement))
    label->setVisible(at == 3);
}
} // namespace blcad::gui
