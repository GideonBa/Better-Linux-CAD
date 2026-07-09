#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace blcad;

namespace {

Parameter make_length_parameter(const char* id, const char* name, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);

  auto parameter = Parameter::create_length(ParameterId(id), name, quantity.value());
  REQUIRE(parameter);

  return parameter.value();
}

PartDocument make_construction_geometry_document() {
  auto document = PartDocument::create(DocumentId("part.construction_json"), "ConstructionJson");
  REQUIRE(document);

  REQUIRE(document.value().add_parameter(make_length_parameter("part.width", "width", 40.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.height", "height", 20.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.depth", "depth", 10.0)));
  REQUIRE(document.value().add_parameter(make_length_parameter("part.plane_offset", "plane_offset", 25.0)));

  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  auto point = ConstructionPoint::create_explicit(
      ConstructionPointId("construction_point.anchor"), "Anchor", Point3{0.0, 0.0, 25.0},
      {ParameterId("part.plane_offset")});
  REQUIRE(point);
  REQUIRE(document.value().add_construction_point(point.value()));

  auto line = ConstructionLine::create_explicit(
      ConstructionLineId("construction_line.axis_z"), "AxisZ", Point3{0.0, 0.0, 0.0},
      Vector3{0.0, 0.0, 1.0});
  REQUIRE(line);
  REQUIRE(document.value().add_construction_line(line.value()));

  auto plane = ConstructionPlane::create_explicit(
      ConstructionPlaneId("construction_plane.offset_xy"), "OffsetXY", Point3{0.0, 0.0, 25.0},
      Vector3{1.0, 0.0, 0.0}, Vector3{0.0, 1.0, 0.0}, Vector3{0.0, 0.0, 1.0},
      {ParameterId("part.plane_offset")});
  REQUIRE(plane);
  REQUIRE(document.value().add_construction_plane(plane.value()));

  auto sketch = Sketch::create(SketchId("sketch.on_construction_plane"), "OnConstructionPlane",
                               DatumPlaneId("construction_plane.offset_xy"));
  REQUIRE(sketch);
  auto rectangle = RectangleProfile::create(ProfileId("profile.rectangle"), ParameterId("part.width"),
                                            ParameterId("part.height"));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  REQUIRE(document.value().add_sketch(sketch.value()));

  auto feature = Feature::create_additive_extrude(
      FeatureId("feature.construction_plane_prism"), "ConstructionPlanePrism",
      SketchId("sketch.on_construction_plane"), ParameterId("part.depth"));
  REQUIRE(feature);
  REQUIRE(document.value().add_feature(feature.value()));

  return document.value();
}

} // namespace

TEST_CASE("PartDocument JSON round-trips explicit construction geometry",
          "[core][json][construction_geometry]") {
  const PartDocument document = make_construction_geometry_document();

  const auto serialized = serialize_part_document_to_json(document);

  REQUIRE(serialized);
  CHECK(serialized.value().find("construction_points") != std::string::npos);
  CHECK(serialized.value().find("construction_lines") != std::string::npos);
  CHECK(serialized.value().find("construction_planes") != std::string::npos);
  CHECK(serialized.value().find("construction_plane.offset_xy") != std::string::npos);
  CHECK(serialized.value().find("parameter_dependencies") != std::string::npos);

  const auto restored = deserialize_part_document_from_json(serialized.value());

  REQUIRE(restored);
  CHECK(restored.value().construction_point_count() == 1);
  CHECK(restored.value().construction_line_count() == 1);
  CHECK(restored.value().construction_plane_count() == 1);
  CHECK(restored.value().sketch_count() == 1);
  CHECK(restored.value().feature_count() == 1);

  const auto* plane =
      restored.value().find_construction_plane(ConstructionPlaneId("construction_plane.offset_xy"));
  REQUIRE(plane != nullptr);
  CHECK(plane->origin() == Point3{0.0, 0.0, 25.0});
  CHECK(plane->x_axis() == Vector3{1.0, 0.0, 0.0});
  CHECK(plane->y_axis() == Vector3{0.0, 1.0, 0.0});
  CHECK(plane->normal() == Vector3{0.0, 0.0, 1.0});
  REQUIRE(plane->parameter_dependencies().size() == 1);
  CHECK(plane->parameter_dependencies().front().value() == "part.plane_offset");

  CHECK(restored.value().dependency_graph().has_dependency("part.plane_offset",
                                                          "construction_plane.offset_xy"));
  CHECK(restored.value().dependency_graph().has_dependency("construction_plane.offset_xy",
                                                          "sketch.on_construction_plane"));
  CHECK(restored.value().dependency_graph().has_dependency("sketch.on_construction_plane",
                                                          "feature.construction_plane_prism"));
}
