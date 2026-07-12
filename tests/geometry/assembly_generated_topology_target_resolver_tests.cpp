#include "assembly_cross_hierarchy_test_support.hpp"

#include "blcad/core/datum_plane.hpp"
#include "blcad/core/generated_topology_reference.hpp"
#include "blcad/core/project_json.hpp"
#include "blcad/geometry/assembly_constraint_target_resolver.hpp"
#include "blcad/geometry/assembly_geometric_target.hpp"
#include "blcad/geometry/assembly_hierarchy_constraint_equation_builder.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;
using Catch::Approx;
namespace ts = blcad::geometry::test_support;

namespace {

constexpr double kTolerance = 1.0e-9;
constexpr const char* kBoxFeature = "feature.base_extrude";
constexpr const char* kHoleFeature = "feature.hole";
constexpr const char* kHoleProfile = "profile.hole";

AssemblyConstraintTarget local_target(const char* component, const std::string& semantic_reference) {
  auto value =
      AssemblyConstraintTarget::create(ComponentInstanceId(component), semantic_reference.c_str());
  REQUIRE(value);
  return value.value();
}

AssemblyHierarchyConstraintTarget hierarchy_target(std::initializer_list<const char*> path,
                                                   const char* component,
                                                   const std::string& semantic_reference) {
  std::vector<SubassemblyInstanceId> occurrence_path;
  for (const char* id : path)
    occurrence_path.emplace_back(id);
  auto value = AssemblyHierarchyConstraintTarget::create(
      std::move(occurrence_path), ComponentInstanceId(component), semantic_reference.c_str());
  REQUIRE(value);
  return value.value();
}

std::string topo_spelling(GeneratedTopologyReferenceIdentity identity) {
  auto spelling = make_generated_topology_target_spelling(identity);
  REQUIRE(spelling);
  return spelling.value();
}

std::string linear_edge_spelling(SemanticEdge edge, const char* feature = kBoxFeature) {
  auto reference = SemanticEdgeReference::create(FeatureId(feature), edge);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

std::string vertex_spelling(SemanticVertex vertex, const char* feature = kBoxFeature) {
  auto reference = SemanticVertexReference::create(FeatureId(feature), vertex);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

std::string cylindrical_face_spelling(const char* feature = kHoleFeature,
                                      const char* profile = kHoleProfile) {
  auto reference = SemanticCylindricalFaceReference::create(FeatureId(feature), ProfileId(profile),
                                                            SemanticCylindricalFace::Wall);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

std::string circular_edge_spelling(SemanticCircularEdge edge, const char* feature = kHoleFeature,
                                   const char* profile = kHoleProfile) {
  auto reference = SemanticCircularEdgeReference::create(FeatureId(feature), ProfileId(profile), edge);
  REQUIRE(reference);
  return topo_spelling(GeneratedTopologyReferenceIdentity{std::move(reference.value())});
}

ComponentInstance component(const char* id, RigidTransform transform) {
  auto value = ComponentInstance::create(
      ComponentInstanceId(id), id, DocumentId("part.block27_plate"), ComponentVisibility::Visible,
      ComponentSuppressionState::Active, ComponentGroundingState::Free, transform);
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
      component("component.root",
                RigidTransform{Vector3{10.0, 20.0, 30.0}, Vector3{5.0, 15.0, 25.0}})));

  auto project =
      Project::create(DocumentId("project.generated_topology"), "GeneratedTopology", root.value());
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
      component("component.child",
                RigidTransform{Vector3{5.0, 3.0, 11.0}, Vector3{90.0, 0.0, 0.0}})));

  auto project = Project::create(DocumentId("project.generated_topology.hierarchy"), "Hierarchy",
                                 root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_child_assembly_document(child.value()));
  REQUIRE(project.value().add_part_document(ts::solver_part()));
  REQUIRE(project.value().validate_assembly_structure());
  return project.value();
}

void check_point_near(const Point3& actual, const Point3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

void check_vector_near(const Vector3& actual, const Vector3& expected) {
  CHECK(actual.x == Approx(expected.x).margin(kTolerance));
  CHECK(actual.y == Approx(expected.y).margin(kTolerance));
  CHECK(actual.z == Approx(expected.z).margin(kTolerance));
}

} // namespace

