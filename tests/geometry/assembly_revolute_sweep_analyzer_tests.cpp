#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_revolute_sweep_analyzer.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace ts = blcad::geometry::test_support;

namespace {

Quantity angle(double degrees, const char* id = "sweep") {
  auto value = Quantity::angle_deg(degrees, id);
  REQUIRE(value);
  return value.value();
}

AssemblyConstraintTarget target(const char* component_id, const char* semantic_reference) {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component_id),
                                                semantic_reference);
  REQUIRE(value);
  return value.value();
}

ComponentInstance motion_component(
    const char* id, ComponentVisibility visibility, ComponentGroundingState grounding,
    RigidTransform transform = identity_rigid_transform()) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.block27_plate"), visibility,
      ComponentSuppressionState::Active, grounding, transform);
  REQUIRE(value);
  return value.value();
}

AssemblyJoint local_revolute_joint() {
  auto value = AssemblyJoint::create(
      AssemblyJointId("joint.local.sweep"), "LocalSweep", AssemblyJointType::Revolute,
      target("component.arm", "feature.hole.seat"),
      target("component.anchor", "feature.hole.seat"), AssemblyJointState::Active,
      angle(-90.0, "joint.local.sweep"), angle(90.0, "joint.local.sweep"),
      angle(0.0, "joint.local.sweep"));
  REQUIRE(value);
  return value.value();
}

Project local_sweep_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(motion_component(
      "component.anchor", ComponentVisibility::Hidden, ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_component_instance(motion_component(
      "component.arm", ComponentVisibility::Visible, ComponentGroundingState::Free)));
  REQUIRE(root.value().add_component_instance(motion_component(
      "component.obstacle", ComponentVisibility::Visible, ComponentGroundingState::Grounded,
      RigidTransform{Vector3{120.0, 0.0, 0.0}, Vector3{}})));
  REQUIRE(root.value().add_joint(local_revolute_joint()));

  auto project = Project::create(DocumentId("project.local_sweep"), "LocalSweep", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

AssemblyHierarchyJoint cross_revolute_joint() {
  auto value = AssemblyHierarchyJoint::create(
      AssemblyJointId("joint.cross.sweep"), "CrossSweep", AssemblyJointType::Revolute,
      ts::endpoint({}, "component.root", "feature.hole.seat"),
      ts::endpoint({"subassembly.left"}, "component.child", "feature.hole.seat"),
      AssemblyJointState::Active, angle(-90.0, "joint.cross.sweep"),
      angle(90.0, "joint.cross.sweep"), angle(0.0, "joint.cross.sweep"));
  REQUIRE(value);
  return value.value();
}

Project cross_sweep_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      ts::component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(
      ts::occurrence("subassembly.left", identity_rigid_transform())));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      ts::component("component.child", ComponentGroundingState::Free)));

  auto project = Project::create(DocumentId("project.cross_sweep"), "CrossSweep", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().add_cross_hierarchy_joint(cross_revolute_joint()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

AssemblyRevoluteSweepRequest local_request(double start_deg, double end_deg,
                                           std::size_t sample_count) {
  return AssemblyRevoluteSweepRequest{
      AssemblyRevoluteSweepJointReference{AssemblyRevoluteSweepJointScope::RootAssemblyLocal,
                                          AssemblyJointId("joint.local.sweep")},
      angle(start_deg), angle(end_deg), sample_count};
}

AssemblyRevoluteSweepRequest cross_request(double start_deg, double end_deg,
                                           std::size_t sample_count) {
  return AssemblyRevoluteSweepRequest{
      AssemblyRevoluteSweepJointReference{AssemblyRevoluteSweepJointScope::ProjectCrossHierarchy,
                                          AssemblyJointId("joint.cross.sweep")},
      angle(start_deg), angle(end_deg), sample_count};
}

const AssemblyContactRecord& only_contact(const AssemblyRevoluteSweepSample& sample) {
  REQUIRE(sample.contact_analysis.contacts.size() == 1U);
  return sample.contact_analysis.contacts.front();
}

} // namespace

TEST_CASE("Sampled root-local Revolute sweep classifies contact at deterministic inclusive coordinates",
          "[geometry][assembly-revolute-sweep]") {
  Project project = local_sweep_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  auto sweep = AssemblyRevoluteSweepAnalyzer{}.analyze(project, local_request(0.0, 90.0, 3U));
  REQUIRE(sweep);
  REQUIRE(sweep.value().samples.size() == 3U);
  CHECK(sweep.value().start_coordinate_deg == 0.0);
  CHECK(sweep.value().end_coordinate_deg == 90.0);

  CHECK(sweep.value().samples[0].sample_index == 0U);
  CHECK(sweep.value().samples[0].coordinate_deg == 0.0);
  CHECK(sweep.value().samples[1].sample_index == 1U);
  CHECK(sweep.value().samples[1].coordinate_deg == 45.0);
  CHECK(sweep.value().samples[2].sample_index == 2U);
  CHECK(sweep.value().samples[2].coordinate_deg == 90.0);
  CHECK(sweep.value().samples[0].applied_transform_count == 1U);
  CHECK(sweep.value().samples[1].applied_transform_count == 1U);
  CHECK(sweep.value().samples[2].applied_transform_count == 1U);

  CHECK(only_contact(sweep.value().samples[0]).classification ==
        AssemblyContactClassification::Touching);
  CHECK(only_contact(sweep.value().samples[1]).classification ==
        AssemblyContactClassification::Interfering);
  CHECK(only_contact(sweep.value().samples[2]).classification ==
        AssemblyContactClassification::Separated);

  for (const auto& sample : sweep.value().samples) {
    CHECK(sample.contact_analysis.component_occurrence_count == 2U);
    CHECK(sample.contact_analysis.evaluated_pair_count == 1U);
    CHECK(sample.contact_analysis.recomputed_part_count == 1U);
    const auto& pair = only_contact(sample).pair;
    CHECK(pair.occurrence_a.component_instance.value() == "component.arm");
    CHECK(pair.occurrence_b.component_instance.value() == "component.obstacle");
  }

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.local.sweep"))->coordinate_deg() ==
        0.0);
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.arm"))
            ->transform() == identity_rigid_transform());
}

