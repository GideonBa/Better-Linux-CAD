#include "blcad/geometry/assembly_constraint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-12;

void check_vector_approx(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

void check_point_approx(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_face_part() {
  auto document = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      document.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));
  return document.value();
}

Project make_two_component_project(const RigidTransform& transform_a,
                                   const RigidTransform& transform_b) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.face"), "FaceAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.face_plate")));

  auto component_a = ComponentInstance::create(
      ComponentInstanceId("component.a"), "Plate A", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, transform_a);
  REQUIRE(component_a);
  REQUIRE(assembly.value().add_component_instance(component_a.value()));

  auto component_b = ComponentInstance::create(
      ComponentInstanceId("component.b"), "Plate B", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, transform_b);
  REQUIRE(component_b);
  REQUIRE(assembly.value().add_component_instance(component_b.value()));

  auto project = Project::create(DocumentId("project.face"), "FaceProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_face_part()));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component_id, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint make_constraint(const char* id,
                                   AssemblyConstraintType type,
                                   AssemblyConstraintTarget target_a,
                                   AssemblyConstraintTarget target_b,
                                   AssemblyConstraintState state = AssemblyConstraintState::Active,
                                   std::optional<Quantity> distance = std::nullopt) {
  auto constraint = AssemblyConstraint::create(AssemblyConstraintId(id), id, type,
                                               std::move(target_a), std::move(target_b), state,
                                               std::move(distance));
  REQUIRE(constraint);
  return constraint.value();
}

Quantity make_distance(double value_mm, const char* id) {
  auto distance = Quantity::length_mm(value_mm, id);
  REQUIRE(distance);
  return distance.value();
}

} // namespace

TEST_CASE("Assembly constraint equation builder constructs satisfied planar Mate residuals",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{15.0, -9.0, 8.0}, Vector3{0.0, 0.0, 90.0}});
  auto constraint = make_constraint(
      "constraint.mate", AssemblyConstraintType::Mate,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.bottom"));
  REQUIRE(project.assembly().add_constraint(constraint));

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, project.assembly().constraints().front());

  REQUIRE(equation);
  CHECK(equation.value().constraint.value() == "constraint.mate");
  CHECK(equation.value().type == AssemblyConstraintType::Mate);
  CHECK(equation.value().target_a.component_instance.value() == "component.a");
  CHECK(equation.value().target_a.semantic_reference == "feature.base_extrude.top");
  CHECK(equation.value().target_b.component_instance.value() == "component.b");
  CHECK(equation.value().target_b.semantic_reference == "feature.base_extrude.bottom");
  check_point_approx(equation.value().target_a.plane.origin, Point3{0.0, 0.0, 8.0});
  check_point_approx(equation.value().target_b.plane.origin, Point3{15.0, -9.0, 8.0});
  check_vector_approx(equation.value().target_a.plane.normal, Vector3{0.0, 0.0, 1.0});
  check_vector_approx(equation.value().target_b.plane.normal, Vector3{0.0, 0.0, -1.0});

  REQUIRE(std::holds_alternative<PlanarMateResidualDescriptor>(equation.value().residual));
  const auto& residual = std::get<PlanarMateResidualDescriptor>(equation.value().residual);
  check_vector_approx(residual.normal_opposition, Vector3{0.0, 0.0, 0.0});
  CHECK(residual.signed_separation_mm == Approx(0.0).margin(kTolerance));
}

TEST_CASE("Assembly constraint equation builder exposes unsatisfied Mate residual components",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{10.0, 20.0, 12.0}, Vector3{0.0, 0.0, 90.0}});
  auto constraint = make_constraint(
      "constraint.mate.unsatisfied", AssemblyConstraintType::Mate,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.top"));
  REQUIRE(project.assembly().add_constraint(constraint));

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, project.assembly().constraints().front());

  REQUIRE(equation);
  check_point_approx(equation.value().target_b.plane.origin, Point3{10.0, 20.0, 20.0});
  check_vector_approx(equation.value().target_b.plane.x_axis, Vector3{0.0, 1.0, 0.0});
  check_vector_approx(equation.value().target_b.plane.y_axis, Vector3{-1.0, 0.0, 0.0});

  const auto& residual = std::get<PlanarMateResidualDescriptor>(equation.value().residual);
  check_vector_approx(residual.normal_opposition, Vector3{0.0, 0.0, 2.0});
  CHECK(residual.signed_separation_mm == Approx(12.0).margin(kTolerance));
}

