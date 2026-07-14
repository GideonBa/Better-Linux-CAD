#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace blcad;
using json = nlohmann::json;

namespace {
SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D local(const char* id) {
  auto result = SketchPointReference3D::create_local_point(SketchEntityId(id));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D construction() {
  auto result =
      SketchPointReference3D::create_construction_point(ConstructionPointId("point.external"));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D planar(const char* sketch, const char* line, bool start) {
  auto target = start ? SketchReferenceTarget::create_line_segment_start(SketchEntityId(line))
                      : SketchReferenceTarget::create_line_segment_end(SketchEntityId(line));
  REQUIRE(target);
  auto result = SketchPointReference3D::create_planar_sketch_point(SketchId(sketch),
                                                                   std::move(target.value()));
  REQUIRE(result);
  return result.value();
}

Parameter length_parameter(const char* id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto result = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(result);
  return result.value();
}

Parameter count_parameter(const char* id, double value) {
  auto quantity = Quantity::count(value, id);
  REQUIRE(quantity);
  auto result = Parameter::create_count(ParameterId(id), id, quantity.value());
  REQUIRE(result);
  return result.value();
}

Sketch planar_sketch(const char* id, const char* plane, const char* line) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(plane));
  REQUIRE(sketch);
  auto segment = LineSegment::create(SketchEntityId(line), {0.0, 0.0}, {3.0, 2.0});
  REQUIRE(segment);
  REQUIRE(sketch.value().add_entity(segment.value()));
  return sketch.value();
}

PartDocument document_with_3d_sketch() {
  auto document = PartDocument::create(DocumentId("part.sketch3d-json"), "3D JSON");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("coordinate.x", 8.0)));
  REQUIRE(document.value().add_parameter(length_parameter("helix.radius", 4.0)));
  REQUIRE(document.value().add_parameter(length_parameter("helix.pitch", 2.0)));
  REQUIRE(document.value().add_parameter(count_parameter("helix.turns", 3.0)));
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto xz = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.xz"), "XZ", {},
                                               {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0});
  auto yz = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.yz"), "YZ", {},
                                               {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0});
  REQUIRE(xz);
  REQUIRE(yz);
  REQUIRE(document.value().add_construction_plane(xz.value()));
  REQUIRE(document.value().add_construction_plane(yz.value()));
  REQUIRE(document.value().add_sketch(planar_sketch("sketch.xy", "datum.xy", "line.xy")));
  REQUIRE(document.value().add_sketch(planar_sketch("sketch.xz", "plane.xz", "line.xz")));
  REQUIRE(document.value().add_sketch(planar_sketch("sketch.yz", "plane.yz", "line.yz")));
  auto external = ConstructionPoint::create_explicit(ConstructionPointId("point.external"),
                                                     "External", {1.0, 2.0, 3.0});
  REQUIRE(external);
  REQUIRE(document.value().add_construction_point(external.value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.helix"), "Axis", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));

  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.main"), "Main 3D sketch");
  REQUIRE(sketch);
  auto parameter_x = SketchCoordinate3D::create_parameter(ParameterId("coordinate.x"));
  REQUIRE(parameter_x);
  auto p0 = SketchPoint3D::create(SketchEntityId("p0"), parameter_x.value(), coordinate(0.0),
                                  coordinate(-2.0));
  auto p1 = SketchPoint3D::create(SketchEntityId("p1"), coordinate(1.0), coordinate(2.0),
                                  coordinate(3.0));
  auto p2 = SketchPoint3D::create(SketchEntityId("p2"), coordinate(4.0), coordinate(5.0),
                                  coordinate(6.0));
  REQUIRE(p0);
  REQUIRE(p1);
  REQUIRE(p2);
  REQUIRE(sketch.value().add_point(p0.value()));
  REQUIRE(sketch.value().add_point(p1.value()));
  REQUIRE(sketch.value().add_point(p2.value()));
  auto line =
      SketchLine3D::create(SketchEntityId("line.main"), SketchEntityId("p0"), SketchEntityId("p1"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_line(line.value()));
  auto polyline =
      SketchPolyline3D::create(SketchEntityId("polyline.main"),
                               {SketchEntityId("p0"), SketchEntityId("p1"), SketchEntityId("p2")});
  REQUIRE(polyline);
  REQUIRE(sketch.value().add_polyline(polyline.value()));
  auto arc = SketchArc3D::create_three_point(SketchEntityId("arc.main"), local("p0"),
                                             construction(), planar("sketch.xy", "line.xy", false));
  REQUIRE(arc);
  REQUIRE(sketch.value().add_arc(arc.value()));
  auto spline = SketchSpline3D::create(
      SketchEntityId("spline.cross-plane"), SketchSpline3DRepresentation::FitPoints,
      {planar("sketch.xy", "line.xy", true), planar("sketch.xz", "line.xz", false),
       planar("sketch.yz", "line.yz", true)},
      2U, SketchSpline3DContinuity::Tangent);
  REQUIRE(spline);
  REQUIRE(sketch.value().add_spline(spline.value()));
  auto helix_axis = SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.helix"));
  REQUIRE(helix_axis);
  auto helix = SketchHelix3D::create(
      SketchEntityId("helix.main"), helix_axis.value(), ParameterId("helix.radius"),
      ParameterId("helix.pitch"), ParameterId("helix.turns"), SketchHelix3DHandedness::LeftHanded);
  REQUIRE(helix);
  REQUIRE(sketch.value().add_helix(helix.value()));
  auto guide =
      SketchGuideCurve3D::create(SketchEntityId("guide.loft"), SketchEntityId("spline.cross-plane"),
                                 SketchGuideCurve3DRole::LoftGuide, "Three-plane rail");
  REQUIRE(guide);
  REQUIRE(sketch.value().add_guide_curve(guide.value()));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));
  return document.value();
}
} // namespace

