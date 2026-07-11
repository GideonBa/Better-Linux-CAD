#include "blcad/geometry/assembly_concentric_constraint_equation_builder.hpp"

#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <initializer_list>
#include <optional>
#include <utility>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-12;

struct ComponentSpec {
  const char* id;
  RigidTransform transform;
};

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_hole_part(Point2 hole_center = Point2{4.0, -6.0},
                            ExtrudeDirection hole_direction = ExtrudeDirection::SketchNormal,
                            bool use_circle_profile = true,
                            bool include_hole_feature = true) {
  auto part = PartDocument::create(DocumentId("part.hole_plate"), "HolePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(part.value().add_parameter(
      make_length_parameter("part.thickness", "thickness", 8.0)));
  REQUIRE(part.value().add_parameter(
      make_length_parameter("part.hole_diameter", "hole_diameter", 20.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto base_sketch =
      Sketch::create(SketchId("sketch.base"), "BaseSketch", DatumPlaneId("datum.xy"));
  REQUIRE(base_sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"),
                                            ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base_sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base_sketch.value()));

  auto base_feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));

  auto hole_sketch =
      Sketch::create(SketchId("sketch.hole"), "HoleSketch", DatumPlaneId("datum.xy"));
  REQUIRE(hole_sketch);
  if (use_circle_profile) {
    auto circle = CircleProfile::create(ProfileId("profile.hole"),
                                        ParameterId("part.hole_diameter"), hole_center);
    REQUIRE(circle);
    REQUIRE(hole_sketch.value().add_profile(circle.value()));
  } else {
    auto rectangle_hole = RectangleProfile::create(
        ProfileId("profile.not_circle"), ParameterId("part.hole_diameter"),
        ParameterId("part.hole_diameter"), hole_center);
    REQUIRE(rectangle_hole);
    REQUIRE(hole_sketch.value().add_profile(rectangle_hole.value()));
  }
  REQUIRE(part.value().add_sketch(hole_sketch.value()));

  if (include_hole_feature) {
    auto hole_feature = Feature::create_subtractive_extrude(
        FeatureId("feature.hole"), "Hole", SketchId("sketch.hole"),
        FeatureId("feature.base_extrude"), SubtractiveExtrudeDepth::ThroughAll, hole_direction);
    REQUIRE(hole_feature);
    REQUIRE(part.value().add_feature(hole_feature.value()));
  }

  return part.value();
}

Project make_project(std::initializer_list<ComponentSpec> components,
                     Point2 hole_center = Point2{4.0, -6.0},
                     ExtrudeDirection hole_direction = ExtrudeDirection::SketchNormal,
                     bool use_circle_profile = true,
                     bool include_hole_feature = true) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.holes"), "HoleAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.hole_plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.hole_plate"),
        ComponentVisibility::Visible, ComponentSuppressionState::Active,
        ComponentGroundingState::Free, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }

  auto project = Project::create(DocumentId("project.holes"), "HoleProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(
      make_hole_part(hole_center, hole_direction, use_circle_profile, include_hole_feature)));
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(target);
  return target.value();
}

AssemblyConstraint make_constraint(const char* id,
                                   AssemblyConstraintType type,
                                   AssemblyConstraintTarget target_a,
                                   AssemblyConstraintTarget target_b,
                                   AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(AssemblyConstraintId(id), id, type,
                                               std::move(target_a), std::move(target_b), state);
  REQUIRE(constraint);
  return constraint.value();
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

} // namespace

TEST_CASE("Assembly target resolver resolves stable circular-cut semantic axes",
          "[geometry][assembly-concentric]") {
  const AssemblyConstraintTargetResolver resolver;

  SECTION("circle center and sketch-normal direction define the local axis") {
    const Project project = make_project(
        {{"component.a", RigidTransform{Vector3{10.0, 20.0, 30.0}, Vector3{5.0, 15.0, 25.0}}}});
    const auto resolved =
        resolver.resolve_axis(project, make_target("component.a", "feature.hole.axis"));

    REQUIRE(resolved);
    CHECK(resolved.value().component_instance.value() == "component.a");
    CHECK(resolved.value().referenced_part_document.value() == "part.hole_plate");
    CHECK(resolved.value().source_feature.value() == "feature.hole");
    CHECK(resolved.value().source_profile.value() == "profile.hole");
    CHECK(resolved.value().axis == SemanticAxis::Primary);
    check_point(resolved.value().local_axis.origin, Point3{4.0, -6.0, 0.0});
    check_vector(resolved.value().local_axis.direction, Vector3{0.0, 0.0, 1.0});
    CHECK((resolved.value().component_transform.translation_mm == Vector3{10.0, 20.0, 30.0}));
    CHECK((resolved.value().component_transform.rotation_deg == Vector3{5.0, 15.0, 25.0}));
  }

  SECTION("opposite extrude direction reverses the deterministic axis direction") {
    const Project project = make_project(
        {{"component.a", identity_rigid_transform()}}, Point2{4.0, -6.0},
        ExtrudeDirection::OppositeSketchNormal);
    const auto resolved =
        resolver.resolve_axis(project, make_target("component.a", "feature.hole.axis"));

    REQUIRE(resolved);
    check_vector(resolved.value().local_axis.direction, Vector3{0.0, 0.0, -1.0});
  }
}

