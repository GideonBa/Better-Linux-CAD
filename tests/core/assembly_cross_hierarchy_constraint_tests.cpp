#include "blcad/core/project.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>
#include <vector>

using namespace blcad;

namespace {

AssemblyHierarchyConstraintEndpoint hierarchy_endpoint(
    std::initializer_list<const char*> path,
    const char* component,
    const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  occurrence_path.reserve(path.size());
  for (const char* occurrence : path) {
    occurrence_path.emplace_back(occurrence);
  }
  auto endpoint = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference);
  REQUIRE(endpoint);
  return endpoint.value();
}

AssemblyHierarchyConstraint hierarchy_constraint(
    const char* id,
    AssemblyConstraintType type,
    AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    AssemblyConstraintState state = AssemblyConstraintState::Active,
    std::optional<Quantity> distance = std::nullopt,
    std::optional<Quantity> angle = std::nullopt) {
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, type, std::move(target_a), std::move(target_b), state,
      std::move(distance), std::move(angle));
  REQUIRE(constraint);
  return constraint.value();
}

ComponentInstance hierarchy_instance(const char* id, double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 2.0, 3.0};
  transform.rotation_deg = Vector3{4.0, 5.0, 6.0};
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance hierarchy_subassembly(const char* id,
                                          const char* referenced_assembly,
                                          double translation_x) {
  RigidTransform transform;
  transform.translation_mm = Vector3{translation_x, 7.0, 8.0};
  transform.rotation_deg = Vector3{9.0, 10.0, 11.0};
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(referenced_assembly),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, transform);
  REQUIRE(instance);
  return instance.value();
}

Project hierarchy_intent_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(root.value().add_component_instance(hierarchy_instance("component.root", 10.0)));
  REQUIRE(root.value().add_subassembly_instance(
      hierarchy_subassembly("subassembly.child", "assembly.child", 100.0)));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(child.value().add_component_instance(hierarchy_instance("component.child", 20.0)));

  auto project = Project::create(DocumentId("project.hierarchy.intent"), "HierarchyIntent",
                                 std::move(root.value()));
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(std::move(child.value())));
  return project.value();
}

Quantity hierarchy_distance(double value_mm, const char* object_id) {
  auto quantity = Quantity::length_mm(value_mm, object_id);
  REQUIRE(quantity);
  return quantity.value();
}

Quantity hierarchy_angle(double value_deg, const char* object_id) {
  auto quantity = Quantity::angle_deg(value_deg, object_id);
  REQUIRE(quantity);
  return quantity.value();
}

} // namespace

TEST_CASE("Cross-hierarchy endpoint intent preserves exact rooted identity",
          "[core][assembly-cross-hierarchy-intent]") {
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {SubassemblyInstanceId("")}, ComponentInstanceId("component.child"),
            "feature.hole.axis")
            .has_error());
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {}, ComponentInstanceId(""), "feature.hole.axis")
            .has_error());
  CHECK(AssemblyHierarchyConstraintEndpoint::create(
            {}, ComponentInstanceId("component.root"), "")
            .has_error());

  const auto root = hierarchy_endpoint({}, "component.root", "feature.base.top");
  CHECK(root.occurrence_path().empty());
  CHECK(root.component_instance().value() == "component.root");
  CHECK(root.semantic_reference() == "feature.base.top");

  const auto nested = hierarchy_endpoint(
      {"subassembly.outer", "subassembly.inner"}, "component.shaft", "feature.bore.axis");
  REQUIRE(nested.occurrence_path().size() == 2U);
  CHECK(nested.occurrence_path()[0].value() == "subassembly.outer");
  CHECK(nested.occurrence_path()[1].value() == "subassembly.inner");
  CHECK(nested.component_instance().value() == "component.shaft");
  CHECK(nested.semantic_reference() == "feature.bore.axis");
}

