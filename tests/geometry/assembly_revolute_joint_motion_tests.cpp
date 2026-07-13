#include "assembly_constraint_numeric_system.hpp"

#include "blcad/geometry/assembly_joint_motion_solver.hpp"
#include "blcad/geometry/assembly_revolute_joint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;

namespace {

constexpr double kTolerance = 1.0e-6;

struct ComponentSpec {
  const char* id;
  RigidTransform transform = identity_rigid_transform();
  ComponentGroundingState grounding = ComponentGroundingState::Free;
  ComponentSuppressionState suppression = ComponentSuppressionState::Active;
};

Parameter length_parameter(const char* id, const char* name, double millimeters) {
  auto quantity = Quantity::length_mm(millimeters, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Quantity angle(double degrees, const char* id) {
  auto quantity = Quantity::angle_deg(degrees, id);
  REQUIRE(quantity);
  return quantity.value();
}

Quantity displacement(double millimeters, const char* id) {
  auto quantity = Quantity::linear_displacement_mm(millimeters, id);
  REQUIRE(quantity);
  return quantity.value();
}

PartDocument make_hole_part() {
  auto part = PartDocument::create(DocumentId("part.revolute_plate"), "RevolutePlate");
  REQUIRE(part);
  REQUIRE(part.value().add_parameter(length_parameter("part.width", "width", 10.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.height", "height", 20.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.thickness", "thickness", 2.0)));
  REQUIRE(part.value().add_parameter(length_parameter("part.hole_diameter", "hole_diameter", 4.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));

  auto base = Sketch::create(SketchId("sketch.base"), "Base", DatumPlaneId("datum.xy"));
  REQUIRE(base);
  auto rectangle = RectangleProfile::create(ProfileId("profile.base"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(base.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(base.value()));

  auto base_feature =
      Feature::create_additive_extrude(FeatureId("feature.base_extrude"), "BaseExtrude",
                                       SketchId("sketch.base"), ParameterId("part.thickness"));
  REQUIRE(base_feature);
  REQUIRE(part.value().add_feature(base_feature.value()));

  auto hole = Sketch::create(SketchId("sketch.hole"), "Hole", DatumPlaneId("datum.xy"));
  REQUIRE(hole);
  auto circle = CircleProfile::create(ProfileId("profile.hole"), ParameterId("part.hole_diameter"),
                                      Point2{0.0, 0.0});
  REQUIRE(circle);
  REQUIRE(hole.value().add_profile(circle.value()));
  REQUIRE(part.value().add_sketch(hole.value()));

  auto hole_feature = Feature::create_subtractive_extrude(
      FeatureId("feature.hole"), "HoleCut", SketchId("sketch.hole"),
      FeatureId("feature.base_extrude"), SubtractiveExtrudeDepth::ThroughAll,
      ExtrudeDirection::SketchNormal);
  REQUIRE(hole_feature);
  REQUIRE(part.value().add_feature(hole_feature.value()));
  return part.value();
}

AssemblyConstraintTarget target(const char* component) {
  auto value =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), "feature.hole.seat");
  REQUIRE(value);
  return value.value();
}

AssemblyJoint make_joint(const char* id, const char* component_a, const char* component_b,
                         AssemblyJointState state = AssemblyJointState::Active,
                         double coordinate_deg = 0.0) {
  auto value = AssemblyJoint::create(AssemblyJointId(id), id, AssemblyJointType::Revolute,
                                     target(component_a), target(component_b), state,
                                     angle(-90.0, id), angle(90.0, id), angle(coordinate_deg, id));
  REQUIRE(value);
  return value.value();
}

AssemblyJoint make_prismatic_joint(const char* id, const char* component_a,
                                   const char* component_b) {
  auto slot = AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
      displacement(0.0, id), displacement(-50.0, id), displacement(50.0, id));
  REQUIRE(slot);
  auto value =
      AssemblyJoint::create(AssemblyJointId(id), id, AssemblyJointType::Prismatic,
                            target(component_a), target(component_b), AssemblyJointState::Active,
                            std::vector<AssemblyJointCoordinateSlot>{slot.value()});
  REQUIRE(value);
  return value.value();
}

AssemblyJoint make_cylindrical_joint(const char* id, const char* component_a,
                                     const char* component_b, double translation_mm = 0.0,
                                     double rotation_deg = 0.0) {
  auto translation = AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Translation, AssemblyJointCoordinateKind::Linear,
      displacement(translation_mm, id), displacement(-50.0, id), displacement(50.0, id));
  auto rotation = AssemblyJointCoordinateSlot::create(
      AssemblyJointCoordinateRole::Rotation, AssemblyJointCoordinateKind::Angular,
      angle(rotation_deg, id), angle(-90.0, id), angle(90.0, id));
  REQUIRE(translation);
  REQUIRE(rotation);
  auto value = AssemblyJoint::create(
      AssemblyJointId(id), id, AssemblyJointType::Cylindrical, target(component_a),
      target(component_b), AssemblyJointState::Active,
      std::vector<AssemblyJointCoordinateSlot>{translation.value(), rotation.value()});
  REQUIRE(value);
  return value.value();
}

Project make_project(std::initializer_list<ComponentSpec> components) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.revolute"), "RevoluteAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.revolute_plate")));
  for (const auto& spec : components) {
    auto component = ComponentInstance::create(
        ComponentInstanceId(spec.id), spec.id, DocumentId("part.revolute_plate"),
        ComponentVisibility::Visible, spec.suppression, spec.grounding, spec.transform);
    REQUIRE(component);
    REQUIRE(assembly.value().add_component_instance(component.value()));
  }
  auto project =
      Project::create(DocumentId("project.revolute"), "RevoluteProject", assembly.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(make_hole_part()));
  return project.value();
}

const ProposedComponentTransform& proposal_for(const AssemblySolveResult& result,
                                               const char* component) {
  for (const auto& proposal : result.proposed_transforms) {
    if (proposal.component_instance.value() == component) {
      return proposal;
    }
  }
  FAIL("missing component transform proposal");
  return result.proposed_transforms.front();
}

void check_vector_zero(const Vector3& value, double tolerance = kTolerance) {
  CHECK(value.x == Approx(0.0).margin(tolerance));
  CHECK(value.y == Approx(0.0).margin(tolerance));
  CHECK(value.z == Approx(0.0).margin(tolerance));
}

std::size_t numeric_rank(detail::NumericMatrix matrix, double tolerance = 1.0e-8) {
  if (matrix.empty()) {
    return 0U;
  }
  const std::size_t rows = matrix.size();
  const std::size_t columns = matrix.front().size();
  std::size_t rank = 0U;
  std::size_t pivot_column = 0U;
  while (rank < rows && pivot_column < columns) {
    std::size_t pivot_row = rank;
    double pivot_magnitude = std::abs(matrix[pivot_row][pivot_column]);
    for (std::size_t row = rank + 1U; row < rows; ++row) {
      const double magnitude = std::abs(matrix[row][pivot_column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }
    if (pivot_magnitude <= tolerance) {
      ++pivot_column;
      continue;
    }
    std::swap(matrix[rank], matrix[pivot_row]);
    const double pivot = matrix[rank][pivot_column];
    for (std::size_t row = rank + 1U; row < rows; ++row) {
      const double factor = matrix[row][pivot_column] / pivot;
      for (std::size_t column = pivot_column; column < columns; ++column) {
        matrix[row][column] -= factor * matrix[rank][column];
      }
    }
    ++rank;
    ++pivot_column;
  }
  return rank;
}

} // namespace

TEST_CASE("Revolute joint equation preserves oriented signed twist semantics",
          "[geometry][assembly-revolute-joint]") {
  Project aligned =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(aligned.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
  const AssemblyJoint* aligned_joint =
      aligned.assembly().find_joint(AssemblyJointId("joint.revolute"));
  REQUIRE(aligned_joint != nullptr);

  const AssemblyRevoluteJointEquationBuilder builder;
  const auto zero = builder.build(aligned, *aligned_joint, angle(0.0, "joint.revolute"));
  REQUIRE(zero);
  check_vector_zero(zero.value().residual.direction_alignment);
  check_vector_zero(zero.value().residual.axis_offset_mm);
  CHECK(zero.value().residual.signed_seating_separation_mm == Approx(0.0).margin(kTolerance));
  CHECK(zero.value().residual.twist_alignment_sine == Approx(0.0).margin(kTolerance));
  CHECK(zero.value().residual.twist_alignment_cosine == Approx(0.0).margin(kTolerance));

  Project rotated =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b", RigidTransform{Vector3{}, Vector3{0.0, 0.0, 30.0}}}});
  REQUIRE(rotated.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
  const AssemblyJoint* rotated_joint =
      rotated.assembly().find_joint(AssemblyJointId("joint.revolute"));
  REQUIRE(rotated_joint != nullptr);
  const auto positive = builder.build(rotated, *rotated_joint, angle(30.0, "joint.revolute"));
  const auto negative = builder.build(rotated, *rotated_joint, angle(-30.0, "joint.revolute"));
  REQUIRE(positive);
  REQUIRE(negative);
  CHECK(positive.value().residual.twist_alignment_sine == Approx(0.0).margin(kTolerance));
  CHECK(positive.value().residual.twist_alignment_cosine == Approx(0.0).margin(kTolerance));
  CHECK(std::abs(negative.value().residual.twist_alignment_sine) > 0.5);
}

TEST_CASE("Shared numeric system gives a regular rank six revolute drive",
          "[geometry][assembly-revolute-joint]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));

  const detail::AssemblyNumericRelationshipSet relationships{
      {},
      {{AssemblyJointId("joint.revolute"),
        {{AssemblyJointCoordinateRole::Rotation, angle(0.0, "joint.revolute")}}}}};
  const auto residuals = detail::evaluate_residuals(project, relationships, 1.0);
  REQUIRE(residuals);
  REQUIRE(residuals.value().size() == 9U);
  for (double residual : residuals.value()) {
    CHECK(residual == Approx(0.0).margin(1.0e-12));
  }

  const std::vector<ComponentInstanceId> variables{ComponentInstanceId("component.b")};
  const detail::NumericVector values = detail::read_variables(project, variables);
  const detail::AssemblyNumericSystemOptions options{1.0, 1.0e-4, 1.0e-4};
  const auto jacobian = detail::build_central_difference_jacobian(
      project, variables, relationships, values, residuals.value(), options);
  REQUIRE(jacobian);
  REQUIRE(jacobian.value().size() == 9U);
  for (const auto& row : jacobian.value()) {
    CHECK(row.size() == 6U);
  }
  CHECK(numeric_rank(jacobian.value()) == 6U);
}

TEST_CASE("Revolute motion solver moves and atomically applies an in-range joint coordinate",
          "[geometry][assembly-revolute-joint]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
  const RigidTransform source_transform =
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();

  const AssemblyJointMotionSolver solver;
  const auto first =
      solver.move(project, AssemblyJointId("joint.revolute"), angle(45.0, "joint.revolute"));
  const auto repeated =
      solver.move(project, AssemblyJointId("joint.revolute"), angle(45.0, "joint.revolute"));
  REQUIRE(first);
  REQUIRE(repeated);
  CHECK(first.value() == repeated.value());
  REQUIRE(first.value().converged());
  CHECK(first.value().source_coordinate_deg == 0.0);
  CHECK(first.value().requested_coordinate_deg == 45.0);
  CHECK(first.value().solve_result.residual_summary.residual_component_count == 9U);
  CHECK(first.value().solve_result.residual_summary.final_rms <= 1.0e-8);
  REQUIRE(first.value().solve_result.proposed_transforms.size() == 1U);
  const auto& proposal = proposal_for(first.value().solve_result, "component.b");
  CHECK(proposal.proposed_transform.translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.rotation_deg.x == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.rotation_deg.y == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.rotation_deg.z == Approx(45.0).margin(kTolerance));
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() == 0.0);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      source_transform);

  const AssemblyJointMotionResultApplier applier;
  const auto applied = applier.apply(project, first.value());
  REQUIRE(applied);
  CHECK(applied.value() == 1U);
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() == 45.0);
  CHECK(project.assembly()
            .find_component_instance(ComponentInstanceId("component.b"))
            ->transform()
            .rotation_deg.z == Approx(45.0).margin(kTolerance));

  const AssemblyJoint* joint = project.assembly().find_joint(AssemblyJointId("joint.revolute"));
  REQUIRE(joint != nullptr);
  const auto equation =
      AssemblyRevoluteJointEquationBuilder{}.build(project, *joint, angle(45.0, "joint.revolute"));
  REQUIRE(equation);
  check_vector_zero(equation.value().residual.direction_alignment);
  check_vector_zero(equation.value().residual.axis_offset_mm);
  CHECK(equation.value().residual.signed_seating_separation_mm == Approx(0.0).margin(kTolerance));
  CHECK(equation.value().residual.twist_alignment_sine == Approx(0.0).margin(kTolerance));
  CHECK(equation.value().residual.twist_alignment_cosine == Approx(0.0).margin(kTolerance));
}

TEST_CASE("Prismatic vector motion drives and atomically applies signed axial translation",
          "[geometry][assembly-prismatic-joint]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(project.assembly().add_joint(
      make_prismatic_joint("joint.slider", "component.a", "component.b")));
  const AssemblyJointDrive drive{
      AssemblyJointId("joint.slider"),
      {{AssemblyJointCoordinateRole::Translation, displacement(12.0, "joint.slider")}}};
  auto result = AssemblyJointMotionSolver{}.move(project, drive);
  REQUIRE(result);
  REQUIRE(result.value().converged());
  const auto& proposal = proposal_for(result.value().solve_result, "component.b");
  CHECK(proposal.proposed_transform.translation_mm.x == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.y == Approx(0.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(12.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.rotation_deg == Vector3{});
  auto applied = AssemblyJointMotionResultApplier{}.apply(project, result.value());
  REQUIRE(applied);
  const auto* slot = project.assembly()
                         .find_joint(AssemblyJointId("joint.slider"))
                         ->find_coordinate_slot(AssemblyJointCoordinateRole::Translation);
  REQUIRE(slot);
  CHECK(slot->value().millimeters() == 12.0);
}

TEST_CASE("Cylindrical vector motion coexists for translation and twist and applies atomically",
          "[geometry][assembly-cylindrical-joint]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(project.assembly().add_joint(
      make_cylindrical_joint("joint.cylindrical", "component.a", "component.b")));
  const AssemblyJointDrive drive{
      AssemblyJointId("joint.cylindrical"),
      {{AssemblyJointCoordinateRole::Rotation, angle(30.0, "joint.cylindrical")},
       {AssemblyJointCoordinateRole::Translation, displacement(12.0, "joint.cylindrical")}}};
  auto result = AssemblyJointMotionSolver{}.move(project, drive);
  REQUIRE(result);
  REQUIRE(result.value().converged());
  REQUIRE(result.value().requested_coordinates.size() == 2U);
  CHECK(result.value().requested_coordinates[0].role == AssemblyJointCoordinateRole::Translation);
  CHECK(result.value().requested_coordinates[1].role == AssemblyJointCoordinateRole::Rotation);
  const auto& proposal = proposal_for(result.value().solve_result, "component.b");
  CHECK(proposal.proposed_transform.translation_mm.z == Approx(12.0).margin(kTolerance));
  CHECK(proposal.proposed_transform.rotation_deg.z == Approx(30.0).margin(kTolerance));
  auto applied = AssemblyJointMotionResultApplier{}.apply(project, result.value());
  REQUIRE(applied);
  const auto* joint = project.assembly().find_joint(AssemblyJointId("joint.cylindrical"));
  REQUIRE(joint);
  CHECK(joint->find_coordinate_slot(AssemblyJointCoordinateRole::Translation)
            ->value()
            .millimeters() == 12.0);
  CHECK(joint->find_coordinate_slot(AssemblyJointCoordinateRole::Rotation)->value().degrees() ==
        30.0);

  Project partial =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(partial.assembly().add_joint(
      make_cylindrical_joint("joint.cylindrical", "component.a", "component.b")));
  auto translation_only = AssemblyJointMotionSolver{}.move(
      partial, AssemblyJointDrive{AssemblyJointId("joint.cylindrical"),
                                  {{AssemblyJointCoordinateRole::Translation,
                                    displacement(-8.0, "joint.cylindrical")}}});
  REQUIRE(translation_only);
  const auto& held = proposal_for(translation_only.value().solve_result, "component.b");
  CHECK(held.proposed_transform.translation_mm.z == Approx(-8.0).margin(kTolerance));
  CHECK(held.proposed_transform.rotation_deg.z == Approx(0.0).margin(kTolerance));
}

