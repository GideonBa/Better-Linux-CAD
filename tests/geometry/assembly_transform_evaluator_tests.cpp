#include "blcad/geometry/assembly_transform_evaluator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-12;

void check_point_approx(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

void check_vector_approx(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

[[nodiscard]] double magnitude(const Vector3& vector) {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

[[nodiscard]] double dot(const Vector3& lhs, const Vector3& rhs) {
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Project make_transform_project() {
  auto part = PartDocument::create(DocumentId("part.face_plate"), "FacePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(make_length_parameter("part.width", "width", 120.0)));
  REQUIRE(part.value().add_parameter(make_length_parameter("part.height", "height", 80.0)));
  REQUIRE(
      part.value().add_parameter(make_length_parameter("part.thickness", "thickness", 8.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto sketch =
      Sketch::create(SketchId("sketch.base"), "Sketch_BaseRectangle", DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base_rectangle"),
                                            ParameterId("part.width"), ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
      ParameterId("part.thickness"));
  REQUIRE(feature);
  REQUIRE(part.value().add_feature(feature.value()));

  auto assembly = AssemblyDocument::create(DocumentId("assembly.face"), "FaceAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.face_plate")));

  const RigidTransform transform{Vector3{10.0, 20.0, 30.0}, Vector3{0.0, 0.0, 90.0}};
  auto component = ComponentInstance::create(
      ComponentInstanceId("component.face_plate"), "Face Plate", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active,
      ComponentGroundingState::Free, transform);
  REQUIRE(component);
  REQUIRE(assembly.value().add_component_instance(component.value()));

  auto project = Project::create(DocumentId("project.face"), "FaceProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(part.value()));
  return project.value();
}

} // namespace

TEST_CASE("Assembly transform evaluator preserves identity and applies translation only to points",
          "[geometry][assembly-transform]") {
  const AssemblyTransformEvaluator evaluator;
  const Point3 local_point{1.25, -2.5, 3.75};
  const Vector3 local_vector{-4.0, 5.5, 6.25};
  const ComponentLocalPlanarDescriptor local_plane{
      Point3{2.0, 3.0, 4.0}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0}};

  SECTION("identity") {
    const RigidTransform transform = identity_rigid_transform();
    CHECK(evaluator.evaluate_point(transform, local_point) == local_point);
    CHECK(evaluator.evaluate_vector(transform, local_vector) == local_vector);

    const AssemblySpacePlanarDescriptor expected{
        local_plane.origin, local_plane.x_axis, local_plane.y_axis, local_plane.normal};
    CHECK(evaluator.evaluate_plane(transform, local_plane) == expected);
  }

  SECTION("pure translation") {
    const RigidTransform transform{Vector3{10.0, -20.0, 30.0}, Vector3{0.0, 0.0, 0.0}};
    const Point3 expected_point{11.25, -22.5, 33.75};
    CHECK(evaluator.evaluate_point(transform, local_point) == expected_point);
    CHECK(evaluator.evaluate_vector(transform, local_vector) == local_vector);

    const AssemblySpacePlanarDescriptor expected_plane{
        Point3{12.0, -17.0, 34.0}, local_plane.x_axis, local_plane.y_axis, local_plane.normal};
    CHECK(evaluator.evaluate_plane(transform, local_plane) == expected_plane);
  }
}

TEST_CASE("Assembly transform evaluator applies right-handed single-axis rotations",
          "[geometry][assembly-transform]") {
  const AssemblyTransformEvaluator evaluator;

  SECTION("positive X rotation maps positive Y to positive Z") {
    const RigidTransform transform{Vector3{}, Vector3{90.0, 0.0, 0.0}};
    check_vector_approx(evaluator.evaluate_vector(transform, Vector3{0.0, 1.0, 0.0}),
                        Vector3{0.0, 0.0, 1.0});
  }

  SECTION("positive Y rotation maps positive Z to positive X") {
    const RigidTransform transform{Vector3{}, Vector3{0.0, 90.0, 0.0}};
    check_vector_approx(evaluator.evaluate_vector(transform, Vector3{0.0, 0.0, 1.0}),
                        Vector3{1.0, 0.0, 0.0});
  }

  SECTION("positive Z rotation maps positive X to positive Y") {
    const RigidTransform transform{Vector3{}, Vector3{0.0, 0.0, 90.0}};
    check_vector_approx(evaluator.evaluate_vector(transform, Vector3{1.0, 0.0, 0.0}),
                        Vector3{0.0, 1.0, 0.0});
  }
}

TEST_CASE("Assembly transform evaluator proves X then Y then Z rotation order",
          "[geometry][assembly-transform]") {
  const AssemblyTransformEvaluator evaluator;
  const RigidTransform transform{Vector3{}, Vector3{90.0, 90.0, 90.0}};

  const Vector3 evaluated = evaluator.evaluate_vector(transform, Vector3{1.0, 2.0, 3.0});

  // Rx(90): (1, -3, 2), then Ry(90): (2, -3, -1), then Rz(90): (3, 2, -1).
  check_vector_approx(evaluated, Vector3{3.0, 2.0, -1.0});
}

TEST_CASE("Assembly transform evaluator maps a resolved target plane into assembly space",
          "[geometry][assembly-transform]") {
  const Project project = make_transform_project();
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId("component.face_plate"),
                                                 "feature.base_extrude.top");
  REQUIRE(target);

  const AssemblyConstraintTargetResolver target_resolver;
  const auto resolved = target_resolver.resolve(project, target.value());
  REQUIRE(resolved);

  CHECK((resolved.value().local_plane.origin == Point3{0.0, 0.0, 8.0}));
  CHECK((resolved.value().component_transform.translation_mm == Vector3{10.0, 20.0, 30.0}));
  CHECK((resolved.value().component_transform.rotation_deg == Vector3{0.0, 0.0, 90.0}));

  const AssemblyTransformEvaluator evaluator;
  const AssemblySpacePlanarDescriptor plane =
      evaluator.evaluate_plane(resolved.value().component_transform, resolved.value().local_plane);

  check_point_approx(plane.origin, Point3{10.0, 20.0, 38.0});
  check_vector_approx(plane.x_axis, Vector3{0.0, 1.0, 0.0});
  check_vector_approx(plane.y_axis, Vector3{-1.0, 0.0, 0.0});
  check_vector_approx(plane.normal, Vector3{0.0, 0.0, 1.0});
}

TEST_CASE("Assembly transform evaluation is deterministic, length-preserving, and read-only",
          "[geometry][assembly-transform]") {
  const AssemblyTransformEvaluator evaluator;
  RigidTransform transform{Vector3{7.0, -11.0, 13.0}, Vector3{23.0, -41.0, 67.0}};
  ComponentLocalPlanarDescriptor local_plane{
      Point3{2.0, -3.0, 5.0}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
      Vector3{0.0, 0.0, 1.0}};
  const RigidTransform transform_before = transform;
  const ComponentLocalPlanarDescriptor local_plane_before = local_plane;

  const AssemblySpacePlanarDescriptor first = evaluator.evaluate_plane(transform, local_plane);
  const AssemblySpacePlanarDescriptor second = evaluator.evaluate_plane(transform, local_plane);
  const Vector3 local_vector{2.0, -3.0, 4.0};
  const Vector3 assembly_vector = evaluator.evaluate_vector(transform, local_vector);

  CHECK(first == second);
  CHECK(transform == transform_before);
  CHECK(local_plane == local_plane_before);
  CHECK(magnitude(first.x_axis) == Approx(1.0).margin(kTolerance));
  CHECK(magnitude(first.y_axis) == Approx(1.0).margin(kTolerance));
  CHECK(magnitude(first.normal) == Approx(1.0).margin(kTolerance));
  CHECK(dot(first.x_axis, first.y_axis) == Approx(0.0).margin(kTolerance));
  CHECK(dot(first.x_axis, first.normal) == Approx(0.0).margin(kTolerance));
  CHECK(dot(first.y_axis, first.normal) == Approx(0.0).margin(kTolerance));
  CHECK(magnitude(assembly_vector) == Approx(magnitude(local_vector)).margin(kTolerance));
}
