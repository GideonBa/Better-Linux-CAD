#include "blcad/geometry/assembly_constraint_target_resolver.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

PartDocument make_face_part(bool include_base_feature = true) {
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

  if (include_base_feature) {
    auto feature = Feature::create_additive_extrude(
        FeatureId("feature.base_extrude"), "BaseExtrude", SketchId("sketch.base"),
        ParameterId("part.thickness"));
    REQUIRE(feature);
    REQUIRE(document.value().add_feature(feature.value()));
  }

  return document.value();
}

Project make_face_project(bool add_part_document = true, bool include_base_feature = true) {
  auto assembly = AssemblyDocument::create(DocumentId("assembly.face"), "FaceAssembly");
  REQUIRE(assembly);
  REQUIRE(assembly.value().add_member_part(DocumentId("part.face_plate")));

  const RigidTransform transform{Vector3{10.0, 20.0, 30.0}, Vector3{5.0, 15.0, 25.0}};
  auto component = ComponentInstance::create(
      ComponentInstanceId("component.face_plate"), "Face Plate", DocumentId("part.face_plate"),
      ComponentVisibility::Visible, ComponentSuppressionState::Active, ComponentGroundingState::Free,
      transform);
  REQUIRE(component);
  REQUIRE(assembly.value().add_component_instance(component.value()));

  auto project = Project::create(DocumentId("project.face"), "FaceProject", assembly.value());
  REQUIRE(project);
  if (add_part_document) {
    REQUIRE(project.value().add_part_document(make_face_part(include_base_feature)));
  }
  return project.value();
}

AssemblyConstraintTarget make_target(const char* component_id, const char* semantic_reference) {
  auto target = AssemblyConstraintTarget::create(ComponentInstanceId(component_id), semantic_reference);
  REQUIRE(target);
  return target.value();
}

} // namespace

TEST_CASE("Assembly target resolver resolves every implemented generated face",
          "[geometry][assembly-target]") {
  const Project project = make_face_project();
  const AssemblyConstraintTargetResolver resolver;

  SECTION("top") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.top"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Top);
    CHECK(resolved.value().local_plane.origin == Point3{0.0, 0.0, 8.0});
    CHECK(resolved.value().local_plane.x_axis == Vector3{1.0, 0.0, 0.0});
    CHECK(resolved.value().local_plane.y_axis == Vector3{0.0, 1.0, 0.0});
    CHECK(resolved.value().local_plane.normal == Vector3{0.0, 0.0, 1.0});
  }

  SECTION("bottom") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.bottom"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Bottom);
    CHECK(resolved.value().local_plane.origin == Point3{0.0, 0.0, 0.0});
    CHECK(resolved.value().local_plane.normal == Vector3{0.0, 0.0, -1.0});
  }

  SECTION("right") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.right"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Right);
    CHECK(resolved.value().local_plane.origin == Point3{60.0, 0.0, 4.0});
    CHECK(resolved.value().local_plane.x_axis == Vector3{0.0, 1.0, 0.0});
    CHECK(resolved.value().local_plane.y_axis == Vector3{0.0, 0.0, 1.0});
    CHECK(resolved.value().local_plane.normal == Vector3{1.0, 0.0, 0.0});
  }

  SECTION("left") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.left"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Left);
    CHECK(resolved.value().local_plane.origin == Point3{-60.0, 0.0, 4.0});
    CHECK(resolved.value().local_plane.normal == Vector3{-1.0, 0.0, 0.0});
  }

  SECTION("front") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.front"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Front);
    CHECK(resolved.value().local_plane.origin == Point3{0.0, 40.0, 4.0});
    CHECK(resolved.value().local_plane.normal == Vector3{0.0, 1.0, 0.0});
  }

  SECTION("back") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.back"));
    REQUIRE(resolved);
    CHECK(resolved.value().face == SemanticFace::Back);
    CHECK(resolved.value().local_plane.origin == Point3{0.0, -40.0, 4.0});
    CHECK(resolved.value().local_plane.normal == Vector3{0.0, -1.0, 0.0});
  }
}