TEST_CASE("Local vector joint drives validate roles snapshot all slots and apply atomically",
          "[geometry][assembly-vector-joint-drive]"
          "[geometry][assembly-vector-joint-drive-application]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(project.assembly().add_joint(make_joint("joint.vector", "component.a", "component.b")));
  const AssemblyJointMotionSolver solver;
  const AssemblyJointDrive drive{
      AssemblyJointId("joint.vector"),
      {{AssemblyJointCoordinateRole::Rotation, angle(45.0, "joint.vector")}}};

  auto vector_result = solver.move(project, drive);
  auto scalar_result =
      solver.move(project, AssemblyJointId("joint.vector"), angle(45.0, "joint.vector"));
  REQUIRE(vector_result);
  REQUIRE(scalar_result);
  CHECK(vector_result.value() == scalar_result.value());
  REQUIRE(vector_result.value().requested_coordinates.size() == 1U);
  CHECK(vector_result.value().requested_coordinates.front().role ==
        AssemblyJointCoordinateRole::Rotation);
  CHECK(vector_result.value().requested_coordinates.front().requested_value.degrees() == 45.0);
  REQUIRE(vector_result.value().joint_snapshots.size() == 1U);
  CHECK(vector_result.value().joint_snapshots.front().coordinate_slots ==
        project.assembly().find_joint(AssemblyJointId("joint.vector"))->coordinate_slots());

  auto tampered = vector_result.value();
  tampered.requested_coordinates.front().role = AssemblyJointCoordinateRole::RotationNormal;
  CHECK(AssemblyJointMotionResultApplier{}.apply(project, tampered).has_error());
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.vector"))->coordinate_deg() == 0.0);
  CHECK(
      project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform() ==
      identity_rigid_transform());

  auto applied = AssemblyJointMotionResultApplier{}.apply(project, vector_result.value());
  REQUIRE(applied);
  CHECK(project.assembly().find_joint(AssemblyJointId("joint.vector"))->coordinate_deg() == 45.0);

  Project invalid =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"}});
  REQUIRE(invalid.assembly().add_joint(make_joint("joint.vector", "component.a", "component.b")));
  CHECK(solver.move(invalid, AssemblyJointDrive{AssemblyJointId("joint.vector"), {}}).has_error());
  CHECK(solver
            .move(invalid,
                  AssemblyJointDrive{
                      AssemblyJointId("joint.vector"),
                      {{AssemblyJointCoordinateRole::Rotation, angle(10.0, "joint.vector")},
                       {AssemblyJointCoordinateRole::Rotation, angle(20.0, "joint.vector")}}})
            .has_error());
  CHECK(solver
            .move(invalid, AssemblyJointDrive{AssemblyJointId("joint.vector"),
                                              {{AssemblyJointCoordinateRole::RotationNormal,
                                                angle(10.0, "joint.vector")}}})
            .has_error());
  auto wrong_kind = Quantity::length_mm(1.0, "joint.vector");
  REQUIRE(wrong_kind);
  CHECK(solver
            .move(invalid,
                  AssemblyJointDrive{AssemblyJointId("joint.vector"),
                                     {{AssemblyJointCoordinateRole::Rotation, wrong_kind.value()}}})
            .has_error());
}

