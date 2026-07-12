#include "blcad/geometry/assembly_insert_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_rigid_body_solver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-10;

struct ComponentSpec {
  const char* id;
  RigidTransform transform;
  ComponentGroundingState grounding = ComponentGroundingState::Free;
};

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_hole_part(Point2 hole_center = Point2{0.0, 0.0},
                            ExtrudeDirection direction = ExtrudeDirection::SketchNormal,
                            bool use_circle = true) {
  auto part = PartDocument::create(DocumentId("part.hole_plate"), "HolePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(part.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base_sketch.value()));

  auto base_feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));

  auto hole_sketch =
      Sketch::create(SketchId("sketch.hole"), "HoleSketch", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  if (use_circle) {
    auto circle = CircleProfile::create(ProfileId("profile.hole"),
                                        ParameterId("part.hole_diameter"), hole_center);
    REQUIRE(circle);
    REQUIRE(hole_sketch.value().add_profile(circle.value()));
  } else {
    auto rectangle_hole =
        RectangleProfile::create(ProfileId("profile.not_circle"), ParameterId("part.hole_diameter"),
                                 ParameterId("part.hole_diameter"), hole_center);
    REQUIRE(rectangle_hole);
    REQUIRE(hole_sketch.value().add_profile(rectangle_hole.value()));
  }
  REQUIRE(part.value().add_sketch(hole_sketch.value()));

  auto hole_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"), FeatureId("feature.base_extrude"),
      SubtractiveExtrudeDepth::ThroughAll, direction);
  REQUIRE(hole_feature);
  REQUIRE(part.value().add_feature(hole_feature.value()));
  return part.value();
}

Project make_project(std::initializer_list<ComponentSpec> components,
                     Point2 hole_center = Point2{0.0, 0.0},
                     ExtrudeDirection direction = ExtrudeDirection::SketchNormal,
                     bool use_circle = true) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.insert"), "InsertAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.hole_plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.hole_plate"),
        ComponentVisibility::Visible, ComponentSuppressionState::Active, spec.grounding,
        spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.insert"), "InsertProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_hole_part(hole_center, direction, use_circle)));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint make_insert(const char* id, const char* component_a, const char* target_a,
                               const char* component_b, const char* target_b,
                               AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId(id), id, AssemblyConstraintType::Insert,
      make_target(component_a, target_a), make_target(component_b, target_b), state);
  REQUIRE(constraint);
  return constraint.value();
}

void add_insert(Project& project, const char* id = "constraint.insert",
                const char* component_a = "component.a", const char* component_b = "component.b") {
  REQUIRE(project.assembly().add_constraint(
      make_insert(id, component_a, "feature.hole.seat", component_b, "feature.hole.seat")));
}