TEST_CASE("Generated rectangular linear edges resolve to component-local lines",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = local_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyConstraintTargetResolver resolver;

  const std::array<std::pair<SemanticEdge, AssemblyLineTargetDescriptor>, 12> edges{{
      {SemanticEdge::TopFront, {Point3{0.0, 40.0, 8.0}, Vector3{1.0, 0.0, 0.0}}},
      {SemanticEdge::TopBack, {Point3{0.0, -40.0, 8.0}, Vector3{1.0, 0.0, 0.0}}},
      {SemanticEdge::TopRight, {Point3{60.0, 0.0, 8.0}, Vector3{0.0, 1.0, 0.0}}},
      {SemanticEdge::TopLeft, {Point3{-60.0, 0.0, 8.0}, Vector3{0.0, 1.0, 0.0}}},
      {SemanticEdge::BottomFront, {Point3{0.0, 40.0, 0.0}, Vector3{1.0, 0.0, 0.0}}},
      {SemanticEdge::BottomBack, {Point3{0.0, -40.0, 0.0}, Vector3{1.0, 0.0, 0.0}}},
      {SemanticEdge::BottomRight, {Point3{60.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}}},
      {SemanticEdge::BottomLeft, {Point3{-60.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}}},
      {SemanticEdge::FrontRight, {Point3{60.0, 40.0, 4.0}, Vector3{0.0, 0.0, 1.0}}},
      {SemanticEdge::FrontLeft, {Point3{-60.0, 40.0, 4.0}, Vector3{0.0, 0.0, 1.0}}},
      {SemanticEdge::BackRight, {Point3{60.0, -40.0, 4.0}, Vector3{0.0, 0.0, 1.0}}},
      {SemanticEdge::BackLeft, {Point3{-60.0, -40.0, 4.0}, Vector3{0.0, 0.0, 1.0}}},
  }};

  for (const auto& [edge, expected] : edges) {
    const std::string reference = linear_edge_spelling(edge);
    auto typed = resolver.resolve_geometric(project, local_target("component.root", reference));
    REQUIRE(typed);
    CHECK(typed.value().source_kind == AssemblyGeometricTargetSourceKind::GeneratedLinearEdge);
    CHECK(typed.value().capabilities ==
          std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Line});
    CHECK(typed.value().coordinate_space == AssemblyGeometricTargetCoordinateSpace::ComponentLocal);
    CHECK(typed.value().source_metadata.source_feature == FeatureId(kBoxFeature));
    CHECK_FALSE(typed.value().source_metadata.source_profile.has_value());
    REQUIRE(typed.value().transforms_inner_to_outer.size() == 1U);
    auto line = project_line(typed.value());
    REQUIRE(line);
    check_point_near(line.value().origin, expected.origin);
    check_vector_near(line.value().direction, expected.direction);
    CHECK(project_axis(typed.value()).has_error());
    CHECK(project_point(typed.value()).has_error());
  }

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Generated rectangular vertices resolve to component-local points",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = local_project();
  const AssemblyConstraintTargetResolver resolver;

  const std::array<std::pair<SemanticVertex, Point3>, 8> vertices{{
      {SemanticVertex::TopFrontRight, Point3{60.0, 40.0, 8.0}},
      {SemanticVertex::TopFrontLeft, Point3{-60.0, 40.0, 8.0}},
      {SemanticVertex::TopBackRight, Point3{60.0, -40.0, 8.0}},
      {SemanticVertex::TopBackLeft, Point3{-60.0, -40.0, 8.0}},
      {SemanticVertex::BottomFrontRight, Point3{60.0, 40.0, 0.0}},
      {SemanticVertex::BottomFrontLeft, Point3{-60.0, 40.0, 0.0}},
      {SemanticVertex::BottomBackRight, Point3{60.0, -40.0, 0.0}},
      {SemanticVertex::BottomBackLeft, Point3{-60.0, -40.0, 0.0}},
  }};

  for (const auto& [vertex, expected] : vertices) {
    const std::string reference = vertex_spelling(vertex);
    auto typed = resolver.resolve_geometric(project, local_target("component.root", reference));
    REQUIRE(typed);
    CHECK(typed.value().source_kind == AssemblyGeometricTargetSourceKind::GeneratedVertex);
    CHECK(typed.value().capabilities ==
          std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Point});
    CHECK(typed.value().source_metadata.source_feature == FeatureId(kBoxFeature));
    auto point = project_point(typed.value());
    REQUIRE(point);
    check_point_near(point.value().point, expected);
    CHECK(project_line(typed.value()).has_error());
  }
}

