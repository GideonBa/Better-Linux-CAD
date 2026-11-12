#include "blcad/gui/shell/shell_feature_edit_tools.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/finishing_selection.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include "blcad/geometry/thread_adapter.hpp"
#include "blcad/geometry/viewport_scene.hpp"
#include "blcad/geometry/workplane_resolver.hpp"
#include "blcad/gui/gui_hole_recipe.hpp"
#include "blcad/gui/gui_hole_thread_repeat.hpp"
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
std::string face_identity(const FaceReference& face) {
  return std::visit(
      [](const auto& source) -> std::string {
        using T = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<T, SemanticFaceReference>)
          return "face/" + std::to_string(source.source_feature().value().size()) + ":" +
                 source.source_feature().value() + "/" + std::string(to_string(source.face()));
        else if constexpr (std::is_same_v<T, SemanticCylindricalFaceReference>)
          return make_generated_topology_target_spelling(GeneratedTopologyReferenceIdentity{source})
              .value();
        else if constexpr (std::is_same_v<T, ImportedTopologyReference>)
          return make_imported_topology_target_spelling(source).value();
        else
          return source.identity();
      },
      face.source());
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
  bool support_editing{};
  std::optional<geometry::ShapeCache> feature_input_cache;
  std::optional<std::size_t> feature_input_prefix;
  enum class Pick { None, Body, Support, LimitFace, LimitBody };
  Pick picking{Pick::None};
  struct SupportChoice {
    std::string semantic_id, label;
    PlaneReference reference;
    geometry::GeometryShape shape;
    std::optional<ResultFaceWorkplane> workplane{};
    geometry::ViewportSceneKind kind{geometry::ViewportSceneKind::Face};
  };
  std::vector<SupportChoice> supports;
  struct ThreadSupportChoice {
    std::string semantic_id, label;
    FaceReference reference;
    geometry::GeometryShape shape;
    geometry::ThreadSupportInspection geometry;
    geometry::ViewportSceneKind kind{geometry::ViewportSceneKind::Face};
    ThreadDirection direction{ThreadDirection::AlongSupportAxis};
  };
  std::vector<ThreadSupportChoice> thread_supports;
  struct LimitFaceChoice {
    std::string semantic_id, label;
    FaceReference reference;
    geometry::GeometryShape shape;
    Json encoded;
    BodyId body;
  };
  std::vector<LimitFaceChoice> limit_faces;
  bool limit_chosen{true};
  std::optional<FaceReference> thread_support_override;
  std::string thread_support_id;
  std::optional<PlaneReference> support_override;
  std::optional<ResultFaceWorkplane> pending_support_plane;
  bool support_chosen{};
  std::optional<Error> support_error;
  std::vector<Parameter> pending_parameters;
  std::string recipe_namespace;
  QComboBox *target_field{}, *support_field{}, *limit_field{};
  ShellPropertySelector *target_selector{}, *support_selector{}, *limit_selector{};
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
  struct ThreadMappingDraft {
    std::optional<EngineeringResourceKey> table;
    std::string row;
    std::map<std::string, std::string> columns;
    bool taper{};
  };
  std::optional<ThreadMappingDraft> thread_mapping;
  bool thread_mapping_pending{};
  std::optional<Error> thread_mapping_error;

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
    return hole_thread() && (path == "/thread/profile/kind" || path.startsWith("/support/") ||
                             path.contains("/table/") || path.contains("/columns/") ||
                             path.endsWith("/row") || path.endsWith("/diameter_column"));
  }
  bool body_available(const Body& body) const {
    return body.kind() == BodyKind::Solid && body.visibility() == BodyVisibility::Visible &&
           session.presentation_visible("body/" + body.id().value()) &&
           session.part_shape_cache() && session.part_shape_cache()->find_body_shape(body.id());
  }
  bool input_selection() const noexcept {
    return creating || support_editing;
  }
  const geometry::ShapeCache* input_cache() const {
    return feature_input_cache ? &*feature_input_cache : session.part_shape_cache();
  }
  bool input_body_available(const Body& body) const {
    return body_available(body) && input_cache() && input_cache()->find_body_shape(body.id());
  }
  std::string body_selection_id(const BodyId& body) const {
    return (feature_input_cache ? "feature-input/body/" : "body/") + body.value();
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
    limit_chosen = true;
    support_editing = false;
    feature_input_cache.reset();
    feature_input_prefix.reset();
    thread_mapping.reset();
    thread_mapping_pending = false;
    thread_mapping_error.reset();
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
    another->show();
    guidance->setText(
        kind == Kind::Thread
            ? "Choose the target and exterior cylindrical or conical support. Edit the "
              "profile and interval, then Apply. Create Another retains the configuration in a "
              "fresh draft; choose another interval or target as needed. Modeled taper remains "
              "unsupported."
            : "Choose target and support, then edit local X/Y positions, recipe and dimensions. "
              "Apply creates one Hole; Create Another retains the recipe with fresh owned inputs. "
              "Choose the next positions before Apply.");
    build_form();
    pick_mode();
    dock->show();
    dock->raise();
    (void)preview();
    window.refresh();
    return Result<std::size_t>::success(1);
  }
  Result<std::size_t> start_repetition(FeatureId previous, Kind kind, std::string old_namespace,
                                       const std::vector<ParameterId>& owned) {
    const auto tag = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    const bool thread = kind == Kind::Thread;
    const FeatureId id((thread ? "feature.thread." : "feature.hole.") + tag);
    const std::string draft = (thread ? "thread.draft." : "hole.draft.") + tag;
    const auto* part = session.part_document();
    const auto count = thread ? part->thread_features().size() : part->hole_features().size();
    auto seed = GuiHoleThreadRepeatCommands::create(
        *part, previous, id, (thread ? "Thread " : "Hole ") + std::to_string(count + 1),
        std::move(old_namespace), draft, owned);
    if (!seed)
      return Result<std::size_t>::failure(seed.error());
    if (thread) {
      const auto& next = *seed.value().find_thread_feature(id);
      auto collected = collect_thread_supports(next.target_body());
      if (!collected)
        return collected;
      thread_support_override = next.support();
      thread_support_id.clear();
      for (const auto& choice : thread_supports)
        if (face_identity(choice.reference) == face_identity(next.support()))
          thread_support_id = choice.semantic_id;
      if (thread_support_id.empty()) {
        thread_support_id = "repeat/current-thread-support";
        thread_supports.push_back({thread_support_id, "Previous support", next.support(), {}, {}});
      }
    } else {
      const auto& next = *seed.value().find_hole_feature(id);
      auto collected = collect_supports(next.target_body());
      if (!collected)
        return collected;
      support_override = next.support();
      pending_support_plane.reset();
      if (std::none_of(supports.begin(), supports.end(), [&](const auto& choice) {
            return plane_identity(choice.reference) == plane_identity(next.support());
          }))
        supports.push_back({"repeat/current-hole-support", "Previous support", next.support(), {}});
    }
    return start_creation(seed.value(), id, kind, draft);
  }
  Result<std::size_t> collect_thread_supports(const BodyId& body) {
    thread_supports.clear();
    const auto* part = session.part_document();
    const auto* cache = input_cache();
    if (!part || !cache || !part->find_body(body) || !input_body_available(*part->find_body(body)))
      return Result<std::size_t>::failure(error("Choose a current visible solid target."));
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    auto faces = geometry::FinishingSelectionBuilder::build(
        *part, *cache, body, PartFeatureInputRole::ShellRemovalFace, context.value().get(),
        feature_input_prefix);
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
          context.value().get(), feature_input_prefix);
      auto direction = ThreadDirection::AlongSupportAxis;
      if (inspected && inspected.value().radius_mm <= 1e-7) {
        direction = ThreadDirection::OppositeSupportAxis;
        inspected = geometry::ThreadAdapter::inspect_support(*part, body, selected.value(), *cache,
                                                             direction, context.value().get(),
                                                             feature_input_prefix);
      }
      if (inspected && inspected.value().radius_mm > 1e-7)
        thread_supports.push_back({face.semantic_id, face.label, selected.value(), face.shape,
                                   inspected.value(), geometry::ViewportSceneKind::Face,
                                   direction});
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
  Result<Json> encode_thread_support(const FaceReference& support) const {
    auto canonical = deserialize_part_document_from_json(source.dump());
    if (!canonical)
      return Result<Json>::failure(canonical.error());
    const auto original = *canonical.value().find_thread_feature(edit.feature());
    auto rebound = ThreadFeature::create(original.id(), original.name(),
                                         BodyId(record.at("target_body").get<std::string>()),
                                         support, original.thread(), original.direction());
    if (!rebound)
      return Result<Json>::failure(rebound.error());
    auto updated = canonical.value().update_thread_feature(std::move(rebound.value()));
    if (!updated)
      return Result<Json>::failure(updated.error());
    auto encoded = serialize_part_document_to_json(canonical.value());
    if (!encoded)
      return Result<Json>::failure(encoded.error());
    return Result<Json>::success(
        Json::parse(encoded.value()).at(collection).at(record_index).at("support"));
  }
  bool choose_support(int index) {
    if (edit.kind() == Kind::Thread) {
      thread_support_override.reset();
      thread_support_id.clear();
      if (index < 0 || std::size_t(index) >= thread_supports.size())
        return false;
      auto encoded = encode_thread_support(thread_supports[index].reference);
      if (!encoded) {
        support_error = encoded.error();
        return false;
      }
      record["support"] = std::move(encoded.value());
      support_error.reset();
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
    const auto* cache = input_cache();
    if (!part || !cache || !part->find_body(body) || !input_body_available(*part->find_body(body)))
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
        *part, *cache, body, PartFeatureInputRole::ShellRemovalFace, context.value().get(),
        feature_input_prefix);
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
    if (feature_input_cache) {
      auto scene = geometry::ViewportSceneBuilder{context.value().get()}.build_part(
          *part, *cache, feature_input_prefix);
      if (!scene)
        return Result<std::size_t>::failure(scene.error());
      for (auto& choice : supports)
        if (choice.shape.empty()) {
          for (const auto& item : scene.value())
            if (item.kind == geometry::ViewportSceneKind::Datum &&
                item.semantic_id == choice.semantic_id) {
              choice.shape = item.shape;
              break;
            }
          choice.semantic_id = "feature-input/" + choice.semantic_id;
          choice.kind = geometry::ViewportSceneKind::Datum;
        }
    }
    return Result<std::size_t>::success(supports.size());
  }
  Result<std::size_t> prepare_support_editing() {
    const auto* part = session.part_document();
    if (!hole_thread() || session.history_prefix_count())
      return Result<std::size_t>::success(0);
    const BodyId body(record.at("target_body").get<std::string>());
    const auto* target = part->find_body(body);
    if (!target || !body_available(*target))
      return Result<std::size_t>::success(0);
    support_editing = true;
    auto history = part->executable_history();
    if (!history)
      return Result<std::size_t>::failure(history.error());
    const auto feature =
        std::find_if(history.value().begin(), history.value().end(),
                     [this](const auto& item) { return item.node_id == edit.feature().value(); });
    if (feature == history.value().end())
      return Result<std::size_t>::failure(error("Feature input history is unavailable."));
    feature_input_prefix = std::size_t(feature - history.value().begin());
    auto cache = geometry::ShapeCache::create(ShapeCacheId("feature.edit.inputs"));
    if (!cache)
      return Result<std::size_t>::failure(cache.error());
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    auto evaluated =
        geometry::GeometryRecomputeExecutor{context.value().get()}.execute_history_prefix(
            *part, *feature_input_prefix, cache.value());
    if (!evaluated)
      return Result<std::size_t>::failure(evaluated.error());
    feature_input_cache = std::move(cache.value());
    auto collected =
        edit.kind() == Kind::Thread ? collect_thread_supports(body) : collect_supports(body);
    support_error = collected ? std::nullopt : std::optional(collected.error());
    support_chosen = true;
    if (edit.kind() == Kind::Thread) {
      const auto& current = part->find_thread_feature(edit.feature())->support();
      thread_support_override = current;
      for (const auto& choice : thread_supports)
        if (face_identity(choice.reference) == face_identity(current)) {
          thread_support_id = choice.semantic_id;
          return Result<std::size_t>::success(1);
        }
      auto inspected = geometry::ThreadAdapter::inspect_support(
          *part, body, current, *feature_input_cache, ThreadDirection::AlongSupportAxis,
          context.value().get(), feature_input_prefix);
      thread_support_id = "feature-input/current-thread-support";
      thread_supports.push_back(
          {thread_support_id,
           "Current support",
           current,
           {},
           inspected ? inspected.value() : geometry::ThreadSupportInspection{}});
    } else {
      const auto& current = part->find_hole_feature(edit.feature())->support();
      support_override = current;
      if (std::none_of(supports.begin(), supports.end(), [&](const auto& choice) {
            return plane_identity(choice.reference) == plane_identity(current);
          }))
        supports.push_back({"feature-input/current-hole-support", "Current support", current, {}});
    }
    return Result<std::size_t>::success(1);
  }
  Result<std::size_t> collect_limit_faces() {
    limit_faces.clear();
    const auto* part = session.part_document();
    const auto* cache = input_cache();
    auto canonical = deserialize_part_document_from_json(source.dump());
    if (!part || !cache || !canonical)
      return Result<std::size_t>::failure(error("Hole limit inputs are unavailable."));
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    for (const auto& body : part->bodies()) {
      if (!input_body_available(body))
        continue;
      auto faces = geometry::FinishingSelectionBuilder::build(
          *part, *cache, body.id(), PartFeatureInputRole::ShellRemovalFace, context.value().get(),
          feature_input_prefix);
      if (!faces)
        return Result<std::size_t>::failure(faces.error());
      for (const auto& face : faces.value()) {
        const auto* reference = std::get_if<FaceReference>(&face.reference);
        if (!reference)
          continue;
        auto selected = std::visit(
            [&](const auto& ref) -> Result<FaceReference> {
              using T = std::decay_t<decltype(ref)>;
              constexpr auto role = PartFeatureInputRole::HoleLimitFace;
              if constexpr (std::is_same_v<T, SemanticFaceReference>)
                return FaceReference::create_planar(role, ref);
              else if constexpr (std::is_same_v<T, ImportedTopologyReference>) {
                auto plane = ImportedTopologyReference::create(ref.catalog(), ref.entry(),
                                                               ImportedTopologyCapability::Plane);
                if (!plane)
                  return Result<FaceReference>::failure(plane.error());
                auto support = PlaneReference::create_imported(
                    PartFeatureInputRole::HoleSupportPlane, plane.value());
                if (!support)
                  return Result<FaceReference>::failure(support.error());
                auto valid = validate_part_feature_input_reference(*part, support.value());
                if (!valid)
                  return Result<FaceReference>::failure(valid.error());
                return FaceReference::create_imported(role, ref);
              } else if constexpr (std::is_same_v<T, ResultFaceReference>) {
                auto plane = ResultFaceWorkplane::create(DatumPlaneId("plane.hole.limit.inspect"),
                                                         "Limit inspection", ref);
                if (!plane)
                  return Result<FaceReference>::failure(plane.error());
                auto resolved = geometry::ResultFaceSupportGeometry::resolve(
                    *part, plane.value(), *cache, context.value().get());
                if (!resolved)
                  return Result<FaceReference>::failure(resolved.error());
                return FaceReference::create_result(role, ref);
              } else
                return Result<FaceReference>::failure(error("Choose a planar Hole limit face."));
            },
            reference->source());
        if (!selected)
          continue;
        // Keep serialization in Core. This disposable transition supplies only
        // the encoded typed face; it does not replace current numeric edits.
        auto candidate =
            GuiHoleRecipeCommands::extent(canonical.value(), edit.feature(), recipe_namespace,
                                          GuiHoleExtentChoice::ToFace, selected.value());
        if (!candidate)
          continue;
        auto encoded = serialize_part_document_to_json(candidate.value());
        if (!encoded)
          return Result<std::size_t>::failure(encoded.error());
        limit_faces.push_back(
            {face.semantic_id, body.name() + " / " + face.label, selected.value(), face.shape,
             Json::parse(encoded.value()).at(collection).at(record_index).at("extent").at("face"),
             body.id()});
      }
    }
    if (context.value()) {
      auto fresh = context.value()->validate_sources();
      if (!fresh)
        return fresh;
    }
    return Result<std::size_t>::success(limit_faces.size());
  }
  Result<std::size_t> publish_input_scene() {
    if (!feature_input_cache)
      return Result<std::size_t>::success(0);
    auto context = make_step_import_context(session);
    if (!context)
      return Result<std::size_t>::failure(context.error());
    const geometry::ViewportSceneBuilder builder{context.value().get()};
    auto scoped =
        builder.build_part(*session.part_document(), *feature_input_cache, feature_input_prefix);
    auto original = builder.build_part(*session.part_document(), *session.part_shape_cache());
    if (!scoped || !original)
      return Result<std::size_t>::failure(scoped ? original.error() : scoped.error());
    std::vector<geometry::GeometryShape> shapes;
    std::vector<std::string> replaced;
    for (const auto& item : scoped.value())
      if (body_item(item) && session.presentation_visible(item.semantic_id))
        shapes.push_back(item.shape);
    for (const auto& item : original.value())
      if (body_item(item) || item.kind == geometry::ViewportSceneKind::Datum)
        replaced.push_back(item.semantic_id);
    return viewport.set_feature_previews(std::move(shapes), std::move(replaced));
  }
  void pick_mode() {
    if (!input_selection() || !edit.active())
      return;
    if (!support_chosen) {
      valid = false;
      viewport.clear_feature_preview();
    }
    viewport.clear_preview_selection_targets();
    std::vector<std::string> scope;
    std::vector<geometry::ViewportSceneItem> targets;
    if (picking != Pick::None && feature_input_cache) {
      auto shown = publish_input_scene();
      if (!shown) {
        (void)reject(shown.error());
        return;
      }
    }
    const bool body_pick = picking == Pick::Body || picking == Pick::LimitBody;
    if ((picking == Pick::LimitFace || picking == Pick::LimitBody) && !feature_input_cache)
      viewport.clear_feature_preview();
    if (body_pick) {
      if (!feature_input_cache)
        viewport.clear_feature_preview();
      for (const auto& body : session.part_document()->bodies())
        if (input_body_available(body) &&
            (picking != Pick::LimitBody ||
             body.id().value() != record.at("target_body").get<std::string>())) {
          const auto id = body_selection_id(body.id());
          scope.push_back(id);
          if (feature_input_cache)
            targets.push_back({geometry::ViewportSceneKind::SolidBody, id,
                               *feature_input_cache->find_body_shape(body.id())});
        }
    } else if (picking == Pick::Support) {
      each_support([&](const auto& choice) {
        scope.push_back(choice.semantic_id);
        if (!choice.shape.empty())
          targets.push_back({choice.kind, choice.semantic_id, choice.shape});
      });
    } else if (picking == Pick::LimitFace) {
      for (const auto& face : limit_faces) {
        scope.push_back(face.semantic_id);
        targets.push_back({geometry::ViewportSceneKind::Face, face.semantic_id, face.shape});
      }
    }
    if (!targets.empty()) {
      auto result = viewport.set_preview_selection_targets(std::move(targets));
      if (!result)
        window.report_error(result.error());
    }
    viewport.set_selection_semantic_scope(std::move(scope));
    viewport.set_selection_filter_mask(
        body_pick                  ? selection_kind_bit(GuiSelectionKind::Body)
        : picking == Pick::Support ? selection_kind_bit(GuiSelectionKind::Face) |
                                         selection_kind_bit(GuiSelectionKind::Datum)
        : picking == Pick::LimitFace ? selection_kind_bit(GuiSelectionKind::Face)
                                     : 0);
    if (target_selector)
      target_selector->set_active(picking == Pick::Body);
    if (support_selector)
      support_selector->set_active(picking == Pick::Support);
    if (limit_selector)
      limit_selector->set_active(picking == Pick::LimitFace || picking == Pick::LimitBody);
    apply->setEnabled(valid && support_chosen && picking == Pick::None);
    another->setEnabled(creating && valid && support_chosen && picking == Pick::None);
  }
  template <class Record> Result<Record> selected_record(Record record) const {
    if constexpr (std::is_same_v<Record, HoleFeature>) {
      if (input_selection() && support_override)
        return HoleFeature::create(record.id(), record.name(), record.kind(), record.target_body(),
                                   *support_override, record.placements(), record.diameter(),
                                   record.extent(), record.direction(), record.tip(),
                                   record.manufacturing(), record.thread());
    } else if constexpr (std::is_same_v<Record, ThreadFeature>) {
      if (input_selection() && thread_support_override)
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
    spin->setDecimals(unit == QuantityKind::Count ? 0 : path.startsWith("/thread/") ? 9 : 6);
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
      if (input_selection() && key == "target_body") {
        for (const auto& body : part.bodies())
          if (input_body_available(body))
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
    binding->setProperty("blcad.restoreFocusAfterRebuild", path.startsWith("/thread/"));
    for (const auto& p : session.part_document()->parameters())
      if (p.type() == selected.type())
        add_choice(binding, p.id().value(), p.name());
    for (const auto& p : pending_parameters)
      if (p.type() == selected.type() && !session.part_document()->find_parameter(p.id()))
        add_choice(binding, p.id().value(), p.name());
    binding->setCurrentIndex(binding->findData(QString::fromStdString(selected.id().value())));
    layout->addWidget(binding);
    auto* scalar = number(row, path, parameter_value(selected), selected.value().kind());
    scalar->setProperty("blcad.restoreFocusAfterRebuild", path.startsWith("/thread/"));
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
          if (thread_mapping_pending && thread_mapping_error)
            materialize_thread_mapping();
          else
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
                       if (thread_mapping_pending && thread_mapping_error)
                         materialize_thread_mapping();
                       else
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
  Result<PartDocument> recipe_document(bool parameter_edits = true,
                                       bool canonical_thread_profile = false) const {
    auto part = decode(parameter_edits, canonical_thread_profile);
    if (!part)
      return part;
    if (edit.kind() == Kind::Thread) {
      auto thread = selected_record(*part.value().find_thread_feature(edit.feature()));
      if (!thread)
        return Result<PartDocument>::failure(thread.error());
      auto updated = part.value().update_thread_feature(std::move(thread.value()));
      if (!updated)
        return Result<PartDocument>::failure(updated.error());
      return part;
    }
    if (edit.kind() != Kind::Hole)
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
  template <class Command>
  bool thread_command(Command command, std::optional<Error>* failure = nullptr,
                      bool replace_profile = false) {
    auto part = recipe_document(false, replace_profile);
    if (!part) {
      if (failure)
        *failure = part.error();
      (void)reject(part.error());
      return false;
    }
    auto result = command(std::move(part.value()));
    const bool accepted = bool(result);
    if (!result && failure)
      *failure = result.error();
    adopt_recipe(std::move(result));
    return accepted;
  }
  std::string fresh_thread_namespace() const {
    return "thread.draft." + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  }
  void materialize_thread_mapping() {
    thread_mapping_pending = true;
    thread_mapping_error.reset();
    if (!thread_mapping || !thread_mapping->table || thread_mapping->row.empty()) {
      deferred_rebuild();
      return;
    }
    auto& draft = *thread_mapping;
    for (const auto* field : {"system", "designation", "tolerance", "major_diameter",
                              "minor_diameter", "pitch", "included_angle"})
      if (draft.columns[field].empty()) {
        deferred_rebuild();
        return;
      }
    if (draft.taper && draft.columns["taper_angle"].empty()) {
      deferred_rebuild();
      return;
    }
    ThreadStandardColumns columns{draft.columns["system"],
                                  draft.columns["designation"],
                                  draft.columns["tolerance"],
                                  draft.columns["major_diameter"],
                                  draft.columns["minor_diameter"],
                                  draft.columns["pitch"],
                                  draft.columns["included_angle"],
                                  draft.taper ? std::optional(draft.columns["taper_angle"])
                                              : std::nullopt};
    const ThreadStandardProfile profile{*draft.table, draft.row, std::move(columns)};
    if (thread_command(
            [&](PartDocument part) {
              return GuiThreadRecipeCommands::profile(std::move(part), edit.feature(), profile,
                                                      parameter_values);
            },
            &thread_mapping_error, true))
      thread_mapping_pending = false;
  }
  void build_thread_mapping(QFormLayout* form) {
    auto& draft = *thread_mapping;
    const auto& resources = session.part_document()->engineering_resources();
    const auto add = [&](const char* field, const QString& title) {
      auto* input = combo(form->parentWidget());
      input->setObjectName("blcad.thread.standard." + QString::fromLatin1(field));
      input->setProperty("blcad.restoreFocusAfterRebuild", true);
      form->addRow(title, input);
      return input;
    };
    auto* table = add("table", "Table / Revision");
    table->addItem("Choose a pinned revision", QStringList{});
    std::vector<EngineeringResourceKey> keys;
    for (const auto& entry : resources.entries())
      if (const auto* resource = std::get_if<EngineeringStandardTable>(&entry)) {
        keys.push_back(resource->key());
        table->addItem(QString::fromStdString(resource->name() + " / " +
                                              resource->key().id().value() + " / " +
                                              resource->key().revision()),
                       QStringList{QString::fromStdString(resource->key().id().value()),
                                   QString::fromStdString(resource->key().revision())});
        table->setItemData(table->count() - 1,
                           QString::fromStdString(resource->key().id().value() + " / " +
                                                  resource->key().revision() + "\n" +
                                                  resource->provenance().source()),
                           Qt::ToolTipRole);
        if (draft.table && *draft.table == resource->key())
          table->setCurrentIndex(table->count() - 1);
      }
    if (table->currentIndex() > 0)
      table->setToolTip(table->currentData(Qt::ToolTipRole).toString());
    QObject::connect(table, &QComboBox::currentIndexChanged, &owner, [this, keys](int index) {
      thread_mapping = ThreadMappingDraft{};
      if (index > 0)
        thread_mapping->table = keys.at(std::size_t(index - 1));
      thread_mapping_pending = true;
      thread_mapping_error.reset();
      deferred_rebuild();
    });
    const auto* selected = draft.table ? resources.find_table(*draft.table) : nullptr;
    auto* row = add("row", "Row");
    row->addItem("Choose a row", "");
    if (selected)
      for (const auto& choice : selected->rows())
        row->addItem(QString::fromStdString(choice.name + " / " + choice.key),
                     QString::fromStdString(choice.key));
    row->setCurrentIndex(std::max(0, row->findData(QString::fromStdString(draft.row))));
    QObject::connect(row, &QComboBox::currentIndexChanged, &owner, [this, row] {
      thread_mapping->row = row->currentData().toString().toStdString();
      thread_mapping->columns.clear();
      thread_mapping_pending = true;
      thread_mapping_error.reset();
      deferred_rebuild();
    });
    const auto* selected_row = selected ? selected->find_row(draft.row) : nullptr;
    auto* tapered = new QCheckBox("Map a taper Angle column", form->parentWidget());
    tapered->setObjectName("blcad.thread.standard.taper.enabled");
    tapered->setProperty("blcad.restoreFocusAfterRebuild", true);
    tapered->setChecked(draft.taper);
    form->addRow(tapered);
    QObject::connect(tapered, &QCheckBox::toggled, &owner, [this](bool enabled) {
      thread_mapping->taper = enabled;
      materialize_thread_mapping();
    });
    for (const auto* field : {"system", "designation", "tolerance", "major_diameter",
                              "minor_diameter", "pitch", "included_angle", "taper_angle"}) {
      const std::string name(field);
      if (name == "taper_angle" && !draft.taper)
        continue;
      const bool text = name == "system" || name == "designation" || name == "tolerance";
      const bool angle = name == "included_angle" || name == "taper_angle";
      auto* column = add(field, label(name));
      column->addItem("Choose a typed column", "");
      if (selected_row)
        for (const auto& cell : selected_row->cells) {
          QString value;
          if (text) {
            const auto* string = std::get_if<std::string>(&cell.value);
            if (!string || QString::fromStdString(*string).trimmed().isEmpty())
              continue;
            value = QString::fromStdString(*string);
          } else {
            const auto* quantity = std::get_if<Quantity>(&cell.value);
            if (!quantity ||
                quantity->kind() != (angle ? QuantityKind::AngleDeg : QuantityKind::LengthMm))
              continue;
            value = QString::number(angle ? quantity->degrees() : quantity->millimeters()) +
                    (angle ? " deg" : " mm");
          }
          column->addItem(QString::fromStdString(cell.column) + " / " + value,
                          QString::fromStdString(cell.column));
          column->setItemData(column->count() - 1, QString::fromStdString(cell.provenance.source()),
                              Qt::ToolTipRole);
        }
      column->setCurrentIndex(
          std::max(0, column->findData(QString::fromStdString(draft.columns[name]))));
      if (column->currentIndex() > 0)
        column->setToolTip(column->currentData(Qt::ToolTipRole).toString());
      QObject::connect(column, &QComboBox::currentIndexChanged, &owner, [this, name, column] {
        thread_mapping->columns[name] = column->currentData().toString().toStdString();
        materialize_thread_mapping();
      });
    }
  }
  void build_thread_controls(QFormLayout* form) {
    auto part = deserialize_part_document_from_json(source.dump());
    if (!part)
      return;
    const ThreadDefinition* definition = nullptr;
    if (edit.kind() == Kind::Thread)
      definition = &part.value().find_thread_feature(edit.feature())->thread();
    else if (const auto* hole = part.value().find_hole_feature(edit.feature());
             hole && hole->thread())
      definition = &*hole->thread();
    if (!definition)
      return;
    const auto* standard = std::get_if<ThreadStandardProfile>(&definition->profile());
    if (standard && !thread_mapping) {
      thread_mapping = ThreadMappingDraft{standard->table,
                                          standard->row,
                                          {{"system", standard->columns.system},
                                           {"designation", standard->columns.designation},
                                           {"tolerance", standard->columns.tolerance},
                                           {"major_diameter", standard->columns.major_diameter},
                                           {"minor_diameter", standard->columns.minor_diameter},
                                           {"pitch", standard->columns.pitch},
                                           {"included_angle", standard->columns.included_angle}},
                                          bool(standard->columns.taper_angle)};
      if (standard->columns.taper_angle)
        thread_mapping->columns["taper_angle"] = *standard->columns.taper_angle;
    }
    auto* source = combo(form->parentWidget());
    source->setObjectName("blcad.thread.source");
    source->setProperty("blcad.restoreFocusAfterRebuild", true);
    source->addItem("Custom parameters", "custom");
    source->addItem("Pinned standard profile", "standard");
    source->setCurrentIndex(thread_mapping ? 1 : 0);
    form->addRow("Thread Profile Source", source);
    QObject::connect(source, &QComboBox::currentIndexChanged, &owner,
                     [this, was_standard = bool(standard)](int index) {
                       if (index == 1) {
                         thread_mapping = ThreadMappingDraft{};
                         thread_mapping_pending = true;
                         thread_mapping_error.reset();
                         deferred_rebuild();
                       } else if (!was_standard) {
                         thread_mapping.reset();
                         thread_mapping_pending = false;
                         thread_mapping_error.reset();
                         deferred_rebuild();
                       } else if (thread_command(
                                      [&](PartDocument part) {
                                        return GuiThreadRecipeCommands::custom_profile(
                                            std::move(part), edit.feature(),
                                            fresh_thread_namespace(), parameter_values);
                                      },
                                      nullptr, true)) {
                         thread_mapping.reset();
                         thread_mapping_pending = false;
                         thread_mapping_error.reset();
                       }
                     });
    if (thread_mapping)
      build_thread_mapping(form);
    const auto optional = [&](const char* name, const QString& title, bool checked, auto changed) {
      auto* input = new QCheckBox(title, form->parentWidget());
      input->setObjectName(QString::fromLatin1(name));
      input->setProperty("blcad.restoreFocusAfterRebuild", true);
      input->setChecked(checked);
      form->addRow(input);
      QObject::connect(input, &QCheckBox::toggled, &owner, changed);
    };
    optional("blcad.thread.start.enabled", "Start offset (omitted = 0 mm)",
             bool(definition->start()), [this](bool enabled) {
               thread_command([&](PartDocument part) {
                 return GuiThreadRecipeCommands::start_offset(std::move(part), edit.feature(),
                                                              fresh_thread_namespace(), enabled,
                                                              parameter_values);
               });
             });
    if (!thread_mapping) {
      const auto& profile = std::get<ThreadParameterProfile>(definition->profile());
      optional("blcad.thread.taper.enabled", "Custom taper angle", bool(profile.taper_angle),
               [this](bool enabled) {
                 thread_command([&](PartDocument part) {
                   return GuiThreadRecipeCommands::taper(std::move(part), edit.feature(),
                                                         fresh_thread_namespace(), enabled,
                                                         parameter_values);
                 });
               });
    }
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
                auto collected = collect_limit_faces();
                if (!collected)
                  return Result<PartDocument>::failure(collected.error());
                const LimitFaceChoice* preferred = nullptr;
                for (const auto& target : limit_faces) {
                  if (!preferred)
                    preferred = &target;
                  if (const auto* native =
                          std::get_if<SemanticFaceReference>(&target.reference.source());
                      native && native->face() == SemanticFace::Bottom &&
                      target.body == hole->target_body()) {
                    preferred = &target;
                    break;
                  }
                }
                if (preferred)
                  face = preferred->reference;
              }
              if (index == int(GuiHoleExtentChoice::UpToBody))
                for (const auto& candidate : part.bodies())
                  if (candidate.id() != hole->target_body() && input_body_available(candidate)) {
                    body = candidate.id();
                    break;
                  }
              auto result =
                  GuiHoleRecipeCommands::extent(std::move(part), edit.feature(), recipe_namespace,
                                                GuiHoleExtentChoice(index), face, body);
              if (result)
                limit_chosen = true;
              return result;
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
            thread_mapping.reset();
            thread_mapping_pending = false;
            thread_mapping_error.reset();
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
  void build_limit_selector(QFormLayout* form, bool face) {
    auto* input = combo(form->parentWidget());
    input->setObjectName(face ? "blcad.hole.limit_face.choices" : "blcad.hole.limit_body.choices");
    int selected = -1;
    if (face) {
      auto collected = collect_limit_faces();
      if (!collected)
        status->setText(QString::fromStdString(collected.error().message()));
      for (const auto& choice : limit_faces) {
        add_choice(input, choice.semantic_id, choice.label);
        if (limit_chosen && choice.encoded == record.at("extent").at("face"))
          selected = input->count() - 1;
      }
    } else
      for (const auto& body : session.part_document()->bodies())
        if (input_body_available(body) &&
            body.id().value() != record.at("target_body").get<std::string>()) {
          add_choice(input, body.id().value(), body.name());
          if (limit_chosen &&
              body.id().value() == record.at("extent").at("body").get<std::string>())
            selected = input->count() - 1;
        }
    input->setCurrentIndex(selected);
    limit_chosen = selected >= 0;
    limit_field = input;
    register_field(input, face ? "/extent/face" : "/extent/body");
    limit_selector = new ShellPropertySelector(
        face ? "blcad.hole.limit_face" : "blcad.hole.limit_body",
        face ? "Limit Face" : "Limit Body", *input, owner,
        [this, face](bool picked) {
          picking = picked ? (face ? Pick::LimitFace : Pick::LimitBody) : Pick::None;
          pick_mode();
          if (!picked)
            (void)preview();
        },
        form->parentWidget());
    form->addRow(limit_selector);
    QObject::connect(input, &QComboBox::currentIndexChanged, &owner,
                     [this, face, input](int index) {
                       if (building)
                         return;
                       picking = Pick::None;
                       limit_chosen = index >= 0;
                       if (limit_chosen) {
                         if (face)
                           record["extent"]["face"] = limit_faces.at(std::size_t(index)).encoded;
                         else
                           record["extent"]["body"] = input->currentData().toString().toStdString();
                       }
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
      if (input_selection() && prefix.isEmpty() && key == "support") {
        build_support_selector(form);
        continue;
      }
      if (input_selection() && edit.kind() == Kind::Hole && prefix == "/extent" &&
          (key == "face" || key == "body")) {
        build_limit_selector(form, key == "face");
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
        if (input_selection() && path == "/target_body") {
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
          if (input_selection() && path == "/target_body") {
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
    target_field = support_field = limit_field = nullptr;
    target_selector = support_selector = limit_selector = nullptr;
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
        if (std::string_view(key) == "thread" && record.contains(key))
          build_thread_controls(form);
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
    pick_mode();
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
  Result<PartDocument> decode(bool parameter_edits = true,
                              bool canonical_thread_profile = false) const {
    Json root = source;
    root.at(collection).at(record_index) = record;
    if (canonical_thread_profile && record.contains("thread"))
      root.at(collection).at(record_index).at("thread").at("profile") =
          source.at(collection).at(record_index).at("thread").at("profile");
    const auto used = bound_parameters(record);
    for (auto& p : root.at("parameters")) {
      const auto id = p.at("id").get<std::string>();
      if (auto found = parameter_values.find(id);
          parameter_edits && found != parameter_values.end() && used.contains(id))
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
    if (input_selection() && picking != Pick::None) {
      apply->setEnabled(false);
      another->setEnabled(false);
      return Result<std::size_t>::success(0);
    }
    if (thread_mapping_pending)
      return reject(thread_mapping_error.value_or(
          error("Complete the pinned Thread revision, row and typed column mappings.")));
    if (input_selection()) {
      const auto* body =
          session.part_document()->find_body(BodyId(record.at("target_body").get<std::string>()));
      if (!body || !input_body_available(*body))
        return reject(error("Choose a current visible solid target."));
      if (support_error)
        return reject(*support_error);
      if (!support_chosen)
        return reject(error(edit.kind() == Kind::Thread
                                ? "Choose an exterior cylindrical or conical face."
                                : "Choose a support plane or planar face."));
      if (edit.kind() == Kind::Hole &&
          (record.at("extent").at("kind") == "to_face" ||
           record.at("extent").at("kind") == "up_to_body") &&
          !limit_chosen)
        return reject(error("Choose a current planar limit face or a distinct solid limit Body."));
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
    thread_mapping.reset();
    thread_mapping_pending = false;
    thread_mapping_error.reset();
    const bool restore_creation = input_selection() && !applied && session.part_document() &&
                                  session.model_revision() == entry_revision;
    const auto camera = entry_camera;
    const auto selection = entry_selection;
    const auto viewed = entry_viewport_selection;
    const bool was_creating = input_selection();
    creating = false;
    support_editing = false;
    feature_input_cache.reset();
    feature_input_prefix.reset();
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
    limit_faces.clear();
    limit_chosen = true;
    thread_supports.clear();
    thread_support_override.reset();
    thread_support_id.clear();
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
  p.limit_chosen = true;
  p.support_editing = false;
  p.feature_input_cache.reset();
  p.feature_input_prefix.reset();
  p.thread_mapping.reset();
  p.thread_mapping_pending = false;
  p.thread_mapping_error.reset();
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
  if (p.hole_thread()) {
    p.entry_revision = p.session.model_revision();
    p.entry_camera = p.viewport.camera_bookmark();
    p.entry_selection = p.session.selection();
    p.entry_viewport_selection = p.viewport.selected_semantic();
  }
  p.viewport.set_selection_filter_mask(0);
  auto scoped = p.prepare_support_editing();
  if (!scoped) {
    const auto failure = scoped.error();
    p.finish(false);
    return Result<std::size_t>::failure(failure);
  }
  p.parameter_values.clear();
  p.array_templates.clear();
  p.dock->setWindowTitle(family(p.edit.kind()) + " Edit");
  p.dynamic->setVisible(!p.hole_thread());
  p.dynamic_label->setVisible(!p.hole_thread());
  p.guidance->setText(
      p.support_editing
          ? "Picks use the model before this feature. Apply validates the complete downstream "
            "model. "
            "Shared parameters affect every use; formula values require an explicit rebind."
      : p.hole_thread()
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
  if (!preview && !p.support_editing) {
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
  const auto diameter_value = 6.0;
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
      support.taper_angle_deg, selected->direction);
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
  if (!p.input_selection() || !active() || p.picking == Impl::Pick::None)
    return false;
  const auto selected = incoming;
  if ((p.picking == Impl::Pick::LimitFace || p.picking == Impl::Pick::LimitBody) && p.limit_field) {
    const bool face = p.picking == Impl::Pick::LimitFace;
    for (int i = 0; i < p.limit_field->count(); ++i) {
      const auto value = p.limit_field->itemData(i).toString().toStdString();
      const auto id = face ? value : p.body_selection_id(BodyId(value));
      if (selected.semantic_id == id &&
          selected.kind == (face ? GuiSelectionKind::Face : GuiSelectionKind::Body)) {
        p.limit_field->setCurrentIndex(i);
        p.limit_chosen = true;
        p.picking = Impl::Pick::None;
        p.pick_mode();
        (void)p.preview();
        return true;
      }
    }
  }
  if (p.picking == Impl::Pick::Body && selected.kind == GuiSelectionKind::Body && p.target_field) {
    for (int i = 0; i < p.target_field->count(); ++i)
      if (selected.semantic_id ==
          p.body_selection_id(BodyId(p.target_field->itemData(i).toString().toStdString()))) {
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
      (create_another && (!p.creating || !p.hole_thread())))
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
    const auto previous = p.edit.feature();
    const auto kind = p.edit.kind();
    const auto old_namespace = p.recipe_namespace;
    std::vector<ParameterId> owned;
    if (create_another)
      for (const auto& parameter : p.pending_parameters)
        if (p.session.part_document()->find_parameter(parameter.id()))
          owned.push_back(parameter.id());
    p.finish(true);
    if (created_hole)
      p.window.remember_repeatable_command(*p.hole_action);
    else if (created_thread)
      p.window.remember_repeatable_command(*p.thread_action);
    if (create_another) {
      auto next = p.start_repetition(previous, kind, old_namespace, owned);
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
