#include "assembly_cross_hierarchy_test_support.hpp"
#include "assembly_semantic_target_freshness.hpp"

#include "blcad/core/assembly_reference_target.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace ts = blcad::geometry::test_support;

namespace {

constexpr double kReferenceTargetTolerance = 1.0e-12;

AssemblyConstraintTarget local_target(const char* component, const char* semantic_reference) {
  auto value = AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference);
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraintTarget hierarchy_target(std::initializer_list<const char*> path,
                                                   const char* component,
                                                   const char* semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path)
    occurrence_path.emplace_back(id);
  auto value = AssemblyHierarchyConstraintTarget::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference);
  REQUIRE(value);
  return value.value();
}

template <typename Id> std::string reference_spelling(Id id) {
  auto spelling = make_assembly_reference_target_spelling(AssemblyReferenceTargetIdentity{id});
  REQUIRE(spelling);
  return spelling.value();
}

ComponentInstance component(const char* id, ComponentGroundingState grounding,
                            RigidTransform transform = identity_rigid_transform()) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.block27_plate"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, grounding, transform);
  REQUIRE(value);
  return value.value();
}

SubassemblyInstance occurrence(const char* id, const char* child, RigidTransform transform) {
  auto value = SubassemblyInstance::create(SubassemblyInstanceId(id), id, DocumentId(child),
                                           ComponentVisibility::Visible,
                                           ComponentSuppressionState::Active, transform);
  REQUIRE(value);
  return value.value();
}

Project local_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(root.value().add_component_instance(
      component("component.root", ComponentGroundingState::Free,
                RigidTransform{Vector3{10.0, 20.0, 30.0}, Vector3{5.0, 15.0, 25.0}})));

  auto project =
      Project::create(DocumentId("project.target_taxonomy"), "TargetTaxonomy", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

Project hierarchy_project() {
  auto root = AssemblyDocument::create(DocumentId("assembly.root"), "Root");
  REQUIRE(root);
  REQUIRE(root.value().add_subassembly_instance(
      occurrence("subassembly.left", "assembly.child",
                 RigidTransform{Vector3{100.0, -20.0, 7.0}, Vector3{0.0, 0.0, 90.0}})));

  auto child = AssemblyDocument::create(DocumentId("assembly.child"), "Child");
  REQUIRE(child);
  REQUIRE(child.value().add_member_part(DocumentId("part.block27_plate")));
  REQUIRE(child.value().add_component_instance(
      component("component.child", ComponentGroundingState::Free,
                RigidTransform{Vector3{5.0, 3.0, 11.0}, Vector3{90.0, 0.0, 0.0}})));

  auto project =
      Project::create(DocumentId("project.target_taxonomy.hierarchy"), "Hierarchy", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

AssemblyGeometricTargetSourceMetadata synthetic_metadata(AssemblyGeometricTargetSourceKind kind) {
  AssemblyGeometricTargetSourceMetadata metadata;
  metadata.referenced_part_document = DocumentId("part.synthetic");
  switch (kind) {
  case AssemblyGeometricTargetSourceKind::GeneratedPlanarFace:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.semantic_face = SemanticFace::Top;
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
    break;
  case AssemblyGeometricTargetSourceKind::GeneratedLinearEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedCircularEdge:
  case AssemblyGeometricTargetSourceKind::GeneratedVertex:
    metadata.source_feature = FeatureId("feature.synthetic");
    break;
  case AssemblyGeometricTargetSourceKind::CircularFeatureSeat:
    metadata.source_feature = FeatureId("feature.synthetic");
    metadata.source_profile = ProfileId("profile.synthetic");
    metadata.semantic_axis = SemanticAxis::Primary;
    metadata.semantic_seating_plane = SemanticSeatingPlane::Primary;
    break;
  case AssemblyGeometricTargetSourceKind::DatumPlane:
  case AssemblyGeometricTargetSourceKind::DatumAxis:
  case AssemblyGeometricTargetSourceKind::ConstructionLine:
  case AssemblyGeometricTargetSourceKind::ConstructionPoint:
    break;
  }
  return metadata;
}

AssemblyResolvedGeometricTarget synthetic_target(AssemblyGeometricTargetSourceKind kind,
                                                 AssemblyGeometricTargetDescriptor descriptor) {
  return AssemblyResolvedGeometricTarget{
      AssemblyLocalGeometricTargetEndpointIdentity{ComponentInstanceId("component.synthetic"),
                                                   "synthetic.source"},
      kind,
      synthetic_metadata(kind),
      std::move(descriptor),
      assembly_geometric_target_capabilities(kind),
      AssemblyGeometricTargetCoordinateSpace::ComponentLocal,
      {identity_rigid_transform()}};
}

void check_plane_equal(const AssemblyPlanarTargetDescriptor& typed,
                       const ComponentLocalPlanarDescriptor& compatibility) {
  CHECK(typed.origin == compatibility.origin);
  CHECK(typed.x_axis == compatibility.x_axis);
  CHECK(typed.y_axis == compatibility.y_axis);
  CHECK(typed.normal == compatibility.normal);
}

void check_plane_equal(const AssemblyPlanarTargetDescriptor& typed,
                       const AssemblySpacePlanarDescriptor& compatibility) {
  CHECK(typed.origin == compatibility.origin);
  CHECK(typed.x_axis == compatibility.x_axis);
  CHECK(typed.y_axis == compatibility.y_axis);
  CHECK(typed.normal == compatibility.normal);
}

void check_axis_equal(const AssemblyAxisTargetDescriptor& typed,
                      const ComponentLocalAxisDescriptor& compatibility) {
  CHECK(typed.origin == compatibility.origin);
  CHECK(typed.direction == compatibility.direction);
}

void check_axis_equal(const AssemblyAxisTargetDescriptor& typed,
                      const AssemblySpaceAxisDescriptor& compatibility) {
  CHECK(typed.origin == compatibility.origin);
  CHECK(typed.direction == compatibility.direction);
}

void check_point_near(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kReferenceTargetTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kReferenceTargetTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kReferenceTargetTolerance));
}

void check_vector_near(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kReferenceTargetTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kReferenceTargetTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kReferenceTargetTolerance));
}