TEST_CASE("Generated cylindrical face resolves to axis and cylinder capabilities",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = local_project();
  const AssemblyConstraintTargetResolver resolver;

  const std::string reference = cylindrical_face_spelling();
  auto typed = resolver.resolve_geometric(project, local_target("component.root", reference));
  REQUIRE(typed);
  CHECK(typed.value().source_kind == AssemblyGeometricTargetSourceKind::GeneratedCylindricalFace);
  CHECK(typed.value().capabilities ==
        (std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Axis,
                                                        AssemblyGeometricTargetCapability::Cylinder}));
  CHECK(typed.value().source_metadata.source_feature == FeatureId(kHoleFeature));
  CHECK(typed.value().source_metadata.source_profile == ProfileId(kHoleProfile));

  auto axis = project_axis(typed.value());
  auto cylinder = project_cylinder(typed.value());
  REQUIRE(axis);
  REQUIRE(cylinder);
  check_point_near(axis.value().origin, Point3{4.0, -6.0, 0.0});
  check_vector_near(axis.value().direction, Vector3{0.0, 0.0, 1.0});
  check_point_near(cylinder.value().axis_origin, Point3{4.0, -6.0, 0.0});
  check_vector_near(cylinder.value().axis_direction, Vector3{0.0, 0.0, 1.0});
  CHECK(cylinder.value().radius_mm == Approx(10.0));
  CHECK(project_plane(typed.value()).has_error());
  CHECK(project_circle(typed.value()).has_error());

  // The canonical topo: cylindrical wall matches the current narrow legacy
  // `.axis` producer for the same single-circle subtractive feature.
  auto legacy = resolver.resolve_geometric(project, local_target("component.root", "feature.hole.axis"));
  REQUIRE(legacy);
  auto legacy_cylinder = project_cylinder(legacy.value());
  REQUIRE(legacy_cylinder);
  CHECK(legacy_cylinder.value() == cylinder.value());
}

TEST_CASE("Generated circular edges resolve to coaxial rims with center and axis",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = local_project();
  const AssemblyConstraintTargetResolver resolver;

  struct RimCase {
    SemanticCircularEdge edge;
    Point3 center;
  };
  const std::array<RimCase, 2> rims{{
      {SemanticCircularEdge::SourceRim, Point3{4.0, -6.0, 0.0}},
      {SemanticCircularEdge::OppositeRim, Point3{4.0, -6.0, 8.0}},
  }};

  for (const RimCase& rim : rims) {
    const std::string reference = circular_edge_spelling(rim.edge);
    auto typed = resolver.resolve_geometric(project, local_target("component.root", reference));
    REQUIRE(typed);
    CHECK(typed.value().source_kind == AssemblyGeometricTargetSourceKind::GeneratedCircularEdge);
    CHECK(typed.value().capabilities ==
          (std::vector<AssemblyGeometricTargetCapability>{AssemblyGeometricTargetCapability::Axis,
                                                          AssemblyGeometricTargetCapability::Point,
                                                          AssemblyGeometricTargetCapability::Circle}));
    CHECK(typed.value().source_metadata.source_profile == ProfileId(kHoleProfile));

    auto circle = project_circle(typed.value());
    auto axis = project_axis(typed.value());
    auto center = project_point(typed.value());
    REQUIRE(circle);
    REQUIRE(axis);
    REQUIRE(center);
    check_point_near(circle.value().center, rim.center);
    CHECK(circle.value().radius_mm == Approx(10.0));
    check_vector_near(circle.value().normal, Vector3{0.0, 0.0, 1.0});
    check_point_near(axis.value().origin, rim.center);
    check_vector_near(axis.value().direction, Vector3{0.0, 0.0, 1.0});
    check_point_near(center.value().point, rim.center);
    CHECK(project_cylinder(typed.value()).has_error());
  }
}

