#pragma once
#include "blcad/core/project.hpp"
#include "blcad/core/project_design_reference.hpp"
#include <catch2/catch_test_macros.hpp>

namespace blcad::test {
inline PartDocument adaptive_plate(const char* id, bool supported = false) {
  auto part = PartDocument::create(DocumentId(id), id);
  REQUIRE(part);
  for (const auto& entry : {std::pair{"width", 20.0}, {"height", 10.0}, {"length", 4.0}}) {
    auto value = Quantity::length_mm(entry.second);
    REQUIRE(value);
    auto parameter = Parameter::create_length(ParameterId(entry.first), entry.first, value.value());
    REQUIRE(parameter);
    REQUIRE(part.value().add_parameter(parameter.value()));
  }
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(part.value().add_datum_plane(xy.value()));
  auto plane = ConstructionPlane::create_explicit(
      ConstructionPlaneId("linked.plane"), "Linked support", {}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1});
  REQUIRE(plane);
  REQUIRE(part.value().add_construction_plane(plane.value()));
  auto point =
      ConstructionPoint::create_explicit(ConstructionPointId("linked.point"), "Linked point", {});
  REQUIRE(point);
  REQUIRE(part.value().add_construction_point(point.value()));
  auto sketch = Sketch::create(SketchId("profile"), "Plate profile",
                               supported ? plane.value().workplane_id() : xy.value().id());
  REQUIRE(sketch);
  auto rectangle =
      RectangleProfile::create(ProfileId("rectangle"), ParameterId("width"), ParameterId("height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(part.value().add_sketch(sketch.value()));
  auto feature = Feature::create_additive_extrude(FeatureId("base"), "Base", SketchId("profile"),
                                                  ParameterId("length"));
  REQUIRE(feature);
  auto body = Body::create(BodyId("body.base"), "Plate", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(part.value().add_body(body.value()));
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  body.value().id());
  REQUIRE(context);
  auto owned = feature.value().with_body_result_context(context.value());
  REQUIRE(owned);
  REQUIRE(part.value().add_feature(owned.value()));
  return part.value();
}
inline Project adaptive_project() {
  auto root = AssemblyDocument::create(DocumentId("root"), "Adaptive assembly");
  REQUIRE(root);
  for (const char* id : {"source", "cover", "irrelevant"})
    REQUIRE(root.value().add_member_part(DocumentId(id)));
  for (const char* id : {"source", "cover", "irrelevant", "source.copy"}) {
    const std::string name(id);
    const bool source = name == "source", cover = name == "cover", copy = name == "source.copy";
    const RigidTransform pose = source  ? RigidTransform{{20, 30, 0}, {0, 0, 90}}
                                : cover ? RigidTransform{{-10, 5, 0}, {0, 0, 180}}
                                : copy  ? RigidTransform{{300, 0, 0}, {}}
                                        : RigidTransform{{1000, 0, 0}, {}};
    auto component = ComponentInstance::create(
        ComponentInstanceId(id), id, DocumentId(copy ? "source" : id), ComponentVisibility::Visible,
        ComponentSuppressionState::Active, ComponentGroundingState::Grounded, pose);
    REQUIRE(component);
    REQUIRE(root.value().add_component_instance(component.value()));
  }
  auto project = Project::create(DocumentId("project.adaptive"), "Adaptive", root.value());
  REQUIRE(project);
  REQUIRE(project.value().add_part_document(adaptive_plate("source")));
  REQUIRE(project.value().add_part_document(adaptive_plate("cover", true)));
  REQUIRE(project.value().add_part_document(adaptive_plate("irrelevant")));
  return project.value();
}
inline ProjectDesignReference adaptive_plane_reference(const char* id = "cover.support",
                                                       const char* source = "source",
                                                       const char* target = "cover") {
  auto ref = ProjectDesignReference::create(
      ProjectDesignReferenceId(id), "Source top support",
      {{}, ComponentInstanceId(source), DocumentId(source)}, primary_model_state_id(),
      ProjectDesignSemanticSource{"base.top"},
      {{}, ComponentInstanceId(target), DocumentId(target)}, ConstructionPlaneId("linked.plane"));
  REQUIRE(ref);
  return ref.value();
}
} // namespace blcad::test