void check_reference_source_metadata(const AssemblyResolvedGeometricTarget& target) {
  CHECK(target.source_metadata.referenced_part_document.value() == "part.block27_plate");
  CHECK_FALSE(target.source_metadata.source_feature.has_value());
  CHECK_FALSE(target.source_metadata.source_profile.has_value());
  CHECK_FALSE(target.source_metadata.semantic_face.has_value());
  CHECK_FALSE(target.source_metadata.semantic_axis.has_value());
  CHECK_FALSE(target.source_metadata.semantic_seating_plane.has_value());
}

void check_local_endpoint(const AssemblyResolvedGeometricTarget& target,
                          const std::string& semantic_reference) {
  const auto* endpoint =
      std::get_if<AssemblyLocalGeometricTargetEndpointIdentity>(&target.endpoint);
  REQUIRE(endpoint != nullptr);
  CHECK(endpoint->component_instance.value() == "component.root");
  CHECK(endpoint->semantic_reference == semantic_reference);
}

void check_hierarchy_endpoint(const AssemblyResolvedGeometricTarget& target,
                              const std::string& semantic_reference) {
  const auto* endpoint =
      std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(&target.endpoint);
  REQUIRE(endpoint != nullptr);
  REQUIRE(endpoint->occurrence_path.size() == 1U);
  CHECK(endpoint->occurrence_path.front().value() == "subassembly.left");
  CHECK(endpoint->component_instance.value() == "component.child");
  CHECK(endpoint->semantic_reference == semantic_reference);
}

} // namespace

