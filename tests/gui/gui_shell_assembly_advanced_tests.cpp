#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/gui/shell/shell_assembly_tools.hpp"
#include "blcad/gui/shell/shell_window.hpp"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
using namespace blcad;
using namespace blcad::gui;
namespace {
using C = ShellAssemblyTools::Command;
template <typename T> auto checked(T result) {
  INFO((result.has_error() ? result.error().message() : "ok"));
  REQUIRE(result);
  return result.value();
}
std::string snapshot(ShellWindow& window) {
  return checked(serialize_project_to_json(*window.session().project()));
}
template <typename T> T& widget(ShellWindow& window, const char* name) {
  auto* value = window.findChild<T*>(QStringLiteral("blcad.assembly.") + name);
  REQUIRE(value);
  return *value;
}
void fixture(ShellWindow& window, RigidTransform moving = {}) {
  GuiDocumentSession source;
  REQUIRE(source.open_file(std::string(BLCAD_SOURCE_DIR) +
                           "/examples/revolute_joint.blcad.project.json"));
  const auto part = source.project()->part_documents().front();
  REQUIRE(window.new_project("Hierarchy controls"));
  REQUIRE(window.session().commit_project_transaction("Fixture", [part, moving](Project& project) {
    REQUIRE(project.add_part_document(part));
    REQUIRE(project.assembly().add_member_part(part.id()));
    REQUIRE(project.assembly().add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId("fixed"), "Fixed", part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Grounded))));
    auto child = checked(AssemblyDocument::create(DocumentId("child"), "Repeated child"));
    REQUIRE(child.add_member_part(part.id()));
    REQUIRE(child.add_component_instance(checked(ComponentInstance::create(
        ComponentInstanceId("moving"), "Moving", part.id(), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Free, moving))));
    REQUIRE(project.add_child_assembly_document(child));
    for (const auto& [name, transform] : std::vector<std::pair<const char*, RigidTransform>>{
             {"left", {}}, {"right", {{200, 0, 0}, {}}}})
      REQUIRE(project.assembly().add_subassembly_instance(checked(SubassemblyInstance::create(
          SubassemblyInstanceId(name), name, child.id(), ComponentVisibility::Visible,
          ComponentSuppressionState::Active, transform))));
    return Result<std::size_t>::success(1);
  }));
  window.resize(1280, 800);
  window.show();
  window.refresh();
  qApp->processEvents();
}
void reference(ShellAssemblyTools& tools, QComboBox& input, const char* path,
               const char* semantic) {
  const auto found = std::find_if(
      tools.hierarchy_targets().begin(), tools.hierarchy_targets().end(),
      [path, semantic](const auto& target) {
        return target.endpoint.occurrence_path() ==
                   (std::string(path).empty()
                        ? std::vector<SubassemblyInstanceId>{}
                        : std::vector<SubassemblyInstanceId>{SubassemblyInstanceId(path)}) &&
               target.endpoint.semantic_reference() == semantic;
      });
  REQUIRE(found != tools.hierarchy_targets().end());
  const int index = input.findData(static_cast<int>(found - tools.hierarchy_targets().begin()));
  REQUIRE(index > 0);
  input.setCurrentIndex(index);
}
void select(QComboBox& input, const QString& data) {
  const int index = input.findData(data);
  REQUIRE(index >= 0);
  input.setCurrentIndex(index);
}
void history(ShellWindow& window, const std::string& before) {
  const auto after = snapshot(window);
  REQUIRE(window.undo());
  CHECK(snapshot(window) == before);
  REQUIRE(window.redo());
  CHECK(snapshot(window) == after);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("hierarchy.blcad.project.json");
  REQUIRE(window.save_to(path));
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(checked(serialize_project_to_json(*reopened.project())) == after);
}
void joint(ShellWindow& window, AssemblyJointType type = AssemblyJointType::Revolute) {
  auto& tools = window.assembly_tools();
  REQUIRE(tools.begin(C::CrossJoint));
  tools.joint_type_editor().setCurrentIndex(static_cast<int>(type));
  if (type == AssemblyJointType::Spherical) {
    const auto point = std::find_if(
        tools.hierarchy_targets().begin(), tools.hierarchy_targets().end(), [](const auto& target) {
          return target.endpoint.occurrence_path().empty() &&
                 target.target.resolved.source_kind ==
                     geometry::AssemblyGeometricTargetSourceKind::GeneratedVertex;
        });
    REQUIRE(point != tools.hierarchy_targets().end());
    const auto source = point->endpoint.semantic_reference();
    reference(tools, tools.first_editor(), "", source.c_str());
    reference(tools, tools.second_editor(), "left", source.c_str());
  } else {
    reference(tools, tools.first_editor(), "", "feature.hole.seat");
    reference(tools, tools.second_editor(), "left", "feature.hole.seat");
  }
}
} // namespace
TEST_CASE("Hierarchy validation reports repeated paths without authoring history",
          "[gui][block136-assembly-advanced]") {
  ShellWindow window;
  fixture(window);
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  REQUIRE(window.assembly_tools().begin(C::ValidateHierarchy));
  const auto text = widget<QLabel>(window, "summary").text();
  CHECK(text.contains("3 Baugruppenvorkommen"));
  CHECK(text.contains("3 sichtbare aktive"));
  CHECK(text.contains("Root / left"));
  CHECK(text.contains("Root / right"));
  CHECK_FALSE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(window.assembly_tools().apply());
  CHECK(snapshot(window) == before);
  window.assembly_tools().cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(snapshot(window) == before);
}
TEST_CASE("Cross relationship author edit and solve retain occurrence paths and shared authority",
          "[gui][block136-assembly-advanced]") {
  ShellWindow window;
  fixture(window, {{0, 0, 30}, {}});
  auto& tools = window.assembly_tools();
  const auto initial = snapshot(window);
  REQUIRE(tools.begin(C::CrossRelationship));
  reference(tools, tools.first_editor(), "", "feature.base_extrude.top");
  reference(tools, tools.second_editor(), "left", "feature.base_extrude.bottom");
  REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == initial);
  REQUIRE(tools.apply());
  const auto& record = window.session().project()->cross_hierarchy_constraints().front();
  const auto id = record.id();
  CHECK(record.target_a().occurrence_path().empty());
  CHECK(record.target_b().occurrence_path() == std::vector{SubassemblyInstanceId("left")});
  CHECK(window.session()
            .project()
            ->find_assembly_document(DocumentId("child"))
            ->component_instances()
            .front()
            .transform()
            .translation_mm.z == 30);
  history(window, initial);
  const auto authored = snapshot(window);
  REQUIRE(tools.begin(C::CrossRelationship));
  select(widget<QComboBox>(window, "cross_record"), QString::fromStdString(id.value()));
  widget<QLineEdit>(window, "cross_name").setText("Edited mate");
  REQUIRE(tools.apply());
  REQUIRE(window.session().project()->cross_hierarchy_constraint_count() == 1);
  CHECK(window.session().project()->find_cross_hierarchy_constraint(id)->name() == "Edited mate");
  history(window, authored);
  const auto unsolved = snapshot(window);
  const auto boundaries = window.session().project()->assembly().subassembly_instances();
  REQUIRE(tools.begin(C::CrossSolve));
  auto& groups = widget<QListWidget>(window, "solve_group");
  REQUIRE(groups.count() == 1);
  CHECK_FALSE(tools.apply());
  groups.item(0)->setSelected(true);
  INFO(widget<QLabel>(window, "summary").text().toStdString());
  REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == unsolved);
  REQUIRE(tools.apply());
  CHECK(window.session()
            .project()
            ->find_assembly_document(DocumentId("child"))
            ->component_instances()
            .front()
            .transform()
            .translation_mm.z != Catch::Approx(30));
  for (std::size_t i = 0; i < boundaries.size(); ++i)
    CHECK(window.session().project()->assembly().subassembly_instances()[i].transform() ==
          boundaries[i].transform());
  const auto leaves =
      checked(AssemblyLeafOccurrenceResolver{}.resolve(*window.session().project()));
  REQUIRE(leaves.size() == 3);
  CHECK(leaves[1].transforms_inner_to_outer.front() == leaves[2].transforms_inner_to_outer.front());
  history(window, unsolved);
}
TEST_CASE("Cross joint families validate typed slots and edit exact stable records",
          "[gui][block136-assembly-advanced]") {
  const auto type = GENERATE(AssemblyJointType::Revolute, AssemblyJointType::Prismatic,
                             AssemblyJointType::Cylindrical, AssemblyJointType::Planar,
                             AssemblyJointType::Spherical);
  CAPTURE(static_cast<int>(type));
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  joint(window, type);
  if (tools.slots_editor().rowCount()) {
    tools.slots_editor().item(0, 1)->setText("invalid");
    CHECK_FALSE(widget<QPushButton>(window, "apply").isEnabled());
    CHECK_FALSE(tools.apply());
    CHECK(snapshot(window) == before);
    tools.slots_editor().item(0, 1)->setText("0");
  }
  REQUIRE(tools.apply());
  const auto id = window.session().project()->cross_hierarchy_joints().front().id();
  CHECK(window.session().project()->cross_hierarchy_joints().front().type() == type);
  history(window, before);
  const auto created = snapshot(window);
  REQUIRE(tools.begin(C::CrossJoint));
  select(widget<QComboBox>(window, "cross_record"), QString::fromStdString(id.value()));
  CHECK(tools.joint_type_editor().currentIndex() == static_cast<int>(type));
  widget<QCheckBox>(window, "cross_active").setChecked(false);
  REQUIRE(tools.apply());
  REQUIRE(window.session().project()->cross_hierarchy_joint_count() == 1);
  CHECK(window.session().project()->find_cross_hierarchy_joint(id)->state() ==
        AssemblyJointState::Inactive);
  history(window, created);
}
TEST_CASE("Cross motion drives typed coordinates and rejects limits without partial shared poses",
          "[gui][block136-assembly-advanced]") {
  const auto type = GENERATE(AssemblyJointType::Revolute, AssemblyJointType::Prismatic,
                             AssemblyJointType::Cylindrical, AssemblyJointType::Planar);
  CAPTURE(static_cast<int>(type));
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  joint(window, type);
  REQUIRE(tools.apply());
  const auto id = window.session().project()->cross_hierarchy_joints().front().id();
  const auto before = snapshot(window);
  REQUIRE(tools.begin(C::CrossMotion));
  select(tools.joint_editor(), QString::fromStdString(id.value()));
  REQUIRE(tools.coordinate_editor().count() > 0);
  const auto role =
      static_cast<AssemblyJointCoordinateRole>(tools.coordinate_editor().currentData().toInt());
  tools.value_editor().setValue(2000);
  CHECK_FALSE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK_FALSE(tools.apply());
  CHECK(snapshot(window) == before);
  tools.value_editor().setValue(20);
  INFO(widget<QLabel>(window, "summary").text().toStdString());
  REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  REQUIRE(tools.apply());
  const auto* slot =
      window.session().project()->find_cross_hierarchy_joint(id)->find_coordinate_slot(role);
  REQUIRE(slot);
  CHECK((slot->kind() == AssemblyJointCoordinateKind::Angular ? slot->value().degrees()
                                                              : slot->value().millimeters()) == 20);
  const auto leaves =
      checked(AssemblyLeafOccurrenceResolver{}.resolve(*window.session().project()));
  CHECK(leaves[1].transforms_inner_to_outer.front() == leaves[2].transforms_inner_to_outer.front());
  CHECK(window.session()
            .project()
            ->assembly()
            .subassembly_instances()[1]
            .transform()
            .translation_mm.x == 200);
  history(window, before);
}
TEST_CASE("Cross tasks preserve Save cancel and stale source lifecycle",
          "[gui][block136-assembly-advanced]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  joint(window);
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("draft.blcad.project.json");
  REQUIRE(window.save_to(path));
  REQUIRE(tools.active());
  GuiDocumentSession reopened;
  REQUIRE(reopened.open_file(path.toStdString()));
  CHECK(reopened.project()->cross_hierarchy_joint_count() == 0);
  tools.cancel();
  CHECK(snapshot(window) == before);
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK(tools.selection_items().empty());
  joint(window);
  REQUIRE(window.session().commit_project_transaction("External placement", [](Project& project) {
    return project.assembly().set_subassembly_instance_transform(SubassemblyInstanceId("right"),
                                                                 {{210, 0, 0}, {}});
  }));
  const auto external = snapshot(window);
  CHECK_FALSE(tools.apply());
  window.refresh();
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == external);
  joint(window);
  REQUIRE(window.session().dirty());
  bool discarded = false;
  QTimer responder;
  responder.setSingleShot(true);
  QObject::connect(&responder, &QTimer::timeout, &window, [&] {
    if (auto* dialog = qobject_cast<QMessageBox*>(qApp->activeModalWidget())) {
      discarded = true;
      dialog->button(QMessageBox::Discard)->click();
    }
  });
  responder.start(0);
  REQUIRE(window.open_path(path));
  REQUIRE(discarded);
  CHECK_FALSE(tools.active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Native hierarchy target picks retain rotated parent and repeated occurrence identity",
          "[gui][block136-assembly-advanced][native-shell-assembly-advanced]") {
  if (QApplication::platformName() != "xcb")
    SKIP("Requires native OCCT occurrence-qualified target picking");
  ShellWindow window;
  fixture(window);
  const RigidTransform boundary{{60, 25, 12}, {20, 15, 35}};
  REQUIRE(window.session().commit_project_transaction(
      "Rotate repeated parent", [boundary](Project& project) {
        return project.assembly().set_subassembly_instance_transform(SubassemblyInstanceId("right"),
                                                                     boundary);
      }));
  window.refresh();
  auto& tools = window.assembly_tools();
  const auto before = snapshot(window);
  const auto mask = window.viewport().selection_filter_mask();
  const auto scope = window.viewport().selection_semantic_scope();
  REQUIRE(tools.begin(C::CrossRelationship));
  reference(tools, tools.first_editor(), "", "feature.base_extrude.top");
  tools.pick_editor().setCurrentIndex(1);
  const auto found = std::find_if(
      tools.hierarchy_targets().begin(), tools.hierarchy_targets().end(), [](const auto& target) {
        return target.endpoint.occurrence_path() == std::vector{SubassemblyInstanceId("right")} &&
               target.endpoint.semantic_reference() == "feature.base_extrude.bottom";
      });
  REQUIRE(found != tools.hierarchy_targets().end());
  const int expected = static_cast<int>(found - tools.hierarchy_targets().begin());
  const auto* plane =
      std::get_if<geometry::AssemblyPlanarTargetDescriptor>(&found->target.resolved.descriptor);
  REQUIRE(plane);
  CHECK(found->target.anchor.x == Catch::Approx(plane->origin.x));
  CHECK(found->target.anchor.y == Catch::Approx(plane->origin.y));
  CHECK(found->target.anchor.z == Catch::Approx(plane->origin.z));
  const auto world =
      geometry::AssemblyHierarchyTransformEvaluator{}.evaluate_point({boundary}, {3, 8, 0});
  REQUIRE(window.viewport().set_plane_camera(world, plane->normal, plane->y_axis));
  window.viewport().fit_all();
  QEventLoop frame;
  QTimer::singleShot(50, &frame, &QEventLoop::quit);
  frame.exec();
  const auto screen = checked(window.viewport().model_to_screen(world));
  const QPointF local(screen.x, screen.y);
  for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
    QMouseEvent event(type, local, window.viewport().mapToGlobal(local.toPoint()), Qt::LeftButton,
                      type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&window.viewport(), &event);
    qApp->processEvents();
  }
  REQUIRE(tools.second_editor().currentIndex() > 0);
  CHECK(tools.second_editor().currentData().toInt() == expected);
  const auto& selected = tools.hierarchy_targets().at(tools.second_editor().currentData().toInt());
  CHECK(selected.endpoint.occurrence_path() == std::vector{SubassemblyInstanceId("right")});
  CHECK(selected.endpoint.component_instance() == ComponentInstanceId("moving"));
  CHECK(selected.endpoint.semantic_reference() == "feature.base_extrude.bottom");
  REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
  CHECK(snapshot(window) == before);
  const auto directory = qEnvironmentVariable("BLCAD_UI_CAPTURE_DIR");
  if (!directory.isEmpty()) {
    REQUIRE(QDir().mkpath(directory));
    CHECK(window.grab().save(QDir(directory).filePath("hierarchy-rotated-target-preview.png")));
  }
  tools.cancel();
  CHECK(window.viewport().selection_filter_mask() == mask);
  CHECK(window.viewport().selection_semantic_scope() == scope);
  CHECK_FALSE(window.viewport().feature_preview_active());
  CHECK(snapshot(window) == before);
}