void check_point(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

void check_vector(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

std::vector<double> flatten(const InsertResidualDescriptor& residual) {
  return {residual.direction_parallelism.x,
          residual.direction_parallelism.y,
          residual.direction_parallelism.z,
          residual.axis_offset_mm.x,
          residual.axis_offset_mm.y,
          residual.axis_offset_mm.z,
          residual.signed_seating_separation_mm};
}

std::vector<double> evaluate_insert(const Project& project) {
  const AssemblyConstraint* constraint =
      project.assembly().find_constraint(AssemblyConstraintId("constraint.insert"));
  REQUIRE(constraint != nullptr);
  const AssemblyInsertConstraintEquationBuilder builder;
  const auto equation = builder.build(project, *constraint);
  REQUIRE(equation);
  return flatten(equation.value().residual);
}

RigidTransform transform_from_variables(const std::vector<double>& values) {
  return RigidTransform{Vector3{values[0], values[1], values[2]},
                        Vector3{values[3], values[4], values[5]}};
}

std::vector<std::vector<double>>
central_difference_jacobian(const Project& project, double translation_step, double rotation_step) {
  const ComponentInstance* component =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"));
  REQUIRE(component != nullptr);
  const RigidTransform& transform = component->transform();
  const std::vector<double> variables{transform.translation_mm.x, transform.translation_mm.y,
                                      transform.translation_mm.z, transform.rotation_deg.x,
                                      transform.rotation_deg.y,   transform.rotation_deg.z};
  const std::vector<double> baseline = evaluate_insert(project);
  std::vector<std::vector<double>> jacobian(baseline.size(),
                                            std::vector<double>(variables.size(), 0.0));

  for (std::size_t column = 0U; column < variables.size(); ++column) {
    const double step = column < 3U ? translation_step : rotation_step;
    std::vector<double> plus = variables;
    std::vector<double> minus = variables;
    plus[column] += step;
    minus[column] -= step;

    Project plus_project = project;
    Project minus_project = project;
    REQUIRE(plus_project.assembly().set_component_instance_transform(
        ComponentInstanceId("component.b"), transform_from_variables(plus)));
    REQUIRE(minus_project.assembly().set_component_instance_transform(
        ComponentInstanceId("component.b"), transform_from_variables(minus)));
    const std::vector<double> plus_residual = evaluate_insert(plus_project);
    const std::vector<double> minus_residual = evaluate_insert(minus_project);

    for (std::size_t row = 0U; row < baseline.size(); ++row) {
      jacobian[row][column] = (plus_residual[row] - minus_residual[row]) / (2.0 * step);
    }
  }
  return jacobian;
}

std::size_t matrix_rank(std::vector<std::vector<double>> matrix, double tolerance) {
  if (matrix.empty())
    return 0U;
  const std::size_t columns = matrix.front().size();
  std::size_t rank = 0U;
  std::size_t column = 0U;
  while (rank < matrix.size() && column < columns) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[rank][column]);
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double magnitude = std::abs(matrix[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }
    if (pivot_magnitude <= tolerance) {
      ++column;
      continue;
    }
    if (pivot_row != rank)
      std::swap(matrix[pivot_row], matrix[rank]);
    const double pivot = matrix[rank][column];
    for (std::size_t row = rank + 1U; row < matrix.size(); ++row) {
      const double factor = matrix[row][column] / pivot;
      for (std::size_t entry = column; entry < columns; ++entry) {
        matrix[row][entry] -= factor * matrix[rank][entry];
      }
    }
    ++rank;
    ++column;
  }
  return rank;
}

std::vector<ComponentInstanceId> group(std::initializer_list<const char*> ids) {
  std::vector<ComponentInstanceId> result;
  for (const char* id : ids)
    result.emplace_back(id);
  return result;
}

} // namespace

TEST_CASE("Assembly target resolver resolves circular feature seating targets",
          "[geometry][assembly-insert]") {
  const AssemblyConstraintTargetResolver resolver;

  SECTION("seat target preserves source identity and couples axis with source workplane") {
    const Project project = make_project(
        {{"component.a", RigidTransform{Vector3{10.0, 20.0, 30.0}, Vector3{}}}}, Point2{4.0, -6.0});
    const auto resolved =
        resolver.resolve_insert(project, make_target("component.a", "feature.hole.seat"));

    REQUIRE(resolved);
    CHECK(resolved.value().component_instance.value() == "component.a");
    CHECK(resolved.value().referenced_part_document.value() == "part.hole_plate");
    CHECK(resolved.value().source_feature.value() == "feature.hole");
    CHECK(resolved.value().source_profile.value() == "profile.hole");
    CHECK(resolved.value().axis == SemanticAxis::Primary);
    CHECK(resolved.value().seating_plane == SemanticSeatingPlane::Primary);
    check_point(resolved.value().local_axis.origin, Point3{4.0, -6.0, 0.0});
    check_vector(resolved.value().local_axis.direction, Vector3{0.0, 0.0, 1.0});
    check_point(resolved.value().local_seating_plane.origin, Point3{4.0, -6.0, 0.0});
    check_vector(resolved.value().local_seating_plane.x_axis, Vector3{1.0, 0.0, 0.0});
    check_vector(resolved.value().local_seating_plane.y_axis, Vector3{0.0, 1.0, 0.0});
    check_vector(resolved.value().local_seating_plane.normal, Vector3{0.0, 0.0, 1.0});
    CHECK((resolved.value().component_transform.translation_mm == Vector3{10.0, 20.0, 30.0}));
  }

  SECTION("opposite extrude direction reverses seat normal and preserves a right-handed frame") {
    const Project project = make_project({{"component.a", identity_rigid_transform()}},
                                         Point2{4.0, -6.0}, ExtrudeDirection::OppositeSketchNormal);
    const auto resolved =
        resolver.resolve_insert(project, make_target("component.a", "feature.hole.seat"));

    REQUIRE(resolved);
    check_vector(resolved.value().local_axis.direction, Vector3{0.0, 0.0, -1.0});
    check_vector(resolved.value().local_seating_plane.x_axis, Vector3{1.0, 0.0, 0.0});
    check_vector(resolved.value().local_seating_plane.y_axis, Vector3{0.0, -1.0, 0.0});
    check_vector(resolved.value().local_seating_plane.normal, Vector3{0.0, 0.0, -1.0});
  }
}