TEST_CASE("Current assembly semantic sources resolve through typed target taxonomy",
          "[geometry][assembly-geometric-target-taxonomy]") {
  Project project = local_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyConstraintTargetResolver resolver;

  const std::array<std::pair<const char*, SemanticFace>, 6> faces{{
      {"feature.base_extrude.top", SemanticFace::Top},
      {"feature.base_extrude.bottom", SemanticFace::Bottom},
      {"feature.base_extrude.right", SemanticFace::Right},
      {"feature.base_extrude.left", SemanticFace::Left},
      {"feature.base_extrude.front", SemanticFace::Front},
      {"feature.base_extrude.back", SemanticFace::Back},
  }};

  for (const auto& [semantic_reference, semantic_face] : faces) {
    const auto target = local_target("component.root", semantic_reference);
    auto typed = resolver.resolve_geometric(project, target);
    auto compatibility = resolver.resolve(project, target);
    REQUIRE(typed);
    REQUIRE(compatibility);
    CHECK(typed.value().source_kind == AssemblyGeometricTargetSourceKind::GeneratedPlanarFace);
    CHECK(typed.value().capabilities ==
          std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Plane});
    CHECK(typed.value().source_metadata.semantic_face == semantic_face);
    CHECK(typed.value().coordinate_space == AssemblyGeometricTargetCoordinateSpace::ComponentLocal);
    REQUIRE(typed.value().transforms_inner_to_outer.size() == 1U);
    auto plane = project_plane(typed.value());
    REQUIRE(plane);
    check_plane_equal(plane.value(), compatibility.value().local_plane);
  }

  const auto axis_target = local_target("component.root", "feature.hole.axis");
  auto typed_axis = resolver.resolve_geometric(project, axis_target);
  auto compatibility_axis = resolver.resolve_axis(project, axis_target);
  REQUIRE(typed_axis);
  REQUIRE(compatibility_axis);
  CHECK(typed_axis.value().source_kind ==
        AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace);
  CHECK(typed_axis.value().capabilities ==
        (std::vector<AssemblyGeometricTargetCapability>{
            AssemblyGeometricTargetCapability::Axis, AssemblyGeometricTargetCapability::Cylinder}));
  auto axis = project_axis(typed_axis.value());
  auto cylinder = project_cylinder(typed_axis.value());
  REQUIRE(axis);
  REQUIRE(cylinder);
  check_axis_equal(axis.value(), compatibility_axis.value().local_axis);
  CHECK(cylinder.value().radius_mm == Approx(10.0));
  CHECK(project_plane(typed_axis.value()).has_error());

  const auto seat_target = local_target("component.root", "feature.hole.seat");
  auto typed_seat = resolver.resolve_geometric(project, seat_target);
  auto compatibility_seat = resolver.resolve_insert(project, seat_target);
  REQUIRE(typed_seat);
  REQUIRE(compatibility_seat);
  CHECK(typed_seat.value().source_kind == AssemblyGeometricTargetSourceKind::CircularFeatureSeat);
  CHECK(typed_seat.value().capabilities ==
        (std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Plane,
                                                        AssemblyGeometricTargetCapability::Axis,
                                                        AssemblyGeometricTargetCapability::Frame}));
  auto seat_plane = project_plane(typed_seat.value());
  auto seat_axis = project_axis(typed_seat.value());
  auto seat_frame = project_frame(typed_seat.value());
  REQUIRE(seat_plane);
  REQUIRE(seat_axis);
  REQUIRE(seat_frame);
  check_plane_equal(seat_plane.value(), compatibility_seat.value().local_seating_plane);
  check_axis_equal(seat_axis.value(), compatibility_seat.value().local_axis);
  CHECK(seat_frame.value().origin == compatibility_seat.value().local_seating_plane.origin);
  CHECK(seat_frame.value().z_axis == compatibility_seat.value().local_axis.direction);
  CHECK(project_circle(typed_seat.value()).has_error());

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Reference geometry semantic sources resolve through typed target taxonomy",
          "[geometry][assembly-reference-target-resolution]") {
  Project project = local_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyConstraintTargetResolver resolver;

  const std::string plane_reference = reference_spelling(DatumPlaneId("datum.xy"));
  auto typed_plane =
      resolver.resolve_geometric(project, local_target("component.root", plane_reference.c_str()));
  REQUIRE(typed_plane);
  check_local_endpoint(typed_plane.value(), plane_reference);
  check_reference_source_metadata(typed_plane.value());
  CHECK(typed_plane.value().source_kind == AssemblyGeometricTargetSourceKind::DatumPlane);
  CHECK(typed_plane.value().capabilities ==
        std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Plane});
  CHECK(typed_plane.value().coordinate_space ==
        AssemblyGeometricTargetCoordinateSpace::ComponentLocal);
  REQUIRE(typed_plane.value().transforms_inner_to_outer.size() == 1U);
  auto plane = project_plane(typed_plane.value());
  REQUIRE(plane);
  CHECK((plane.value().origin == Point3{}));
  CHECK((plane.value().x_axis == Vector3{1.0, 0.0, 0.0}));
  CHECK((plane.value().y_axis == Vector3{0.0, 1.0, 0.0}));
  CHECK((plane.value().normal == Vector3{0.0, 0.0, 1.0}));
  CHECK(project_axis(typed_plane.value()).has_error());

  const std::string explicit_axis_reference =
      reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  auto typed_axis = resolver.resolve_geometric(
      project, local_target("component.root", explicit_axis_reference.c_str()));
  REQUIRE(typed_axis);
  check_local_endpoint(typed_axis.value(), explicit_axis_reference);
  check_reference_source_metadata(typed_axis.value());
  CHECK(typed_axis.value().source_kind == AssemblyGeometricTargetSourceKind::DatumAxis);
  CHECK(typed_axis.value().capabilities ==
        (std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Axis,
                                                        AssemblyGeometricTargetCapability::Line}));
  auto axis = project_axis(typed_axis.value());
  auto axis_line = project_line(typed_axis.value());
  REQUIRE(axis);
  REQUIRE(axis_line);
  CHECK((axis.value().origin == Point3{-2.0, 5.0, 7.0}));
  CHECK((axis.value().direction == Vector3{0.0, 0.0, 1.0}));
  CHECK((axis_line.value() ==
         AssemblyLineTargetDescriptor{Point3{-2.0, 5.0, 7.0}, Vector3{0.0, 0.0, 1.0}}));
  CHECK(project_plane(typed_axis.value()).has_error());

  const std::string derived_axis_reference =
      reference_spelling(DatumAxisId("datum_axis.from_line"));
  auto typed_derived_axis = resolver.resolve_geometric(
      project, local_target("component.root", derived_axis_reference.c_str()));
  REQUIRE(typed_derived_axis);
  CHECK(typed_derived_axis.value().source_kind == AssemblyGeometricTargetSourceKind::DatumAxis);
  auto derived_axis = project_axis(typed_derived_axis.value());
  auto derived_line = project_line(typed_derived_axis.value());
  REQUIRE(derived_axis);
  REQUIRE(derived_line);
  CHECK((derived_axis.value().origin == Point3{1.0, 2.0, 3.0}));
  CHECK((derived_axis.value().direction == Vector3{0.0, 1.0, 0.0}));
  CHECK((derived_line.value() ==
         AssemblyLineTargetDescriptor{Point3{1.0, 2.0, 3.0}, Vector3{0.0, 1.0, 0.0}}));

  const std::string line_reference =
      reference_spelling(ConstructionLineId("construction_line.axis_y"));
  auto typed_line =
      resolver.resolve_geometric(project, local_target("component.root", line_reference.c_str()));
  REQUIRE(typed_line);
  check_local_endpoint(typed_line.value(), line_reference);
  check_reference_source_metadata(typed_line.value());
  CHECK(typed_line.value().source_kind == AssemblyGeometricTargetSourceKind::ConstructionLine);
  CHECK(typed_line.value().capabilities ==
        std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Line});
  auto line = project_line(typed_line.value());
  REQUIRE(line);
  CHECK((line.value() ==
         AssemblyLineTargetDescriptor{Point3{1.0, 2.0, 3.0}, Vector3{0.0, 1.0, 0.0}}));
  CHECK(project_axis(typed_line.value()).has_error());

  const std::string point_reference =
      reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto typed_point =
      resolver.resolve_geometric(project, local_target("component.root", point_reference.c_str()));
  REQUIRE(typed_point);
  check_local_endpoint(typed_point.value(), point_reference);
  check_reference_source_metadata(typed_point.value());
  CHECK(typed_point.value().source_kind == AssemblyGeometricTargetSourceKind::ConstructionPoint);
  CHECK(typed_point.value().capabilities ==
        std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Point});
  auto point = project_point(typed_point.value());
  REQUIRE(point);
  CHECK((point.value() == AssemblyPointTargetDescriptor{Point3{1.0, 2.0, 3.0}}));
  CHECK(project_line(typed_point.value()).has_error());

  auto malformed = resolver.resolve_geometric(
      project, local_target("component.root", "ref:datum_axis:datum_axis.explicit_z"));
  REQUIRE(malformed.has_error());
  CHECK(malformed.error().message() ==
        "assembly reference target id byte must be unreserved or escaped");

  const std::string missing_axis_reference = reference_spelling(DatumAxisId("datum_axis.missing"));
  auto missing = resolver.resolve_geometric(
      project, local_target("component.root", missing_axis_reference.c_str()));
  REQUIRE(missing.has_error());
  CHECK(missing.error().message() ==
        "datum-axis assembly target must exist in referenced part document");

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Geometric target capability projection follows one deterministic source matrix",
          "[geometry][assembly-geometric-target-taxonomy]") {
  const AssemblyPlanarTargetDescriptor plane{Point3{1.0, 2.0, 3.0}, Vector3{1.0, 0.0, 0.0},
                                             Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}};
  auto planar = synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace, plane);
  REQUIRE(validate_resolved_geometric_target(planar));
  REQUIRE(project_plane(planar));
  CHECK(project_axis(planar).has_error());

  const AssemblyAxisTargetDescriptor datum_axis{Point3{2.0, 3.0, 4.0}, Vector3{0.0, 0.0, 1.0}};
  auto axis_target = synthetic_target(AssemblyGeometricTargetSourceKind::DatumAxis, datum_axis);
  REQUIRE(validate_resolved_geometric_target(axis_target));
  auto projected_axis = project_axis(axis_target);
  auto projected_line = project_line(axis_target);
  REQUIRE(projected_axis);
  REQUIRE(projected_line);
  CHECK(projected_axis.value() == datum_axis);
  CHECK((projected_line.value() ==
         AssemblyLineTargetDescriptor{datum_axis.origin, datum_axis.direction}));

  const AssemblyLineTargetDescriptor line{Point3{4.0, 5.0, 6.0}, Vector3{1.0, 0.0, 0.0}};
  auto line_target = synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionLine, line);
  REQUIRE(project_line(line_target));
  CHECK(project_axis(line_target).has_error());

  const AssemblyPointTargetDescriptor point{Point3{7.0, 8.0, 9.0}};
  auto point_target = synthetic_target(AssemblyGeometricTargetSourceKind::ConstructionPoint, point);
  REQUIRE(project_point(point_target));

  const AssemblyCircularEdgeTargetDescriptor circle{Point3{10.0, 11.0, 12.0},
                                                    Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                                    Vector3{0.0, 0.0, 1.0}, 5.0};
  auto circle_target =
      synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCircularEdge, circle);
  auto projected_circle = project_circle(circle_target);
  auto circle_axis = project_axis(circle_target);
  auto circle_center = project_point(circle_target);
  REQUIRE(projected_circle);
  REQUIRE(circle_axis);
  REQUIRE(circle_center);
  CHECK(projected_circle.value() == circle);
  CHECK((circle_axis.value() == AssemblyAxisTargetDescriptor{circle.center, circle.normal}));
  CHECK((circle_center.value() == AssemblyPointTargetDescriptor{circle.center}));

  const AssemblyCylindricalSurfaceTargetDescriptor cylinder{Point3{13.0, 14.0, 15.0},
                                                            Vector3{0.0, 1.0, 0.0}, 3.0};
  auto cylinder_target =
      synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace, cylinder);
  REQUIRE(project_cylinder(cylinder_target));
  auto cylinder_axis = project_axis(cylinder_target);
  REQUIRE(cylinder_axis);
  CHECK((cylinder_axis.value() ==
         AssemblyAxisTargetDescriptor{cylinder.axis_origin, cylinder.axis_direction}));

  const AssemblyFrameTargetDescriptor frame{Point3{16.0, 17.0, 18.0}, Vector3{1.0, 0.0, 0.0},
                                            Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}};
  auto frame_target =
      synthetic_target(AssemblyGeometricTargetSourceKind::CircularFeatureSeat, frame);
  REQUIRE(project_frame(frame_target));
  REQUIRE(project_plane(frame_target));
  REQUIRE(project_axis(frame_target));
  CHECK(project_line(frame_target).has_error());

  CHECK(assembly_geometric_target_capabilities(
            AssemblyGeometricTargetSourceKind::GeneratedCircularEdge) ==
        (std::vector<AssemblyGeometricTargetCapability>{
            AssemblyGeometricTargetCapability::Axis, AssemblyGeometricTargetCapability::Point,
            AssemblyGeometricTargetCapability::Circle}));
  CHECK(assembly_geometric_target_capabilities(
            AssemblyGeometricTargetSourceKind::CircularFeatureSeat) ==
        (std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Plane,
                                                        AssemblyGeometricTargetCapability::Axis,
                                                        AssemblyGeometricTargetCapability::Frame}));
}

