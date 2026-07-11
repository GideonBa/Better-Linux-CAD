#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/datum_plane.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>

using namespace blcad;
using Catch::Approx;

namespace {

AssemblyConstraintTarget make_target(const char* component, const char* reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component), reference);
  REQUIRE(target);
  return target.value();
}

ComponentInstance make_instance(const char* id, double x) {
  auto instance = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.shared"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free,
      RigidTransform{Vector3{x, 2.0, 3.0}, Vector3{4.0, 5.0, 6.0}});
  REQUIRE(instance);
  return instance.value();
}

AssemblyDocument make_assembly() {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.angle"), "AngleAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.a", 10.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.b", 20.0)));
  return assembly.value();
}

Quantity make_angle(double degrees, const char* id = "constraint.angle") {
  auto angle = Quantity::angle_deg(degrees, id);
  REQUIRE(angle);
  return angle.value();
}

AssemblyConstraint
make_angle_constraint(double degrees,
                      AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.angle"), "Angle", AssemblyConstraintType::Angle,
      make_target("component.a", "feature.base_extrude.top"),
      make_target("component.b", "feature.base_extrude.top"), state, std::nullopt,
      make_angle(degrees));
  REQUIRE(constraint);
  return constraint.value();
}

} // namespace

TEST_CASE("Quantity angle accepts finite signed degrees", "[core][assembly-angle]") {
  const Quantity angle = make_angle(-30.0);
  CHECK(angle.kind() == QuantityKind::AngleDeg);
  CHECK(angle.degrees() == Approx(-30.0));
  CHECK(angle.unit() == "deg");
  CHECK(angle.is_valid_angle());
  CHECK_FALSE(angle.is_positive_length());
  CHECK_FALSE(angle.is_valid_count());

  CHECK(Quantity::angle_deg(std::numeric_limits<double>::infinity()).has_error());
  CHECK(Quantity::angle_deg(std::numeric_limits<double>::quiet_NaN()).has_error());
}

TEST_CASE("Angle is explicit angle-valued assembly relationship intent", "[core][assembly-angle]") {
  const auto target_a = make_target("component.a", "feature.base_extrude.top");
  const auto target_b = make_target("component.b", "feature.base_extrude.top");

  SECTION("angle constraints require an angle value in degrees") {
    const auto missing = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.angle"), "Angle", AssemblyConstraintType::Angle, target_a,
        target_b, AssemblyConstraintState::Active);
    REQUIRE(missing.has_error());
    CHECK(missing.error().message() == "angle assembly constraint requires an angle value");

    auto length = Quantity::length_mm(30.0, "constraint.angle");
    REQUIRE(length);
    const auto wrong_kind = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.angle"), "Angle", AssemblyConstraintType::Angle, target_a,
        target_b, AssemblyConstraintState::Active, std::nullopt, length.value());
    REQUIRE(wrong_kind.has_error());
    CHECK(wrong_kind.error().message() ==
          "angle assembly constraint requires an angle value in degrees");
  }

  SECTION("angle constraints reject distance values and vice versa") {
    auto distance = Quantity::length_mm(12.0, "constraint.angle");
    REQUIRE(distance);
    const auto with_distance = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.angle"), "Angle", AssemblyConstraintType::Angle, target_a,
        target_b, AssemblyConstraintState::Active, distance.value(), make_angle(45.0));
    REQUIRE(with_distance.has_error());
    CHECK(with_distance.error().message() ==
          "only distance assembly constraints may store a distance value");

    const auto mate_with_angle = AssemblyConstraint::create(
        AssemblyConstraintId("constraint.mate"), "Mate", AssemblyConstraintType::Mate, target_a,
        target_b, AssemblyConstraintState::Active, std::nullopt, make_angle(45.0));
    REQUIRE(mate_with_angle.has_error());
    CHECK(mate_with_angle.error().message() ==
          "only angle assembly constraints may store an angle value");
  }

  SECTION("a valid angle constraint stores the angle") {
    const AssemblyConstraint constraint = make_angle_constraint(45.0);
    CHECK(constraint.type() == AssemblyConstraintType::Angle);
    CHECK(to_string(constraint.type()) == "angle");
    REQUIRE(constraint.angle().has_value());
    CHECK(constraint.angle()->degrees() == Approx(45.0));
    CHECK_FALSE(constraint.distance().has_value());
  }
}

TEST_CASE("Angle intent roundtrips without mutating component placement",
          "[core][assembly-angle]") {
  auto assembly = make_assembly();
  const RigidTransform before_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();
  REQUIRE(assembly.add_constraint(make_angle_constraint(-72.5, AssemblyConstraintState::Inactive)));

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"type\": \"angle\"") != std::string::npos);
  CHECK(serialized.value().find("\"unit\": \"deg\"") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  const AssemblyConstraint* constraint =
      restored.value().find_constraint(AssemblyConstraintId("constraint.angle"));
  REQUIRE(constraint != nullptr);
  CHECK(constraint->type() == AssemblyConstraintType::Angle);
  CHECK(constraint->state() == AssemblyConstraintState::Inactive);
  REQUIRE(constraint->angle().has_value());
  CHECK(constraint->angle()->degrees() == Approx(-72.5));
  CHECK(constraint->angle()->unit() == "deg");
  CHECK_FALSE(constraint->distance().has_value());
  CHECK(restored.value().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        before_b);
}