TEST_CASE("Revolute motion enforces limits grounding activity and suppression boundaries",
          "[geometry][assembly-revolute-joint]") {
  const AssemblyJointMotionSolver solver;

  SECTION("out-of-range coordinates are rejected rather than clamped") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b"}});
    REQUIRE(
        project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
    const auto upper =
        solver.move(project, AssemblyJointId("joint.revolute"), angle(91.0, "joint.revolute"));
    const auto lower =
        solver.move(project, AssemblyJointId("joint.revolute"), angle(-91.0, "joint.revolute"));
    REQUIRE(upper.has_error());
    REQUIRE(lower.has_error());
    CHECK(upper.error().message() ==
          "requested revolute joint coordinate must lie within joint limits");
    CHECK(lower.error().message() ==
          "requested revolute joint coordinate must lie within joint limits");
  }

  SECTION("a surviving motion group requires one grounded component") {
    Project project = make_project({{"component.a"}, {"component.b"}});
    REQUIRE(
        project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
    const auto moved =
        solver.move(project, AssemblyJointId("joint.revolute"), angle(20.0, "joint.revolute"));
    REQUIRE(moved.has_error());
    CHECK(moved.error().message() ==
          "solver connected group requires at least one grounded component");
  }

  SECTION("inactive joints and suppressed selected endpoints cannot be driven") {
    Project inactive = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b"}});
    REQUIRE(inactive.assembly().add_joint(
        make_joint("joint.revolute", "component.a", "component.b", AssemblyJointState::Inactive)));
    const auto inactive_result =
        solver.move(inactive, AssemblyJointId("joint.revolute"), angle(20.0, "joint.revolute"));
    REQUIRE(inactive_result.has_error());
    CHECK(inactive_result.error().message() == "revolute joint motion requires an active joint");

    Project suppressed = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b", identity_rigid_transform(), ComponentGroundingState::Free,
          ComponentSuppressionState::Suppressed}});
    REQUIRE(suppressed.assembly().add_joint(
        make_joint("joint.revolute", "component.a", "component.b")));
    const auto suppressed_result =
        solver.move(suppressed, AssemblyJointId("joint.revolute"), angle(20.0, "joint.revolute"));
    REQUIRE(suppressed_result.has_error());
    CHECK(suppressed_result.error().message() ==
          "revolute joint motion requires active non-suppressed target components");
  }
}