TEST_CASE("Geometric target validation fails closed on malformed descriptors and capability state",
          "[geometry][assembly-geometric-target-taxonomy]") {
  auto wrong_order = synthetic_target(
      AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
      AssemblyFrameTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                    Vector3{0.0, 0.0, 1.0}});
  std::swap(wrong_order.capabilities[0], wrong_order.capabilities[1]);
  CHECK(validate_resolved_geometric_target(wrong_order).has_error());

  auto non_finite =
      synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
                       AssemblyPlanarTargetDescriptor{
                           Point3{std::numeric_limits<double>::infinity(), 0.0, 0.0},
                           Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0}});
  CHECK(project_plane(non_finite).has_error());

  auto wrong_descriptor =
      synthetic_target(AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
                       AssemblyAxisTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 1.0}});
  CHECK(validate_resolved_geometric_target(wrong_descriptor).has_error());

  auto degenerate_axis =
      synthetic_target(AssemblyGeometricTargetSourceKind::DatumAxis,
                       AssemblyAxisTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 0.0}});
  CHECK(project_axis(degenerate_axis).has_error());

  auto zero_radius = synthetic_target(
      AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace,
      AssemblyCylindricalSurfaceTargetDescriptor{Point3{}, Vector3{0.0, 0.0, 1.0}, 0.0});
  CHECK(project_cylinder(zero_radius).has_error());

  auto left_handed_frame = synthetic_target(
      AssemblyGeometricTargetSourceKind::CircularFeatureSeat,
      AssemblyFrameTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                    Vector3{0.0, 0.0, -1.0}});
  CHECK(project_frame(left_handed_frame).has_error());

  auto left_handed_circle = synthetic_target(
      AssemblyGeometricTargetSourceKind::GeneratedCircularEdge,
      AssemblyCircularEdgeTargetDescriptor{Point3{}, Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                           Vector3{0.0, 0.0, -1.0}, 2.0});
  CHECK(project_circle(left_handed_circle).has_error());

  auto non_unit_plane = synthetic_target(
      AssemblyGeometricTargetSourceKind::GeneratedPlanarFace,
      AssemblyPlanarTargetDescriptor{Point3{}, Vector3{2.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0},
                                     Vector3{0.0, 0.0, 1.0}});
  CHECK(project_plane(non_unit_plane).has_error());
}

