#include "blcad/geometry/assembly_cross_hierarchy_constraint_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

Parameter length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument solver_part() {
  auto part = PartDocument::create(DocumentId("part.solver_plate"), "SolverPlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(part.value().add_parameter(
      length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto base = Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(base);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base.value()));

  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));

  auto hole = Sketch::create(SketchId("sketch.hole"), "HoleSketch", DatumPlaneId("datum.xy"));
  REQUIRE(hole);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("part.hole_diameter"),
                                      Point2{4.0, -6.0});
  REQUIRE(circle);
  REQUIRE(hole.value().add_profile(circle.value()));
  REQUIRE(part.value().add_sketch(hole.value()));

  auto hole_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"),
      FeatureId("feature.base_extrude"), SubtractiveExtrudeDepth::ThroughAll,
      ExtrudeDirection::SketchNormal);
  REQUIRE(hole_feature);
  REQUIRE(part.value().add_feature(hole_feature.value()));
  return part.value();
}

ComponentInstance component(
    const char* id, ComponentGroundingState grounding,
    RigidTransform transform = identity_rigid_transform(),
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.solver_plate"), ComponentVisibility::Visible,
      suppression, grounding, transform);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance occurrence(
    const char* id, const char* child, RigidTransform transform,
    ComponentSuppressionState suppression = ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId(child), ComponentVisibility::Visible, suppression,
      transform);
  REQUIRE(instance);
  return instance.value();
}

AssemblyHierarchyConstraintEndpoint endpoint(std::initializer_list<const char*> path,
                                             const char* component_id,
                                             const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path) {
    occurrence_path.emplace_back(id);
  }
  auto value = AssemblyHierarchyConstraintEndpoint::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(value);
  return value.value();
}

Quantity distance(double value_mm, const char* id) {
  auto value = Quantity::length_mm(value_mm, id);
  REQUIRE(value);
  return value.value();
}

Quantity angle(double value_deg, const char* id) {
  auto value = Quantity::angle_deg(value_deg, id);
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraint cross_constraint(
    const char* id, AssemblyConstraintType type, AssemblyHierarchyConstraintEndpoint target_a,
    AssemblyHierarchyConstraintEndpoint target_b,
    std::optional<Quantity> distance_value = std::nullopt,
    std::optional<Quantity> angle_value = std::nullopt,
    AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyHierarchyConstraint::create(
      AssemblyConstraintId(id), id, type, std::move(target_a), std::move(target_b), state,
      std::move(distance_value), std::move(angle_value));
  REQUIRE(constraint);
  return constraint.value();
}

AssemblyConstraintTarget local_target(const char* component_id, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint local_mate(const char* id, const char* component_a, const char* reference_a,
                              const char* component_b, const char* reference_b) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Mate,
      local_target(component_a, reference_a), local_target(component_b, reference_b));
  REQUIRE(constraint);
  return constraint.value();
}

Project single_child_project(
    RigidTransform child_boundary, ComponentGroundingState root_grounding,
    ComponentGroundingState child_grounding,
    RigidTransform child_component_transform = identity_rigid_transform(), bool repeated = false,
    RigidTransform repeated_boundary = identity_rigid_transform()) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(root.value().add_component_instance(component("component.root", root_grounding)));
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.left", "assembly.child", child_boundary)));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(
        occurrence("subassembly.right", "assembly.child", repeated_boundary)));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", child_grounding, child_component_transform)));

  auto project = Project::create(DocumentId("project.cross_solver"), "CrossSolver", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

void add_cross(Project& project, AssemblyHierarchyConstraint constraint) {
  REQUIRE(project.add_cross_hierarchy_constraint(std::move(constraint)));
  REQUIRE(project.validate_assembly_structure());
}

AssemblyCrossHierarchySolveGroup only_group(const Project& project) {
  auto graph = AssemblyCrossHierarchyConstraintGraph::build(project);
  REQUIRE(graph);
  REQUIRE(graph.value().solve_group_count() == 1U);
  return graph.value().solve_groups().front();
}

AssemblyCrossHierarchySolveResult solve_only_group(const Project& project) {
  const AssemblyCrossHierarchyConstraintSolver solver;
  auto result = solver.solve(project, only_group(project));
  REQUIRE(result);
  return result.value();
}

Project repeated_occurrence_project(bool reverse_relationship_insertion = false) {
  Project project = single_child_project(
      identity_rigid_transform(), ComponentGroundingState::Grounded,
      ComponentGroundingState::Free, identity_rigid_transform(), true,
      RigidTransform{Vector3{0.0, 0.0, 10.0}, Vector3{}});

  std::vector<AssemblyHierarchyConstraint> constraints;
  constraints.push_back(cross_constraint(
      "constraint.cross.anchor", AssemblyConstraintType::Distance,
      endpoint({}, "component.root", "feature.base_extrude.top"),
      endpoint({"subassembly.left"}, "component.child", "feature.base_extrude.top"),
      distance(20.0, "constraint.cross.anchor")));
  constraints.push_back(cross_constraint(
      "constraint.cross.same_authority", AssemblyConstraintType::Distance,
      endpoint({"subassembly.left"}, "component.child", "feature.base_extrude.top"),
      endpoint({"subassembly.right"}, "component.child", "feature.base_extrude.top"),
      distance(10.0, "constraint.cross.same_authority")));
  if (reverse_relationship_insertion) {
    std::reverse(constraints.begin(), constraints.end());
  }
  for (AssemblyHierarchyConstraint& constraint : constraints) {
    add_cross(project, std::move(constraint));
  }
  return project;
}

} // namespace