TEST_CASE("Block 77 roundtrips all 3D sketch entities and typed references",
          "[core][sketch-3d-json]") {
  auto document = document_with_3d_sketch();
  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  const json encoded = json::parse(serialized.value());
  const auto& sketch_json = encoded.at("sketches_3d").at(0);
  CHECK(sketch_json.at("points").at(0).at("x").at("source") == "parameter");
  CHECK(sketch_json.at("points").at(0).at("z").at("value_mm") == -2.0);
  CHECK(sketch_json.at("arcs").at(0).at("intermediate").at("source") == "construction_point");
  CHECK(sketch_json.at("helices").at(0).at("handedness") == "left_handed");
  CHECK(sketch_json.at("guide_curves").at(0).at("role") == "loft_guide");

  auto restored = deserialize_part_document_from_json(serialized.value());
  REQUIRE(restored);
  const auto* sketch = restored.value().find_sketch_3d(Sketch3DId("sketch3d.main"));
  REQUIRE(sketch != nullptr);
  CHECK(sketch->entity_count() == 9U);
  CHECK(sketch->find_spline(SketchEntityId("spline.cross-plane"))->continuity() ==
        SketchSpline3DContinuity::Tangent);
  CHECK(sketch->find_helix(SketchEntityId("helix.main"))->handedness() ==
        SketchHelix3DHandedness::LeftHanded);
  CHECK(restored.value().dependency_graph().has_dependency("sketch.yz", "sketch3d.main"));
}

TEST_CASE("Block 77 preserves three-plane semantic spline identity deterministically",
          "[core][sketch-3d-json]") {
  auto first = serialize_part_document_to_json(document_with_3d_sketch());
  REQUIRE(first);
  auto restored = deserialize_part_document_from_json(first.value());
  REQUIRE(restored);
  auto second = serialize_part_document_to_json(restored.value());
  REQUIRE(second);
  CHECK(second.value() == first.value());

  const json encoded = json::parse(first.value());
  const auto& points = encoded.at("sketches_3d").at(0).at("splines").at(0).at("points");
  REQUIRE(points.size() == 3U);
  CHECK(points.at(0).at("sketch") == "sketch.xy");
  CHECK(points.at(1).at("sketch") == "sketch.xz");
  CHECK(points.at(2).at("sketch") == "sketch.yz");
  for (const auto& point : points) {
    CHECK(point.size() == 3U);
    CHECK_FALSE(point.contains("position"));
  }
}

TEST_CASE("Block 77 remains backward compatible without sketches_3d", "[core][sketch-3d-json]") {
  auto document = PartDocument::create(DocumentId("part.old"), "Old");
  REQUIRE(document);
  auto serialized = serialize_part_document_to_json(document.value());
  REQUIRE(serialized);
  json encoded = json::parse(serialized.value());
  encoded.erase("sketches_3d");
  auto restored = deserialize_part_document_from_json(encoded.dump());
  REQUIRE(restored);
  CHECK(restored.value().sketch_3d_count() == 0U);
}

TEST_CASE("Block 77 rejects ambiguous and non-canonical 3D sketch JSON", "[core][sketch-3d-json]") {
  auto serialized = serialize_part_document_to_json(document_with_3d_sketch());
  REQUIRE(serialized);
  json unknown = json::parse(serialized.value());
  unknown["sketches_3d"][0]["splines"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(unknown.dump()).has_error());

  json copied = json::parse(serialized.value());
  copied["sketches_3d"][0]["splines"][0]["points"][0]["position"] =
      json{{"x", 1.0}, {"y", 2.0}, {"z", 3.0}};
  CHECK(deserialize_part_document_from_json(copied.dump()).has_error());

  json source = json::parse(serialized.value());
  source["sketches_3d"][0]["arcs"][0]["start"]["source"] = "resolved_point";
  CHECK(deserialize_part_document_from_json(source.dump()).has_error());

  json collection = json::parse(serialized.value());
  collection["sketches_3d"] = json::object();
  CHECK(deserialize_part_document_from_json(collection.dump()).has_error());
}
