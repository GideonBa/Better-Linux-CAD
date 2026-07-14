#include "blcad/core/part_document.hpp"
#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;

namespace {
SketchCoordinate3D explicit_coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "coordinate");
  REQUIRE(quantity);
  auto coordinate = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(coordinate);
  return coordinate.value();
}

SketchCoordinate3D parameter_coordinate(const char* id) {
  auto coordinate = SketchCoordinate3D::create_parameter(ParameterId(id));
  REQUIRE(coordinate);
  return coordinate.value();
}

SketchPoint3D point(const char* id, double x, double y, double z) {
  auto value = SketchPoint3D::create(SketchEntityId(id), explicit_coordinate(x),
                                     explicit_coordinate(y), explicit_coordinate(z));
  REQUIRE(value);
  return value.value();
}

SketchPoint3D parameter_point(const char* id) {
  auto value = SketchPoint3D::create(SketchEntityId(id), parameter_coordinate("path.x"),
                                     explicit_coordinate(-2.0), parameter_coordinate("path.z"));
  REQUIRE(value);
  return value.value();
}

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Parameter angle_parameter(const char* id, double value) {
  auto quantity = Quantity::angle_deg(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Sketch3D populated_sketch() {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point("point.a", 0.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point("point.b", 10.0, 0.0, 5.0)));
  REQUIRE(sketch.value().add_point(point("point.c", 10.0, 8.0, 12.0)));
  auto line = SketchLine3D::create(SketchEntityId("line.ab"), SketchEntityId("point.a"),
                                   SketchEntityId("point.b"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_line(line.value()));
  auto polyline = SketchPolyline3D::create(
      SketchEntityId("polyline.path"),
      {SketchEntityId("point.a"), SketchEntityId("point.b"), SketchEntityId("point.c")});
  REQUIRE(polyline);
  REQUIRE(sketch.value().add_polyline(polyline.value()));
  return sketch.value();
}
} // namespace

TEST_CASE("Block 75 freezes explicit and parameter-driven model-space coordinates",
          "[core][sketch-3d]") {
  auto explicit_value = explicit_coordinate(-12.5);
  CHECK(explicit_value.source() == SketchCoordinate3DSource::Explicit);
  REQUIRE(explicit_value.explicit_coordinate().has_value());
  CHECK(explicit_value.explicit_coordinate()->millimeters() == -12.5);
  CHECK_FALSE(explicit_value.parameter().has_value());

  auto driven = parameter_coordinate("path.x");
  CHECK(driven.source() == SketchCoordinate3DSource::Parameter);
  REQUIRE(driven.parameter().has_value());
  CHECK(*driven.parameter() == ParameterId("path.x"));
  CHECK_FALSE(driven.explicit_coordinate().has_value());

  auto wrong_unit = Quantity::angle_deg(5.0, "coordinate");
  REQUIRE(wrong_unit);
  CHECK(SketchCoordinate3D::create_explicit(wrong_unit.value()).has_error());
  CHECK(SketchCoordinate3D::create_parameter(ParameterId()).has_error());
}

TEST_CASE("Block 75 owns points lines and ordered polyline vertices in one entity scope",
          "[core][sketch-3d]") {
  auto sketch = populated_sketch();
  CHECK(sketch.entity_count() == 5U);
  REQUIRE(sketch.find_polyline(SketchEntityId("polyline.path")) != nullptr);
  const auto& vertices = sketch.find_polyline(SketchEntityId("polyline.path"))->ordered_vertices();
  REQUIRE(vertices.size() == 3U);
  CHECK(vertices[0] == SketchEntityId("point.a"));
  CHECK(vertices[1] == SketchEntityId("point.b"));
  CHECK(vertices[2] == SketchEntityId("point.c"));

  CHECK(sketch.add_point(point("line.ab", 1.0, 2.0, 3.0)).has_error());
  auto missing = SketchLine3D::create(SketchEntityId("line.missing"), SketchEntityId("point.a"),
                                      SketchEntityId("point.missing"));
  REQUIRE(missing);
  CHECK(sketch.add_line(missing.value()).has_error());
  CHECK(sketch.remove_point(SketchEntityId("point.a")).has_error());
  REQUIRE(sketch.remove_line(SketchEntityId("line.ab")));
  REQUIRE(sketch.remove_polyline(SketchEntityId("polyline.path")));
  REQUIRE(sketch.remove_point(SketchEntityId("point.a")));
  CHECK(sketch.entity_count() == 2U);
}

TEST_CASE("Block 75 connects coordinate parameters to 3D sketch invalidation and removal",
          "[core][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.sketch3d"), "3D sketch");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("path.x", 10.0)));
  REQUIRE(document.value().add_parameter(length_parameter("path.z", 5.0)));
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.path"), "Spatial path");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(parameter_point("point.driven")));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));

  CHECK(document.value().sketch_3d_count() == 1U);
  CHECK(document.value().find_sketch_3d(Sketch3DId("sketch3d.path")) != nullptr);
  CHECK(document.value().dependency_graph().has_dependency("path.x", "sketch3d.path"));
  CHECK(document.value().dependency_graph().has_dependency("path.z", "sketch3d.path"));
  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);
  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  CHECK(restored.value().find_sketch_3d(Sketch3DId("sketch3d.path")) != nullptr);

  document.value().mark_all_clean();
  auto changed = Quantity::length_mm(14.0, "path.x");
  REQUIRE(changed);
  auto affected = document.value().set_parameter_value(ParameterId("path.x"), changed.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "sketch3d.path") !=
        affected.value().end());
  auto plan = document.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(std::any_of(plan.value().steps().begin(), plan.value().steps().end(),
                    [](const RecomputeStep& step) { return step.node_id == "sketch3d.path"; }));

  REQUIRE(document.value().remove_sketch_3d(Sketch3DId("sketch3d.path")));
  CHECK(document.value().sketch_3d_count() == 0U);
  CHECK_FALSE(document.value().dependency_graph().has_node("sketch3d.path"));
}

TEST_CASE("Block 75 validates document and entity identity transactionally", "[core][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.sketch3d-validation"), "Validation");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(angle_parameter("wrong.angle", 15.0)));

  auto wrong_unit = Sketch3D::create(Sketch3DId("sketch3d.wrong"), "Wrong unit");
  REQUIRE(wrong_unit);
  auto driven =
      SketchPoint3D::create(SketchEntityId("point.driven"), parameter_coordinate("wrong.angle"),
                            explicit_coordinate(0.0), explicit_coordinate(0.0));
  REQUIRE(driven);
  REQUIRE(wrong_unit.value().add_point(driven.value()));
  CHECK(document.value().add_sketch_3d(wrong_unit.value()).has_error());
  CHECK(document.value().sketch_3d_count() == 0U);
  CHECK_FALSE(document.value().dependency_graph().has_node("sketch3d.wrong"));

  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto planar = Sketch::create(SketchId("shared.sketch"), "Planar", DatumPlaneId("datum.xy"));
  REQUIRE(planar);
  REQUIRE(document.value().add_sketch(planar.value()));
  auto spatial = Sketch3D::create(Sketch3DId("shared.sketch"), "Spatial");
  REQUIRE(spatial);
  CHECK(document.value().add_sketch_3d(spatial.value()).has_error());
  CHECK(document.value().remove_sketch_3d(Sketch3DId("missing")).has_error());

  CHECK(SketchLine3D::create(SketchEntityId("line.zero"), SketchEntityId("point.a"),
                             SketchEntityId("point.a"))
            .has_error());
  CHECK(SketchPolyline3D::create(SketchEntityId("polyline.short"), {SketchEntityId("point.a")})
            .has_error());
}