TEST_CASE("Insert equation builder constructs canonical composite residuals",
          "[geometry][assembly-insert]") {
  const AssemblyInsertConstraintEquationBuilder builder;

  SECTION("coincident aligned seat targets satisfy Insert") {
    Project project =
        make_project({{"component.a", identity_rigid_transform()},
                      {"component.b", RigidTransform{Vector3{}, Vector3{0.0, 0.0, 47.0}}}});
    const AssemblyConstraint constraint =
        make_insert("constraint.insert", "component.a", "feature.hole.seat", "component.b",
                    "feature.hole.seat");

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    CHECK(equation.value().constraint.value() == "constraint.insert");
    CHECK(equation.value().target_a.semantic_reference == "feature.hole.seat");
    CHECK(equation.value().target_a.source_feature.value() == "feature.hole");
    CHECK(equation.value().target_a.source_profile.value() == "profile.hole");
    check_vector(equation.value().residual.direction_parallelism, Vector3{});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{});
    CHECK(equation.value().residual.signed_seating_separation_mm == Approx(0.0));
  }

  SECTION("component placement evaluates both axis and seating plane in assembly space") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", RigidTransform{Vector3{10.0, 20.0, 30.0}, Vector3{90.0, 0.0, 0.0}}}},
        Point2{4.0, -6.0});
    const AssemblyConstraint constraint =
        make_insert("constraint.transformed", "component.a", "feature.hole.seat", "component.b",
                    "feature.hole.seat");

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    check_point(equation.value().target_b.axis.origin, Point3{14.0, 20.0, 24.0});
    check_vector(equation.value().target_b.axis.direction, Vector3{0.0, -1.0, 0.0});
    check_point(equation.value().target_b.seating_plane.origin, Point3{14.0, 20.0, 24.0});
    check_vector(equation.value().target_b.seating_plane.normal, Vector3{0.0, -1.0, 0.0});
  }

  SECTION("axial separation is an explicit signed seating residual") {
    Project project =
        make_project({{"component.a", identity_rigid_transform()},
                      {"component.b", RigidTransform{Vector3{0.0, 0.0, 12.0}, Vector3{}}}});
    const AssemblyConstraint forward =
        make_insert("constraint.forward", "component.a", "feature.hole.seat", "component.b",
                    "feature.hole.seat");
    const AssemblyConstraint reverse =
        make_insert("constraint.reverse", "component.b", "feature.hole.seat", "component.a",
                    "feature.hole.seat");

    const auto forward_equation = builder.build(project, forward);
    const auto reverse_equation = builder.build(project, reverse);

    REQUIRE(forward_equation);
    REQUIRE(reverse_equation);
    check_vector(forward_equation.value().residual.axis_offset_mm, Vector3{});
    CHECK(forward_equation.value().residual.signed_seating_separation_mm == Approx(12.0));
    CHECK(reverse_equation.value().residual.signed_seating_separation_mm == Approx(-12.0));
  }

  SECTION("lateral offset and tilt remain separate from seating separation") {
    Project offset_project =
        make_project({{"component.a", identity_rigid_transform()},
                      {"component.b", RigidTransform{Vector3{3.0, 0.0, 0.0}, Vector3{}}}});
    const AssemblyConstraint offset =
        make_insert("constraint.offset", "component.a", "feature.hole.seat", "component.b",
                    "feature.hole.seat");
    const auto offset_equation = builder.build(offset_project, offset);
    REQUIRE(offset_equation);
    check_vector(offset_equation.value().residual.direction_parallelism, Vector3{});
    check_vector(offset_equation.value().residual.axis_offset_mm, Vector3{0.0, -3.0, 0.0});
    CHECK(offset_equation.value().residual.signed_seating_separation_mm == Approx(0.0));

    Project tilt_project =
        make_project({{"component.a", identity_rigid_transform()},
                      {"component.b", RigidTransform{Vector3{}, Vector3{90.0, 0.0, 0.0}}}});
    const AssemblyConstraint tilt = make_insert(
        "constraint.tilt", "component.a", "feature.hole.seat", "component.b", "feature.hole.seat");
    const auto tilt_equation = builder.build(tilt_project, tilt);
    REQUIRE(tilt_equation);
    check_vector(tilt_equation.value().residual.direction_parallelism, Vector3{1.0, 0.0, 0.0});
    CHECK(tilt_equation.value().residual.signed_seating_separation_mm == Approx(0.0));
  }

  SECTION("opposed axis directions remain valid when the seats are coincident") {
    Project project =
        make_project({{"component.a", identity_rigid_transform()},
                      {"component.b", RigidTransform{Vector3{}, Vector3{180.0, 0.0, 0.0}}}});
    const AssemblyConstraint constraint =
        make_insert("constraint.opposed", "component.a", "feature.hole.seat", "component.b",
                    "feature.hole.seat");

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    check_vector(equation.value().target_b.axis.direction, Vector3{0.0, 0.0, -1.0});
    check_vector(equation.value().residual.direction_parallelism, Vector3{});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{});
    CHECK(equation.value().residual.signed_seating_separation_mm == Approx(0.0));
  }
}