TEST_CASE("Cross Planar workflow retains undriven coordinates through three accepted shared poses",
          "[gui][block136-assembly-advanced]") {
  ShellWindow window;
  fixture(window);
  auto& tools = window.assembly_tools();
  joint(window, AssemblyJointType::Planar);
  REQUIRE(tools.apply());
  const auto id = window.session().project()->cross_hierarchy_joints().front().id();
  const std::array<AssemblyJointCoordinateRole, 3> roles{
      AssemblyJointCoordinateRole::TranslationU, AssemblyJointCoordinateRole::TranslationV,
      AssemblyJointCoordinateRole::RotationNormal};
  const std::array<double, 3> requested{10, -15, 25};
  std::array<double, 3> accepted{};
  std::vector<std::string> snapshots{snapshot(window)};
  for (std::size_t step = 0; step < roles.size(); ++step) {
    CAPTURE(step);
    REQUIRE(tools.begin(C::CrossMotion));
    select(tools.joint_editor(), QString::fromStdString(id.value()));
    const int row = tools.coordinate_editor().findData(static_cast<int>(roles[step]));
    REQUIRE(row >= 0);
    tools.coordinate_editor().setCurrentIndex(row);
    CHECK(tools.value_editor().value() == 0);
    if (step == 1) {
      tools.value_editor().setValue(2000);
      CHECK_FALSE(widget<QPushButton>(window, "apply").isEnabled());
      CHECK_FALSE(tools.apply());
      CHECK(snapshot(window) == snapshots.back());
    }
    tools.value_editor().setValue(requested[step]);
    INFO(widget<QLabel>(window, "summary").text().toStdString());
    REQUIRE(widget<QPushButton>(window, "apply").isEnabled());
    CHECK(snapshot(window) == snapshots.back());
    REQUIRE(tools.apply());
    accepted[step] = requested[step];
    const auto* record = window.session().project()->find_cross_hierarchy_joint(id);
    REQUIRE(record);
    for (std::size_t coordinate = 0; coordinate < roles.size(); ++coordinate) {
      const auto* slot = record->find_coordinate_slot(roles[coordinate]);
      REQUIRE(slot);
      CHECK((slot->kind() == AssemblyJointCoordinateKind::Angular
                 ? slot->value().degrees()
                 : slot->value().millimeters()) == accepted[coordinate]);
    }
    const auto leaves =
        checked(AssemblyLeafOccurrenceResolver{}.resolve(*window.session().project()));
    REQUIRE(leaves.size() == 3);
    CHECK(leaves[1].transforms_inner_to_outer.front() ==
          leaves[2].transforms_inner_to_outer.front());
    CHECK(window.session()
              .project()
              ->assembly()
              .subassembly_instances()[1]
              .transform()
              .translation_mm.x == 200);
    snapshots.push_back(snapshot(window));
    CHECK(snapshots.back() != snapshots[snapshots.size() - 2]);
  }
  for (std::size_t step = roles.size(); step > 0; --step) {
    REQUIRE(window.undo());
    CHECK(snapshot(window) == snapshots[step - 1]);
  }
  for (std::size_t step = 1; step < snapshots.size(); ++step) {
    REQUIRE(window.redo());
    CHECK(snapshot(window) == snapshots[step]);
  }
  QTemporaryDir directory;
  REQUIRE(directory.isValid());
  const auto path = directory.filePath("planar-motion.blcad.project.json");
  REQUIRE(window.save_to(path));
  REQUIRE(window.open_path(path));
  CHECK(snapshot(window) == snapshots.back());
  CHECK_FALSE(window.viewport().feature_preview_active());
}
