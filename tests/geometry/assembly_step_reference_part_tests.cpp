#include "blcad/core/assembly_leaf_occurrence.hpp"
#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/core/step_import_freshness.hpp"
#include "blcad/geometry/assembly_contact_analyzer.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"
#include "blcad/geometry/assembly_cross_hierarchy_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_flexible_subassembly_solver.hpp"
#include "blcad/geometry/assembly_hierarchy_transform_evaluator.hpp"
#include "blcad/geometry/assembly_interference_analyzer.hpp"
#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_revolute_sweep_analyzer.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"
#include "blcad/geometry/step_exporter.hpp"
#include "blcad/geometry/step_import_reader.hpp"
#include "blcad/geometry/step_import_topology.hpp"
#include "blcad/geometry/step_import_topology_context.hpp"
#include "blcad/geometry/step_reference_commands.hpp"
#include "blcad/geometry/viewport_scene.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace {
template <class T> T value(Result<T> result) {
  INFO((result ? "ok" : result.error().message()));
  REQUIRE(result);
  return std::move(result.value());
}
std::string json(const Project& project) {
  return value(serialize_project_to_json(project));
}
Quantity angle(double degrees) {
  return value(Quantity::angle_deg(degrees));
}
std::string frame() {
  return value(make_assembly_frame_target_spelling(DatumPlaneId("datum.xy")));
}
AssemblyConstraintTarget target(const char* component, const std::string& semantic) {
  return value(AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic));
}
AssemblyHierarchyConstraintEndpoint endpoint(std::vector<SubassemblyInstanceId> path,
                                             const char* component, const std::string& semantic) {
  return value(AssemblyHierarchyConstraintEndpoint::create(
      std::move(path), ComponentInstanceId(component), semantic));
}
ComponentInstance occurrence(const char* component, ComponentGroundingState grounding,
                             RigidTransform transform = identity_rigid_transform()) {
  return value(ComponentInstance::create(ComponentInstanceId(component), component,
                                         DocumentId("part.reference"), ComponentVisibility::Visible,
                                         ComponentSuppressionState::Active, grounding, transform));
}
SubassemblyInstance boundary(const char* id, const char* assembly, RigidTransform transform) {
  return value(SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(assembly),
                                           ComponentVisibility::Visible,
                                           ComponentSuppressionState::Active, transform));
}
void coincident(AssemblyDocument& assembly, const char* id, const char* first, const char* second,
                const std::string& semantic) {
  REQUIRE(assembly.add_constraint(value(
      AssemblyConstraint::create(AssemblyConstraintId(id), id, AssemblyConstraintType::Coincident,
                                 target(first, semantic), target(second, semantic)))));
}
std::vector<SubassemblyInstanceId> nested_path() {
  return {SubassemblyInstanceId("outer.left"), SubassemblyInstanceId("inner")};
}
void near(Point3 a, Point3 b) {
  CHECK(a.x == Approx(b.x).margin(1e-5));
  CHECK(a.y == Approx(b.y).margin(1e-5));
  CHECK(a.z == Approx(b.z).margin(1e-5));
}
Point3 world_point(const Project& project, const char* component,
                   const std::vector<SubassemblyInstanceId>& path, Point3 local) {
  auto leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  auto leaf = std::find_if(leaves.begin(), leaves.end(), [&](const auto& item) {
    return item.component_instance == ComponentInstanceId(component) &&
           item.subassembly_path == path;
  });
  REQUIRE(leaf != leaves.end());
  return AssemblyHierarchyTransformEvaluator{}.evaluate_point(leaf->transforms_inner_to_outer,
                                                              local);
}

