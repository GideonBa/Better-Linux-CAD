#include "blcad/core/assembly_document_json.hpp"
#include "blcad/core/datum_plane.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

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
  auto assembly = AssemblyDocument::create(DocumentId("assembly.insert"), "InsertAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.shared")));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.a", 10.0)));
  REQUIRE(assembly.value().add_component_instance(make_instance("component.b", 20.0)));
  return assembly.value();
}

AssemblyConstraint make_insert(AssemblyConstraintState state = AssemblyConstraintState::Active) {
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.insert"), "Insert", AssemblyConstraintType::Insert,
      make_target("component.a", "feature.hole.seat"),
      make_target("component.b", "feature.hole.seat"), state);
  REQUIRE(constraint);
  return constraint.value();
}

} // namespace

TEST_CASE("SemanticSeatingPlaneReference exposes stable generated-seat identity",
          "[core][semantic-seat]") {
  const auto seat = SemanticSeatingPlaneReference::create(FeatureId("feature.hole"));

  REQUIRE(seat);
  CHECK(seat.value().source_feature().value() == "feature.hole");
  CHECK(seat.value().plane() == SemanticSeatingPlane::Primary);
  CHECK(to_string(seat.value().plane()) == "seat");
  CHECK(seat.value().node_id() == "feature.hole.seat");

  const auto invalid = SemanticSeatingPlaneReference::create(FeatureId());
  REQUIRE(invalid.has_error());
  CHECK(invalid.error().message() ==
        "semantic seating plane source feature id must not be empty");
}

TEST_CASE("Insert is explicit distance-free assembly relationship intent",
          "[core][assembly-insert]") {
  const auto target_a = make_target("component.a", "feature.hole.seat");
  const auto target_b = make_target("component.b", "feature.hole.seat");
  auto distance = Quantity::length_mm(12.0, "constraint.insert");
  REQUIRE(distance);

  const auto insert = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.insert"), "Insert", AssemblyConstraintType::Insert,
      target_a, target_b, AssemblyConstraintState::Inactive);
  REQUIRE(insert);
  CHECK(insert.value().type() == AssemblyConstraintType::Insert);
  CHECK(to_string(insert.value().type()) == "insert");
  CHECK(insert.value().state() == AssemblyConstraintState::Inactive);
  CHECK_FALSE(insert.value().distance().has_value());

  const auto invalid = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.insert.distance"), "Insert", AssemblyConstraintType::Insert,
      target_a, target_b, AssemblyConstraintState::Active, distance.value());
  REQUIRE(invalid.has_error());
  CHECK(invalid.error().message() ==
        "only distance assembly constraints may store a distance value");
}

TEST_CASE("Insert intent roundtrips without mutating component placement",
          "[core][assembly-insert]") {
  auto assembly = make_assembly();
  const RigidTransform before_a =
      assembly.find_component_instance(ComponentInstanceId("component.a"))->transform();
  const RigidTransform before_b =
      assembly.find_component_instance(ComponentInstanceId("component.b"))->transform();

  REQUIRE(assembly.add_constraint(make_insert(AssemblyConstraintState::Inactive)));
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.a"))->transform() ==
        before_a);
  CHECK(assembly.find_component_instance(ComponentInstanceId("component.b"))->transform() ==
        before_b);

  const auto serialized = serialize_assembly_document_to_json(assembly);
  REQUIRE(serialized);
  CHECK(serialized.value().find("\"type\": \"insert\"") != std::string::npos);
  CHECK(serialized.value().find("feature.hole.seat") != std::string::npos);

  const auto restored = deserialize_assembly_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().constraint_count() == 1U);
  const AssemblyConstraint* constraint =
      restored.value().find_constraint(AssemblyConstraintId("constraint.insert"));
  REQUIRE(constraint != nullptr);
  CHECK(constraint->type() == AssemblyConstraintType::Insert);
  CHECK(constraint->state() == AssemblyConstraintState::Inactive);
  CHECK(constraint->target_a().semantic_reference() == "feature.hole.seat");
  CHECK(constraint->target_b().semantic_reference() == "feature.hole.seat");
  CHECK_FALSE(constraint->distance().has_value());
  CHECK(restored.value()
            .find_component_instance(ComponentInstanceId("component.a"))
            ->transform() == before_a);
  CHECK(restored.value()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform() == before_b);
}