TEST_CASE("Generated topology target resolution fails closed on unsupported identity",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = local_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyConstraintTargetResolver resolver;

  // Family/role unsupported by the source feature producer (linear edge on a
  // single-circle subtractive extrude).
  auto wrong_family = resolver.resolve_geometric(
      project, local_target("component.root", linear_edge_spelling(SemanticEdge::TopFront, kHoleFeature)));
  REQUIRE(wrong_family.has_error());

  // Wrong source profile id fails Block-35 identity validation.
  auto wrong_profile = resolver.resolve_geometric(
      project, local_target("component.root", cylindrical_face_spelling(kHoleFeature, "profile.wrong")));
  REQUIRE(wrong_profile.has_error());

  // Missing source feature fails producer classification.
  auto missing_feature = resolver.resolve_geometric(
      project, local_target("component.root", vertex_spelling(SemanticVertex::TopFrontRight, "feature.missing")));
  REQUIRE(missing_feature.has_error());

  // Malformed topo: role token fails canonical parsing before resolution.
  auto malformed = resolver.resolve_geometric(
      project, local_target("component.root", "topo:linear_edge:feature%2Ebase_extrude:not_a_role"));
  REQUIRE(malformed.has_error());

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Generated topology targets resolve through exact rooted transform chains",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = hierarchy_project();
  const auto before = serialize_project_to_json(project);
  REQUIRE(before);
  const AssemblyHierarchyConstraintTargetResolver resolver;

  auto typed_vertex = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child",
                                vertex_spelling(SemanticVertex::BottomBackLeft)));
  REQUIRE(typed_vertex);
  CHECK(typed_vertex.value().coordinate_space == AssemblyGeometricTargetCoordinateSpace::RootAssembly);
  REQUIRE(typed_vertex.value().transforms_inner_to_outer.size() == 2U);
  const auto* endpoint = std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(
      &typed_vertex.value().endpoint);
  REQUIRE(endpoint != nullptr);
  REQUIRE(endpoint->occurrence_path.size() == 1U);
  CHECK(endpoint->occurrence_path.front().value() == "subassembly.left");
  CHECK(endpoint->component_instance.value() == "component.child");
  auto vertex_point = project_point(typed_vertex.value());
  REQUIRE(vertex_point);
  check_point_near(vertex_point.value().point, Point3{97.0, -75.0, -22.0});

  auto typed_cylinder = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", cylindrical_face_spelling()));
  REQUIRE(typed_cylinder);
  auto axis = project_axis(typed_cylinder.value());
  auto cylinder = project_cylinder(typed_cylinder.value());
  REQUIRE(axis);
  REQUIRE(cylinder);
  check_point_near(axis.value().origin, Point3{97.0, -11.0, 12.0});
  check_vector_near(axis.value().direction, Vector3{1.0, 0.0, 0.0});
  CHECK(cylinder.value().radius_mm == Approx(10.0));

  auto typed_rim = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child",
                                circular_edge_spelling(SemanticCircularEdge::OppositeRim)));
  REQUIRE(typed_rim);
  auto rim_center = project_point(typed_rim.value());
  REQUIRE(rim_center);
  check_point_near(rim_center.value().point, Point3{105.0, -11.0, 12.0});

  const auto after = serialize_project_to_json(project);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Repeated rooted occurrences share one target but keep distinct root-space geometry",
          "[geometry][assembly-generated-topology-target-resolution]") {
  Project project = ts::distance_project(20.0, /*repeated=*/true);
  const AssemblyHierarchyConstraintTargetResolver resolver;
  const std::string reference = vertex_spelling(SemanticVertex::BottomBackLeft);

  auto left = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.left"}, "component.child", reference));
  auto right = resolver.resolve_geometric(
      project, hierarchy_target({"subassembly.right"}, "component.child", reference));
  REQUIRE(left);
  REQUIRE(right);

  auto left_point = project_point(left.value());
  auto right_point = project_point(right.value());
  REQUIRE(left_point);
  REQUIRE(right_point);
  check_point_near(left_point.value().point, Point3{-60.0, -40.0, 0.0});
  check_point_near(right_point.value().point, Point3{-60.0, -40.0, 10.0});

  const auto* left_endpoint =
      std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(&left.value().endpoint);
  const auto* right_endpoint =
      std::get_if<AssemblyHierarchyGeometricTargetEndpointIdentity>(&right.value().endpoint);
  REQUIRE(left_endpoint != nullptr);
  REQUIRE(right_endpoint != nullptr);
  CHECK(left_endpoint->semantic_reference == right_endpoint->semantic_reference);
  CHECK(left_endpoint->component_instance == right_endpoint->component_instance);
  CHECK(left_endpoint->occurrence_path != right_endpoint->occurrence_path);
}