struct Fixture {
  std::filesystem::path root;
  StepImportSource source;
  ImportedTopologyCatalog catalog;
  PartDocument part;
  Fixture()
      : root(directory()), source(write_source()), catalog(capture()),
        part(value(StepReferenceCommands{}.create_definition(
            source, catalog, DocumentId("part.reference"), "Shared Reference", root))) {}
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
  static std::filesystem::path directory() {
    static std::atomic<unsigned> sequence{};
    auto path = std::filesystem::temp_directory_path() /
                ("blcad-assembly-reference-" + std::to_string(::getpid()) + "-" +
                 std::to_string(++sequence));
    REQUIRE(std::filesystem::create_directories(path));
    return path;
  }
  StepImportSource write_source() const {
    auto shape = value(RectangleExtrusionAdapter{}.make_extruded_rectangle(
        value(Quantity::length_mm(20)), value(Quantity::length_mm(10)),
        value(Quantity::length_mm(5))));
    REQUIRE(StepExporter{}.write_step(shape, (root / "supplier.step").string()));
    auto report = value(inspect_step_import_asset(root, "supplier.step"));
    REQUIRE(report.observed_sha256_digest());
    return value(StepImportSource::create(StepImportSourceId("source.supplier"), "Supplier",
                                          "supplier.step", *report.observed_sha256_digest(),
                                          StepImportMode::Reference));
  }
  ImportedTopologyCatalog capture() const {
    return value(StepImportTopology{}.create_catalog(ImportedTopologyCatalogId("catalog.reference"),
                                                     value(StepImportReader{}.read(source, root)),
                                                     0));
  }
  const ImportedTopologyEntry& point_entry() const {
    auto found =
        std::find_if(catalog.entries().begin(), catalog.entries().end(), [](const auto& entry) {
          const auto& d = entry.descriptor;
          return d.kind == ImportedTopologyKind::Vertex && std::abs(d.centroid.x - 10) < 1e-7 &&
                 std::abs(d.centroid.y - 5) < 1e-7 && std::abs(d.centroid.z - 5) < 1e-7;
        });
    REQUIRE(found != catalog.entries().end());
    return *found;
  }
  std::string point() const {
    return value(make_imported_topology_target_spelling(value(ImportedTopologyReference::create(
        catalog.id(), point_entry().id, ImportedTopologyCapability::Point))));
  }
  Project pair(RigidTransform moving = identity_rigid_transform()) const {
    auto assembly = value(AssemblyDocument::create(DocumentId("assembly.root"), "Root"));
    REQUIRE(assembly.add_member_part(part.id()));
    REQUIRE(
        assembly.add_component_instance(occurrence("fixed", ComponentGroundingState::Grounded)));
    REQUIRE(assembly.add_component_instance(
        occurrence("moving", ComponentGroundingState::Free, moving)));
    auto project =
        value(Project::create(DocumentId("project.reference"), "Reference Project", assembly));
    REQUIRE(project.add_part_document(part));
    return project;
  }
  Project local_solve() const {
    auto project = pair({{17, 12, 4}, {}});
    coincident(project.assembly(), "point.local", "fixed", "moving", point());
    return project;
  }
  Project local_motion() const {
    auto project = pair();
    REQUIRE(project.assembly().add_joint(value(AssemblyJoint::create(
        AssemblyJointId("joint.local"), "Explicit origin Frame", AssemblyJointType::Revolute,
        target("fixed", frame()), target("moving", frame()), AssemblyJointState::Active, angle(-60),
        angle(60), angle(0)))));
    return project;
  }
  Project nested(bool flexible = false, bool noncommuting = true) const {
    auto project = pair();
    const auto outer =
        noncommuting ? RigidTransform{{17, 19, 23}, {0, 0, 90}} : identity_rigid_transform();
    const auto inner =
        noncommuting ? RigidTransform{{7, 11, 13}, {90, 0, 0}} : identity_rigid_transform();
    auto grand = value(AssemblyDocument::create(DocumentId("assembly.grand"), "Grandchild"));
    REQUIRE(grand.add_member_part(part.id()));
    REQUIRE(grand.add_component_instance(occurrence(
        "nested", ComponentGroundingState::Free,
        noncommuting ? RigidTransform{{3, 4, 5}, {0, 90, 0}} : identity_rigid_transform())));
    auto child = value(AssemblyDocument::create(DocumentId("assembly.child"), "Child"));
    REQUIRE(child.add_subassembly_instance(boundary("inner", "assembly.grand", inner)));
    if (flexible) {
      REQUIRE(child.add_member_part(part.id()));
      REQUIRE(child.add_component_instance(
          occurrence("child.fixed", ComponentGroundingState::Grounded)));
      REQUIRE(child.add_component_instance(
          occurrence("child.moving", ComponentGroundingState::Free, {{5, 3, 8}, {}})));
      coincident(child, "point.child", "child.fixed", "child.moving", point());
    }
    REQUIRE(project.add_child_assembly_document(grand));
    REQUIRE(project.add_child_assembly_document(child));
    REQUIRE(project.assembly().add_subassembly_instance(
        boundary("outer.left", "assembly.child", outer)));
    REQUIRE(project.assembly().add_subassembly_instance(
        boundary("outer.right", "assembly.child", {{100, 50, 20}, {0, 0, -90}})));
    REQUIRE(project.validate_assembly_structure());
    return project;
  }
  Project cross_solve() const {
    auto project = nested();
    REQUIRE(project.add_cross_hierarchy_constraint(value(AssemblyHierarchyConstraint::create(
        AssemblyConstraintId("point.cross"), "Root to grandchild imported point",
        AssemblyConstraintType::Coincident, endpoint({}, "fixed", point()),
        endpoint(nested_path(), "nested", point())))));
    return project;
  }
  Project cross_motion() const {
    auto project = nested(false, false);
    REQUIRE(project.add_cross_hierarchy_joint(value(AssemblyHierarchyJoint::create(
        AssemblyJointId("joint.cross"), "Root to grandchild explicit origin Frame",
        AssemblyJointType::Revolute, endpoint({}, "fixed", frame()),
        endpoint(nested_path(), "nested", frame()), AssemblyJointState::Active, angle(-60),
        angle(60), angle(0)))));
    return project;
  }
  void change_source() const {
    std::ifstream input(root / "supplier.step", std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    auto end = bytes.find("ENDSEC;");
    REQUIRE(end != std::string::npos);
    bytes.insert(end, "/* external supplier revision B */\n");
    input.close();
    std::ofstream output(root / "supplier.step", std::ios::binary | std::ios::trunc);
    output << bytes;
    REQUIRE(output.good());
  }
};
AssemblyCrossHierarchySolveGroup cross_group(const Project& project) {
  auto graph = value(AssemblyCrossHierarchyConstraintGraph::build(project));
  REQUIRE(graph.solve_group_count() == 1);
  return graph.solve_groups().front();
}
} // namespace