TEST_CASE("Cross-hierarchy numeric solver flattens all five relationship families",
          "[geometry][assembly-cross-hierarchy-solver]") {
  SECTION("Mate") {
    Project project = single_child_project(
        RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}},
        ComponentGroundingState::Grounded, ComponentGroundingState::Grounded);
    add_cross(project, cross_constraint(
                           "constraint.cross.mate", AssemblyConstraintType::Mate,
                           endpoint({}, "component.root", "feature.base_extrude.top"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.base_extrude.bottom")));
    const auto result = solve_only_group(project);
    CHECK(result.converged());
    CHECK(result.residual_summary.residual_component_count == 4U);
    CHECK(result.residual_summary.final_rms == Approx(0.0).margin(1.0e-12));
    CHECK(result.proposed_transforms.empty());
  }

  SECTION("Distance") {
    Project project = single_child_project(
        RigidTransform{Vector3{0.0, 0.0, 12.0}, Vector3{}},
        ComponentGroundingState::Grounded, ComponentGroundingState::Grounded);
    add_cross(project, cross_constraint(
                           "constraint.cross.distance", AssemblyConstraintType::Distance,
                           endpoint({}, "component.root", "feature.base_extrude.top"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.base_extrude.top"),
                           distance(12.0, "constraint.cross.distance")));
    const auto result = solve_only_group(project);
    CHECK(result.converged());
    CHECK(result.residual_summary.residual_component_count == 4U);
    CHECK(result.residual_summary.final_rms == Approx(0.0).margin(1.0e-12));
  }

  SECTION("Angle") {
    Project project = single_child_project(
        RigidTransform{Vector3{}, Vector3{90.0, 0.0, 0.0}},
        ComponentGroundingState::Grounded, ComponentGroundingState::Grounded);
    add_cross(project, cross_constraint(
                           "constraint.cross.angle", AssemblyConstraintType::Angle,
                           endpoint({}, "component.root", "feature.base_extrude.top"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.base_extrude.top"),
                           std::nullopt, angle(90.0, "constraint.cross.angle")));
    const auto result = solve_only_group(project);
    CHECK(result.converged());
    CHECK(result.residual_summary.residual_component_count == 1U);
    CHECK(result.residual_summary.final_rms == Approx(0.0).margin(1.0e-12));
  }

  SECTION("Concentric") {
    Project project = single_child_project(
        RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}},
        ComponentGroundingState::Grounded, ComponentGroundingState::Grounded);
    add_cross(project, cross_constraint(
                           "constraint.cross.concentric", AssemblyConstraintType::Concentric,
                           endpoint({}, "component.root", "feature.hole.axis"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.hole.axis")));
    const auto result = solve_only_group(project);
    CHECK(result.converged());
    CHECK(result.residual_summary.residual_component_count == 6U);
    CHECK(result.residual_summary.final_rms == Approx(0.0).margin(1.0e-12));
  }

  SECTION("Insert") {
    Project project = single_child_project(identity_rigid_transform(),
                                           ComponentGroundingState::Grounded,
                                           ComponentGroundingState::Grounded);
    add_cross(project, cross_constraint(
                           "constraint.cross.insert", AssemblyConstraintType::Insert,
                           endpoint({}, "component.root", "feature.hole.seat"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.hole.seat")));
    const auto result = solve_only_group(project);
    CHECK(result.converged());
    CHECK(result.residual_summary.residual_component_count == 7U);
    CHECK(result.residual_summary.final_rms == Approx(0.0).margin(1.0e-12));
  }
}

TEST_CASE("Cross-hierarchy numeric solver converges through authority-scoped variables",
          "[geometry][assembly-cross-hierarchy-solver]") {
  Project project = single_child_project(identity_rigid_transform(),
                                         ComponentGroundingState::Grounded,
                                         ComponentGroundingState::Free);
  add_cross(project, cross_constraint(
                         "constraint.cross.distance", AssemblyConstraintType::Distance,
                         endpoint({}, "component.root", "feature.base_extrude.top"),
                         endpoint({"subassembly.left"}, "component.child",
                                  "feature.base_extrude.top"),
                         distance(20.0, "constraint.cross.distance")));

  const RigidTransform source_child = project.find_assembly_document(DocumentId("assembly.child"))
                                          ->find_component_instance(
                                              ComponentInstanceId("component.child"))
                                          ->transform();
  const RigidTransform source_boundary =
      project.assembly()
          .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
          ->transform();

  const auto result = solve_only_group(project);
  REQUIRE(result.converged());
  REQUIRE(result.authority_snapshots.size() == 2U);
  REQUIRE(result.fixed_authorities.size() == 1U);
  REQUIRE(result.proposed_transforms.size() == 1U);

  const ComponentTransformAuthority expected_root_authority{
      DocumentId("assembly.root"), ComponentInstanceId("component.root")};
  const ComponentTransformAuthority expected_child_authority{
      DocumentId("assembly.child"), ComponentInstanceId("component.child")};
  CHECK(result.fixed_authorities.front() == expected_root_authority);
  CHECK(result.proposed_transforms.front().authority == expected_child_authority);
  CHECK(result.proposed_transforms.front().proposed_transform.translation_mm.z ==
        Approx(20.0).margin(1.0e-5));
  CHECK(result.residual_summary.final_rms <= 1.0e-8);

  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == source_child);
  CHECK(project.assembly()
            .find_subassembly_instance(SubassemblyInstanceId("subassembly.left"))
            ->transform() == source_boundary);
}

TEST_CASE("Cross-hierarchy numeric solver evaluates nested endpoints through exact parent chains",
          "[geometry][assembly-cross-hierarchy-solver]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.outer", "assembly.outer",
      RigidTransform{Vector3{0.0, 0.0, 5.0}, Vector3{}})));

  auto outer = AssemblyDocument::create(DocumentId("assembly.outer"), "Outer");
  REQUIRE(outer);
  REQUIRE(outer.value().add_subassembly_instance(occurrence(
      "subassembly.inner", "assembly.nested",
      RigidTransform{Vector3{0.0, 0.0, 7.0}, Vector3{}})));

  auto nested = AssemblyDocument::create(DocumentId("assembly.nested"), "Nested");
  REQUIRE(nested);
  REQUIRE(nested.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(nested.value().add_component_instance(
      component("component.leaf", ComponentGroundingState::Free)));

  auto project = Project::create(DocumentId("project.nested_solver"), "NestedSolver", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(outer.value()));
  REQUIRE(project.value().add_child_assembly_document(nested.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  add_cross(project.value(), cross_constraint(
                                 "constraint.cross.nested", AssemblyConstraintType::Distance,
                                 endpoint({}, "component.root", "feature.base_extrude.top"),
                                 endpoint({"subassembly.outer", "subassembly.inner"},
                                          "component.leaf", "feature.base_extrude.top"),
                                 distance(20.0, "constraint.cross.nested")));

  const auto result = solve_only_group(project.value());
  REQUIRE(result.converged());
  REQUIRE(result.proposed_transforms.size() == 1U);
  CHECK(result.proposed_transforms.front().authority.assembly_document.value() ==
        "assembly.nested");
  CHECK(result.proposed_transforms.front().authority.component_instance.value() ==
        "component.leaf");
  CHECK(result.proposed_transforms.front().proposed_transform.translation_mm.z ==
        Approx(8.0).margin(1.0e-5));
}

TEST_CASE("Cross-hierarchy numeric solver shares one variable block across repeated occurrences",
          "[geometry][assembly-cross-hierarchy-solver]") {
  const Project project = repeated_occurrence_project();
  const auto group = only_group(project);
  REQUIRE(group.authorities.size() == 2U);

  const AssemblyCrossHierarchyConstraintSolver solver;
  auto result = solver.solve(project, group);
  REQUIRE(result);
  REQUIRE(result.value().converged());
  REQUIRE(result.value().proposed_transforms.size() == 1U);

  const ComponentTransformAuthority expected_child_authority{
      DocumentId("assembly.child"), ComponentInstanceId("component.child")};
  CHECK(result.value().proposed_transforms.front().authority == expected_child_authority);
  CHECK(result.value().proposed_transforms.front().proposed_transform.translation_mm.z ==
        Approx(20.0).margin(1.0e-5));
  CHECK(result.value().residual_summary.residual_component_count == 8U);
  CHECK(result.value().relationships == group.relationships);

  const Project reverse = repeated_occurrence_project(true);
  auto reverse_result = solver.solve(reverse, only_group(reverse));
  REQUIRE(reverse_result);
  CHECK(reverse_result.value() == result.value());
}

TEST_CASE("Cross-hierarchy numeric solver combines local and cross residuals in one group",
          "[geometry][assembly-cross-hierarchy-solver]") {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Grounded)));
  REQUIRE(root.value().add_subassembly_instance(occurrence(
      "subassembly.left", "assembly.child",
      RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.solver_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free)));
  REQUIRE(child.value().add_component_instance(component(
      "component.anchor", ComponentGroundingState::Grounded,
      RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}})));
  REQUIRE(child.value().add_constraint(local_mate(
      "constraint.local.child_anchor", "component.child", "feature.base_extrude.top",
      "component.anchor", "feature.base_extrude.bottom")));

  auto project = Project::create(DocumentId("project.mixed_solver"), "MixedSolver", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(solver_part()));
  add_cross(project.value(), cross_constraint(
                                 "constraint.cross.root_child", AssemblyConstraintType::Mate,
                                 endpoint({}, "component.root", "feature.base_extrude.top"),
                                 endpoint({"subassembly.left"}, "component.child",
                                          "feature.base_extrude.bottom")));

  const auto group = only_group(project.value());
  REQUIRE(group.relationships.size() == 2U);
  CHECK(std::holds_alternative<AssemblyLocalRelationshipIdentity>(group.relationships[0]));
  CHECK(std::holds_alternative<AssemblyProjectCrossHierarchyRelationshipIdentity>(
      group.relationships[1]));

  const auto result = solve_only_group(project.value());
  CHECK(result.converged());
  CHECK(result.residual_summary.residual_component_count == 8U);
  REQUIRE(result.proposed_transforms.size() == 1U);
  const ComponentTransformAuthority expected_child_authority{
      DocumentId("assembly.child"), ComponentInstanceId("component.child")};
  CHECK(result.proposed_transforms.front().authority == expected_child_authority);
}

TEST_CASE("Cross-hierarchy numeric solver enforces current group and grounded-reference contracts",
          "[geometry][assembly-cross-hierarchy-solver]") {
  SECTION("No grounded authority") {
    Project project = single_child_project(identity_rigid_transform(),
                                           ComponentGroundingState::Free,
                                           ComponentGroundingState::Free);
    add_cross(project, cross_constraint(
                           "constraint.cross.distance", AssemblyConstraintType::Distance,
                           endpoint({}, "component.root", "feature.base_extrude.top"),
                           endpoint({"subassembly.left"}, "component.child",
                                    "feature.base_extrude.top"),
                           distance(20.0, "constraint.cross.distance")));
    const AssemblyCrossHierarchyConstraintSolver solver;
    CHECK(solver.solve(project, only_group(project)).has_error());
  }

  SECTION("Suppression invalidates a previously selected group") {
    Project project = repeated_occurrence_project();
    const auto old_group = only_group(project);
    REQUIRE(project.assembly().set_subassembly_instance_suppression_state(
        SubassemblyInstanceId("subassembly.left"), ComponentSuppressionState::Suppressed));
    const AssemblyCrossHierarchyConstraintSolver solver;
    CHECK(solver.solve(project, old_group).has_error());
  }

  SECTION("Mutated group identity is rejected") {
    const Project project = repeated_occurrence_project();
    auto invalid_group = only_group(project);
    REQUIRE_FALSE(invalid_group.relationships.empty());
    invalid_group.relationships.pop_back();
    const AssemblyCrossHierarchyConstraintSolver solver;
    CHECK(solver.solve(project, invalid_group).has_error());
  }
}