TEST_CASE("Assembly constraint equation builder constructs signed planar Distance residuals",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{25.0, -15.0, 12.0}, Vector3{0.0, 0.0, 90.0}});
  auto constraint = make_constraint(
      "constraint.distance", AssemblyConstraintType::Distance,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.top"), AssemblyConstraintState::Active,
      make_distance(12.0, "constraint.distance"));
  REQUIRE(project.assembly().add_constraint(constraint));

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, project.assembly().constraints().front());

  REQUIRE(equation);
  CHECK(equation.value().constraint.value() == "constraint.distance");
  CHECK(equation.value().type == AssemblyConstraintType::Distance);
  check_point_approx(equation.value().target_b.plane.origin, Point3{25.0, -15.0, 20.0});
  check_vector_approx(equation.value().target_b.plane.x_axis, Vector3{0.0, 1.0, 0.0});

  REQUIRE(std::holds_alternative<PlanarDistanceResidualDescriptor>(equation.value().residual));
  const auto& residual = std::get<PlanarDistanceResidualDescriptor>(equation.value().residual);
  check_vector_approx(residual.normal_parallelism, Vector3{0.0, 0.0, 0.0});
  CHECK(residual.target_distance_mm == Approx(12.0).margin(kTolerance));
  CHECK(residual.signed_separation_mm == Approx(12.0).margin(kTolerance));
  CHECK(residual.distance_residual_mm == Approx(0.0).margin(kTolerance));
}

TEST_CASE("Assembly constraint equation builder reports nonparallel Distance plane residuals",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{}, Vector3{90.0, 0.0, 0.0}});
  auto constraint = make_constraint(
      "constraint.distance.nonparallel", AssemblyConstraintType::Distance,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.top"), AssemblyConstraintState::Active,
      make_distance(8.0, "constraint.distance.nonparallel"));
  REQUIRE(project.assembly().add_constraint(constraint));

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, project.assembly().constraints().front());

  REQUIRE(equation);
  check_vector_approx(equation.value().target_b.plane.normal, Vector3{0.0, -1.0, 0.0});
  const auto& residual = std::get<PlanarDistanceResidualDescriptor>(equation.value().residual);
  check_vector_approx(residual.normal_parallelism, Vector3{1.0, 0.0, 0.0});
}

TEST_CASE("Planar Distance signed separation follows target A to target B order",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{0.0, 0.0, 12.0}, Vector3{0.0, 0.0, 0.0}});
  auto forward = make_constraint(
      "constraint.distance.forward", AssemblyConstraintType::Distance,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.top"), AssemblyConstraintState::Active,
      make_distance(10.0, "constraint.distance.forward"));
  auto reverse = make_constraint(
      "constraint.distance.reverse", AssemblyConstraintType::Distance,
      make_target("component.b", "feature.base_extrude.top"),
      make_target("component.a", "feature.base_extrude.top"), AssemblyConstraintState::Active,
      make_distance(10.0, "constraint.distance.reverse"));
  REQUIRE(project.assembly().add_constraint(forward));
  REQUIRE(project.assembly().add_constraint(reverse));

  const AssemblyConstraintEquationBuilder builder;
  const auto forward_equation = builder.build(project, project.assembly().constraints()[0]);
  const auto reverse_equation = builder.build(project, project.assembly().constraints()[1]);

  REQUIRE(forward_equation);
  REQUIRE(reverse_equation);
  const auto& forward_residual =
      std::get<PlanarDistanceResidualDescriptor>(forward_equation.value().residual);
  const auto& reverse_residual =
      std::get<PlanarDistanceResidualDescriptor>(reverse_equation.value().residual);
  CHECK(forward_residual.signed_separation_mm == Approx(12.0).margin(kTolerance));
  CHECK(forward_residual.distance_residual_mm == Approx(2.0).margin(kTolerance));
  CHECK(reverse_residual.signed_separation_mm == Approx(-12.0).margin(kTolerance));
  CHECK(reverse_residual.distance_residual_mm == Approx(-22.0).margin(kTolerance));
}