TEST_CASE("Reference assembly geometry shares one definition through exact repeated nested "
          "transform chains",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  const auto project = fixture.nested();
  const auto before = json(project);
  REQUIRE(project.part_document_count() == 1);
  const auto leaves = value(AssemblyLeafOccurrenceResolver{}.resolve(project));
  REQUIRE(leaves.size() == 4);
  for (const auto& leaf : leaves)
    CHECK(leaf.referenced_part_document == fixture.part.id());
  near(world_point(project, "nested", nested_path(), fixture.point_entry().descriptor.centroid),
       {1, 34, 45});
  StepImportTopologyContext context(fixture.root);
  const auto scene = value(ViewportSceneBuilder{&context}.build_project(project));
  CHECK(scene.size() == 4);
  const auto contacts = value(AssemblyContactAnalyzer{&context}.analyze(project));
  CHECK(contacts.component_occurrence_count == 4);
  CHECK(contacts.recomputed_part_count == 1);
  CHECK(contacts.evaluated_pair_count == 6);
  CHECK(json(project) == before);
  const auto saved = fixture.root / "assembly.blcad.project.json";
  REQUIRE(write_project_json_file(project, saved));
  const auto reopened = value(read_project_json_file(saved));
  CHECK(json(reopened) == before);
  StepImportTopologyContext reopened_context(fixture.root);
  CHECK(value(ViewportSceneBuilder{&reopened_context}.build_project(reopened)).size() == 4);
  near(world_point(reopened, "nested", nested_path(), fixture.point_entry().descriptor.centroid),
       {1, 34, 45});
}

