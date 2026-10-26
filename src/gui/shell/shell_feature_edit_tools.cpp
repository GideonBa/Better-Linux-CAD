#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <set>

namespace blcad::gui {
namespace {
using Json = nlohmann::json;
using Kind = GuiFeatureEditKind;
constexpr const char* task_id = "part.feature_edit";
constexpr const char* collections[] = {"features",        "revolve_features", "sweep_features",
                                       "loft_features",   "surface_features", "part_patterns",
                                       "mirror_features", "edge_treatments",  "shell_features",
                                       "draft_features",  "body_booleans"};
QString label(const std::string& key) {
  static const std::map<std::string, QString> labels = {
      {"id", "Identifier"},
      {"name", "Name"},
      {"type", "Feature Type"},
      {"kind", "Reference Type"},
      {"source_kind", "Reference Type"},
      {"operation_mode", "Result Type"},
      {"produced_body", "Result Body"},
      {"result_body", "Result Body"},
      {"target_body", "Target Body"},
      {"target_feature", "Target Feature"},
      {"input_sketch", "Sketch"},
      {"sketch", "Sketch"},
      {"profile", "Profile"},
      {"path", "Path"},
      {"path_curve", "Path"},
      {"axis", "Axis"},
      {"datum_axis", "Reference Axis"},
      {"datum_plane", "Reference Plane"},
      {"construction_line", "Construction Line"},
      {"construction_plane", "Construction Plane"},
      {"source_feature", "Source Feature"},
      {"feature", "Feature"},
      {"body", "Body"},
      {"face", "Face"},
      {"edge", "Edge"},
      {"semantic_edge", "Model Edge"},
      {"semantic_face", "Model Face"},
      {"edges", "Edges"},
      {"faces", "Faces"},
      {"removed_faces", "Removed Faces"},
      {"sources", "Sources"},
      {"length_parameter", "Length"},
      {"radius_parameter", "Radius"},
      {"thickness_parameter", "Wall Thickness"},
      {"angle_parameter", "Angle"},
      {"count_parameter", "Count"},
      {"first_parameter", "First Size"},
      {"second_parameter", "Second Size"},
      {"parameter", "Size"},
      {"first_distance_parameter", "First Length"},
      {"second_distance_parameter", "Second Length"},
      {"direction", "Direction"},
      {"extent", "Extent"},
      {"extrude", "Extrude"},
      {"mode", "Mode"},
      {"side", "Side"},
      {"depth", "Depth"},
      {"angle_deg", "Angle"},
      {"total_angle_deg", "Total Angle"},
      {"taper_angle_deg", "Draft Angle"},
      {"equal_spacing", "Uniform Distribution"},
      {"mirror_plane", "Mirror Plane"},
      {"neutral_plane", "Neutral Plane"},
      {"pull_direction", "Pull Direction"},
      {"boundaries", "Boundaries"},
      {"boundary", "Boundary"},
      {"target", "Target Face"},
      {"trimming", "Cut Contour"},
      {"distance_parameter", "Distance"},
      {"tolerance_parameter", "Tolerance"},
      {"surfaces", "Faces"},
      {"shell", "Shell"},
      {"sections", "Sections"},
      {"guide_curves", "Guide Curves"},
      {"guide_path", "Guide Path"},
      {"continuity", "Continuity"},
      {"flip_normal", "Reverse Normal"},
      {"reverse", "Reverse"},
      {"rotation_offset", "Angle Offset"},
      {"alignment_reference", "Alignment Point"},
      {"twist_parameter", "Twist"},
      {"orientation_override", "Orientation"},
      {"fixed_up_vector_override", "Up Vector"},
      {"x", "X"},
      {"y", "Y"},
      {"z", "Z"},
      {"tools", "Tool Bodies"},
      {"keep_tools", "Keep Tools"},
      {"operation", "Operation"}};
  if (auto found = labels.find(key); found != labels.end())
    return found->second;
  auto text = QString::fromStdString(key);
  text.replace('_', ' ');
  return text;
}
QString enum_label(const std::string& value) {
  static const std::map<std::string, QString> names = {
      {"new_body", "New Body"},
      {"modify_target", "Modify Target Body"},
      {"add_to_target", "Add to Target Body"},
      {"cut_from_target", "Remove from Target Body"},
      {"closed_region", "Closed Profile"},
      {"open_path", "Open Path"},
      {"datum_axis", "Reference Axis"},
      {"datum_plane", "Reference Plane"},
      {"construction_line", "Construction Line"},
      {"construction_plane", "Construction Plane"},
      {"semantic_linear_edge", "Straight Model Edge"},
      {"semantic_planar_face", "Planar Model Face"},
      {"equal_distance", "Equal Distances"},
      {"two_distance", "Two Distances"},
      {"distance_angle", "Distance and Angle"},
      {"spacing", "Distance"},
      {"total_extent", "Overall Extent"},
      {"through_all", "Through All"},
      {"full", "Full"},
      {"angle", "Angle"},
      {"symmetric", "Symmetric"},
      {"distance", "Distance"},
      {"c0", "C0"},
      {"g1", "G1"},
      {"g2", "G2"},
      {"positive", "Positive"},
      {"negative", "Negative"},
      {"inward", "Inward"},
      {"outward", "Outward"},
      {"add", "Union"},
      {"subtract", "Subtract"},
      {"intersect", "Intersect"}};
  auto found = names.find(value);
  return found == names.end() ? label(value) : found->second;
}
QString family(Kind kind) {
  switch (kind) {
  case Kind::Extrude:
    return "Extrude";
  case Kind::Revolve:
    return "Revolve";
  case Kind::Sweep:
    return "Sweep";
  case Kind::Loft:
    return "Loft";
  case Kind::Surface:
    return "Face";
  case Kind::LinearPattern:
    return "Linear Pattern";
  case Kind::CircularPattern:
    return "Circular Pattern";
  case Kind::Mirror:
    return "Mirror";
  case Kind::Fillet:
    return "Fillet";
  case Kind::Chamfer:
    return "Chamfer";
  case Kind::Shell:
    return "Shell";
  case Kind::Draft:
    return "Draft";
  case Kind::BodyBoolean:
    return "Boolean Operation";
  }
  return {};
}
template <typename Callback>
auto with_record(const PartDocument& part, FeatureId id, Kind kind, Callback callback) {
  switch (kind) {
#define BLCAD_EDIT_RECORD(K, FIND, UPDATE)                                                         \
  case Kind::K:                                                                                    \
    return callback(*part.FIND(id), &PartDocument::UPDATE);
    BLCAD_EDIT_RECORD(Extrude, find_feature, update_feature)
    BLCAD_EDIT_RECORD(Revolve, find_revolve_feature, update_revolve_feature)
    BLCAD_EDIT_RECORD(Sweep, find_sweep_feature, update_sweep_feature)
    BLCAD_EDIT_RECORD(Loft, find_loft_feature, update_loft_feature)
    BLCAD_EDIT_RECORD(Surface, find_surface_feature, update_surface_feature)
    BLCAD_EDIT_RECORD(LinearPattern, find_linear_pattern_feature, update_linear_pattern_feature)
    BLCAD_EDIT_RECORD(CircularPattern, find_circular_pattern_feature,
                      update_circular_pattern_feature)
    BLCAD_EDIT_RECORD(Mirror, find_mirror_feature, update_mirror_feature)
    BLCAD_EDIT_RECORD(Fillet, find_fillet_feature, update_fillet_feature)
    BLCAD_EDIT_RECORD(Chamfer, find_chamfer_feature, update_chamfer_feature)
    BLCAD_EDIT_RECORD(Shell, find_shell_feature, update_shell_feature)
    BLCAD_EDIT_RECORD(Draft, find_draft_feature, update_draft_feature)
    BLCAD_EDIT_RECORD(BodyBoolean, find_body_boolean_feature, update_body_boolean_feature)
#undef BLCAD_EDIT_RECORD
  }
  return callback(*part.find_feature(id), &PartDocument::update_feature);
}
Error error(std::string message) {
  return Error::validation("gui.shell.feature_edit", std::move(message));
}
bool body_item(const geometry::ViewportSceneItem& item) {
  return item.kind == geometry::ViewportSceneKind::SolidBody ||
         item.kind == geometry::ViewportSceneKind::SurfaceBody;
}
bool parameter_key(std::string_view key) {
  return key.ends_with("_parameter") || key == "parameter" || key == "rotation_offset";
}
std::set<std::string> bound_parameters(const Json& value) {
  std::set<std::string> result;
  if (value.is_object()) {
    for (auto it = value.begin(); it != value.end(); ++it) {
      if (parameter_key(it.key()) && it.value().is_string())
        result.insert(it.value().get<std::string>());
      else {
        auto nested = bound_parameters(it.value());
        result.insert(nested.begin(), nested.end());
      }
    }
  } else if (value.is_array()) {
    for (const auto& item : value) {
      auto nested = bound_parameters(item);
      result.insert(nested.begin(), nested.end());
    }
  }
  return result;
}
} // namespace

struct ShellFeatureEditTools::Impl {
  ShellFeatureEditTools& owner;
  ShellWindow& window;
  GuiDocumentSession& session;
  OcctViewport& viewport;
  GuiFeatureEditController edit;
  GuiViewportManipulatorLayer layer;
  QDockWidget* dock{};
  QScrollArea* scroll{};
  QLabel* status{};
  QComboBox* dynamic{};
  QPushButton* apply{};
  QWidget* overlay{};
  Json source, record;
  std::string source_text, collection;
  std::size_t record_index{};
  std::map<std::string, double> parameter_values;
  std::map<QString, Json> array_templates;
  std::map<QString, QWidget*> fields;
  std::map<QString, QDoubleSpinBox*> numbers;
  std::map<QString, QuantityKind> units;
  std::uint32_t previous_mask{};
  std::optional<std::vector<std::string>> previous_scope;
  bool valid{}, building{};
  std::optional<double> drag_start;
  GuiSketchScreenPoint last_pointer{};
  QString buffer;