TEST_CASE("Assembly axis transform evaluation rotates the direction and translates only the origin",
          "[geometry][assembly-concentric]") {
  const ComponentLocalAxisDescriptor local_axis{Point3{4.0, -6.0, 0.0},
                                                Vector3{0.0, 0.0, 1.0}};
  const RigidTransform transform{Vector3{10.0, 20.0, 30.0}, Vector3{90.0, 0.0, 0.0}};
  const AssemblyTransformEvaluator evaluator;

  const AssemblySpaceAxisDescriptor axis = evaluator.evaluate_axis(transform, local_axis);

  check_point(axis.origin, Point3{14.0, 20.0, 24.0});
  check_vector(axis.direction, Vector3{0.0, -1.0, 0.0});
  check_point(local_axis.origin, Point3{4.0, -6.0, 0.0});
  check_vector(local_axis.direction, Vector3{0.0, 0.0, 1.0});
}

TEST_CASE("Concentric equation builder constructs canonical axis-line residuals",
          "[geometry][assembly-concentric]") {
  const AssemblyConcentricConstraintEquationBuilder builder;

  SECTION("axial separation is allowed for coincident parallel axes") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", RigidTransform{Vector3{0.0, 0.0, 25.0}, Vector3{}}}});
    const auto constraint = make_constraint(
        "constraint.concentric", AssemblyConstraintType::Concentric,
        make_target("component.a", "feature.hole.axis"),
        make_target("component.b", "feature.hole.axis"));

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    CHECK(equation.value().constraint.value() == "constraint.concentric");
    CHECK(equation.value().target_a.component_instance.value() == "component.a");
    CHECK(equation.value().target_a.semantic_reference == "feature.hole.axis");
    CHECK(equation.value().target_b.component_instance.value() == "component.b");
    check_point(equation.value().target_a.axis.origin, Point3{4.0, -6.0, 0.0});
    check_point(equation.value().target_b.axis.origin, Point3{4.0, -6.0, 25.0});
    check_vector(equation.value().residual.direction_parallelism, Vector3{0.0, 0.0, 0.0});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{0.0, 0.0, 0.0});
  }

  SECTION("opposed axis directions are also concentric") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", RigidTransform{Vector3{0.0, -12.0, 25.0},
                                         Vector3{180.0, 0.0, 0.0}}}});
    const auto constraint = make_constraint(
        "constraint.opposed", AssemblyConstraintType::Concentric,
        make_target("component.a", "feature.hole.axis"),
        make_target("component.b", "feature.hole.axis"));

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    check_vector(equation.value().target_b.axis.direction, Vector3{0.0, 0.0, -1.0});
    check_vector(equation.value().residual.direction_parallelism, Vector3{0.0, 0.0, 0.0});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{0.0, 0.0, 0.0});
  }

  SECTION("parallel offset axes expose a perpendicular millimeter residual") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", RigidTransform{Vector3{3.0, 0.0, 25.0}, Vector3{}}}});
    const auto constraint = make_constraint(
        "constraint.offset", AssemblyConstraintType::Concentric,
        make_target("component.a", "feature.hole.axis"),
        make_target("component.b", "feature.hole.axis"));

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    check_vector(equation.value().residual.direction_parallelism, Vector3{0.0, 0.0, 0.0});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{0.0, -3.0, 0.0});
  }

  SECTION("nonparallel axes expose direction residual independently from offset") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", RigidTransform{Vector3{0.0, -6.0, 6.0},
                                         Vector3{90.0, 0.0, 0.0}}}});
    const auto constraint = make_constraint(
        "constraint.nonparallel", AssemblyConstraintType::Concentric,
        make_target("component.a", "feature.hole.axis"),
        make_target("component.b", "feature.hole.axis"));

    const auto equation = builder.build(project, constraint);

    REQUIRE(equation);
    check_point(equation.value().target_b.axis.origin, Point3{4.0, -6.0, 0.0});
    check_vector(equation.value().target_b.axis.direction, Vector3{0.0, -1.0, 0.0});
    check_vector(equation.value().residual.direction_parallelism, Vector3{1.0, 0.0, 0.0});
    check_vector(equation.value().residual.axis_offset_mm, Vector3{0.0, 0.0, 0.0});
  }
}