TEST_CASE("Reference imported points solve local and occurrence-qualified relationships without "
          "feature producers",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_solve();
  const auto before = json(local);
  const auto part_before = value(serialize_part_document_to_json(fixture.part));
  StepImportTopologyContext context(fixture.root);
  auto solved = value(AssemblyRigidBodySolver{&context}.solve(
      local, {ComponentInstanceId("fixed"), ComponentInstanceId("moving")}));
  REQUIRE(solved.converged());
  CHECK(solved.residual_summary.final_rms < 1e-7);
  CHECK(json(local) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblySolveResultApplier{&apply_context}.apply(local, solved));
  near(world_point(local, "fixed", {}, fixture.point_entry().descriptor.centroid),
       world_point(local, "moving", {}, fixture.point_entry().descriptor.centroid));
  CHECK(value(serialize_part_document_to_json(*local.find_part_document(fixture.part.id()))) ==
        part_before);
  CHECK(local.find_part_document(fixture.part.id())->features().empty());
  auto cross = fixture.cross_solve();
  const auto cross_before = json(cross);
  const auto root_fixed =
      cross.assembly().find_component_instance(ComponentInstanceId("fixed"))->transform();
  StepImportTopologyContext cross_context(fixture.root);
  auto cross_solved = value(
      AssemblyCrossHierarchyConstraintSolver{&cross_context}.solve(cross, cross_group(cross)));
  REQUIRE(cross_solved.converged());
  CHECK(cross_solved.residual_summary.final_rms < 1e-7);
  CHECK(json(cross) == cross_before);
  StepImportTopologyContext cross_apply(fixture.root);
  REQUIRE(AssemblyCrossHierarchySolveResultApplier{&cross_apply}.apply(cross, cross_solved));
  near(world_point(cross, "fixed", {}, fixture.point_entry().descriptor.centroid),
       world_point(cross, "nested", nested_path(), fixture.point_entry().descriptor.centroid));
  CHECK(cross.assembly().find_component_instance(ComponentInstanceId("fixed"))->transform() ==
        root_fixed);
}

TEST_CASE("Flexible Reference child solving retains reachable grandchildren and repeated shared "
          "authority",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto project = fixture.nested(true);
  const auto before = json(project);
  const auto left_boundary = project.assembly()
                                 .find_subassembly_instance(SubassemblyInstanceId("outer.left"))
                                 ->transform();
  const auto grand_boundary = project.find_assembly_document(DocumentId("assembly.child"))
                                  ->find_subassembly_instance(SubassemblyInstanceId("inner"))
                                  ->transform();
  const auto grand_transform = project.find_assembly_document(DocumentId("assembly.grand"))
                                   ->component_instances()
                                   .front()
                                   .transform();
  StepImportTopologyContext context(fixture.root);
  auto result = value(AssemblyFlexibleSubassemblySolver{&context}.solve(
      project, {SubassemblyInstanceId("outer.left")},
      {ComponentInstanceId("child.fixed"), ComponentInstanceId("child.moving")}));
  REQUIRE(result.converged());
  CHECK(json(project) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblyFlexibleSubassemblySolveResultApplier{&apply_context}.apply(project, result));
  for (const char* occurrence_id : {"outer.left", "outer.right"}) {
    const std::vector<SubassemblyInstanceId> path{SubassemblyInstanceId(occurrence_id)};
    near(world_point(project, "child.fixed", path, fixture.point_entry().descriptor.centroid),
         world_point(project, "child.moving", path, fixture.point_entry().descriptor.centroid));
  }
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("outer.left"))
            ->transform() == left_boundary);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_subassembly_instance(SubassemblyInstanceId("inner"))
            ->transform() == grand_boundary);
  CHECK(project.find_assembly_document(DocumentId("assembly.grand"))
            ->component_instances()
            .front()
            .transform() == grand_transform);
  StepImportTopologyContext scene_context(fixture.root);
  CHECK(value(ViewportSceneBuilder{&scene_context}.build_project(project)).size() == 8);
}