  Impl(ShellFeatureEditTools& owner_, ShellWindow& window_)
      : owner(owner_), window(window_), session(window.session()), viewport(window.viewport()) {
    dock = new QDockWidget("Edit Feature", &window);
    dock->setObjectName("blcad.shell.feature_edit_task");
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    auto* content = new QWidget(dock);
    auto* column = new QVBoxLayout(content);
    status = new QLabel(content);
    status->setObjectName("blcad.feature_edit.status");
    status->setWordWrap(true);
    column->addWidget(status);
    dynamic = new QComboBox(content);
    dynamic->setObjectName("blcad.feature_edit.dynamic");
    dynamic->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    column->addWidget(new QLabel("Dynamic Value", content));
    column->addWidget(dynamic);
    scroll = new QScrollArea(content);
    scroll->setObjectName("blcad.feature_edit.inputs_scroll");
    scroll->setMinimumWidth(380);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    column->addWidget(scroll, 1);
    auto* hint = new QLabel("The preview also checks downstream features. Shared parameters "
                            "affect every use; formula values are determined by their formulas. "
                            "Escape resets a drag; press Escape again to cancel.",
                            content);
    hint->setWordWrap(true);
    column->addWidget(hint);
    auto* footer = new QHBoxLayout;
    apply = new QPushButton("Apply", content);
    apply->setObjectName("blcad.feature_edit.apply");
    apply->setProperty("guiRole", "primary");
    auto* cancel = new QPushButton("Cancel", content);
    cancel->setObjectName("blcad.feature_edit.cancel");
    footer->addWidget(apply);
    footer->addWidget(cancel);
    column->addLayout(footer);
    dock->setWidget(content);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    overlay = make_shell_manipulator_overlay(&viewport, layer);
    overlay->setObjectName("blcad.feature_edit.overlay");
    QObject::connect(apply, &QPushButton::clicked, &owner, [this] {
      auto result = owner.apply();
      if (!result)
        window.report_error(result.error());
    });
    QObject::connect(cancel, &QPushButton::clicked, &owner, [this] { owner.cancel(); });
    QObject::connect(dynamic, &QComboBox::currentIndexChanged, &owner, [this] {
      if (!building && layer.active_handle_id().empty())
        publish_handle();
    });
    viewport.installEventFilter(&owner);
    window.installEventFilter(&owner);
  }
  ~Impl() {
    delete overlay;
  }