TEST_CASE("Assembly constraint equation builder rejects inactive and Concentric records",
          "[geometry][assembly-equation]") {
  const Project project =
      make_two_component_project(identity_rigid_transform(), identity_rigid_transform());
  const AssemblyConstraintEquationBuilder builder;

  SECTION("inactive") {
    const auto constraint = make_constraint(
        "constraint.inactive", AssemblyConstraintType::Mate,
        make_target("component.a", "feature.base_extrude.top"),
        make_target("component.b", "feature.base_extrude.bottom"),
        AssemblyConstraintState::Inactive);
    const auto equation = builder.build(project, constraint);
    REQUIRE(equation.has_error());
    CHECK(equation.error().message() ==
          "assembly constraint equation construction requires an active constraint");
  }

  SECTION("Concentric") {
    const auto constraint = make_constraint(
        "constraint.concentric", AssemblyConstraintType::Concentric,
        make_target("component.a", "bolt.main_axis"),
        make_target("component.b", "bolt.main_axis"));
    const auto equation = builder.build(project, constraint);
    REQUIRE(equation.has_error());
    CHECK(equation.error().message() ==
          "concentric equation construction requires semantic axis target support");
  }
}

TEST_CASE("Assembly constraint equation builder propagates unsupported target-family errors",
          "[geometry][assembly-equation]") {
  const Project project =
      make_two_component_project(identity_rigid_transform(), identity_rigid_transform());
  const auto constraint = make_constraint(
      "constraint.unsupported", AssemblyConstraintType::Mate,
      make_target("component.a", "feature.base_extrude.edge.top_front"),
      make_target("component.b", "feature.base_extrude.top"));

  const AssemblyConstraintEquationBuilder builder;
  const auto equation = builder.build(project, constraint);

  REQUIRE(equation.has_error());
  CHECK(equation.error().message() == "unsupported assembly semantic reference family");
}

TEST_CASE("Assembly constraint equation construction is deterministic and leaves project intent unchanged",
          "[geometry][assembly-equation]") {
  Project project = make_two_component_project(
      identity_rigid_transform(),
      RigidTransform{Vector3{3.0, 4.0, 8.0}, Vector3{0.0, 0.0, 90.0}});
  auto constraint = make_constraint(
      "constraint.read_only", AssemblyConstraintType::Mate,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.bottom"));
  REQUIRE(project.assembly().add_constraint(constraint));

  const RigidTransform transform_a_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform transform_b_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const std::size_t constraint_count_before = project.assembly().constraint_count();
  const std::string target_a_before =
      project.assembly().constraints().front().target_a().semantic_reference();
  const std::string target_b_before =
      project.assembly().constraints().front().target_b().semantic_reference();
  const std::size_t workplane_count_before =
      project.find_part_document(DocumentId("part.face_plate"))->derived_workplane_count();

  const AssemblyConstraintEquationBuilder builder;
  const auto first = builder.build(project, project.assembly().constraints().front());
  const auto second = builder.build(project, project.assembly().constraints().front());

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.a"))
            ->transform() == transform_a_before);
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == transform_b_before);
  CHECK(project.assembly().constraint_count() == constraint_count_before);
  CHECK(project.assembly().constraints().front().target_a().semantic_reference() ==
        target_a_before);
  CHECK(project.assembly().constraints().front().target_b().semantic_reference() ==
        target_b_before);
  CHECK(project.find_part_document(DocumentId("part.face_plate"))->derived_workplane_count() ==
        workplane_count_before);
}