TEST_CASE(
    "Immutable Reference origin Frames drive local and nested Revolute motion with atomic apply",
    "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_motion();
  const auto before = json(local);
  StepImportTopologyContext context(fixture.root);
  auto motion = value(
      AssemblyJointMotionSolver{&context}.move(local, AssemblyJointId("joint.local"), angle(25)));
  REQUIRE(motion.converged());
  CHECK(json(local) == before);
  StepImportTopologyContext apply_context(fixture.root);
  REQUIRE(AssemblyJointMotionResultApplier{&apply_context}.apply(local, motion));
  CHECK(local.assembly().joints().front().coordinate_deg() == Approx(25));
  const auto& transform =
      local.assembly().find_component_instance(ComponentInstanceId("moving"))->transform();
  CHECK(std::abs(transform.rotation_deg.z) == Approx(25).margin(1e-5));
  CHECK(local.find_part_document(fixture.part.id())->is_reference_part());
  auto nested = fixture.cross_motion();
  const auto nested_before = json(nested);
  StepImportTopologyContext nested_context(fixture.root);
  auto nested_motion = value(AssemblyCrossHierarchyJointMotionSolver{&nested_context}.move(
      nested, AssemblyJointId("joint.cross"), angle(15)));
  REQUIRE(nested_motion.converged());
  CHECK(json(nested) == nested_before);
  StepImportTopologyContext nested_apply(fixture.root);
  REQUIRE(
      AssemblyCrossHierarchyJointMotionResultApplier{&nested_apply}.apply(nested, nested_motion));
  CHECK(nested.cross_hierarchy_joints().front().coordinate_deg() == Approx(15));
  CHECK(nested.find_assembly_document(DocumentId("assembly.grand"))
            ->component_instances()
            .front()
            .transform() != identity_rigid_transform());
  CHECK(nested.assembly().component_instances().front().transform() == identity_rigid_transform());
}

TEST_CASE("Reference posed contact interference clearance and sampled sweep use actual shared "
          "imported bodies",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto separated = fixture.pair({{22, 0, 0}, {}});
  const auto before = json(separated);
  StepImportTopologyContext context(fixture.root);
  const auto contacts = value(AssemblyContactAnalyzer{&context}.analyze(separated));
  REQUIRE(contacts.records.size() == 1);
  CHECK(contacts.records.front().classification == AssemblyContactClassification::Separated);
  REQUIRE(contacts.records.front().minimum_distance_mm);
  CHECK(*contacts.records.front().minimum_distance_mm == Approx(2).margin(1e-5));
  const auto clearance = value(AssemblyClearanceAnalyzer{&context}.analyze(separated, {3, 1e-6}));
  REQUIRE(clearance.clearance_violations.size() == 1);
  CHECK(clearance.clearance_violations.front().minimum_distance_mm == Approx(2).margin(1e-5));
  CHECK(clearance.interferences.empty());
  CHECK(clearance.recomputed_part_count == 1);
  CHECK(json(separated) == before);
  auto touching = fixture.pair({{20, 0, 0}, {}});
  CHECK(value(AssemblyContactAnalyzer{&context}.analyze(touching)).records.front().classification ==
        AssemblyContactClassification::Touching);
  auto overlapping = fixture.pair({{10, 0, 0}, {}});
  const auto overlap = value(AssemblyInterferenceAnalyzer{&context}.analyze(overlapping));
  REQUIRE(overlap.interferences.size() == 1);
  CHECK(overlap.interferences.front().overlap_volume_mm3 == Approx(500).margin(1e-5));
  auto motion = fixture.local_motion();
  const auto motion_before = json(motion);
  const AssemblyRevoluteSweepRequest request{
      {AssemblyRevoluteSweepJointScope::RootAssemblyLocal, AssemblyJointId("joint.local")},
      angle(-20),
      angle(20),
      3};
  auto sweep = value(AssemblyRevoluteSweepAnalyzer{&context}.analyze(motion, request));
  REQUIRE(sweep.samples.size() == 3);
  for (std::size_t i = 0; i < sweep.samples.size(); ++i) {
    const auto& sample = sweep.samples[i];
    CHECK(sample.coordinate_deg == Approx(-20 + 20 * static_cast<double>(i)));
    REQUIRE(sample.contact_analysis.records.size() == 1);
    CHECK(sample.contact_analysis.records.front().classification ==
          AssemblyContactClassification::Interfering);
  }
  CHECK(json(motion) == motion_before);
}