  Json& value(const QString& path) {
    return record.at(Json::json_pointer(path.toStdString()));
  }
  const Parameter* parameter(const std::string& id) const {
    return session.part_document()->find_parameter(ParameterId(id));
  }
  double parameter_value(const Parameter& parameter) const {
    if (auto found = parameter_values.find(parameter.id().value()); found != parameter_values.end())
      return found->second;
    return parameter.value().millimeters();
  }
  void register_field(QWidget* widget, const QString& path) {
    fields[path] = widget;
    widget->setObjectName("blcad.feature_edit.field" + path);
  }
  QDoubleSpinBox* number(QWidget* parent, const QString& path, double scalar, QuantityKind unit,
                         bool unitless = false) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setDecimals(unit == QuantityKind::Count ? 0 : 6);
    spin->setRange(-1e9, 1e9);
    spin->setKeyboardTracking(false);
    spin->setValue(scalar);
    spin->setSuffix(unitless                         ? ""
                    : unit == QuantityKind::AngleDeg ? " deg"
                    : unit == QuantityKind::Count    ? ""
                                                     : " mm");
    register_field(spin, path);
    numbers[path] = spin;
    units[path] = unit;
    if (!unitless)
      dynamic->addItem(path_label(path), path);
    return spin;
  }
  QString path_label(const QString& path) const {
    QStringList result;
    for (const auto& part : path.split('/', Qt::SkipEmptyParts))
      result.push_back(label(part.toStdString()));
    return result.join(" / ");
  }
  QComboBox* combo(QWidget* parent) {
    auto* box = new QComboBox(parent);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(10);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QObject::connect(box, &QComboBox::currentTextChanged, box, &QWidget::setToolTip);
    return box;
  }
  void add_choice(QComboBox* box, const std::string& id, const std::string& name) {
    box->addItem(QString::fromStdString(name), QString::fromStdString(id));
    box->setItemData(box->count() - 1, QString::fromStdString(id), Qt::ToolTipRole);
  }
  std::vector<std::pair<std::string, std::string>> references(const std::string& key,
                                                              const QString& path) const {
    std::vector<std::pair<std::string, std::string>> result;
    const auto& part = *session.part_document();
    const auto add = [&](const auto& range) {
      for (const auto& item : range)
        result.emplace_back(item.id().value(), item.name());
    };
    if (key == "input_sketch" || key == "sketch")
      add(part.sketches());
    else if (key == "profile") {
      for (const auto& sketch : part.sketches()) {
        auto add_profiles = [&](const auto& profiles) {
          for (const auto& p : profiles)
            result.emplace_back(p.id().value(), sketch.name() + " / " + p.id().value());
        };
        add_profiles(sketch.rectangle_profiles());
        add_profiles(sketch.circle_profiles());
        add_profiles(sketch.closed_profiles());
        add_profiles(sketch.arc_closed_profiles());
        add_profiles(sketch.composite_closed_profiles());
      }
    } else if (key == "path" || key == "path_curve" || key == "guide_path" || key == "guide_curves")
      add(part.path_curves());
    else if (key == "datum_axis")
      add(part.datum_axes());
    else if (key == "datum_plane")
      add(part.datum_planes());
    else if (key == "construction_line")
      add(part.construction_lines());
    else if (key == "construction_plane")
      add(part.construction_planes());
    else if (key == "body" || key == "target_body" || key == "tools" || key == "tool_bodies")
      add(part.bodies());
    else if (key == "source_feature" || key == "target_feature" || key == "feature") {
      for (const auto* array : collections)
        for (const auto& f : source.value(array, Json::array()))
          if (f.at("id") != edit.feature().value())
            result.emplace_back(f.at("id").get<std::string>(),
                                f.value("name", f.at("id").get<std::string>()));
    } else if (key == "face") {
      for (const char* face : {"top", "bottom", "front", "back", "left", "right"})
        result.emplace_back(face, face);
    } else if (key == "edge") {
      const auto parent_path = path.left(path.lastIndexOf('/'));
      const auto& parent = record.at(Json::json_pointer(parent_path.toStdString()));
      const bool circular = parent.value("source_kind", "") == "semantic_circular_edge";
      for (int i = 0; i < (circular ? 2 : 12); ++i) {
        if (circular) {
          auto spelling = std::string(to_string(static_cast<SemanticCircularEdge>(i)));
          result.emplace_back(spelling, spelling);
          continue;
        }
        auto spelling = std::string(to_string(static_cast<SemanticEdge>(i)));
        result.emplace_back(spelling, spelling);
      }
    } else if (key == "alignment_reference") {
      for (const auto& sketch : part.sketches()) {
        for (const auto& line : sketch.line_segments())
          result.emplace_back(line.id().value(), sketch.name() + " / " + line.id().value());
      }
    }
    return result;
  }
  void parameter_field(QFormLayout* form, const std::string& key, const QString& path,
                       const Parameter& selected) {
    auto* row = new QWidget(form->parentWidget());
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* binding = combo(row);
    binding->setObjectName("blcad.feature_edit.binding" + path);
    for (const auto& p : session.part_document()->parameters())
      if (p.type() == selected.type())
        add_choice(binding, p.id().value(), p.name());
    binding->setCurrentIndex(binding->findData(QString::fromStdString(selected.id().value())));
    layout->addWidget(binding);
    auto* scalar = number(row, path, parameter_value(selected), selected.value().kind());
    scalar->setEnabled(!selected.is_expression());
    if (selected.formula())
      scalar->setToolTip("Formula: " + QString::fromStdString(*selected.formula()));
    layout->addWidget(scalar);
    form->addRow(label(key), row);
    QObject::connect(
        binding, &QComboBox::currentIndexChanged, &owner, [this, path, binding, scalar] {
          value(path) = binding->currentData().toString().toStdString();
          const auto* p = parameter(value(path).get<std::string>());
          const QSignalBlocker block(scalar);
          scalar->setValue(parameter_value(*p));
          scalar->setEnabled(!p->is_expression());
          scalar->setToolTip(p->formula() ? "Formula: " + QString::fromStdString(*p->formula())
                                          : QString());
          (void)preview();
        });
    QObject::connect(scalar, &QDoubleSpinBox::valueChanged, &owner,
                     [this, path](double scalar_value) {
                       const auto id = value(path).get<std::string>();
                       parameter_values[id] = scalar_value;
                       for (const auto& [other_path, other] : numbers) {
                         if (value(other_path).is_string() && value(other_path) == id) {
                           const QSignalBlocker block(other);
                           other->setValue(scalar_value);
                         }
                       }
                       (void)preview();
                     });
  }
  void deferred_rebuild() {
    scroll->widget()->setEnabled(false);
    QTimer::singleShot(0, &owner, [this] {
      if (edit.active()) {
        build_form();
        (void)preview();
      }
    });
  }
  void build_fields(QFormLayout* form, const Json& object, const QString& prefix) {
    for (auto it = object.begin(); it != object.end(); ++it) {
      const auto key = it.key();
      if (key == "id" || key == "type")
        continue;
      const auto path = prefix + "/" + QString::fromStdString(key);
      const auto& item = it.value();
      std::optional<Json> optional_default;
      bool optional = false;
      if ((prefix.isEmpty() &&
           (key == "guide_path" || (key == "path_curve" && edit.kind() == Kind::Loft))) ||
          key == "alignment_reference") {
        optional = true;
        auto choices = references(key, path);
        if (!choices.empty())
          optional_default = choices.front().first;
      } else if (key == "twist_parameter" || key == "rotation_offset") {
        optional = true;
        for (const auto& parameter : session.part_document()->parameters())
          if (parameter.type() == ParameterType::Angle) {
            optional_default = parameter.id().value();
            break;
          }
      } else if (key == "orientation_override") {
        optional = true;
        optional_default = "minimum_twist";
      } else if (key == "fixed_up_vector_override") {
        optional = true;
        optional_default = Json{{"x", 0.0}, {"y", 0.0}, {"z", 1.0}};
      }
      if (optional) {
        auto* enabled = new QCheckBox(label(key) + " Use", form->parentWidget());
        enabled->setObjectName("blcad.feature_edit.enable" + path);
        enabled->setChecked(!item.is_null());
        enabled->setEnabled(!item.is_null() || optional_default.has_value());
        form->addRow(enabled);
        QObject::connect(enabled, &QCheckBox::toggled, &owner,
                         [this, path, optional_default](bool state) {
                           value(path) = state ? *optional_default : Json(nullptr);
                           deferred_rebuild();
                         });
      }
      if (item.is_null())
        continue;
      if (item.is_object()) {
        auto* group = new QGroupBox(label(key), form->parentWidget());
        auto* nested = new QFormLayout(group);
        build_fields(nested, item, path);
        form->addRow(group);
      } else if (item.is_array()) {
        if (item.empty() && key != "guide_curves" && !array_templates.contains(path))
          continue;
        if (!item.empty())
          array_templates[path] = item.back();
        auto* group = new QGroupBox(label(key), form->parentWidget());
        auto* nested = new QFormLayout(group);
        for (std::size_t index = 0; index < item.size(); ++index) {
          const auto entry = path + "/" + QString::number(index);
          auto* box = new QGroupBox(QString::number(index + 1), group);
          auto* inputs = new QFormLayout(box);
          if (item[index].is_object())
            build_fields(inputs, item[index], entry);
          else
            build_leaf(inputs, key, entry, item[index]);
          auto* buttons = new QHBoxLayout;
          for (auto operation : {QString("↑"), QString("↓"), QString("Remove")}) {
            auto* button = new QPushButton(operation, box);
            // Keep the established automation identity independent of the display language.
            const QString identity =
                operation == "Remove" ? QStringLiteral("Entfernen") : operation;
            button->setObjectName("blcad.feature_edit." + identity + entry);
            button->setEnabled(operation == "↑"   ? index > 0
                               : operation == "↓" ? index + 1 < item.size()
                                                  : true);
            buttons->addWidget(button);
            QObject::connect(button, &QPushButton::clicked, &owner, [this, path, index, operation] {
              auto& array = value(path);
              if (operation == "Remove")
                array.erase(array.begin() + static_cast<Json::difference_type>(index));
              else
                std::swap(array[index], array[operation == "↑" ? index - 1 : index + 1]);
              deferred_rebuild();
            });
          }
          inputs->addRow(buttons);
          nested->addRow(box);
        }
        auto* add = new QPushButton("Add Entry", group);
        add->setObjectName("blcad.feature_edit.add" + path);
        const auto choices = references(key, path);
        add->setEnabled(!item.empty() || array_templates.contains(path) || !choices.empty());
        QObject::connect(add, &QPushButton::clicked, &owner, [this, path, choices] {
          auto& array = value(path);
          if (!array.empty())
            array.push_back(array.back());
          else if (auto previous = array_templates.find(path); previous != array_templates.end())
            array.push_back(previous->second);
          else if (!choices.empty())
            array.push_back(choices.front().first);
          deferred_rebuild();
        });
        nested->addRow(add);
        form->addRow(group);
      } else
        build_leaf(form, key, path, item);
    }
  }
  void build_leaf(QFormLayout* form, const std::string& key, const QString& path,
                  const Json& item) {
    auto* parent = form->parentWidget();
    if (item.is_string()) {
      const auto text = item.get<std::string>();
      if (const auto* p = parameter_key(key) ? parameter(text) : nullptr) {
        parameter_field(form, key, path, *p);
        return;
      }
      const bool fixed = key == "kind" || key == "source_kind" || key == "result_mode" ||
                         key == "operation_mode" || key == "produced_body" ||
                         key == "result_body" || key == "mode" || key == "extent";
      auto choices =
          fixed ? std::vector<std::pair<std::string, std::string>>{} : references(key, path);
      if (!fixed && (key == "side" || key == "direction")) {
        if (text == "positive" || text == "negative")
          choices = {{"positive", "Positive"}, {"negative", "Negative"}};
        else if (text == "+Z" || text == "opposite_sketch_normal")
          choices = {{"+Z", "Sketch Normal"}, {"opposite_sketch_normal", "Reverse Normal"}};
        else if (text == "inward" || text == "outward")
          choices = {{"inward", "Inward"}, {"outward", "Outward"}};
      }
      if (key == "continuity")
        choices = {{"c0", "C0"}, {"g1", "G1"}, {"g2", "G2"}};
      if (key == "operation")
        choices = {{"add", "Union"}, {"subtract", "Subtract"}, {"intersect", "Intersect"}};
      if (!choices.empty()) {
        auto* input = combo(parent);
        for (const auto& [id, name] : choices)
          add_choice(input, id, name);
        auto selected = input->findData(QString::fromStdString(text));
        if (selected < 0) {
          add_choice(input, text, text);
          selected = input->count() - 1;
        }
        input->setCurrentIndex(selected);
        register_field(input, path);
        form->addRow(label(key), input);
        QObject::connect(input, &QComboBox::currentIndexChanged, &owner, [this, path, input] {
          value(path) = input->currentData().toString().toStdString();
          (void)preview();
        });
      } else if (fixed) {
        QString display = enum_label(text);
        if (key == "produced_body" || key == "result_body")
          if (const auto* body = session.part_document()->find_body(BodyId(text)))
            display = QString::fromStdString(body->name());
        auto* output = new QLabel(display, parent);
        output->setWordWrap(true);
        register_field(output, path);
        form->addRow(label(key), output);
      } else {
        auto* input = new QLineEdit(QString::fromStdString(text), parent);
        input->setReadOnly(fixed);
        register_field(input, path);
        form->addRow(label(key), input);
        QObject::connect(input, &QLineEdit::editingFinished, &owner, [this, path, input] {
          if (!input->isReadOnly()) {
            value(path) = input->text().toStdString();
            (void)preview();
          }
        });
      }
    } else if (item.is_boolean()) {
      auto* input = new QCheckBox(parent);
      input->setChecked(item.get<bool>());
      register_field(input, path);
      form->addRow(label(key), input);
      QObject::connect(input, &QCheckBox::toggled, &owner, [this, path](bool state) {
        value(path) = state;
        (void)preview();
      });
    } else if (item.is_number()) {
      const auto unit =
          key.find("angle") != std::string::npos ? QuantityKind::AngleDeg : QuantityKind::LengthMm;
      auto* input =
          number(parent, path, item.get<double>(), unit, key == "x" || key == "y" || key == "z");
      form->addRow(label(key), input);
      QObject::connect(input, &QDoubleSpinBox::valueChanged, &owner, [this, path](double scalar) {
        value(path) = scalar;
        (void)preview();
      });
    }
  }
  void build_form() {
    building = true;
    const auto selected = dynamic->currentData();
    fields.clear();
    numbers.clear();
    units.clear();
    dynamic->clear();
    dynamic->addItem("No Handle", QString());
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    auto* form = new QFormLayout;
    column->addLayout(form);
    column->addStretch();
    build_fields(form, record, {});
    auto* old = scroll->takeWidget();
    scroll->setWidget(panel);
    if (old) {
      for (auto* object : old->findChildren<QObject*>())
        QObject::disconnect(object, nullptr, &owner, nullptr);
      old->deleteLater();
    }
    const int retained = dynamic->findData(selected);
    dynamic->setCurrentIndex(retained > 0 ? retained : dynamic->count() > 1 ? 1 : 0);
    building = false;
  }
  GuiPartMutation parameter_updates(const PartDocument& decoded) const {
    std::vector<Parameter> changes;
    const auto used = bound_parameters(record);
    for (const auto& [id, scalar] : parameter_values) {
      (void)scalar;
      if (used.contains(id))
        changes.push_back(*decoded.find_parameter(ParameterId(id)));
    }
    return [changes](PartDocument& part) -> Result<std::size_t> {
      for (const auto& p : changes) {
        auto set = part.set_parameter_value(p.id(), p.value());
        if (!set)
          return Result<std::size_t>::failure(set.error());
      }
      return Result<std::size_t>::success(changes.size());
    };
  }
  Result<PartDocument> decode() const {
    Json root = source;
    root.at(collection).at(record_index) = record;
    const auto used = bound_parameters(record);
    for (auto& p : root.at("parameters")) {
      const auto id = p.at("id").get<std::string>();
      if (auto found = parameter_values.find(id);
          found != parameter_values.end() && used.contains(id))
        p["value"] = found->second;
    }
    return deserialize_part_document_from_json(root.dump());
  }
  bool fresh() const {
    if (!edit.active() || !session.part_document() || !session.derived_results_fresh() ||
        session.task().command_id() != task_id || session.task().stage() != GuiTaskStage::Preview)
      return false;
    auto current = serialize_part_document_to_json(*session.part_document());
    return current && current.value() == source_text;
  }
  Result<std::size_t> reject(Error failure) {
    valid = false;
    apply->setEnabled(false);
    viewport.clear_feature_preview();
    status->setText(QString::fromStdString(failure.message()));
    overlay->hide();
    return Result<std::size_t>::failure(std::move(failure));
  }
  Result<std::size_t> preview() {
    if (building)
      return Result<std::size_t>::success(0);
    if (!fresh())
      return reject(error("Feature or document changed. Restart editing."));
    auto decoded = decode();
    if (!decoded)
      return reject(decoded.error());
    auto candidate = with_record(decoded.value(), edit.feature(), edit.kind(),
                                 [&](auto replacement, auto updater) {
                                   return edit.candidate(session, std::move(replacement), updater,
                                                         parameter_updates(decoded.value()));
                                 });
    if (!candidate)
      return reject(candidate.error());
    auto cache = geometry::ShapeCache::create(ShapeCacheId("feature.edit.preview"));
    if (!cache)
      return reject(cache.error());
    auto executed =
        geometry::GeometryRecomputeExecutor{}.execute_document(candidate.value(), cache.value());
    if (!executed)
      return reject(executed.error());
    auto scene = geometry::ViewportSceneBuilder{}.build_part(candidate.value(), cache.value());
    if (!scene)
      return reject(scene.error());
    auto original = geometry::ViewportSceneBuilder{}.build_part(*session.part_document(),
                                                                *session.part_shape_cache());
    if (!original)
      return reject(original.error());
    std::vector<geometry::GeometryShape> shapes;
    std::vector<std::string> replaced;
    for (auto& item : scene.value())
      if (body_item(item))
        shapes.push_back(std::move(item.shape));
    for (const auto& item : original.value())
      if (body_item(item))
        replaced.push_back(item.semantic_id);
    if (!shapes.empty() || !replaced.empty()) {
      auto published = viewport.set_feature_previews(std::move(shapes), std::move(replaced));
      if (!published)
        return reject(published.error());
    } else
      viewport.clear_feature_preview();
    valid = true;
    apply->setEnabled(layer.active_handle_id().empty());
    status->setText("Preview valid, including downstream features. Commit changes with "
                    "Apply.");
    if (layer.active_handle_id().empty())
      publish_handle();
    owner.refresh_presentation();
    return Result<std::size_t>::success(1);
  }
  void publish_handle() {
    if (!edit.active() || building)
      return;
    std::vector<GuiViewportManipulatorHandle> handles;
    const auto path = dynamic->currentData().toString();
    if (auto selected = numbers.find(path);
        selected != numbers.end() && selected->second->isEnabled()) {
      GuiViewportManipulatorHandle handle;
      handle.id = "feature.edit.value";
      const auto unit = units.at(path);
      handle.kind = unit == QuantityKind::AngleDeg ? GuiViewportManipulatorKind::Angular
                    : unit == QuantityKind::Count  ? GuiViewportManipulatorKind::PatternCount
                                                   : GuiViewportManipulatorKind::Linear;
      handle.reference_value = selected->second->value();
      handle.axis = {0, 0, 1};
      handle.plane_normal = {0, 0, 1};
      handle.reference_direction = {1, 0, 0};
      if (const auto* feature = session.part_document()->find_feature(edit.feature())) {
        auto plane = GuiSketchWorkbench{}.plane_view(session, feature->input_sketch());
        if (plane) {
          handle.origin = plane.value().origin;
          handle.axis = plane.value().normal;
          handle.plane_normal = plane.value().normal;
          handle.reference_direction = plane.value().x_axis;
          if (feature->direction() == ExtrudeDirection::OppositeSketchNormal)
            handle.axis = {-handle.axis.x, -handle.axis.y, -handle.axis.z};
        }
      }
      handles.push_back(handle);
    }
    (void)layer.set_handles(std::move(handles));
    owner.refresh_presentation();
  }
  Result<std::size_t> accept_candidate(const GuiViewportManipulatorCandidate& candidate) {
    const auto selected = numbers.find(dynamic->currentData().toString());
    if (selected == numbers.end())
      return Result<std::size_t>::failure(error("No dynamic value selected"));
    selected->second->setValue(candidate.scalar_value);
    return valid ? Result<std::size_t>::success(1)
                 : Result<std::size_t>::failure(error("Invalid Preview"));
  }
  void finish(bool applied) {
    edit.cancel();
    layer.cancel();
    (void)layer.set_handles({});
    valid = false;
    drag_start.reset();
    if (viewport.mouseGrabber() == &viewport)
      viewport.release_interaction_mouse();
    viewport.clear_feature_preview();
    viewport.set_selection_filter_mask(previous_mask);
    viewport.set_selection_semantic_scope(previous_scope);
    if (session.task().command_id() == task_id) {
      if (applied)
        (void)session.task().apply();
      else
        (void)session.task().cancel();
    }
    dock->hide();
    overlay->hide();
    fields.clear();
    numbers.clear();
    window.refresh();
  }
  void restore_drag() {
    layer.cancel();
    if (viewport.mouseGrabber() == &viewport)
      viewport.release_interaction_mouse();
    if (drag_start) {
      auto found = numbers.find(dynamic->currentData().toString());
      if (found != numbers.end())
        found->second->setValue(*drag_start);
    }
    drag_start.reset();
    (void)preview();
  }
};