TEST_CASE("Revolute motion filters suppressed related joints and keeps full stale snapshots",
          "[geometry][assembly-revolute-joint]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"},
                    {"component.c", identity_rigid_transform(), ComponentGroundingState::Free,
                     ComponentSuppressionState::Suppressed}});
  REQUIRE(project.assembly().add_joint(make_joint("joint.a", "component.a", "component.b")));
  REQUIRE(project.assembly().add_joint(make_joint("joint.z", "component.b", "component.c")));

  const auto moved =
      AssemblyJointMotionSolver{}.move(project, AssemblyJointId("joint.a"), angle(30.0, "joint.a"));
  REQUIRE(moved);
  REQUIRE(moved.value().converged());
  CHECK(moved.value().solve_result.residual_summary.residual_component_count == 9U);
  CHECK(moved.value().solve_result.component_snapshots.size() == 3U);
  REQUIRE(moved.value().solve_result.proposed_transforms.size() == 1U);
  CHECK(moved.value().solve_result.proposed_transforms.front().component_instance.value() ==
        "component.b");
}

TEST_CASE("Revolute motion result application rejects stale component and joint inputs",
          "[geometry][assembly-revolute-joint]") {
  const AssemblyJointMotionSolver solver;
  const AssemblyJointMotionResultApplier applier;

  SECTION("component solve input changed") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b"}});
    REQUIRE(
        project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
    const auto moved =
        solver.move(project, AssemblyJointId("joint.revolute"), angle(30.0, "joint.revolute"));
    REQUIRE(moved);
    REQUIRE(moved.value().converged());
    REQUIRE(project.assembly().set_component_instance_transform(
        ComponentInstanceId("component.b"), RigidTransform{Vector3{1.0, 0.0, 0.0}, Vector3{}}));
    const auto applied = applier.apply(project, moved.value());
    REQUIRE(applied.has_error());
    CHECK(applied.error().message() ==
          "assembly solve result is stale because component solve input changed");
    CHECK(project.assembly().find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() ==
          0.0);
  }

  SECTION("joint authored coordinate changed") {
    Project project = make_project(
        {{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
         {"component.b"}});
    REQUIRE(
        project.assembly().add_joint(make_joint("joint.revolute", "component.a", "component.b")));
    const auto moved =
        solver.move(project, AssemblyJointId("joint.revolute"), angle(30.0, "joint.revolute"));
    REQUIRE(moved);
    REQUIRE(moved.value().converged());
    REQUIRE(project.assembly().set_joint_coordinate(AssemblyJointId("joint.revolute"),
                                                    angle(10.0, "joint.revolute")));
    const RigidTransform transform =
        project.assembly().find_component_instance(ComponentInstanceId("component.b"))->transform();
    const auto applied = applier.apply(project, moved.value());
    REQUIRE(applied.has_error());
    CHECK(applied.error().message() == "joint motion result is stale because joint input changed");
    CHECK(project.assembly()
              .find_component_instance(ComponentInstanceId("component.b"))
              ->transform() == transform);
    CHECK(project.assembly().find_joint(AssemblyJointId("joint.revolute"))->coordinate_deg() ==
          10.0);
  }
}

TEST_CASE("Revolute motion holds other active joints at persisted coordinates",
          "[geometry][assembly-revolute-joint][geometry][assembly-vector-joint-drive]") {
  Project project =
      make_project({{"component.a", identity_rigid_transform(), ComponentGroundingState::Grounded},
                    {"component.b"},
                    {"component.c"}});
  REQUIRE(project.assembly().add_joint(make_joint("joint.a", "component.a", "component.b")));
  REQUIRE(project.assembly().add_joint(make_joint("joint.z", "component.b", "component.c")));

  const auto moved =
      AssemblyJointMotionSolver{}.move(project, AssemblyJointId("joint.z"), angle(35.0, "joint.z"));
  REQUIRE(moved);
  REQUIRE(moved.value().converged());
  CHECK(moved.value().solve_result.residual_summary.residual_component_count == 18U);
  const auto& proposal_b = proposal_for(moved.value().solve_result, "component.b");
  const auto& proposal_c = proposal_for(moved.value().solve_result, "component.c");
  CHECK(proposal_b.proposed_transform.rotation_deg.z == Approx(0.0).margin(kTolerance));
  CHECK(proposal_c.proposed_transform.rotation_deg.z == Approx(35.0).margin(kTolerance));
}
