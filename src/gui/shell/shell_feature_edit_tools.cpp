#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
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
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QUuid>
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
constexpr const char* collections[] = {
    "features",      "revolve_features", "sweep_features",  "loft_features",  "surface_features",
    "part_patterns", "mirror_features",  "edge_treatments", "shell_features", "draft_features",
    "body_booleans", "hole_features",    "thread_features"};
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
      {"placements", "Placements"},
      {"position", "Position"},
      {"diameter", "Diameter"},
      {"manufacturing", "Seat / Manufacturing"},
      {"tip", "Drill Tip"},
      {"support", "Support"},
      {"handedness", "Handedness"},
      {"representation", "Representation"},
      {"major_diameter", "Major Diameter"},
      {"minor_diameter", "Minor Diameter"},
      {"pitch", "Pitch"},
      {"included_angle", "Included Angle"},
      {"taper_angle", "Taper Angle"},
      {"start", "Start Offset"},
      {"length", "Length"},
      {"total_depth", "Total Depth"},
      {"table", "Pinned Table"},
      {"revision", "Revision"},
      {"row", "Source Row"},
      {"columns", "Source Columns"},
      {"diameter_column", "Source Column"},
      {"system", "Thread System"},
      {"designation", "Designation"},
      {"tolerance", "Tolerance"},
      {"second_direction", "Direction 2"},
      {"skipped", "Skipped Occurrences"},
      {"first", "Column"},
      {"second", "Row"},
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
  case Kind::Hole:
    return "Hole";
  case Kind::Thread:
    return "Thread";
  }
  return {};
}
template <typename Callback>
auto with_record(const PartDocument& part, FeatureId id, Kind kind, Callback callback,
                 bool creating = false) {
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
  case Kind::Hole:
    return callback(*part.find_hole_feature(id), creating ? &PartDocument::add_hole_feature
                                                          : &PartDocument::update_hole_feature);
    BLCAD_EDIT_RECORD(Thread, find_thread_feature, update_thread_feature)
#undef BLCAD_EDIT_RECORD
  }
  return callback(*part.find_feature(id), &PartDocument::update_feature);
}
Error error(std::string message) {
  return Error::validation("gui.shell.feature_edit", std::move(message));
}
std::string plane_identity(const PlaneReference& plane) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return "face/" + source.source_feature().value() + "/" +
                 std::string(to_string(source.face()));
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return make_imported_topology_target_spelling(source).value();
        else if constexpr (std::is_same_v<T, ConstructionPlaneId>)
          return "construction/" + source.value();
        else
          return "datum/" + source.value();
      },
      plane.source());
}
bool body_item(const geometry::ViewportSceneItem& item) {
  return item.kind == geometry::ViewportSceneKind::SolidBody ||
         item.kind == geometry::ViewportSceneKind::SurfaceBody;
}
bool parameter_key(std::string_view key) {
  return key.ends_with("_parameter") || key == "parameter" || key == "rotation_offset" ||
         key == "major_diameter" || key == "minor_diameter" || key == "pitch" ||
         key == "included_angle" || key == "taper_angle" || key == "start" || key == "length" ||
         key == "depth" || key == "total_depth";
}
std::set<std::string> bound_parameters(const Json& value) {
  std::set<std::string> result;
  if (value.is_object()) {
    for (auto it = value.begin(); it != value.end(); ++it) {
      if (it.key() == "table" || it.key() == "columns")
        continue;
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
  QLabel* dynamic_label{};
  QLabel* guidance{};
  QAction* hole_action{};
  QPushButton* another{};
  bool creating{};
  enum class Pick { None, Body, Support };
  Pick picking{Pick::None};
  struct SupportChoice {
    std::string semantic_id, label;
    PlaneReference reference;
    geometry::GeometryShape shape;
  };
  std::vector<SupportChoice> supports;
  std::optional<PlaneReference> support_override;
  bool support_chosen{};
  std::optional<Error> support_error;
  std::vector<Parameter> pending_parameters;
  QComboBox *target_field{}, *support_field{};
  ShellPropertySelector *target_selector{}, *support_selector{};
  std::optional<double> next_diameter;
  std::optional<BodyId> next_target;
  std::size_t entry_revision{};
  std::optional<GuiViewportCameraBookmark> entry_camera;
  GuiSelectionModel entry_selection;
  std::optional<GuiSelection> entry_viewport_selection;
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
    dynamic_label = new QLabel("Dynamic Value", content);
    column->addWidget(dynamic_label);
    column->addWidget(dynamic);
    scroll = new QScrollArea(content);
    scroll->setObjectName("blcad.feature_edit.inputs_scroll");
    scroll->setMinimumWidth(380);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    column->addWidget(scroll, 1);
    guidance = new QLabel(content);
    guidance->setWordWrap(true);
    column->addWidget(guidance);
    auto* footer = new QHBoxLayout;
    apply = make_gui_button("Apply", content);
    apply->setObjectName("blcad.feature_edit.apply");
    apply->setProperty("guiRole", "primary");
    auto* cancel = make_gui_button("Cancel", content);
    cancel->setObjectName("blcad.feature_edit.cancel");
    footer->addWidget(apply);
    another = make_gui_button("Create Another", content);
    another->setObjectName("blcad.feature_edit.create_another");
    another->hide();
    footer->addWidget(another);
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
    QObject::connect(another, &QPushButton::clicked, &owner, [this] {
      auto result = owner.apply(true);
      if (!result)
        window.report_error(result.error());
    });
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
  bool hole_thread() const noexcept {
    return edit.kind() == Kind::Hole || edit.kind() == Kind::Thread;
  }
  QFormLayout* make_form(QWidget* parent = nullptr) const {
    auto* form = new QFormLayout(parent);
    if (hole_thread()) {
      form->setRowWrapPolicy(QFormLayout::WrapLongRows);
      form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    }
    return form;
  }
  bool immutable_field(const QString& path) const {
    return hole_thread() && (path.startsWith("/support/") || path.contains("/table/") ||
                             path.contains("/columns/") || path.endsWith("/row") ||
                             path.endsWith("/diameter_column"));
  }
  bool body_available(const Body& body) const {
    return body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
           session.presentation_visible("body/" + body.id().value()) &&
           session.part_shape_cache() && session.part_shape_cache()->find_body_shape(body.id());
  }
  bool can_create() const {
    const auto* part = session.part_document();
    return part && !part->is_reference_part() && session.workspace() == GuiWorkspace::Part &&
           session.part_modeling_ready() && !session.reference_repair_mode() &&
           !session.task().active() && !session.part_workflow_active() && !window.active_sketch() &&
           !edit.active() &&
           std::any_of(part->bodies().begin(), part->bodies().end(),
                       [this](const auto& b) { return body_available(b); });
  }
  Result<std::size_t> collect_supports(const BodyId& body) {
    supports.clear();
    const auto* part = session.part_document();
    const auto* cache = session.part_shape_cache();
    if (!part || !cache || !part->find_body(body) || !body_available(*part->find_body(body)))
      return Result<std::size_t>::failure(error("Choose a current visible solid target."));
    const auto add_plane = [&](const auto& plane, bool construction, const char* prefix) {
      if (!geometry::WorkplaneResolver{}.resolve(*part, DatumPlaneId(plane.id().value()), *cache))
        return;
      auto ref =
          construction
              ? PlaneReference::create_construction_plane(PartFeatureInputRole::HoleSupportPlane,
                                                          ConstructionPlaneId(plane.id().value()))
              : PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                   DatumPlaneId(plane.id().value()));
      if (ref && validate_part_feature_input_reference(*part, ref.value()))
        supports.push_back(
            {std::string(prefix) + plane.id().value(), plane.name(), ref.value(), {}});
    };
    for (const auto& plane : part->datum_planes())
      add_plane(plane, false, "datum-plane/");
    for (const auto& plane : part->construction_planes())
      add_plane(plane, true, "construction-plane/");
    for (const auto& plane : part->derived_workplanes())
      add_plane(plane, false, "derived-workplane/");
    for (const auto& plane : part->result_face_workplanes())
      add_plane(plane, false, "result-face-workplane/");
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    auto faces = geometry::FinishingSelectionBuilder::build(
        *part, *cache, body, PartFeatureInputRole::ShellRemovalFace, context.value().get());
    if (!faces)
      return Result<std::size_t>::failure(faces.error());
    for (const auto& face : faces.value())
      if (const auto* ref = std::get_if<FaceReference>(&face.reference))
        if (const auto* native = std::get_if<SemanticFaceReference>(&ref->source())) {
          auto plane =
              PlaneReference::create_semantic_face(PartFeatureInputRole::HoleSupportPlane, *native);
          if (plane)
            supports.push_back({face.semantic_id, face.label, plane.value(), face.shape});
        }
    if (context.value()) {
      auto fresh = context.value()->validate_sources();
      if (!fresh)
        return fresh;
    }
    return Result<std::size_t>::success(supports.size());
  }
  void pick_mode() {
    if (!creating || !edit.active())
      return;
    if (!support_chosen) {
      valid = false;
      viewport.clear_feature_preview();
    }
    viewport.clear_preview_selection_targets();
    std::vector<std::string> scope;
    if (picking == Pick::Body) {
      viewport.clear_feature_preview();
      for (const auto& body : session.part_document()->bodies())
        if (body_available(body))
          scope.push_back("body/" + body.id().value());
    } else if (picking == Pick::Support) {
      std::vector<geometry::ViewportSceneItem> targets;
      for (const auto& choice : supports) {
        scope.push_back(choice.semantic_id);
        if (!choice.shape.empty())
          targets.push_back({geometry::ViewportSceneKind::Face, choice.semantic_id, choice.shape});
      }
      if (!targets.empty()) {
        auto result = viewport.set_preview_selection_targets(std::move(targets));
        if (!result)
          window.report_error(result.error());
      }
    }
    viewport.set_selection_semantic_scope(std::move(scope));
    viewport.set_selection_filter_mask(
        picking == Pick::Body      ? selection_kind_bit(GuiSelectionKind::Body)
        : picking == Pick::Support ? selection_kind_bit(GuiSelectionKind::Face) |
                                         selection_kind_bit(GuiSelectionKind::Datum)
                                   : 0);
    if (target_selector)
      target_selector->set_active(picking == Pick::Body);
    if (support_selector)
      support_selector->set_active(picking == Pick::Support);
    apply->setEnabled(valid && support_chosen && picking == Pick::None);
    another->setEnabled(valid && support_chosen && picking == Pick::None);
  }
  template <class Record> Result<Record> selected_record(Record record) const {
    if constexpr (std::is_same_v<Record, HoleFeature>) {
      if (creating && support_override)
        return HoleFeature::create(record.id(), record.name(), record.kind(), record.target_body(),
                                   *support_override, record.placements(), record.diameter(),
                                   record.extent(), record.direction(), record.tip(),
                                   record.manufacturing(), record.thread());
    }
    return Result<Record>::success(std::move(record));
  }
  const Parameter* parameter(const std::string& id) const {
    if (const auto* found = session.part_document()->find_parameter(ParameterId(id)))
      return found;
    const auto found = std::find_if(pending_parameters.begin(), pending_parameters.end(),
                                    [&](const auto& p) { return p.id().value() == id; });
    return found == pending_parameters.end() ? nullptr : &*found;
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
    else if (key == "body" || key == "target_body" || key == "tools" || key == "tool_bodies") {
      if (creating && key == "target_body") {
        for (const auto& body : part.bodies())
          if (body_available(body))
            result.emplace_back(body.id().value(), body.name());
      } else
        add(part.bodies());
    } else if (key == "source_feature" || key == "target_feature" || key == "feature") {
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
    for (const auto& p : pending_parameters)
      if (p.type() == selected.type() && !session.part_document()->find_parameter(p.id()))
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
        if (!fresh()) {
          owner.cancel();
          return;
        }
        build_form();
        (void)preview();
      }
    });
  }
  void build_support_selector(QFormLayout* form) {
    auto* input = combo(form->parentWidget());
    input->setObjectName("blcad.hole.support.choices");
    int selected = -1;
    for (std::size_t i = 0; i < supports.size(); ++i) {
      input->addItem(QString::fromStdString(supports[i].label),
                     QString::fromStdString(supports[i].semantic_id));
      if (support_chosen && support_override &&
          plane_identity(*support_override) == plane_identity(supports[i].reference))
        selected = int(i);
    }
    input->setCurrentIndex(selected);
    support_field = input;
    register_field(input, "/support");
    support_selector = new ShellPropertySelector(
        "blcad.hole.support", "Support", *input, owner,
        [this](bool picked) {
          picking = picked ? Pick::Support : Pick::None;
          pick_mode();
          if (!picked)
            (void)preview();
        },
        form->parentWidget());
    form->addRow(support_selector);
    QObject::connect(input, &QComboBox::currentIndexChanged, &owner, [this](int index) {
      if (building)
        return;
      picking = Pick::None;
      support_chosen = index >= 0 && std::size_t(index) < supports.size();
      if (support_chosen)
        support_override = supports[index].reference;
      else
        support_override.reset();
      pick_mode();
      (void)preview();
    });
  }
  void build_fields(QFormLayout* form, const Json& object, const QString& prefix) {
    for (auto it = object.begin(); it != object.end(); ++it) {
      const auto key = it.key();
      if ((key == "id" && !prefix.endsWith("/table")) || key == "type")
        continue;
      const auto path = prefix + "/" + QString::fromStdString(key);
      const auto& item = it.value();
      if (creating && prefix.isEmpty() && key == "support") {
        build_support_selector(form);
        continue;
      }
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
        auto* nested = make_form(group);
        build_fields(nested, item, path);
        form->addRow(group);
      } else if (item.is_array()) {
        const bool stable_ids =
            edit.kind() == Kind::Hole && (path == "/placements" || path == "/manufacturing/steps");
        if (item.empty() && key != "guide_curves" && !array_templates.contains(path))
          continue;
        if (!item.empty())
          array_templates[path] = item.back();
        auto* group = new QGroupBox(label(key), form->parentWidget());
        auto* nested = make_form(group);
        for (std::size_t index = 0; index < item.size(); ++index) {
          const auto entry = path + "/" + QString::number(index);
          auto* box = new QGroupBox(QString::number(index + 1), group);
          box->setObjectName("blcad.feature_edit.entry" + entry);
          if (stable_ids)
            box->setProperty("semanticId",
                             QString::fromStdString(item[index].at("id").get<std::string>()));
          auto* inputs = make_form(box);
          if (item[index].is_object())
            build_fields(inputs, item[index], entry);
          else
            build_leaf(inputs, key, entry, item[index]);
          auto* buttons = new QHBoxLayout;
          for (auto operation : {QString("↑"), QString("↓"), QString("Remove")}) {
            auto* button = make_gui_button(operation, box);
            // Keep the established automation identity independent of the display language.
            const QString identity =
                operation == "Remove" ? QStringLiteral("Entfernen") : operation;
            button->setObjectName("blcad.feature_edit." + identity + entry);
            button->setEnabled(!immutable_field(path + "/") &&
                               (operation == "↑"   ? index > 0
                                : operation == "↓" ? index + 1 < item.size()
                                                   : true));
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
        auto* add = make_gui_button("Add Entry", group);
        add->setObjectName("blcad.feature_edit.add" + path);
        const auto choices = references(key, path);
        add->setEnabled(!immutable_field(path + "/") &&
                        (!item.empty() || array_templates.contains(path) || !choices.empty()));
        QObject::connect(add, &QPushButton::clicked, &owner, [this, path, choices, stable_ids] {
          auto& array = value(path);
          if (!array.empty())
            array.push_back(array.back());
          else if (auto previous = array_templates.find(path); previous != array_templates.end())
            array.push_back(previous->second);
          else if (!choices.empty())
            array.push_back(choices.front().first);
          if (stable_ids && !array.empty()) {
            const std::string prefix = path == "/placements" ? "placement." : "step.";
            std::string id;
            do {
              id = prefix + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            } while (std::any_of(array.begin(), array.end(), [&](const auto& old) {
              return old.at("id").template get<std::string>() == id;
            }));
            array.back()["id"] = id;
          }
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
      if (immutable_field(path)) {
        auto* output = new QLabel(QString::fromStdString(text), parent);
        output->setWordWrap(true);
        output->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        output->setTextInteractionFlags(Qt::TextSelectableByMouse);
        register_field(output, path);
        form->addRow(label(key), output);
        return;
      }
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
      if (hole_thread() && key == "direction")
        choices = edit.kind() == Kind::Hole
                      ? std::vector<std::pair<std::string, std::string>>{{"support_normal",
                                                                          "Support Normal"},
                                                                         {"opposite_support_normal",
                                                                          "Reverse Support Normal"}}
                      : std::vector<std::pair<std::string, std::string>>{
                            {"along_support_axis", "Along Support Axis"},
                            {"opposite_support_axis", "Reverse Support Axis"}};
      if (hole_thread() && key == "handedness")
        choices = {{"right", "Right Hand"}, {"left", "Left Hand"}};
      if (hole_thread() && key == "representation")
        choices = {{"cosmetic", "Cosmetic"}, {"modeled", "Modeled"}};
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
        if (creating && path == "/target_body") {
          target_field = input;
          target_selector = new ShellPropertySelector(
              "blcad.hole.target", "Target Body", *input, owner,
              [this](bool picked) {
                picking = picked ? Pick::Body : Pick::None;
                pick_mode();
                if (!picked)
                  (void)preview();
              },
              parent);
          form->addRow(target_selector);
        } else
          form->addRow(label(key), input);
        QObject::connect(input, &QComboBox::currentIndexChanged, &owner, [this, path, input] {
          value(path) = input->currentData().toString().toStdString();
          if (creating && path == "/target_body") {
            picking = Pick::None;
            support_chosen = false;
            support_override.reset();
            auto catalog = collect_supports(BodyId(value(path).get<std::string>()));
            support_error = catalog ? std::nullopt : std::optional(catalog.error());
            pick_mode();
            deferred_rebuild();
            return;
          }
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
    } else if (item.is_number_unsigned() && path.startsWith("/placements/skipped/") &&
               (key == "first" || key == "second")) {
      auto* input = new QSpinBox(parent);
      input->setRange(1, static_cast<int>(pattern_placement_limit));
      input->setValue(item.get<unsigned>() + 1);
      input->setKeyboardTracking(false);
      register_field(input, path);
      form->addRow(label(key), input);
      QObject::connect(input, &QSpinBox::valueChanged, &owner, [this, path](int index) {
        value(path) = static_cast<unsigned>(index - 1);
        (void)preview();
      });
    } else if (item.is_number()) {
      const auto unit =
          key.find("angle") != std::string::npos ? QuantityKind::AngleDeg : QuantityKind::LengthMm;
      auto* input = number(parent, path, item.get<double>(), unit,
                           !hole_thread() && (key == "x" || key == "y" || key == "z"));
      form->addRow(label(key), input);
      QObject::connect(input, &QDoubleSpinBox::valueChanged, &owner, [this, path](double scalar) {
        value(path) = scalar;
        (void)preview();
      });
    }
  }
  void build_form() {
    building = true;
    target_field = support_field = nullptr;
    target_selector = support_selector = nullptr;
    const auto selected = dynamic->currentData();
    fields.clear();
    numbers.clear();
    units.clear();
    dynamic->clear();
    dynamic->addItem("No Handle", QString());
    auto* panel = new QWidget;
    auto* column = new QVBoxLayout(panel);
    auto* form = make_form();
    column->addLayout(form);
    column->addStretch();
    if (hole_thread()) {
      for (const auto* key : {"name", "placements", "target_body", "support", "direction", "kind",
                              "diameter", "manufacturing", "extent", "tip", "thread"})
        if (record.contains(key))
          build_fields(form, Json{{key, record.at(key)}}, {});
    } else
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
    std::vector<Parameter> changes, added;
    const auto used = bound_parameters(record);
    if (creating)
      for (const auto& p : decoded.parameters())
        if (used.contains(p.id().value()) && !session.part_document()->find_parameter(p.id()))
          added.push_back(p);
    for (const auto& [id, scalar] : parameter_values) {
      (void)scalar;
      if (used.contains(id) && session.part_document()->find_parameter(ParameterId(id)))
        changes.push_back(*decoded.find_parameter(ParameterId(id)));
    }
    return [changes, added](PartDocument& part) -> Result<std::size_t> {
      for (const auto& p : added) {
        auto result = part.add_parameter(p);
        if (!result)
          return result;
      }
      for (const auto& p : changes) {
        auto set = part.set_parameter_value(p.id(), p.value());
        if (!set)
          return Result<std::size_t>::failure(set.error());
      }
      return Result<std::size_t>::success(changes.size() + added.size());
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
    if (!edit.active() || !session.part_document() || !session.part_modeling_ready() ||
        session.task().command_id() != task_id || session.task().stage() != GuiTaskStage::Preview)
      return false;
    auto current = serialize_part_document_to_json(*session.part_document());
    return current && current.value() == source_text;
  }
  Result<std::size_t> reject(Error failure) {
    valid = false;
    apply->setEnabled(false);
    another->setEnabled(false);
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
    if (creating) {
      const auto* body =
          session.part_document()->find_body(BodyId(record.at("target_body").get<std::string>()));
      if (!body || !body_available(*body))
        return reject(error("Choose a current visible solid target."));
      if (support_error)
        return reject(*support_error);
      if (!support_chosen)
        return reject(error("Choose a support plane or planar face."));
      if (picking != Pick::None)
        return Result<std::size_t>::success(0);
    }
    auto import_context = make_step_import_context(session);
    if (!import_context)
      return reject(import_context.error());
    auto* context = import_context.value().get();
    auto decoded = decode();
    if (!decoded)
      return reject(decoded.error());
    auto candidate = with_record(
        decoded.value(), edit.feature(), edit.kind(),
        [&](auto replacement, auto updater) {
          auto selected = selected_record(std::move(replacement));
          if (!selected)
            return Result<PartDocument>::failure(selected.error());
          return edit.candidate(session, std::move(selected.value()), updater,
                                parameter_updates(decoded.value()));
        },
        creating);
    if (!candidate)
      return reject(candidate.error());
    auto cache = geometry::ShapeCache::create(ShapeCacheId("feature.edit.preview"));
    if (!cache)
      return reject(cache.error());
    if (session.history_prefix_count()) {
      auto scoped = session.preview_part_history_candidate(candidate.value());
      if (!scoped)
        return reject(scoped.error());
      cache = std::move(scoped);
    } else {
      auto executed = geometry::GeometryRecomputeExecutor{context}.execute_document(
          candidate.value(), cache.value());
      if (!executed)
        return reject(executed.error());
    }
    const geometry::ViewportSceneBuilder builder{context};
    auto scene =
        builder.build_part(candidate.value(), cache.value(), session.history_prefix_count());
    if (!scene)
      return reject(scene.error());
    auto original = builder.build_part(*session.part_document(), *session.part_shape_cache(),
                                       session.history_prefix_count());
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
    if (context) {
      auto source_fresh = context->validate_sources();
      if (!source_fresh)
        return reject(source_fresh.error());
    }
    if (!shapes.empty() || !replaced.empty()) {
      auto published = viewport.set_feature_previews(std::move(shapes), std::move(replaced));
      if (!published)
        return reject(published.error());
    } else
      viewport.clear_feature_preview();
    valid = true;
    apply->setEnabled(layer.active_handle_id().empty());
    another->setEnabled(creating && layer.active_handle_id().empty());
    status->setText(
        session.history_prefix_count()
            ? "Prefix preview valid. Apply stages this edit; finish End of Part to validate the "
              "complete model."
        : creating
            ? "Plain through hole preview valid. Apply creates the Hole and its required inputs."
            : "Preview valid, including downstream features. Commit changes with Apply.");
    if (layer.active_handle_id().empty())
      publish_handle();
    owner.refresh_presentation();
    return Result<std::size_t>::success(1);
  }
  void publish_handle() {
    if (!edit.active() || building)
      return;
    std::vector<GuiViewportManipulatorHandle> handles;
    // Hole/Thread need support-anchored grips in 157B. Keep this slice numeric.
    if (hole_thread()) {
      (void)layer.set_handles({});
      owner.refresh_presentation();
      return;
    }
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
    const bool restore_creation = creating && !applied && session.part_document() &&
                                  session.model_revision() == entry_revision;
    const auto camera = entry_camera;
    const auto selection = entry_selection;
    const auto viewed = entry_viewport_selection;
    const bool was_creating = creating;
    creating = false;
    picking = Pick::None;
    edit.cancel();
    layer.cancel();
    (void)layer.set_handles({});
    valid = false;
    drag_start.reset();
    if (viewport.mouseGrabber() == &viewport)
      viewport.release_interaction_mouse();
    viewport.clear_feature_preview();
    if (was_creating)
      viewport.clear_preview_selection_targets();
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
    pending_parameters.clear();
    supports.clear();
    support_override.reset();
    support_error.reset();
    another->hide();
    window.refresh();
    if (restore_creation) {
      GuiSelectionModel restored;
      restored.set_filter_mask(selection.filter_mask());
      for (const auto& entry : selection.entries())
        if (viewport.select_semantic(entry.semantic_id))
          (void)restored.add(entry);
      if (!viewed || !viewport.select_semantic(viewed->semantic_id))
        viewport.clear_selection();
      session.selection() = std::move(restored);
      if (camera)
        (void)viewport.restore_camera_bookmark(*camera);
      window.refresh_context_toolbar();
    }
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

ShellFeatureEditTools::ShellFeatureEditTools(ShellWindow& window, ShellRibbon& ribbon,
                                             int model_tab)
    : QObject(&window), impl_(std::make_unique<Impl>(*this, window)) {
  impl_->hole_action = new QAction("Hole", this);
  impl_->hole_action->setObjectName("blcad.action.semantic_hole");
  impl_->hole_action->setToolTip(
      "Create a plain through hole with stable positions and editable Hole properties.");
  apply_gui_action_icon(*impl_->hole_action);
  ribbon.add_button(ribbon.add_group(model_tab, "Design"), impl_->hole_action);
  connect(impl_->hole_action, &QAction::triggered, this, [this] {
    auto result = begin_new_hole();
    if (!result)
      impl_->window.report_error(result.error());
  });
}
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
      !p.session.part_modeling_ready())
    return Result<std::size_t>::failure(
        error("Feature editing requires a current part without an active command"));
  if (const auto prefix = p.session.history_prefix_count()) {
    const auto history = p.session.part_document()->executable_history();
    if (!history)
      return Result<std::size_t>::failure(history.error());
    const auto found = std::find_if(history.value().begin(), history.value().end(),
                                    [&](const auto& row) { return row.node_id == id.value(); });
    if (found == history.value().end() || std::size_t(found - history.value().begin()) >= *prefix)
      return Result<std::size_t>::failure(
          error("Move End of Part after this feature before editing it."));
  }
  auto begun = p.edit.begin(p.session, id);
  if (!begun)
    return begun;
  p.creating = false;
  p.pending_parameters.clear();
  p.supports.clear();
  p.support_override.reset();
  p.support_error.reset();
  p.picking = Impl::Pick::None;
  p.another->hide();
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
  p.dynamic->setVisible(!p.hole_thread());
  p.dynamic_label->setVisible(!p.hole_thread());
  p.guidance->setText(
      p.hole_thread()
          ? "The preview checks downstream features. Shared parameters affect every use. "
            "Formula values follow their expressions; select another binding to rebind. "
            "Escape cancels editing."
          : "The preview also checks downstream features. Shared parameters affect every use; "
            "formula values are determined by their formulas. Escape resets a drag; press "
            "Escape again to cancel.");
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
Result<std::size_t> ShellFeatureEditTools::begin_new_hole() {
  auto& p = *impl_;
  if (!p.can_create()) {
    p.next_diameter.reset();
    p.next_target.reset();
    return Result<std::size_t>::failure(
        error("Hole creation requires a complete editable Part with a visible solid and no active "
              "command or workflow."));
  }
  const auto& part = *p.session.part_document();
  const auto source_text = serialize_part_document_to_json(part);
  if (!source_text)
    return Result<std::size_t>::failure(source_text.error());
  BodyId target("");
  for (const auto& body : part.bodies())
    if (p.body_available(body)) {
      target = body.id();
      break;
    }
  bool explicit_body = false;
  if (p.next_target) {
    if (const auto* body = part.find_body(*p.next_target); body && p.body_available(*body)) {
      target = body->id();
      explicit_body = true;
    }
  }
  std::optional<std::string> reference;
  const auto selections = p.session.selection().entries();
  for (const auto& selected : selections) {
    if (selected.kind == GuiSelectionKind::Body) {
      bool found = false;
      for (const auto& body : part.bodies())
        if (selected.semantic_id == "body/" + body.id().value() && p.body_available(body)) {
          target = body.id();
          explicit_body = true;
          found = true;
          break;
        }
      if (!found)
        return Result<std::size_t>::failure(error("Select a current visible solid target."));
    } else if (selected.kind == GuiSelectionKind::Face || selected.kind == GuiSelectionKind::Datum)
      reference = selected.semantic_id;
  }
  std::optional<PlaneReference> support;
  if (reference) {
    for (const auto& body : part.bodies()) {
      if (!p.body_available(body) || (explicit_body && body.id() != target))
        continue;
      auto catalog = p.collect_supports(body.id());
      if (!catalog)
        return catalog;
      for (const auto& choice : p.supports)
        if (choice.semantic_id == *reference) {
          target = body.id();
          support = choice.reference;
          break;
        }
      if (support)
        break;
    }
    if (!support)
      return Result<std::size_t>::failure(
          error("Choose a supported native planar face or existing workplane."));
  } else {
    auto catalog = p.collect_supports(target);
    if (!catalog)
      return catalog;
  }
  if (p.supports.empty())
    return Result<std::size_t>::failure(error("No resolvable Hole support is available."));
  if (!support) {
    support = p.supports.front().reference;
    for (const auto& choice : p.supports)
      if (const auto* face = std::get_if<SemanticFaceReference>(&choice.reference.source());
          face && face->face() == SemanticFace::Top) {
        support = choice.reference;
        break;
      }
  }
  const auto diameter_value = p.next_diameter.value_or(6.0);
  p.next_diameter.reset();
  p.next_target.reset();
  const auto tag = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  const FeatureId id("feature.hole." + tag);
  const ParameterId diameter("parameter.hole." + tag + ".diameter");
  std::size_t index = 1;
  while (part.find_parameter(std::string_view("hole_" + std::to_string(index) + "_diameter")))
    ++index;
  auto quantity = Quantity::length_mm(diameter_value);
  if (!quantity)
    return Result<std::size_t>::failure(quantity.error());
  auto parameter = Parameter::create_length(diameter, "hole_" + std::to_string(index) + "_diameter",
                                            quantity.value());
  if (!parameter)
    return Result<std::size_t>::failure(parameter.error());
  auto definition =
      HoleFeature::create(id, "Hole " + std::to_string(index), HoleKind::Plain, target, *support,
                          {{HolePlacementId("placement." + tag), Point2{}}}, diameter);
  if (!definition)
    return Result<std::size_t>::failure(definition.error());
  auto seed = part;
  auto added = seed.add_parameter(parameter.value());
  if (added)
    added = seed.add_hole_feature(definition.value());
  if (!added)
    return added;
  auto encoded = serialize_part_document_to_json(seed);
  if (!encoded)
    return Result<std::size_t>::failure(encoded.error());
  auto begun = p.edit.begin_new_hole(p.session, id);
  if (!begun)
    return begun;
  p.entry_revision = p.session.model_revision();
  p.entry_camera = p.viewport.camera_bookmark();
  p.entry_selection = p.session.selection();
  p.entry_viewport_selection = p.viewport.selected_semantic();
  p.previous_mask = p.viewport.selection_filter_mask();
  p.previous_scope = p.viewport.selection_semantic_scope();
  p.creating = true;
  p.picking = Impl::Pick::None;
  p.support_override = support;
  p.support_chosen = true;
  p.support_error.reset();
  p.pending_parameters = {parameter.value()};
  p.source_text = source_text.value();
  p.source = Json::parse(encoded.value());
  p.collection = "hole_features";
  p.record_index = p.source.at(p.collection).size() - 1;
  p.record = p.source.at(p.collection).at(p.record_index);
  p.parameter_values.clear();
  p.array_templates.clear();
  if (!p.session.task().begin(task_id) || !p.session.task().begin_parameter_editing() ||
      !p.session.task().show_preview()) {
    p.edit.cancel();
    p.creating = false;
    p.pending_parameters.clear();
    return Result<std::size_t>::failure(error("Could not start Hole task."));
  }
  p.dock->setWindowTitle("Create Hole");
  p.dynamic->hide();
  p.dynamic_label->hide();
  p.another->show();
  p.guidance->setText(
      "Plain through hole. Choose target and support, then edit local X/Y positions and diameter. "
      "Apply creates one Hole; Create Another opens a fresh draft.");
  p.build_form();
  p.pick_mode();
  p.dock->show();
  p.dock->raise();
  (void)p.preview();
  p.window.refresh();
  return Result<std::size_t>::success(1);
}

bool ShellFeatureEditTools::accept_selection(const GuiSelection& incoming) {
  auto& p = *impl_;
  if (!p.creating || !active() || p.picking == Impl::Pick::None)
    return false;
  const auto selected = incoming;
  if (p.picking == Impl::Pick::Body && selected.kind == GuiSelectionKind::Body && p.target_field) {
    for (int i = 0; i < p.target_field->count(); ++i)
      if (selected.semantic_id == "body/" + p.target_field->itemData(i).toString().toStdString()) {
        const bool changed = i != p.target_field->currentIndex();
        p.target_field->setCurrentIndex(i);
        if (!changed) {
          p.picking = Impl::Pick::None;
          p.pick_mode();
          (void)p.preview();
        }
        return true;
      }
  }
  if (p.picking == Impl::Pick::Support && p.support_field) {
    for (std::size_t i = 0; i < p.supports.size(); ++i)
      if (selected.semantic_id == p.supports[i].semantic_id &&
          (selected.kind == GuiSelectionKind::Face || selected.kind == GuiSelectionKind::Datum)) {
        const QSignalBlocker block(p.support_field);
        p.support_field->setCurrentIndex(int(i));
        p.support_override = p.supports[i].reference;
        p.support_chosen = true;
        p.picking = Impl::Pick::None;
        p.pick_mode();
        (void)p.preview();
        return true;
      }
  }
  return false;
}

Result<std::size_t> ShellFeatureEditTools::apply(bool create_another) {
  auto& p = *impl_;
  if (!p.fresh() || !p.layer.active_handle_id().empty() || p.picking != Impl::Pick::None ||
      (create_another && !p.creating))
    return Result<std::size_t>::failure(error("No current applicable feature edit"));
  auto preview = p.preview();
  if (!preview)
    return preview;
  auto decoded = p.decode();
  if (!decoded)
    return Result<std::size_t>::failure(decoded.error());
  auto committed = with_record(
      decoded.value(), p.edit.feature(), p.edit.kind(),
      [&](auto replacement, auto updater) {
        auto selected = p.selected_record(std::move(replacement));
        if (!selected)
          return Result<std::size_t>::failure(selected.error());
        return p.edit.commit(p.session, std::move(selected.value()), updater,
                             p.parameter_updates(decoded.value()));
      },
      p.creating);
  if (committed) {
    const bool created_hole = p.creating;
    std::optional<double> repeated_diameter;
    std::optional<BodyId> repeated_target;
    if (p.creating) {
      const auto* hole = p.session.part_document()->find_hole_feature(p.edit.feature());
      if (hole) {
        auto size = resolve_hole_diameter(*p.session.part_document(), hole->diameter());
        if (size)
          repeated_diameter = size.value().diameter.millimeters();
        repeated_target = hole->target_body();
      }
    }
    p.finish(true);
    if (created_hole)
      p.window.remember_repeatable_command(*p.hole_action);
    if (create_another) {
      p.next_diameter = repeated_diameter;
      p.next_target = repeated_target;
      auto next = begin_new_hole();
      if (!next)
        p.window.report_error(next.error());
    }
  }
  return committed;
}
void ShellFeatureEditTools::cancel() {
  if (active())
    impl_->finish(false);
}
void ShellFeatureEditTools::refresh() {
  auto& p = *impl_;
  if (p.hole_action)
    p.hole_action->setEnabled(p.can_create());
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