TEST_CASE("Sampled Revolute sweep preserves caller interval direction",
          "[geometry][assembly-revolute-sweep]") {
  auto sweep =
      AssemblyRevoluteSweepAnalyzer{}.analyze(local_sweep_project(), local_request(90.0, 0.0, 3U));
  REQUIRE(sweep);
  REQUIRE(sweep.value().samples.size() == 3U);
  CHECK(sweep.value().samples[0].coordinate_deg == 90.0);
  CHECK(sweep.value().samples[1].coordinate_deg == 45.0);
  CHECK(sweep.value().samples[2].coordinate_deg == 0.0);
}

TEST_CASE("Sampled cross-hierarchy Revolute sweep reuses atomic cross motion on fresh Project copies",
          "[geometry][assembly-revolute-sweep]") {
  Project project = cross_sweep_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);

  auto sweep = AssemblyRevoluteSweepAnalyzer{}.analyze(project, cross_request(0.0, 45.0, 2U));
  REQUIRE(sweep);
  REQUIRE(sweep.value().samples.size() == 2U);
  CHECK(sweep.value().samples[0].coordinate_deg == 0.0);
  CHECK(sweep.value().samples[1].coordinate_deg == 45.0);
  CHECK(sweep.value().samples[0].applied_transform_count == 1U);
  CHECK(sweep.value().samples[1].applied_transform_count == 1U);
  CHECK(only_contact(sweep.value().samples[0]).classification ==
        AssemblyContactClassification::Interfering);
  CHECK(only_contact(sweep.value().samples[1]).classification ==
        AssemblyContactClassification::Interfering);

  const auto& pair = only_contact(sweep.value().samples[1]).pair;
  CHECK(pair.occurrence_a.containing_assembly_occurrence.occurrence_path.empty());
  REQUIRE(pair.occurrence_b.containing_assembly_occurrence.occurrence_path.size() == 1U);
  CHECK(pair.occurrence_b.containing_assembly_occurrence.occurrence_path.front().value() ==
        "subassembly.left");

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
  CHECK(project.find_cross_hierarchy_joint(AssemblyJointId("joint.cross.sweep"))
            ->coordinate_deg() == 0.0);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == identity_rigid_transform());
}

TEST_CASE("Sampled Revolute sweep validates units bounds sample count and selected scope",
          "[geometry][assembly-revolute-sweep]") {
  const Project project = local_sweep_project();
  const AssemblyRevoluteSweepAnalyzer analyzer;

  CHECK(analyzer.analyze(project, local_request(0.0, 90.0, 1U)).has_error());
  CHECK(analyzer
            .analyze(project,
                     local_request(0.0, 90.0, kAssemblyRevoluteSweepMaximumSampleCount + 1U))
            .has_error());
  CHECK(analyzer.analyze(project, local_request(-91.0, 0.0, 2U)).has_error());
  CHECK(analyzer.analyze(project, local_request(0.0, 91.0, 2U)).has_error());

  auto length = Quantity::length_mm(1.0, "sweep");
  REQUIRE(length);
  AssemblyRevoluteSweepRequest wrong_units{
      AssemblyRevoluteSweepJointReference{AssemblyRevoluteSweepJointScope::RootAssemblyLocal,
                                          AssemblyJointId("joint.local.sweep")},
      length.value(), angle(0.0), 2U};
  CHECK(analyzer.analyze(project, wrong_units).has_error());

  AssemblyRevoluteSweepRequest wrong_scope{
      AssemblyRevoluteSweepJointReference{AssemblyRevoluteSweepJointScope::ProjectCrossHierarchy,
                                          AssemblyJointId("joint.local.sweep")},
      angle(0.0), angle(10.0), 2U};
  CHECK(analyzer.analyze(project, wrong_scope).has_error());
}

TEST_CASE("Sampled Revolute sweep fails closed with sample context when contact analysis fails",
          "[geometry][assembly-revolute-sweep]") {
  AssemblyRevoluteSweepAnalysisOptions options;
  options.contact.touching_tolerance_mm = -1.0;
  auto result = AssemblyRevoluteSweepAnalyzer{}.analyze(
      local_sweep_project(), local_request(0.0, 10.0, 2U), options);
  REQUIRE_FALSE(result);
  CHECK(result.error().object_id() == "geometry.assembly_revolute_sweep_analyzer");
  CHECK(result.error().message().find("sample 0 at 0 deg contact analysis failed") !=
        std::string::npos);
}
