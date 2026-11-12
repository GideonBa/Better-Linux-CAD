#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/gui_hole_recipe.hpp"
#include "blcad/gui/gui_icons.hpp"
#include "blcad/gui/gui_step_import_context.hpp"
#include "blcad/gui/gui_thread_recipe.hpp"
#include "blcad/gui/shell/shell_manipulator_overlay.hpp"
#include "blcad/gui/shell/shell_property_controls.hpp"
#include "blcad/gui/shell/shell_ribbon.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
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
#include <utility>

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
  case Kind::Thread:
    return callback(*part.find_thread_feature(id), creating ? &PartDocument::add_thread_feature
                                                            : &PartDocument::update_thread_feature);
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
  QAction* thread_action{};
  QPushButton* another{};
  bool creating{};
  enum class Pick { None, Body, Support };
  Pick picking{Pick::None};
  struct SupportChoice {
    std::string semantic_id, label;
    PlaneReference reference;
    geometry::GeometryShape shape;
    std::optional<ResultFaceWorkplane> workplane{};
  };
  std::vector<SupportChoice> supports;
  struct ThreadSupportChoice {
    std::string semantic_id, label;
    FaceReference reference;
    geometry::GeometryShape shape;
    geometry::ThreadSupportInspection geometry;
  };
  std::vector<ThreadSupportChoice> thread_supports;
  std::optional<FaceReference> thread_support_override;
  std::string thread_support_id;
  std::optional<PlaneReference> support_override;
  std::optional<ResultFaceWorkplane> pending_support_plane;
  bool support_chosen{};
  std::optional<Error> support_error;
  std::vector<Parameter> pending_parameters;
  std::string recipe_namespace;
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
  QString pending_focus_id;

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
  Result<std::size_t> start_creation(const PartDocument& seed, FeatureId id, Kind kind,
                                     std::string draft_namespace) {
    auto encoded = serialize_part_document_to_json(seed);
    auto original = serialize_part_document_to_json(*session.part_document());
    if (!encoded || !original)
      return Result<std::size_t>::failure(encoded ? original.error() : encoded.error());
    auto begun = kind == Kind::Thread ? edit.begin_new_thread(session, id)
                                      : edit.begin_new_hole(session, id);
    if (!begun)
      return begun;
    entry_revision = session.model_revision();
    entry_camera = viewport.camera_bookmark();
    entry_selection = session.selection();
    entry_viewport_selection = viewport.selected_semantic();
    previous_mask = viewport.selection_filter_mask();
    previous_scope = viewport.selection_semantic_scope();
    creating = true;
    recipe_namespace = std::move(draft_namespace);
    picking = Pick::None;
    support_chosen = true;
    support_error.reset();
    pending_parameters.clear();
    for (const auto& parameter : seed.parameters())
      if (!session.part_document()->find_parameter(parameter.id()))
        pending_parameters.push_back(parameter);
    source_text = original.value();
    source = Json::parse(encoded.value());
    collection = kind == Kind::Thread ? "thread_features" : "hole_features";
    record_index = source.at(collection).size() - 1;
    record = source.at(collection).at(record_index);
    parameter_values.clear();
    array_templates.clear();
    if (!session.task().begin(task_id) || !session.task().begin_parameter_editing() ||
        !session.task().show_preview()) {
      edit.cancel();
      creating = false;
      pending_parameters.clear();
      return Result<std::size_t>::failure(error("Could not start feature creation task."));
    }
    dock->setWindowTitle(kind == Kind::Thread ? "Create Thread" : "Create Hole");
    dynamic->hide();
    dynamic_label->hide();
    another->setVisible(kind == Kind::Hole);
    guidance->setText(
        kind == Kind::Thread
            ? "Choose the target and exterior cylindrical or conical support. Edit the custom "
              "profile and interval, then Apply. Modeled taper remains unsupported."
            : "Choose target and support, then edit local X/Y positions, recipe and dimensions. "
              "Apply creates one Hole; Create Another opens a fresh draft.");
    build_form();
    pick_mode();
    dock->show();
    dock->raise();
    (void)preview();
    window.refresh();
    return Result<std::size_t>::success(1);
  }
  Result<std::size_t> collect_thread_supports(const BodyId& body) {
    thread_supports.clear();
    const auto* part = session.part_document();
    const auto* cache = session.part_shape_cache();
    if (!part || !cache || !part->find_body(body) || !body_available(*part->find_body(body)))
      return Result<std::size_t>::failure(error("Choose a current visible solid target."));
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    auto faces = geometry::FinishingSelectionBuilder::build(
        *part, *cache, body, PartFeatureInputRole::ShellRemovalFace, context.value().get());
    if (!faces)
      return Result<std::size_t>::failure(faces.error());
    for (const auto& face : faces.value()) {
      const auto* ref = std::get_if<FaceReference>(&face.reference);
      if (!ref)
        continue;
      auto selected = std::visit(
          [](const auto& source) -> Result<FaceReference> {
            using T = std::decay_t<decltype(source)>;
            constexpr auto role = PartFeatureInputRole::ThreadSupportFace;
            if constexpr (std::is_same_v<T, SemanticFaceReference>)
              return FaceReference::create_planar(role, source);
            else if constexpr (std::is_same_v<T, SemanticCylindricalFaceReference>)
              return FaceReference::create_cylindrical(role, source);
            else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
              return FaceReference::create_imported(role, source);
            else
              return FaceReference::create_result(role, source);
          },
          ref->source());
      if (!selected)
        continue;
      auto inspected = geometry::ThreadAdapter::inspect_support(
          *part, body, selected.value(), *cache, ThreadDirection::AlongSupportAxis,
          context.value().get());
      if (inspected)
        thread_supports.push_back(
            {face.semantic_id, face.label, selected.value(), face.shape, inspected.value()});
    }
    if (context.value()) {
      auto fresh = context.value()->validate_sources();
      if (!fresh)
        return fresh;
    }
    return Result<std::size_t>::success(thread_supports.size());
  }
  template <class Callback> void each_support(Callback callback) const {
    if (edit.kind() == Kind::Thread)
      for (const auto& support : thread_supports)
        callback(support);
    else
      for (const auto& support : supports)
        callback(support);
  }
  bool choose_support(int index) {
    if (edit.kind() == Kind::Thread) {
      thread_support_override.reset();
      thread_support_id.clear();
      if (index < 0 || std::size_t(index) >= thread_supports.size())
        return false;
      thread_support_override = thread_supports[index].reference;
      thread_support_id = thread_supports[index].semantic_id;
    } else {
      support_override.reset();
      pending_support_plane.reset();
      if (index < 0 || std::size_t(index) >= supports.size())
        return false;
      support_override = supports[index].reference;
      pending_support_plane = supports[index].workplane;
    }
    return true;
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
      if (const auto* ref = std::get_if<FaceReference>(&face.reference)) {
        if (const auto* native = std::get_if<SemanticFaceReference>(&ref->source())) {
          auto plane =
              PlaneReference::create_semantic_face(PartFeatureInputRole::HoleSupportPlane, *native);
          if (plane)
            supports.push_back({face.semantic_id, face.label, plane.value(), face.shape});
        } else if (const auto* imported = std::get_if<ImportedTopologyReference>(&ref->source())) {
          auto identity = ImportedTopologyReference::create(imported->catalog(), imported->entry(),
                                                            ImportedTopologyCapability::Plane);
          if (!identity)
            continue;
          auto plane = PlaneReference::create_imported(PartFeatureInputRole::HoleSupportPlane,
                                                       identity.value());
          if (plane && validate_part_feature_input_reference(*part, plane.value()))
            supports.push_back({face.semantic_id,
                                face.label + " / (" + QString::number(face.anchor.x).toStdString() +
                                    ", " + QString::number(face.anchor.y).toStdString() + ", " +
                                    QString::number(face.anchor.z).toStdString() + ") mm",
                                plane.value(), face.shape});
        } else if (const auto* result = std::get_if<ResultFaceReference>(&ref->source())) {
          const auto tag = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
          auto workplane = ResultFaceWorkplane::create(DatumPlaneId("plane.hole." + tag),
                                                       face.label + " / Hole support", *result);
          if (!workplane || !geometry::ResultFaceSupportGeometry::resolve(
                                *part, workplane.value(), *cache, context.value().get()))
            continue;
          auto plane = PlaneReference::create_datum_plane(PartFeatureInputRole::HoleSupportPlane,
                                                          workplane.value().id());
          if (plane)
            supports.push_back(
                {face.semantic_id, face.label, plane.value(), face.shape, workplane.value()});
        }
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
      each_support([&](const auto& choice) {
        scope.push_back(choice.semantic_id);
        if (!choice.shape.empty())
          targets.push_back({geometry::ViewportSceneKind::Face, choice.semantic_id, choice.shape});
      });
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
    } else if constexpr (std::is_same_v<Record, ThreadFeature>) {
      if (creating && thread_support_override)
        return ThreadFeature::create(record.id(), record.name(), record.target_body(),
                                     *thread_support_override, record.thread(), record.direction());
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
  QString retained_focus_id() const {
    for (auto* focused = QApplication::focusWidget(); focused && scroll->isAncestorOf(focused);
         focused = focused->parentWidget())
      if (focused->property("blcad.restoreFocusAfterRebuild").toBool())
        return focused->objectName();
    return {};
  }
  void deferred_rebuild() {
    if (const auto focus = retained_focus_id(); !focus.isEmpty())
      pending_focus_id = focus;
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
  Result<PartDocument> recipe_document() const {
    auto part = decode();
    if (!part || edit.kind() != Kind::Hole)
      return part;
    if (pending_support_plane &&
        !part.value().find_result_face_workplane(pending_support_plane->id())) {
      auto added = part.value().add_result_face_workplane(*pending_support_plane);
      if (!added)
        return Result<PartDocument>::failure(added.error());
    }
    auto hole = selected_record(*part.value().find_hole_feature(edit.feature()));
    if (!hole)
      return Result<PartDocument>::failure(hole.error());
    auto updated = part.value().update_hole_feature(std::move(hole.value()));
    if (!updated)
      return Result<PartDocument>::failure(updated.error());
    return part;
  }
  void adopt_recipe(Result<PartDocument> candidate) {
    if (!candidate) {
      const auto failure = candidate.error();
      build_form();
      (void)reject(failure);
      return;
    }
    auto encoded = serialize_part_document_to_json(candidate.value());
    if (!encoded) {
      (void)reject(encoded.error());
      return;
    }
    pending_parameters.clear();
    std::erase_if(parameter_values, [this](const auto& entry) {
      return !session.part_document()->find_parameter(ParameterId(entry.first));
    });
    for (const auto& parameter : candidate.value().parameters())
      if (!session.part_document()->find_parameter(parameter.id()))
        pending_parameters.push_back(parameter);
    source = Json::parse(encoded.value());
    record = source.at(collection).at(record_index);
    deferred_rebuild();
  }
  template <class Command> void recipe_command(Command command) {
    auto part = recipe_document();
    if (!part) {
      (void)reject(part.error());
      return;
    }
    adopt_recipe(command(std::move(part.value())));
  }
  static std::optional<GuiHoleDimensionSlot> dimension_slot(const QString& path) {
    using R = GuiHoleDimensionRole;
    if (path == "/diameter")
      return GuiHoleDimensionSlot{R::Bore};
    if (path == "/manufacturing/diameter")
      return GuiHoleDimensionSlot{R::SeatDiameter};
    if (path == "/manufacturing/depth")
      return GuiHoleDimensionSlot{R::SeatDepth};
    if (path == "/manufacturing/included_angle")
      return GuiHoleDimensionSlot{R::SeatAngle};
    if (path == "/manufacturing/minimum_diameter")
      return GuiHoleDimensionSlot{R::Minimum};
    if (path == "/manufacturing/maximum_diameter")
      return GuiHoleDimensionSlot{R::Maximum};
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() == 4 && parts[0] == "manufacturing" && parts[1] == "steps") {
      bool ok = false;
      const auto step = parts[2].toULongLong(&ok);
      if (ok && (parts[3] == "diameter" || parts[3] == "depth"))
        return GuiHoleDimensionSlot{parts[3] == "diameter" ? R::StepDiameter : R::StepDepth,
                                    std::size_t(step)};
    }
    return {};
  }
  void build_dimension_source(QFormLayout* form, const QString& path, GuiHoleDimensionSlot slot) {
    auto part = recipe_document();
    if (!part)
      return;
    auto current = GuiHoleRecipeCommands::dimension_source(
        *part.value().find_hole_feature(edit.feature()), slot);
    if (!current)
      return;
    auto* input = combo(form->parentWidget());
    input->setObjectName("blcad.hole.source" + path);
    input->setProperty("blcad.restoreFocusAfterRebuild", true);
    input->addItem("Custom parameter", "custom");
    std::vector<HoleStandardSelection> choices;
    int selected = 0;
    const bool angle = slot.role == GuiHoleDimensionRole::SeatAngle;
    for (const auto& resource : part.value().engineering_resources().entries())
      if (const auto* table = std::get_if<EngineeringStandardTable>(&resource))
        for (const auto& row : table->rows())
          for (const auto& cell : row.cells)
            if (const auto* quantity = std::get_if<Quantity>(&cell.value);
                quantity &&
                quantity->kind() == (angle ? QuantityKind::AngleDeg : QuantityKind::LengthMm) &&
                (angle || quantity->is_positive_length())) {
              choices.push_back({table->key(), row.key, cell.column});
              const auto label = table->name() + " / " + table->key().revision() + " / " +
                                 row.name + " / " + cell.column;
              input->addItem(QString::fromStdString(label));
              input->setItemData(input->count() - 1,
                                 QStringList{QString::fromStdString(table->key().id().value()),
                                             QString::fromStdString(table->key().revision()),
                                             QString::fromStdString(row.key),
                                             QString::fromStdString(cell.column)});
              input->setItemData(input->count() - 1,
                                 QString::fromStdString(cell.provenance.source()), Qt::ToolTipRole);
              if (const auto* pinned = std::get_if<HoleStandardSelection>(&current.value());
                  pinned && *pinned == choices.back())
                selected = input->count() - 1;
            }
    input->setCurrentIndex(selected);
    form->addRow("Size Source", input);
    if (std::holds_alternative<HoleStandardSelection>(current.value())) {
      const auto resolved = resolve_hole_dimension(part.value(), current.value(), angle);
      if (resolved) {
        auto* output = new QLabel(QString::number(angle ? resolved.value().diameter.degrees()
                                                        : resolved.value().diameter.millimeters()) +
                                      (angle ? " deg" : " mm"),
                                  form->parentWidget());
        output->setObjectName("blcad.hole.resolved" + path);
        output->setWordWrap(true);
        output->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        if (resolved.value().provenance)
          output->setToolTip(QString::fromStdString(resolved.value().provenance->source()));
        form->addRow("Resolved Size", output);
      }
    }
    QObject::connect(
        input, &QComboBox::currentIndexChanged, &owner, [this, slot, choices](int index) {
          recipe_command([&](PartDocument part) {
            return index == 0
                       ? GuiHoleRecipeCommands::custom_dimension(std::move(part), edit.feature(),
                                                                 recipe_namespace, slot)
                       : GuiHoleRecipeCommands::dimension(std::move(part), edit.feature(), slot,
                                                          choices.at(std::size_t(index - 1)));
          });
        });
  }
  void build_hole_modes(QFormLayout* form, const std::string& key) {
    if (edit.kind() != Kind::Hole)
      return;
    const auto add = [&](const char* name, const QString& title, const QStringList& labels,
                         int selected, auto changed) {
      auto* input = combo(form->parentWidget());
      input->setObjectName(QString::fromLatin1(name));
      input->setProperty("blcad.restoreFocusAfterRebuild", true);
      input->addItems(labels);
      input->setCurrentIndex(selected);
      form->addRow(title, input);
      QObject::connect(input, &QComboBox::currentIndexChanged, &owner, changed);
    };
    if (key == "manufacturing") {
      const auto mode = record.contains("manufacturing")
                            ? record["manufacturing"].value("kind", "simple")
                            : "simple";
      int selected = QStringList{"simple", "counterbore", "countersink", "spotface", "stepped",
                                 "fitted", "reamed",      "center",      "slot"}
                         .indexOf(QString::fromStdString(mode));
      if (mode == "fitted" && record["manufacturing"].value("process", "fitted") == "reamed")
        selected = 6;
      add("blcad.hole.recipe", "Seat / Manufacturing",
          {"Simple", "Counterbore", "Countersink", "Spotface", "Stepped", "Fitted", "Reamed",
           "Center", "Slot"},
          selected, [this](int index) {
            recipe_command([&](PartDocument part) {
              std::optional<ProfileRegionReference> slot;
              for (const auto& sketch : part.sketches())
                if (!sketch.arc_closed_profiles().empty()) {
                  auto region = ProfileRegionReference::create(
                      sketch.id(), sketch.arc_closed_profiles().front().id());
                  if (region) {
                    slot = region.value();
                    break;
                  }
                }
              return GuiHoleRecipeCommands::recipe(std::move(part), edit.feature(),
                                                   recipe_namespace, GuiHoleRecipe(index), slot);
            });
          });
    } else if (key == "extent") {
      const auto mode = record["extent"].value("kind", "through_all");
      add("blcad.hole.extent", "Termination",
          {"Blind", "Through All", "To Next", "To Face", "Up to Body", "Symmetric"},
          QStringList{"blind", "through_all", "to_next", "to_face", "up_to_body", "symmetric"}
              .indexOf(QString::fromStdString(mode)),
          [this](int index) {
            recipe_command([&](PartDocument part) -> Result<PartDocument> {
              std::optional<FaceReference> face;
              std::optional<BodyId> body;
              const auto* hole = part.find_hole_feature(edit.feature());
              if (index == int(GuiHoleExtentChoice::ToFace)) {
                auto context = make_step_import_context(session);
                if (!context)
                  return Result<PartDocument>::failure(context.error());
                auto targets = geometry::FinishingSelectionBuilder::build(
                    *session.part_document(), *session.part_shape_cache(), hole->target_body(),
                    PartFeatureInputRole::ShellRemovalFace, context.value().get());
                if (!targets)
                  return Result<PartDocument>::failure(targets.error());
                for (const auto& target : targets.value())
                  if (const auto* ref = std::get_if<FaceReference>(&target.reference))
                    if (const auto* native = std::get_if<SemanticFaceReference>(&ref->source());
                        native && native->face() == SemanticFace::Bottom) {
                      auto limit = FaceReference::create_planar(PartFeatureInputRole::HoleLimitFace,
                                                                *native);
                      if (!limit)
                        return Result<PartDocument>::failure(limit.error());
                      face = std::move(limit.value());
                      break;
                    }
              }
              if (index == int(GuiHoleExtentChoice::UpToBody))
                for (const auto& candidate : part.bodies())
                  if (candidate.id() != hole->target_body() && body_available(candidate)) {
                    body = candidate.id();
                    break;
                  }
              return GuiHoleRecipeCommands::extent(std::move(part), edit.feature(),
                                                   recipe_namespace, GuiHoleExtentChoice(index),
                                                   face, body);
            });
          });
    } else if (key == "tip") {
      add("blcad.hole.tip", "Drill Tip", {"Flat", "Conical"},
          record["tip"].value("kind", "flat") == "conical" ? 1 : 0, [this](int index) {
            recipe_command([&](PartDocument part) {
              return GuiHoleRecipeCommands::tip(std::move(part), edit.feature(), recipe_namespace,
                                                index == 1);
            });
          });
    } else if (key == "thread") {
      add("blcad.hole.internal_thread", "Internal Thread", {"None", "Enabled"},
          record.contains("thread") ? 1 : 0, [this](int index) {
            recipe_command([&](PartDocument part) {
              return GuiHoleRecipeCommands::internal_thread(std::move(part), edit.feature(),
                                                            recipe_namespace, index == 1);
            });
          });
    }
  }
  void build_support_selector(QFormLayout* form) {
    auto* input = combo(form->parentWidget());
    const bool thread = edit.kind() == Kind::Thread;
    input->setObjectName(thread ? "blcad.thread.support.choices" : "blcad.hole.support.choices");
    int selected = -1;
    each_support([&](const auto& support) {
      input->addItem(QString::fromStdString(support.label),
                     QString::fromStdString(support.semantic_id));
      if constexpr (std::is_same_v<std::decay_t<decltype(support)>, ThreadSupportChoice>) {
        if (support_chosen && thread_support_id == support.semantic_id)
          selected = input->count() - 1;
      } else if (support_chosen && support_override &&
                 plane_identity(*support_override) == plane_identity(support.reference))
        selected = input->count() - 1;
    });
    input->setCurrentIndex(selected);
    support_field = input;
    register_field(input, "/support");
    support_selector = new ShellPropertySelector(
        thread ? "blcad.thread.support" : "blcad.hole.support", "Support", *input, owner,
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
      support_chosen = choose_support(index);
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
      if (edit.kind() == Kind::Hole && key == "kind" &&
          (dimension_slot(prefix) || prefix == "/manufacturing" || prefix == "/extent" ||
           prefix == "/tip"))
        continue;
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
        if (edit.kind() == Kind::Hole)
          if (const auto slot = dimension_slot(path))
            build_dimension_source(nested, path, *slot);
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
      const bool fixed = (key == "kind" && !(edit.kind() == Kind::Hole && path == "/kind")) ||
                         key == "source_kind" || key == "result_mode" || key == "operation_mode" ||
                         key == "produced_body" || key == "result_body" || key == "mode" ||
                         key == "extent";
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
      if (edit.kind() == Kind::Hole && path == "/kind")
        choices = {{"plain", "Plain"}, {"clearance", "Clearance"}};
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
              edit.kind() == Kind::Thread ? "blcad.thread.target" : "blcad.hole.target",
              "Target Body", *input, owner,
              [this](bool picked) {
                picking = picked ? Pick::Body : Pick::None;
                pick_mode();
                if (!picked)
                  (void)preview();
              },
              parent);
          form->addRow(target_selector);
        } else
          form->addRow(edit.kind() == Kind::Hole && path == "/kind" ? QString("Hole Type")
                                                                    : label(key),
                       input);
        QObject::connect(input, &QComboBox::currentIndexChanged, &owner, [this, path, input] {
          value(path) = input->currentData().toString().toStdString();
          if (creating && path == "/target_body") {
            picking = Pick::None;
            support_chosen = false;
            support_override.reset();
            pending_support_plane.reset();
            thread_support_override.reset();
            thread_support_id.clear();
            const BodyId body(value(path).get<std::string>());
            auto catalog = edit.kind() == Kind::Thread ? collect_thread_supports(body)
                                                       : collect_supports(body);
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
    auto focus_id = std::exchange(pending_focus_id, QString{});
    if (focus_id.isEmpty())
      focus_id = retained_focus_id();
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
                              "diameter", "manufacturing", "extent", "tip", "thread"}) {
        build_hole_modes(form, key);
        if (record.contains(key))
          build_fields(form, Json{{key, record.at(key)}}, {});
      }
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
    if (!focus_id.isEmpty())
      QTimer::singleShot(0, &owner, [this, focus_id, draft = recipe_namespace] {
        if (!edit.active() || recipe_namespace != draft || !scroll->widget())
          return;
        if (auto* focused = scroll->widget()->findChild<QWidget*>(focus_id);
            focused && focused->isEnabled()) {
          scroll->ensureWidgetVisible(focused);
          focused->setFocus(Qt::OtherFocusReason);
        }
      });
  }
  GuiPartMutation input_updates(const PartDocument& decoded) const {
    std::vector<Parameter> changes, added;
    const auto used = bound_parameters(record);
    for (const auto& p : decoded.parameters())
      if (used.contains(p.id().value()) && !session.part_document()->find_parameter(p.id()))
        added.push_back(p);
    for (const auto& [id, scalar] : parameter_values) {
      (void)scalar;
      if (used.contains(id) && session.part_document()->find_parameter(ParameterId(id)))
        changes.push_back(*decoded.find_parameter(ParameterId(id)));
    }
    std::optional<ResultFaceWorkplane> plane;
    if (pending_support_plane && support_override)
      if (const auto* selected = std::get_if<DatumPlaneId>(&support_override->source());
          selected && *selected == pending_support_plane->id())
        plane = pending_support_plane;
    return [changes, added, plane](PartDocument& part) -> Result<std::size_t> {
      std::size_t plane_count = 0;
      if (plane && !part.find_result_face_workplane(plane->id())) {
        auto inserted = part.add_result_face_workplane(*plane);
        if (!inserted)
          return inserted;
        plane_count = inserted.value();
      }
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
      return Result<std::size_t>::success(changes.size() + added.size() + plane_count);
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
        return reject(error(edit.kind() == Kind::Thread
                                ? "Choose an exterior cylindrical or conical face."
                                : "Choose a support plane or planar face."));
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
                                input_updates(decoded.value()));
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
        : creating ? "Preview valid. Apply creates the feature and its required inputs."
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
    pending_focus_id.clear();
    supports.clear();
    support_override.reset();
    pending_support_plane.reset();
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
      "Create a semantic Hole with stable positions, seats and optional internal threads.");
  apply_gui_action_icon(*impl_->hole_action);
  auto* design = ribbon.add_group(model_tab, "Design");
  ribbon.add_button(design, impl_->hole_action);
  connect(impl_->hole_action, &QAction::triggered, this, [this] {
    auto result = begin_new_hole();
    if (!result)
      impl_->window.report_error(result.error());
  });
  impl_->thread_action = new QAction("Thread", this);
  impl_->thread_action->setObjectName("blcad.action.semantic_thread");
  impl_->thread_action->setToolTip("Create a Thread on an exterior cylindrical or conical face.");
  apply_gui_action_icon(*impl_->thread_action);
  ribbon.add_button(design, impl_->thread_action);
  connect(impl_->thread_action, &QAction::triggered, this, [this] {
    auto result = begin_new_thread();
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
  p.recipe_namespace =
      "hole.draft." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  p.pending_parameters.clear();
  p.supports.clear();
  p.thread_supports.clear();
  p.thread_support_override.reset();
  p.thread_support_id.clear();
  p.support_override.reset();
  p.pending_support_plane.reset();
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
  std::optional<ResultFaceWorkplane> pending_plane;
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
          pending_plane = choice.workplane;
          break;
        }
      if (support)
        break;
    }
    if (!support)
      return Result<std::size_t>::failure(
          error("Choose a current planar face or resolvable workplane."));
  } else {
    auto catalog = p.collect_supports(target);
    if (!catalog)
      return catalog;
  }
  if (p.supports.empty())
    return Result<std::size_t>::failure(error("No resolvable Hole support is available."));
  if (!support) {
    support = p.supports.front().reference;
    pending_plane = p.supports.front().workplane;
    for (const auto& choice : p.supports)
      if (const auto* face = std::get_if<SemanticFaceReference>(&choice.reference.source());
          face && face->face() == SemanticFace::Top) {
        support = choice.reference;
        pending_plane = choice.workplane;
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
  auto added = pending_plane ? seed.add_result_face_workplane(*pending_plane)
                             : Result<std::size_t>::success(0);
  if (added)
    added = seed.add_parameter(parameter.value());
  if (added)
    added = seed.add_hole_feature(definition.value());
  if (!added)
    return added;
  p.thread_supports.clear();
  p.thread_support_override.reset();
  p.thread_support_id.clear();
  p.support_override = support;
  p.pending_support_plane = pending_plane;
  return p.start_creation(seed, id, Kind::Hole,
                          "hole.draft." +
                              QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
}

Result<std::size_t> ShellFeatureEditTools::begin_new_thread() {
  auto& p = *impl_;
  if (!p.can_create())
    return Result<std::size_t>::failure(
        error("Thread creation requires a complete editable Part with a visible solid and no "
              "active command or workflow."));
  const auto& part = *p.session.part_document();
  std::optional<BodyId> requested_body;
  std::optional<std::string> requested_face;
  for (const auto& selected : p.session.selection().entries()) {
    if (selected.kind == GuiSelectionKind::Body) {
      bool found = false;
      for (const auto& body : part.bodies())
        if (selected.semantic_id == "body/" + body.id().value() && p.body_available(body)) {
          requested_body = body.id();
          found = true;
          break;
        }
      if (!found)
        return Result<std::size_t>::failure(error("Select a current visible solid target."));
    } else if (selected.kind == GuiSelectionKind::Face || selected.kind == GuiSelectionKind::Datum)
      requested_face = selected.semantic_id;
  }
  std::optional<BodyId> target;
  std::optional<Impl::ThreadSupportChoice> selected;
  for (const auto& body : part.bodies()) {
    if (!p.body_available(body) || (requested_body && *requested_body != body.id()))
      continue;
    auto catalog = p.collect_thread_supports(body.id());
    if (!catalog)
      return catalog;
    for (const auto& choice : p.thread_supports)
      if (!requested_face || choice.semantic_id == *requested_face) {
        target = body.id();
        selected = choice;
        break;
      }
    if (selected)
      break;
  }
  if (!selected)
    return Result<std::size_t>::failure(
        error("Choose a current exterior cylindrical or conical face on the target solid."));
  const auto tag = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  const FeatureId id("feature.thread." + tag);
  const std::string draft = "thread.draft." + tag;
  const auto& support = selected->geometry;
  auto seed = GuiThreadRecipeCommands::create_external(
      part, id, "Thread " + std::to_string(part.thread_features().size() + 1), *target,
      selected->reference, draft, support.radius_mm, support.axial_length_mm,
      support.taper_angle_deg);
  if (!seed)
    return Result<std::size_t>::failure(seed.error());
  p.supports.clear();
  p.support_override.reset();
  p.pending_support_plane.reset();
  p.thread_support_override = selected->reference;
  p.thread_support_id = selected->semantic_id;
  return p.start_creation(seed.value(), id, Kind::Thread, draft);
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
    for (int i = 0; i < p.support_field->count(); ++i)
      if (selected.semantic_id == p.support_field->itemData(i).toString().toStdString() &&
          (selected.kind == GuiSelectionKind::Face || selected.kind == GuiSelectionKind::Datum)) {
        const QSignalBlocker block(p.support_field);
        p.support_field->setCurrentIndex(i);
        p.support_chosen = p.choose_support(i);
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
      (create_another && (!p.creating || p.edit.kind() != Kind::Hole)))
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
                             p.input_updates(decoded.value()));
      },
      p.creating);
  if (committed) {
    const bool created_hole = p.creating && p.edit.kind() == Kind::Hole;
    const bool created_thread = p.creating && p.edit.kind() == Kind::Thread;
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
    else if (created_thread)
      p.window.remember_repeatable_command(*p.thread_action);
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
  if (p.thread_action)
    p.thread_action->setEnabled(p.can_create());
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