ShellFeatureEditTools::ShellFeatureEditTools(ShellWindow& window)
    : QObject(&window), impl_(std::make_unique<Impl>(*this, window)) {}
ShellFeatureEditTools::~ShellFeatureEditTools() = default;
bool ShellFeatureEditTools::active() const noexcept {
  return impl_->edit.active();
}
QWidget* ShellFeatureEditTools::field(const QString& path) const {
  auto found = impl_->fields.find(path);
  return found == impl_->fields.end() ? nullptr : found->second;
}
GuiViewportManipulatorLayer& ShellFeatureEditTools::manipulators() {
  return impl_->layer;
}
Result<std::size_t> ShellFeatureEditTools::begin(FeatureId id) {
  auto& p = *impl_;
  if (active() || p.window.active_sketch() || p.session.task().active() ||
      !p.session.derived_results_fresh())
    return Result<std::size_t>::failure(
        error("Feature editing requires a current part without an active command"));
  auto begun = p.edit.begin(p.session, id);
  if (!begun)
    return begun;
  auto serialized = serialize_part_document_to_json(*p.session.part_document());
  if (!serialized) {
    p.edit.cancel();
    return Result<std::size_t>::failure(serialized.error());
  }
  p.source_text = serialized.value();
  p.source = Json::parse(p.source_text);
  p.collection.clear();
  for (const auto* name : collections) {
    if (!p.source.contains(name))
      continue;
    for (std::size_t index = 0; index < p.source.at(name).size(); ++index)
      if (p.source.at(name).at(index).at("id") == id.value()) {
        p.collection = name;
        p.record_index = index;
        p.record = p.source.at(name).at(index);
        break;
      }
    if (!p.collection.empty())
      break;
  }
  if (p.collection.empty()) {
    p.edit.cancel();
    return Result<std::size_t>::failure(error("Feature type cannot be edited"));
  }
  if (!p.session.task().begin(task_id) || !p.session.task().begin_parameter_editing() ||
      !p.session.task().show_preview()) {
    p.edit.cancel();
    return Result<std::size_t>::failure(error("Could not start editing task"));
  }
  p.previous_mask = p.viewport.selection_filter_mask();
  p.previous_scope = p.viewport.selection_semantic_scope();
  p.viewport.set_selection_filter_mask(0);
  p.parameter_values.clear();
  p.array_templates.clear();
  p.dock->setWindowTitle(family(p.edit.kind()) + " Edit");
  p.build_form();
  p.dock->show();
  p.dock->raise();
  auto preview = p.preview();
  if (!preview) {
    const auto failure = preview.error();
    p.finish(false);
    return Result<std::size_t>::failure(failure);
  }
  p.window.refresh();
  return begun;
}
Result<std::size_t> ShellFeatureEditTools::apply() {
  auto& p = *impl_;
  if (!p.fresh() || !p.layer.active_handle_id().empty())
    return Result<std::size_t>::failure(error("No current applicable feature edit"));
  auto preview = p.preview();
  if (!preview)
    return preview;
  auto decoded = p.decode();
  if (!decoded)
    return Result<std::size_t>::failure(decoded.error());
  auto committed = with_record(decoded.value(), p.edit.feature(), p.edit.kind(),
                               [&](auto replacement, auto updater) {
                                 return p.edit.commit(p.session, std::move(replacement), updater,
                                                      p.parameter_updates(decoded.value()));
                               });
  if (committed)
    p.finish(true);
  return committed;
}
void ShellFeatureEditTools::cancel() {
  if (active())
    impl_->finish(false);
}
void ShellFeatureEditTools::refresh() {
  auto& p = *impl_;
  if (!active())
    return;
  if (!p.fresh()) {
    cancel();
    p.window.statusBar()->showMessage("Feature changed. Restart editing.");
    return;
  }
  refresh_presentation();
}
void ShellFeatureEditTools::refresh_presentation() {
  auto& p = *impl_;
  auto mapping = GuiViewportManipulatorMapping::create(
      [&p](GuiSketchScreenPoint point) { return p.viewport.screen_to_view_ray(point); },
      [&p](Point3 point) { return p.viewport.model_to_screen(point); });
  if (mapping)
    (void)p.layer.set_mapping(std::move(mapping.value()));
  p.overlay->setGeometry(p.viewport.rect());
  p.overlay->setVisible(active() && p.valid && !p.layer.handles().empty());
  if (p.overlay->isVisible()) {
    p.overlay->raise();
    p.overlay->update();
  }
}
Result<std::size_t> ShellFeatureEditTools::set_drag_text(const QString& text) {
  auto& p = *impl_;
  if (p.layer.active_handle_id().empty())
    return Result<std::size_t>::failure(error("Drag a handle first"));
  auto input = text.trimmed();
  const auto kind = p.units.at(p.dynamic->currentData().toString());
  if (std::none_of(input.begin(), input.end(), [](QChar c) { return c.isLetter(); }))
    input += kind == QuantityKind::AngleDeg ? " deg" : kind == QuantityKind::Count ? "" : " mm";
  if (!p.layer.set_numeric_input(input.toStdString()) || !p.layer.candidate())
    return Result<std::size_t>::failure(error("Invalid dimension input"));
  return p.accept_candidate(*p.layer.candidate());
}
bool ShellFeatureEditTools::eventFilter(QObject* watched, QEvent* event) {
  if (!active())
    return QObject::eventFilter(watched, event);
  auto& p = *impl_;
  if (event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Escape) {
      if (p.layer.active_handle_id().empty())
        cancel();
      else
        p.restore_drag();
      return true;
    }
    if (watched == &p.viewport && !p.layer.active_handle_id().empty()) {
      if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        auto candidate = p.layer.end_drag(p.last_pointer);
        p.viewport.release_interaction_mouse();
        if (candidate)
          (void)p.accept_candidate(*candidate);
        p.drag_start.reset();
        p.publish_handle();
        p.apply->setEnabled(p.valid);
        return true;
      }
      if (key->key() == Qt::Key_Backspace)
        p.buffer.chop(1);
      else
        p.buffer += key->text();
      if (p.buffer.isEmpty()) {
        if (p.layer.clear_numeric_override() && p.layer.candidate())
          (void)p.accept_candidate(*p.layer.candidate());
      } else
        (void)set_drag_text(p.buffer);
      return true;
    }
  }
  if (watched == &p.viewport) {
    if (event->type() == QEvent::Resize)
      refresh_presentation();
    if (event->type() == QEvent::MouseButtonPress && p.valid) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        p.last_pointer = {mouse->position().x(), mouse->position().y()};
        if (p.layer.begin_drag(p.last_pointer)) {
          p.drag_start = p.numbers.at(p.dynamic->currentData().toString())->value();
          p.buffer.clear();
          p.viewport.setFocus();
          p.viewport.grab_interaction_mouse();
          p.apply->setEnabled(false);
          return true;
        }
      }
    }
    if (event->type() == QEvent::MouseMove && !p.layer.active_handle_id().empty()) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      p.last_pointer = {mouse->position().x(), mouse->position().y()};
      if (p.layer.update_drag(p.last_pointer) && p.layer.candidate())
        (void)p.accept_candidate(*p.layer.candidate());
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && !p.layer.active_handle_id().empty()) {
      auto* mouse = static_cast<QMouseEvent*>(event);
      auto candidate = p.layer.end_drag({mouse->position().x(), mouse->position().y()});
      p.viewport.release_interaction_mouse();
      if (candidate) {
        (void)p.accept_candidate(*candidate);
        p.drag_start.reset();
        p.publish_handle();
      } else
        p.restore_drag();
      p.apply->setEnabled(p.valid);
      return true;
    }
  }
  if (event->type() == QEvent::WindowDeactivate && !p.layer.active_handle_id().empty())
    p.restore_drag();
  return QObject::eventFilter(watched, event);
}
} // namespace blcad::gui