TEST_CASE("Fresh contexts reject every Reference solve and motion publication after external "
          "source changes",
          "[geometry][assembly-step-reference-part]") {
  Fixture fixture;
  auto local = fixture.local_solve();
  auto cross = fixture.cross_solve();
  auto flexible = fixture.nested(true);
  auto motion = fixture.local_motion();
  auto cross_motion = fixture.cross_motion();
  const auto local_before = json(local), cross_before = json(cross),
             flexible_before = json(flexible), motion_before = json(motion),
             cross_motion_before = json(cross_motion);
  StepImportTopologyContext query(fixture.root);
  auto solved = value(AssemblyRigidBodySolver{&query}.solve(
      local, {ComponentInstanceId("fixed"), ComponentInstanceId("moving")}));
  auto cross_solved =
      value(AssemblyCrossHierarchyConstraintSolver{&query}.solve(cross, cross_group(cross)));
  auto child_solved = value(AssemblyFlexibleSubassemblySolver{&query}.solve(
      flexible, {SubassemblyInstanceId("outer.left")},
      {ComponentInstanceId("child.fixed"), ComponentInstanceId("child.moving")}));
  auto moved = value(
      AssemblyJointMotionSolver{&query}.move(motion, AssemblyJointId("joint.local"), angle(25)));
  auto cross_moved = value(AssemblyCrossHierarchyJointMotionSolver{&query}.move(
      cross_motion, AssemblyJointId("joint.cross"), angle(15)));
  REQUIRE(solved.converged());
  REQUIRE(cross_solved.converged());
  REQUIRE(child_solved.converged());
  REQUIRE(moved.converged());
  REQUIRE(cross_moved.converged());
  SECTION("Changed accepted bytes") {
    fixture.change_source();
  }
  SECTION("Missing accepted asset") {
    REQUIRE(std::filesystem::remove(fixture.root / "supplier.step"));
  }
  StepImportTopologyContext apply_local(fixture.root), apply_cross(fixture.root),
      apply_child(fixture.root), apply_motion(fixture.root), apply_cross_motion(fixture.root);
  CHECK_FALSE(AssemblySolveResultApplier{&apply_local}.apply(local, solved));
  CHECK_FALSE(AssemblyCrossHierarchySolveResultApplier{&apply_cross}.apply(cross, cross_solved));
  CHECK_FALSE(
      AssemblyFlexibleSubassemblySolveResultApplier{&apply_child}.apply(flexible, child_solved));
  CHECK_FALSE(AssemblyJointMotionResultApplier{&apply_motion}.apply(motion, moved));
  CHECK_FALSE(AssemblyCrossHierarchyJointMotionResultApplier{&apply_cross_motion}.apply(
      cross_motion, cross_moved));
  CHECK(json(local) == local_before);
  CHECK(json(cross) == cross_before);
  CHECK(json(flexible) == flexible_before);
  CHECK(json(motion) == motion_before);
  CHECK(json(cross_motion) == cross_motion_before);
  StepImportTopologyContext analysis(fixture.root);
  CHECK_FALSE(AssemblyContactAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(AssemblyInterferenceAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(AssemblyClearanceAnalyzer{&analysis}.analyze(local));
  CHECK_FALSE(ViewportSceneBuilder{&analysis}.build_project(local));
  CHECK_FALSE(AssemblyRevoluteSweepAnalyzer{&analysis}.analyze(
      motion, {{AssemblyRevoluteSweepJointScope::RootAssemblyLocal, AssemblyJointId("joint.local")},
               angle(-10),
               angle(10),
               3}));
}