TEST_CASE("Cross-hierarchy constraint intent reuses all local value-family rules",
          "[core][assembly-cross-hierarchy-intent]") {
  const auto target_a = hierarchy_endpoint({}, "component.root", "feature.base.top");
  const auto target_b = hierarchy_endpoint(
      {"subassembly.child"}, "component.child", "feature.base.bottom");
  const Quantity distance = hierarchy_distance(12.5, "constraint.cross.distance");
  const Quantity angle = hierarchy_angle(35.0, "constraint.cross.angle");
  auto count = Quantity::count(4.0, "constraint.cross.count");
  REQUIRE(count);

  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId(""), "Mate", AssemblyConstraintType::Mate, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.mate"), "", AssemblyConstraintType::Mate,
            target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.distance"), "Distance",
            AssemblyConstraintType::Distance, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.distance"), "Distance",
            AssemblyConstraintType::Distance, target_a, target_b,
            AssemblyConstraintState::Active, count.value())
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.angle"), "Angle",
            AssemblyConstraintType::Angle, target_a, target_b)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.mate"), "Mate", AssemblyConstraintType::Mate,
            target_a, target_b, AssemblyConstraintState::Active, distance)
            .has_error());
  CHECK(AssemblyHierarchyConstraint::create(
            AssemblyConstraintId("constraint.cross.insert"), "Insert",
            AssemblyConstraintType::Insert, target_a, target_b, AssemblyConstraintState::Active,
            std::nullopt, angle)
            .has_error());

  const auto mate = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.mate"), "Mate", AssemblyConstraintType::Mate,
      target_a, target_b);
  const auto concentric = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.concentric"), "Concentric",
      AssemblyConstraintType::Concentric, target_a, target_b, AssemblyConstraintState::Inactive);
  const auto distance_constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.distance"), "Distance",
      AssemblyConstraintType::Distance, target_a, target_b, AssemblyConstraintState::Active,
      distance);
  const auto insert = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.insert"), "Insert", AssemblyConstraintType::Insert,
      target_a, target_b);
  const auto angle_constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId("constraint.cross.angle"), "Angle", AssemblyConstraintType::Angle,
      target_a, target_b, AssemblyConstraintState::Active, std::nullopt, angle);

  REQUIRE(mate);
  REQUIRE(concentric);
  REQUIRE(distance_constraint);
  REQUIRE(insert);
  REQUIRE(angle_constraint);
  CHECK(concentric.value().state() == AssemblyConstraintState::Inactive);
  REQUIRE(distance_constraint.value().distance().has_value());
  CHECK(distance_constraint.value().distance()->millimeters() == 12.5);
  REQUIRE(angle_constraint.value().angle().has_value());
  CHECK(angle_constraint.value().angle()->degrees() == 35.0);
  CHECK(angle_constraint.value().target_a() == target_a);
  CHECK(angle_constraint.value().target_b() == target_b);
}

TEST_CASE("Project owns cross-hierarchy constraints in a separate project-wide id scope",
          "[core][assembly-cross-hierarchy-intent]") {
  auto project = hierarchy_intent_project();

  auto local_target_a = AssemblyConstraintTarget::create(
      ComponentInstanceId("component.root"), "feature.base.top");
  auto local_target_b = AssemblyConstraintTarget::create(
      ComponentInstanceId("component.root"), "feature.base.bottom");
  REQUIRE(local_target_a);
  REQUIRE(local_target_b);
  auto local = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.shared"), "Local", AssemblyConstraintType::Mate,
      std::move(local_target_a.value()), std::move(local_target_b.value()));
  REQUIRE(local);
  REQUIRE(project.assembly().add_constraint(std::move(local.value())));

  const auto cross = hierarchy_constraint(
      "constraint.shared", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.base.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.base.bottom"));
  REQUIRE(project.add_cross_hierarchy_constraint(cross));

  CHECK(project.cross_hierarchy_constraint_count() == 1U);
  CHECK(project.cross_hierarchy_constraints().size() == 1U);
  const AssemblyHierarchyConstraint* stored =
      project.find_cross_hierarchy_constraint(AssemblyConstraintId("constraint.shared"));
  REQUIRE(stored != nullptr);
  CHECK(stored->id().value() == "constraint.shared");
  CHECK(stored->target_b().occurrence_path().size() == 1U);
  CHECK(project.add_cross_hierarchy_constraint(cross).has_error());
  CHECK(project.cross_hierarchy_constraint_count() == 1U);
}

TEST_CASE("Project-level cross-hierarchy intent does not resolve paths components or geometry",
          "[core][assembly-cross-hierarchy-intent]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  auto project = Project::create(DocumentId("project.intent.only"), "IntentOnly",
                                 std::move(root.value()));
  REQUIRE(project);

  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.root_child", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.root.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.child.bottom"))));
  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.left_right", AssemblyConstraintType::Concentric,
      hierarchy_endpoint({"subassembly.left"}, "component.shaft", "feature.bore.axis"),
      hierarchy_endpoint({"subassembly.right"}, "component.shaft", "feature.bore.axis"))));
  REQUIRE(project.value().add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.nested_root", AssemblyConstraintType::Insert,
      hierarchy_endpoint({"subassembly.outer", "subassembly.inner"}, "component.pin",
                         "feature.pin.seat"),
      hierarchy_endpoint({}, "component.root", "feature.root.seat"))));

  CHECK(project.value().cross_hierarchy_constraint_count() == 3U);
  CHECK(project.value().child_assembly_document_count() == 0U);
  CHECK(project.value().assembly().component_instance_count() == 0U);
}

TEST_CASE("Adding cross-hierarchy constraint intent never mutates authored transforms",
          "[core][assembly-cross-hierarchy-intent]") {
  auto project = hierarchy_intent_project();
  const RigidTransform root_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform();
  const RigidTransform child_transform = project
                                             .find_assembly_document(DocumentId("assembly.child"))
                                             ->find_component_instance(
                                                 ComponentInstanceId("component.child"))
                                             ->transform();
  const RigidTransform boundary_transform =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
          ->transform();

  REQUIRE(project.add_cross_hierarchy_constraint(hierarchy_constraint(
      "constraint.cross.mate", AssemblyConstraintType::Mate,
      hierarchy_endpoint({}, "component.root", "feature.base.top"),
      hierarchy_endpoint({"subassembly.child"}, "component.child", "feature.base.bottom"))));

  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.root"))
            ->transform() == root_transform);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == child_transform);
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.child"))
            ->transform() == boundary_transform);
}