TEST_CASE("Assembly target resolver keeps component placement separate from local geometry",
          "[geometry][assembly-target]") {
  const Project project = make_face_project();
  const AssemblyConstraintTargetResolver resolver;
  const auto resolved = resolver.resolve(
      project, make_target("component.face_plate", "feature.base_extrude.top"));

  REQUIRE(resolved);
  CHECK(resolved.value().component_instance.value() == "component.face_plate");
  CHECK(resolved.value().referenced_part_document.value() == "part.face_plate");
  CHECK(resolved.value().source_feature.value() == "feature.base_extrude");
  CHECK(resolved.value().local_plane.origin == Point3{0.0, 0.0, 8.0});
  CHECK(resolved.value().component_transform.translation_mm == Vector3{10.0, 20.0, 30.0});
  CHECK(resolved.value().component_transform.rotation_deg == Vector3{5.0, 15.0, 25.0});
}

TEST_CASE("Assembly target resolver is deterministic and leaves project intent unchanged",
          "[geometry][assembly-target]") {
  Project project = make_face_project();
  const auto target = make_target("component.face_plate", "feature.base_extrude.top");
  auto constraint = AssemblyConstraint::create(
      AssemblyConstraintId("constraint.face"), "Face Mate", AssemblyConstraintType::Mate, target,
      target);
  REQUIRE(constraint);
  REQUIRE(project.assembly().add_constraint(constraint.value()));

  const RigidTransform transform_before =
      project.assembly().find_component_instance(ComponentInstanceId("component.face_plate"))->transform();
  const std::size_t constraint_count_before = project.assembly().constraint_count();
  const std::size_t workplane_count_before =
      project.find_part_document(DocumentId("part.face_plate"))->derived_workplane_count();
  const std::string semantic_reference_before =
      project.assembly().constraints().front().target_a().semantic_reference();

  const AssemblyConstraintTargetResolver resolver;
  const auto first = resolver.resolve(project, target);
  const auto second = resolver.resolve(project, target);

  REQUIRE(first);
  REQUIRE(second);
  CHECK(first.value() == second.value());
  CHECK(project.assembly().find_component_instance(ComponentInstanceId("component.face_plate"))->transform() ==
        transform_before);
  CHECK(project.assembly().constraint_count() == constraint_count_before);
  CHECK(project.assembly().constraints().front().target_a().semantic_reference() ==
        semantic_reference_before);
  CHECK(project.find_part_document(DocumentId("part.face_plate"))->derived_workplane_count() ==
        workplane_count_before);
}

TEST_CASE("Assembly target resolver rejects missing component, part, and feature targets",
          "[geometry][assembly-target]") {
  const AssemblyConstraintTargetResolver resolver;

  SECTION("missing component") {
    const Project project = make_face_project();
    const auto resolved = resolver.resolve(
        project, make_target("component.missing", "feature.base_extrude.top"));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() ==
          "assembly constraint target component instance must exist in project assembly");
  }

  SECTION("missing referenced part") {
    const Project project = make_face_project(false);
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.top"));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() ==
          "assembly constraint target referenced part must resolve to an owned project part document");
  }

  SECTION("missing source feature") {
    const Project project = make_face_project(true, false);
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.top"));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() ==
          "generated-face assembly target source feature must exist in referenced part document");
  }
}

TEST_CASE("Assembly target resolver rejects malformed and unsupported semantic target families",
          "[geometry][assembly-target]") {
  const Project project = make_face_project();
  const AssemblyConstraintTargetResolver resolver;

  SECTION("malformed token") {
    const auto resolved =
        resolver.resolve(project, make_target("component.face_plate", "feature.base_extrude."));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() == "assembly semantic reference is malformed");
  }

  SECTION("semantic axis family") {
    const auto resolved =
        resolver.resolve(project, make_target("component.face_plate", "bolt.main_axis"));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() == "unsupported assembly semantic reference family");
  }

  SECTION("generated edge family") {
    const auto resolved = resolver.resolve(
        project, make_target("component.face_plate", "feature.base_extrude.edge.top_front"));
    REQUIRE(resolved.has_error());
    CHECK(resolved.error().message() == "unsupported assembly semantic reference family");
  }
}