TEST_CASE("Hierarchy typed target projection preserves existing root-space geometry",
          "[geometry][assembly-geometric-target-taxonomy]") {
  Project project = hierarchy_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyHierarchyConstraintTargetResolver resolver;

  const auto planar_target =
      hierarchy_target({"subassembly.left"}, "component.child", "feature.base_extrude.top");
  auto typed_plane = resolver.resolve_geometric(project, planar_target);
  auto compatibility_plane = resolver.resolve_planar(project, planar_target);
  REQUIRE(typed_plane);
  REQUIRE(compatibility_plane);
  CHECK(typed_plane.value().coordinate_space ==
        AssemblyGeometricTargetCoordinateSpace::RootAssembly);
  REQUIRE(typed_plane.value().transforms_inner_to_outer.size() == 2U);
  const auto* hierarchy_identity =
      std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(&typed_plane.value().endpoint);
  REQUIRE(hierarchy_identity != nullptr);
  REQUIRE(hierarchy_identity->occurrence_path.size() == 1U);
  CHECK(hierarchy_identity->occurrence_path.front().value() == "subassembly.left");
  CHECK(hierarchy_identity->component_instance.value() == "component.child");
  CHECK(hierarchy_identity->semantic_reference == "feature.base_extrude.top");
  auto plane = project_plane(typed_plane.value());
  REQUIRE(plane);
  check_plane_equal(plane.value(), compatibility_plane.value().plane);

  const auto axis_target =
      hierarchy_target({"subassembly.left"}, "component.child", "feature.hole.axis");
  auto typed_axis = resolver.resolve_geometric(project, axis_target);
  auto compatibility_axis = resolver.resolve_axis(project, axis_target);
  REQUIRE(typed_axis);
  REQUIRE(compatibility_axis);
  auto axis = project_axis(typed_axis.value());
  REQUIRE(axis);
  check_axis_equal(axis.value(), compatibility_axis.value().axis);
  auto cylinder = project_cylinder(typed_axis.value());
  REQUIRE(cylinder);
  CHECK(cylinder.value().radius_mm == Approx(10.0));

  const auto seat_target =
      hierarchy_target({"subassembly.left"}, "component.child", "feature.hole.seat");
  auto typed_seat = resolver.resolve_geometric(project, seat_target);
  auto compatibility_seat = resolver.resolve_insert(project, seat_target);
  REQUIRE(typed_seat);
  REQUIRE(compatibility_seat);
  auto seat_axis = project_axis(typed_seat.value());
  auto seat_plane = project_plane(typed_seat.value());
  REQUIRE(seat_axis);
  REQUIRE(seat_plane);
  check_axis_equal(seat_axis.value(), compatibility_seat.value().axis);
  check_plane_equal(seat_plane.value(), compatibility_seat.value().seating_plane);

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Hierarchy reference geometry targets preserve exact transform chains",
          "[geometry][assembly-reference-target-resolution]") {
  Project project = hierarchy_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyHierarchyConstraintTargetResolver resolver;

  const std::string plane_reference = reference_spelling(DatumPlaneId("datum.xy"));
  auto typed_plane = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", plane_reference.c_str()));
  REQUIRE(typed_plane);
  check_hierarchy_endpoint(typed_plane.value(), plane_reference);
  check_reference_source_metadata(typed_plane.value());
  CHECK(typed_plane.value().source_kind == AssemblyGeometricTargetSourceKind::DatumPlane);
  CHECK(typed_plane.value().coordinate_space ==
        AssemblyGeometricTargetCoordinateSpace::RootAssembly);
  REQUIRE(typed_plane.value().transforms_inner_to_outer.size() == 2U);
  auto plane = project_plane(typed_plane.value());
  REQUIRE(plane);
  check_point_near(plane.value().origin, Point3{97.0, -15.0, 18.0});
  check_vector_near(plane.value().x_axis, Vector3{0.0, 1.0, 0.0});
  check_vector_near(plane.value().y_axis, Vector3{0.0, 0.0, 1.0});
  check_vector_near(plane.value().normal, Vector3{1.0, 0.0, 0.0});

  const std::string axis_reference = reference_spelling(DatumAxisId("datum_axis.explicit_z"));
  auto typed_axis = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", axis_reference.c_str()));
  REQUIRE(typed_axis);
  check_hierarchy_endpoint(typed_axis.value(), axis_reference);
  check_reference_source_metadata(typed_axis.value());
  CHECK(typed_axis.value().source_kind == AssemblyGeometricTargetSourceKind::DatumAxis);
  auto axis = project_axis(typed_axis.value());
  auto axis_line = project_line(typed_axis.value());
  REQUIRE(axis);
  REQUIRE(axis_line);
  check_point_near(axis.value().origin, Point3{104.0, -17.0, 23.0});
  check_vector_near(axis.value().direction, Vector3{1.0, 0.0, 0.0});
  check_point_near(axis_line.value().origin, Point3{104.0, -17.0, 23.0});
  check_vector_near(axis_line.value().direction, Vector3{1.0, 0.0, 0.0});

  const std::string line_reference =
      reference_spelling(ConstructionLineId("construction_line.axis_y"));
  auto typed_line = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", line_reference.c_str()));
  REQUIRE(typed_line);
  check_hierarchy_endpoint(typed_line.value(), line_reference);
  CHECK(typed_line.value().source_kind == AssemblyGeometricTargetSourceKind::ConstructionLine);
  auto line = project_line(typed_line.value());
  REQUIRE(line);
  check_point_near(line.value().origin, Point3{100.0, -14.0, 20.0});
  check_vector_near(line.value().direction, Vector3{0.0, 0.0, 1.0});

  const std::string point_reference =
      reference_spelling(ConstructionPointId("construction_point.anchor"));
  auto typed_point = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", point_reference.c_str()));
  REQUIRE(typed_point);
  check_hierarchy_endpoint(typed_point.value(), point_reference);
  CHECK(typed_point.value().source_kind == AssemblyGeometricTargetSourceKind::ConstructionPoint);
  auto point = project_point(typed_point.value());
  REQUIRE(point);
  check_point_near(point.value().point, Point3{100.0, -14.0, 20.0});

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Reference target source edits are protected by canonical PartDocument freshness",
          "[geometry][assembly-reference-target-resolution]") {
  const std::string point_reference =
      reference_spelling(ConstructionPointId("construction_point.anchor"));

  SECTION("Local target") {
    Project project = local_project();
    const AssemblyConstraintTargetResolver resolver;
    auto before = resolver.resolve_geometric(
        project, local_target("component.root", point_reference.c_str()));
    REQUIRE(before);
    auto before_point = project_point(before.value());
    REQUIRE(before_point);
    CHECK((before_point.value() == AssemblyPointTargetDescriptor{Point3{1.0, 2.0, 3.0}}));

    auto snapshots =
        detail::make_semantic_target_part_snapshots(project, {DocumentId("part.block27_plate")});
    REQUIRE(snapshots);
    REQUIRE(detail::validate_semantic_target_part_snapshots(
        project, {DocumentId("part.block27_plate")}, snapshots.value()));

    REQUIRE_FALSE(project.part_documents().empty());
    project.part_documents().front() = ts::solver_part(Point3{9.0, 2.0, 3.0});
    REQUIRE(project.validate_assembly_structure());

    auto after = resolver.resolve_geometric(
        project, local_target("component.root", point_reference.c_str()));
    REQUIRE(after);
    auto after_point = project_point(after.value());
    REQUIRE(after_point);
    CHECK((after_point.value() == AssemblyPointTargetDescriptor{Point3{9.0, 2.0, 3.0}}));

    auto stale = detail::validate_semantic_target_part_snapshots(
        project, {DocumentId("part.block27_plate")}, snapshots.value());
    REQUIRE(stale.has_error());
    CHECK(stale.error().message() == "assembly solve result is stale because semantic "
                                     "target-producing part model intent changed");
  }

  SECTION("Hierarchy target") {
    Project project = hierarchy_project();
    const AssemblyHierarchyConstraintTargetResolver resolver;
    auto before = resolver.resolve_geometric(
        project,
        hierarchy_target({"subassembly.left"}, "component.child", point_reference.c_str()));
    REQUIRE(before);
    auto before_point = project_point(before.value());
    REQUIRE(before_point);
    check_point_near(before_point.value().point, Point3{100.0, -14.0, 20.0});

    auto snapshots =
        detail::make_semantic_target_part_snapshots(project, {DocumentId("part.block27_plate")});
    REQUIRE(snapshots);

    REQUIRE_FALSE(project.part_documents().empty());
    project.part_documents().front() = ts::solver_part(Point3{9.0, 2.0, 3.0});
    REQUIRE(project.validate_assembly_structure());

    auto after = resolver.resolve_geometric(
        project,
        hierarchy_target({"subassembly.left"}, "component.child", point_reference.c_str()));
    REQUIRE(after);
    auto after_point = project_point(after.value());
    REQUIRE(after_point);
    check_point_near(after_point.value().point, Point3{100.0, -6.0, 20.0});

    auto stale = detail::validate_semantic_target_part_snapshots(
        project, {DocumentId("part.block27_plate")}, snapshots.value());
    REQUIRE(stale.has_error());
  }
}