TEST_CASE("Insert construction is deterministic read-only and validates target families",
          "[geometry][assembly-insert]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform()},
                    {"component.b", RigidTransform{Vector3{3.0, 0.0, 12.0}, Vector3{}}}});
  const RigidTransform before_b =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const AssemblyConstraint constraint = make_insert(
      "constraint.insert", "component.a", "feature.hole.seat", "component.b", "feature.hole.seat");
  const AssemblyInsertConstraintEquationBuilder builder;

  const auto first = builder.build(project, constraint);
  const auto second = builder.build(project, constraint);

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      before_b);

  const auto inactive =
      make_insert("constraint.inactive", "component.a", "feature.missing.seat", "component.b",
                  "feature.hole.seat", AssemblyConstraintState::Inactive);
  const auto inactive_result = builder.build(project, inactive);
  REQUIRE(inactive_result.has_error());
  CHECK(inactive_result.error().message() ==
        "Insert equation construction requires an active constraint");

  auto mate = AssemblyConstraint::create(AssemblyConstraintId("constraint.mate"), "Mate",
                                         AssemblyConstraintType::Mate,
                                         make_target("component.a", "feature.missing.seat"),
                                         make_target("component.b", "feature.hole.seat"));
  REQUIRE(mate);
  const auto wrong_type = builder.build(project, mate.value());
  REQUIRE(wrong_type.has_error());
  CHECK(wrong_type.error().message() ==
        "Insert equation construction requires an Insert constraint");

  const auto wrong_family = make_insert("constraint.axis", "component.a", "feature.hole.axis",
                                        "component.b", "feature.hole.seat");
  const auto wrong_family_result = builder.build(project, wrong_family);
  REQUIRE(wrong_family_result.has_error());
  CHECK(wrong_family_result.error().object_id() == "geometry.assembly_target_compatibility");

  const Project non_circle_project = make_project(
      {{"component.a", identity_rigid_transform()}, {"component.b", identity_rigid_transform()}},
      Point2{}, ExtrudeDirection::SketchNormal, false);
  const auto non_circle_result = builder.build(non_circle_project, constraint);
  REQUIRE(non_circle_result.has_error());
  CHECK(non_circle_result.error().message() ==
        "generated-seat assembly target requires exactly one CircleProfile in the source sketch");
}

TEST_CASE("Regular Insert residual Jacobian has rank five and leaves only axis rotation free",
          "[geometry][assembly-insert]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", identity_rigid_transform()}});
  add_insert(project);
  const RigidTransform before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

  const std::vector<double> residual = evaluate_insert(project);
  REQUIRE(residual.size() == 7U);
  for (double value : residual)
    CHECK(value == Approx(0.0).margin(kTolerance));

  const auto jacobian = central_difference_jacobian(project, 1.0e-4, 1.0e-4);
  REQUIRE(jacobian.size() == 7U);
  for (const auto& row : jacobian)
    REQUIRE(row.size() == 6U);
  CHECK(matrix_rank(jacobian, 1.0e-8) == 5U);

  Project axis_rotation_project = project;
  REQUIRE(axis_rotation_project.assembly().set_component_instance_transform(
      ComponentInstanceId("component.b"), RigidTransform{Vector3{}, Vector3{0.0, 0.0, 37.0}}));
  const std::vector<double> rotated_residual = evaluate_insert(axis_rotation_project);
  for (double value : rotated_residual)
    CHECK(value == Approx(0.0).margin(kTolerance));
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      before);
}

TEST_CASE("Insert is solved through the shared numeric solver without mutating the source project",
          "[geometry][assembly-insert]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{3.0, 0.0, 12.0}, Vector3{}}}});
  add_insert(project);
  const RigidTransform before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const AssemblyRigidBodySolver solver;

  const auto solved = solver.solve(project, group({"component.a", "component.b"}));

  REQUIRE(solved);
  REQUIRE(solved.value().converged());
  CHECK(solved.value().residual_summary.residual_component_count == 7U);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      before);
}