TEST_CASE("Concentric equation builder preserves target order, determinism, and project intent",
          "[geometry][assembly-concentric]") {
  Project project = make_project(
      {{"component.a", identity_rigid_transform()},
       {"component.b", RigidTransform{Vector3{3.0, 0.0, 25.0}, Vector3{}}}});
  auto forward = make_constraint(
      "constraint.forward", AssemblyConstraintType::Concentric,
      make_target("component.a", "feature.hole.axis"),
      make_target("component.b", "feature.hole.axis"));
  auto reverse = make_constraint(
      "constraint.reverse", AssemblyConstraintType::Concentric,
      make_target("component.b", "feature.hole.axis"),
      make_target("component.a", "feature.hole.axis"));
  REQUIRE(project.assembly().add_constraint(forward));
  REQUIRE(project.assembly().add_constraint(reverse));

  const RigidTransform transform_a_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform transform_b_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
  const std::size_t constraint_count_before = project.assembly().constraint_count();
  const std::size_t workplane_count_before =
      project.find_part_document(DocumentId("part.hole_plate"))->derived_workplane_count();
  const AssemblyConcentricConstraintEquationBuilder builder;

  const auto first = builder.build(project, project.assembly().constraints()[0]);
  const auto second = builder.build(project, project.assembly().constraints()[0]);
  const auto reversed = builder.build(project, project.assembly().constraints()[1]);

  REQUIRE(first);
  REQUIRE(second);
  REQUIRE(reversed);
  CHECK(first.value() == second.value());
  check_vector(first.value().residual.axis_offset_mm, Vector3{0.0, -3.0, 0.0});
  check_vector(reversed.value().residual.axis_offset_mm, Vector3{0.0, 3.0, 0.0});
  CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        transform_a_before);
  CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        transform_b_before);
  CHECK(project.assembly().constraint_count() == constraint_count_before);
  CHECK(project.find_part_document(DocumentId("part.hole_plate"))->derived_workplane_count() ==
        workplane_count_before);
}

TEST_CASE("Semantic axis and Concentric builders reject unsupported intent explicitly",
          "[geometry][assembly-concentric]") {
  const AssemblyConstraintTargetResolver resolver;
  const AssemblyConcentricConstraintEquationBuilder builder;

  SECTION("axis reference suffix must be the supported generated-axis family") {
    const Project project = make_project({{"component.a", identity_rigid_transform()}});
    const auto malformed =
        resolver.resolve_axis(project, make_target("component.a", "feature.hole."));
    REQUIRE(malformed.has_error());
    CHECK(malformed.error().message() == "assembly semantic axis reference is malformed");

    const auto unsupported =
        resolver.resolve_axis(project, make_target("component.a", "feature.hole.center_axis"));
    REQUIRE(unsupported.has_error());
    CHECK(unsupported.error().message() ==
          "unsupported assembly semantic axis reference family");
  }

  SECTION("only a single CircleProfile subtractive extrude exposes the primary axis") {
    const Project wrong_profile = make_project(
        {{"component.a", identity_rigid_transform()}}, Point2{4.0, -6.0},
        ExtrudeDirection::SketchNormal, false);
    const auto wrong =
        resolver.resolve_axis(wrong_profile, make_target("component.a", "feature.hole.axis"));
    REQUIRE(wrong.has_error());
    CHECK(wrong.error().message() ==
          "generated-axis assembly target requires exactly one CircleProfile in the source sketch");

    const Project missing_feature = make_project(
        {{"component.a", identity_rigid_transform()}}, Point2{4.0, -6.0},
        ExtrudeDirection::SketchNormal, true, false);
    const auto missing = resolver.resolve_axis(
        missing_feature, make_target("component.a", "feature.hole.axis"));
    REQUIRE(missing.has_error());
    CHECK(missing.error().message() ==
          "generated-axis assembly target source feature must exist in referenced part document");

    const Project project = make_project({{"component.a", identity_rigid_transform()}});
    const auto wrong_feature_type = resolver.resolve_axis(
        project, make_target("component.a", "feature.base_extrude.axis"));
    REQUIRE(wrong_feature_type.has_error());
    CHECK(wrong_feature_type.error().message() ==
          "generated-axis assembly target source feature must be a subtractive extrude");
  }

  SECTION("Concentric builder validates state and type before target resolution") {
    const Project project = make_project(
        {{"component.a", identity_rigid_transform()},
         {"component.b", identity_rigid_transform()}});
    const auto inactive = make_constraint(
        "constraint.inactive", AssemblyConstraintType::Concentric,
        make_target("component.a", "bad"), make_target("component.b", "bad"),
        AssemblyConstraintState::Inactive);
    const auto inactive_equation = builder.build(project, inactive);
    REQUIRE(inactive_equation.has_error());
    CHECK(inactive_equation.error().message() ==
          "Concentric equation construction requires an active constraint");

    auto mate = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.mate"), "Mate", AssemblyConstraintType::Mate,
        make_target("component.a", "bad"), make_target("component.b", "bad"));
    REQUIRE(mate);
    const auto wrong_type = builder.build(project, mate.value());
    REQUIRE(wrong_type.has_error());
    CHECK(wrong_type.error().message() ==
          "Concentric equation construction requires a Concentric constraint");
  }
}
