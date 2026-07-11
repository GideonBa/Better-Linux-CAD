#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace cross_hierarchy_relationship_tests {

using Catch::Approx;

constexpr double kTolerance = 1.0e-12;

Parameter length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument hole_plate() {
  auto part = PartDocument::create(DocumentId("part.hole_plate"), "HolePlate");
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
  auto circle = CircleProfile::create(ProfileId("profile.hole"),
                                      ParameterId("part.hole_diameter"), Point2{4.0, -6.0});
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

ComponentInstance component(const char* id, RigidTransform transform = identity_rigid_transform()) {
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.hole_plate"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
  REQUIRE(instance);
  return instance.value();
}

SubassemblyInstance child_occurrence(const char* id, RigidTransform transform,
                                     ComponentSuppressionState suppression =
                                         ComponentSuppressionState::Active) {
  auto instance = SubassemblyInstance::create(
      SubassemblyInstanceId(id), id, DocumentId("assembly.child"), ComponentVisibility::Visible,
      suppression, transform);
  REQUIRE(instance);
  return instance.value();
}

Project project_with_child(RigidTransform child_transform, bool repeated = false,
                           ComponentSuppressionState suppression =
                               ComponentSuppressionState::Active) {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.hole_plate")));
  REQUIRE(root.value().add_component_instance(component("component.root")));
  REQUIRE(root.value().add_subassembly_instance(
      child_occurrence("subassembly.left", child_transform, suppression)));
  if (repeated) {
    REQUIRE(root.value().add_subassembly_instance(child_occurrence(
        "subassembly.right", RigidTransform{Vector3{-100.0, 0.0, 0.0}, Vector3{}})));
  }

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.hole_plate")));
  REQUIRE(child.value().add_component_instance(component("component.child")));

  auto project = Project::create(DocumentId("project.cross_hierarchy"), "CrossHierarchy",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(hole_plate()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

std::vector<SubassemblyInstanceId> path(std::initializer_list<const char*> ids) {
  std::vector<SubassemblyInstanceId> result;
  result.reserve(ids.size());
  for (const char* id : ids) {
    result.emplace_back(id);
  }
  return result;
}

AssemblyHierarchyConstraintTarget target(std::vector<SubassemblyInstanceId> occurrence_path,
                                         const char* component_id,
                                         const char* semantic_reference) {
  auto result = AssemblyHierarchyConstraintTarget::create(
      std::move(occurrence_path), ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(result);
  return result.value();
}

AssemblyHierarchyConstraintQuery query(
    const char* id, AssemblyConstraintType type, AssemblyHierarchyConstraintTarget target_a,
    AssemblyHierarchyConstraintTarget target_b, std::optional<Quantity> distance = std::nullopt,
    std::optional<Quantity> angle = std::nullopt) {
  auto result = AssemblyHierarchyConstraintQuery::create(
      AssemblyConstraintId(id), type, std::move(target_a), std::move(target_b), distance, angle);
  REQUIRE(result);
  return result.value();
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

void check_vector(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

void check_point(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

} // namespace cross_hierarchy_relationship_tests

namespace ch = cross_hierarchy_relationship_tests;

TEST_CASE("Cross-hierarchy relationship endpoints and queries validate stable identity",
          "[geometry][assembly-cross-hierarchy]") {
  CHECK(AssemblyHierarchyConstraintTarget::create(
            {SubassemblyInstanceId("")}, ComponentInstanceId("component.child"),
            "feature.base_extrude.top")
            .has_error());
  CHECK(AssemblyHierarchyConstraintTarget::create({}, ComponentInstanceId(""),
                                                   "feature.base_extrude.top")
            .has_error());
  CHECK(AssemblyHierarchyConstraintTarget::create({}, ComponentInstanceId("component.root"), "")
            .has_error());

  const auto root = ch::target({}, "component.root", "feature.base_extrude.top");
  const auto child = ch::target(ch::path({"subassembly.left"}), "component.child",
                                "feature.base_extrude.top");
  CHECK(root.occurrence_path().empty());
  CHECK(child.occurrence_path() == ch::path({"subassembly.left"}));
  CHECK(child.component_instance().value() == "component.child");
  CHECK(child.semantic_reference() == "feature.base_extrude.top");

  CHECK(AssemblyHierarchyConstraintQuery::create(
            AssemblyConstraintId("query.distance.bad"), AssemblyConstraintType::Distance, root,
            child)
            .has_error());
  CHECK(AssemblyHierarchyConstraintQuery::create(
            AssemblyConstraintId("query.angle.bad"), AssemblyConstraintType::Angle, root, child,
            ch::distance(10.0, "query.angle.bad"))
            .has_error());
  CHECK(AssemblyHierarchyConstraintQuery::create(
            AssemblyConstraintId("query.insert.bad"), AssemblyConstraintType::Insert, root, child,
            ch::distance(10.0, "query.insert.bad"))
            .has_error());
}

TEST_CASE("Cross-hierarchy target resolution preserves exact repeated-occurrence identity",
          "[geometry][assembly-cross-hierarchy]") {
  const Project project = ch::project_with_child(
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{}}, true);
  const AssemblyHierarchyConstraintTargetResolver resolver;

  const auto left = resolver.resolve_axis(
      project, ch::target(ch::path({"subassembly.left"}), "component.child", "feature.hole.axis"));
  const auto right = resolver.resolve_axis(
      project, ch::target(ch::path({"subassembly.right"}), "component.child", "feature.hole.axis"));
  const auto root = resolver.resolve_planar(
      project, ch::target({}, "component.root", "feature.base_extrude.top"));

  REQUIRE(left);
  REQUIRE(right);
  REQUIRE(root);
  CHECK(left.value().assembly_document.value() == "assembly.child");
  CHECK(right.value().assembly_document.value() == "assembly.child");
  CHECK(left.value().component_instance == right.value().component_instance);
  CHECK(left.value().source_feature.value() == "feature.hole");
  CHECK(left.value().source_profile.value() == "profile.hole");
  CHECK(left.value().occurrence_path == ch::path({"subassembly.left"}));
  CHECK(right.value().occurrence_path == ch::path({"subassembly.right"}));
  ch::check_point(left.value().axis.origin, Point3{104.0, -6.0, 0.0});
  ch::check_point(right.value().axis.origin, Point3{-96.0, -6.0, 0.0});
  ch::check_vector(left.value().axis.direction, Vector3{0.0, 0.0, 1.0});
  CHECK(root.value().occurrence_path.empty());
  CHECK(root.value().assembly_document.value() == "assembly.root");
  ch::check_point(root.value().plane.origin, Point3{0.0, 0.0, 8.0});
}

TEST_CASE("Cross-hierarchy equation builder reuses all five residual family semantics",
          "[geometry][assembly-cross-hierarchy]") {
  const AssemblyHierarchyConstraintEquationBuilder builder;

  SECTION("Mate") {
    const Project project = ch::project_with_child(
        RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}});
    const auto equation = builder.build(
        project, ch::query("query.mate", AssemblyConstraintType::Mate,
                           ch::target({}, "component.root", "feature.base_extrude.top"),
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.base_extrude.bottom")));
    REQUIRE(equation);
    REQUIRE(std::holds_alternative<PlanarMateResidualDescriptor>(equation.value().residual));
    const auto& residual = std::get<PlanarMateResidualDescriptor>(equation.value().residual);
    ch::check_vector(residual.normal_opposition, Vector3{});
    CHECK(residual.signed_separation_mm == Approx(0.0).margin(ch::kTolerance));
  }

  SECTION("Distance keeps target A to target B sign") {
    const Project project = ch::project_with_child(
        RigidTransform{Vector3{0.0, 0.0, 12.0}, Vector3{}});
    const auto forward = builder.build(
        project, ch::query("query.distance.forward", AssemblyConstraintType::Distance,
                           ch::target({}, "component.root", "feature.base_extrude.top"),
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.base_extrude.top"),
                           ch::distance(12.0, "query.distance.forward")));
    const auto reverse = builder.build(
        project, ch::query("query.distance.reverse", AssemblyConstraintType::Distance,
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.base_extrude.top"),
                           ch::target({}, "component.root", "feature.base_extrude.top"),
                           ch::distance(12.0, "query.distance.reverse")));
    REQUIRE(forward);
    REQUIRE(reverse);
    const auto& forward_residual =
        std::get<PlanarDistanceResidualDescriptor>(forward.value().residual);
    const auto& reverse_residual =
        std::get<PlanarDistanceResidualDescriptor>(reverse.value().residual);
    CHECK(forward_residual.signed_separation_mm == Approx(12.0).margin(ch::kTolerance));
    CHECK(forward_residual.distance_residual_mm == Approx(0.0).margin(ch::kTolerance));
    CHECK(reverse_residual.signed_separation_mm == Approx(-12.0).margin(ch::kTolerance));
    CHECK(reverse_residual.distance_residual_mm == Approx(-24.0).margin(ch::kTolerance));
  }

  SECTION("Angle") {
    const Project project = ch::project_with_child(
        RigidTransform{Vector3{}, Vector3{90.0, 0.0, 0.0}});
    const auto equation = builder.build(
        project, ch::query("query.angle", AssemblyConstraintType::Angle,
                           ch::target({}, "component.root", "feature.base_extrude.top"),
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.base_extrude.top"),
                           std::nullopt, ch::angle(90.0, "query.angle")));
    REQUIRE(equation);
    const auto& residual = std::get<PlanarAngleResidualDescriptor>(equation.value().residual);
    CHECK(residual.normal_dot == Approx(0.0).margin(ch::kTolerance));
    CHECK(residual.angle_alignment == Approx(0.0).margin(ch::kTolerance));
  }

  SECTION("Concentric") {
    const Project project = ch::project_with_child(
        RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}});
    const auto equation = builder.build(
        project, ch::query("query.concentric", AssemblyConstraintType::Concentric,
                           ch::target({}, "component.root", "feature.hole.axis"),
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.hole.axis")));
    REQUIRE(equation);
    REQUIRE(std::holds_alternative<ConcentricResidualDescriptor>(equation.value().residual));
    const auto& residual = std::get<ConcentricResidualDescriptor>(equation.value().residual);
    ch::check_vector(residual.direction_parallelism, Vector3{});
    ch::check_vector(residual.axis_offset_mm, Vector3{});
  }

  SECTION("Insert") {
    const Project project = ch::project_with_child(identity_rigid_transform());
    const auto equation = builder.build(
        project, ch::query("query.insert", AssemblyConstraintType::Insert,
                           ch::target({}, "component.root", "feature.hole.seat"),
                           ch::target(ch::path({"subassembly.left"}), "component.child",
                                      "feature.hole.seat")));
    REQUIRE(equation);
    REQUIRE(std::holds_alternative<InsertResidualDescriptor>(equation.value().residual));
    const auto& residual = std::get<InsertResidualDescriptor>(equation.value().residual);
    ch::check_vector(residual.direction_parallelism, Vector3{});
    ch::check_vector(residual.axis_offset_mm, Vector3{});
    CHECK(residual.signed_seating_separation_mm == Approx(0.0).margin(ch::kTolerance));
  }
}

TEST_CASE("Cross-hierarchy residuals distinguish repeated occurrences of one child document",
          "[geometry][assembly-cross-hierarchy]") {
  const Project project = ch::project_with_child(
      RigidTransform{Vector3{100.0, 0.0, 0.0}, Vector3{}}, true);
  const auto relationship = ch::query(
      "query.repeated.concentric", AssemblyConstraintType::Concentric,
      ch::target(ch::path({"subassembly.left"}), "component.child", "feature.hole.axis"),
      ch::target(ch::path({"subassembly.right"}), "component.child", "feature.hole.axis"));

  const AssemblyHierarchyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, relationship);
  REQUIRE(equation);
  const auto& target_a =
      std::get<AssemblyHierarchyAxisConstraintTargetDescriptor>(equation.value().target_a);
  const auto& target_b =
      std::get<AssemblyHierarchyAxisConstraintTargetDescriptor>(equation.value().target_b);
  const auto& residual = std::get<ConcentricResidualDescriptor>(equation.value().residual);
  CHECK(target_a.occurrence_path == ch::path({"subassembly.left"}));
  CHECK(target_b.occurrence_path == ch::path({"subassembly.right"}));
  CHECK(target_a.assembly_document == target_b.assembly_document);
  ch::check_vector(residual.direction_parallelism, Vector3{});
  ch::check_vector(residual.axis_offset_mm, Vector3{0.0, 200.0, 0.0});
}

TEST_CASE("Cross-hierarchy equation construction fails closed and remains read only",
          "[geometry][assembly-cross-hierarchy]") {
  Project project = ch::project_with_child(
      RigidTransform{Vector3{0.0, 0.0, 8.0}, Vector3{}});
  const RigidTransform root_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform();
  const RigidTransform child_before = project.find_assembly_document(DocumentId("assembly.child"))
                                          ->find_component_instance(ComponentInstanceId("component.child"))
                                          ->transform();
  const std::size_t root_constraint_count = project.assembly().constraint_count();
  const std::size_t child_constraint_count =
      project.find_assembly_document(DocumentId("assembly.child"))->constraint_count();

  const AssemblyHierarchyConstraintEquationBuilder builder;
  const auto relationship = ch::query(
      "query.read_only", AssemblyConstraintType::Mate,
      ch::target({}, "component.root", "feature.base_extrude.top"),
      ch::target(ch::path({"subassembly.left"}), "component.child",
                 "feature.base_extrude.bottom"));
  const auto first = builder.build(project, relationship);
  const auto second = builder.build(project, relationship);
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());

  CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.root"))->transform() ==
        root_before);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))
            ->find_component_instance(ComponentInstanceId("component.child"))
            ->transform() == child_before);
  CHECK(project.assembly().constraint_count() == root_constraint_count);
  CHECK(project.find_assembly_document(DocumentId("assembly.child"))->constraint_count() ==
        child_constraint_count);

  CHECK(builder.build(
            project, ch::query("query.missing.path", AssemblyConstraintType::Mate,
                               ch::target({}, "component.root", "feature.base_extrude.top"),
                               ch::target(ch::path({"subassembly.missing"}), "component.child",
                                          "feature.base_extrude.bottom")))
            .has_error());
  CHECK(builder.build(
            project, ch::query("query.missing.component", AssemblyConstraintType::Mate,
                               ch::target({}, "component.root", "feature.base_extrude.top"),
                               ch::target(ch::path({"subassembly.left"}), "component.missing",
                                          "feature.base_extrude.bottom")))
            .has_error());
  CHECK(builder.build(
            project, ch::query("query.wrong.family", AssemblyConstraintType::Concentric,
                               ch::target({}, "component.root", "feature.base_extrude.top"),
                               ch::target(ch::path({"subassembly.left"}), "component.child",
                                          "feature.hole.axis")))
            .has_error());
}
