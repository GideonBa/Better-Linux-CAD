#include "blcad/geometry/sketch_3d_geometry_adapter.hpp"

#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {
SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point3d(const char* id, double x, double y, double z) {
  auto result =
      SketchPoint3D::create(SketchEntityId(id), coordinate(x), coordinate(y), coordinate(z));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D local(const char* id) {
  auto result = SketchPointReference3D::create_local_point(SketchEntityId(id));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D construction(const char* id) {
  auto result = SketchPointReference3D::create_construction_point(ConstructionPointId(id));
  REQUIRE(result);
  return result.value();
}

SketchPointReference3D planar(const char* sketch, const char* line, bool start = true) {
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

PartDocument full_geometry_document() {
  auto document = PartDocument::create(DocumentId("part.sketch3d-geometry"), "3D Geometry");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("helix.radius", 2.0)));
  REQUIRE(document.value().add_parameter(length_parameter("helix.pitch", 1.5)));
  REQUIRE(document.value().add_parameter(count_parameter("helix.turns", 2.0)));
  auto external = ConstructionPoint::create_explicit(ConstructionPointId("point.external"),
                                                     "External", {2.0, 0.0, 0.0});
  REQUIRE(external);
  REQUIRE(document.value().add_construction_point(external.value()));
  auto axis = DatumAxis::create_explicit(DatumAxisId("axis.helix"), "Axis", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));

  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.full"), "Full");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point3d("p0", 0.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point3d("p1", 2.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point3d("p2", 2.0, 2.0, 0.0)));
  REQUIRE(sketch.value().add_point(point3d("p3", 3.0, 2.0, 1.0)));
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
                                             construction("point.external"), local("p2"));
  REQUIRE(arc);
  REQUIRE(sketch.value().add_arc(arc.value()));
  auto spline = SketchSpline3D::create(SketchEntityId("spline.control"),
                                       SketchSpline3DRepresentation::ControlPoints,
                                       {local("p0"), local("p1"), local("p2"), local("p3")}, 3U,
                                       SketchSpline3DContinuity::Curvature);
  REQUIRE(spline);
  REQUIRE(sketch.value().add_spline(spline.value()));
  auto helix_axis = SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.helix"));
  REQUIRE(helix_axis);
  auto helix = SketchHelix3D::create(
      SketchEntityId("helix.main"), helix_axis.value(), ParameterId("helix.radius"),
      ParameterId("helix.pitch"), ParameterId("helix.turns"), SketchHelix3DHandedness::RightHanded);
  REQUIRE(helix);
  REQUIRE(sketch.value().add_helix(helix.value()));
  auto guide =
      SketchGuideCurve3D::create(SketchEntityId("guide.main"), SketchEntityId("spline.control"),
                                 SketchGuideCurve3DRole::SweepPath);
  REQUIRE(guide);
  REQUIRE(sketch.value().add_guide_curve(guide.value()));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));
  return document.value();
}

Sketch planar_sketch(const char* id, const char* plane, const char* line) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(plane));
  REQUIRE(sketch);
  auto segment = LineSegment::create(SketchEntityId(line), {0.0, 0.0}, {3.0, 2.0});
  REQUIRE(segment);
  REQUIRE(sketch.value().add_entity(segment.value()));
  return sketch.value();
}

PartDocument cross_plane_document() {
  auto document = PartDocument::create(DocumentId("part.cross-plane"), "Cross plane");
  REQUIRE(document);
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
  auto spatial = Sketch3D::create(Sketch3DId("sketch3d.cross-plane"), "Cross plane");
  REQUIRE(spatial);
  auto spline = SketchSpline3D::create(
      SketchEntityId("spline.cross-plane"), SketchSpline3DRepresentation::FitPoints,
      {planar("sketch.xy", "line.xy", true), planar("sketch.xz", "line.xz", false),
       planar("sketch.yz", "line.yz", false)},
      2U, SketchSpline3DContinuity::Tangent);
  REQUIRE(spline);
  REQUIRE(spatial.value().add_spline(spline.value()));
  REQUIRE(document.value().add_sketch_3d(spatial.value()));
  return document.value();
}
} // namespace

TEST_CASE("Block 78 converts every supported 3D sketch entity deterministically",
          "[geometry][sketch-3d]") {
  auto document = full_geometry_document();
  auto before = serialize_part_document_to_json(document);
  REQUIRE(before);
  auto result = Sketch3DGeometryAdapter{}.convert(document, Sketch3DId("sketch3d.full"));
  REQUIRE(result);
  REQUIRE(result.value().points().size() == 4U);
  REQUIRE(result.value().products().size() == 9U);
  const std::vector<SketchEntityId> expected{
      SketchEntityId("p0"),        SketchEntityId("p1"),
      SketchEntityId("p2"),        SketchEntityId("p3"),
      SketchEntityId("line.main"), SketchEntityId("polyline.main"),
      SketchEntityId("arc.main"),  SketchEntityId("spline.control"),
      SketchEntityId("helix.main")};
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK(result.value().products()[index].entity_id == expected[index]);
    CHECK_FALSE(result.value().products()[index].shape.empty());
  }
  CHECK(result.value().find_product(SketchEntityId("polyline.main"))->edge_count == 2U);
  CHECK(result.value().find_product(SketchEntityId("polyline.main"))->wire_count == 1U);
  CHECK(result.value().find_product(SketchEntityId("helix.main"))->kind ==
        Sketch3DGeometryProductKind::Helix);
  auto after = serialize_part_document_to_json(document);
  REQUIRE(after);
  CHECK(after.value() == before.value());
}

TEST_CASE("Block 78 resolves a spline across three differently oriented planar sketches",
          "[geometry][sketch-3d]") {
  auto document = cross_plane_document();
  auto result = Sketch3DGeometryAdapter{}.convert(document, Sketch3DId("sketch3d.cross-plane"));
  REQUIRE(result);
  REQUIRE(result.value().products().size() == 1U);
  CHECK(result.value().products()[0].kind == Sketch3DGeometryProductKind::Spline);
  CHECK_FALSE(result.value().products()[0].shape.empty());
  CHECK(result.value().products()[0].edge_count == 1U);
}

TEST_CASE("Block 78 resolves parameter-driven point coordinates on every conversion",
          "[geometry][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.driven"), "Driven");
  REQUIRE(document);
  REQUIRE(document.value().add_parameter(length_parameter("point.x", 2.0)));
  auto x = SketchCoordinate3D::create_parameter(ParameterId("point.x"));
  REQUIRE(x);
  auto point =
      SketchPoint3D::create(SketchEntityId("p"), x.value(), coordinate(0.0), coordinate(0.0));
  REQUIRE(point);
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.driven"), "Driven");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point.value()));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));
  auto first = Sketch3DGeometryAdapter{}.convert(document.value(), Sketch3DId("sketch3d.driven"));
  REQUIRE(first);
  CHECK(first.value().find_point(SketchEntityId("p"))->position.x == 2.0);
  auto changed = Quantity::length_mm(7.0, "point.x");
  REQUIRE(changed);
  REQUIRE(document.value().set_parameter_value(ParameterId("point.x"), changed.value()));
  auto second = Sketch3DGeometryAdapter{}.convert(document.value(), Sketch3DId("sketch3d.driven"));
  REQUIRE(second);
  CHECK(second.value().find_point(SketchEntityId("p"))->position.x == 7.0);
}

TEST_CASE("Block 78 fails closed for degenerate curves and missing sketches",
          "[geometry][sketch-3d]") {
  auto document = PartDocument::create(DocumentId("part.invalid"), "Invalid");
  REQUIRE(document);
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.invalid"), "Invalid");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point3d("a", 0.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point3d("b", 1.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point3d("c", 2.0, 0.0, 0.0)));
  auto arc = SketchArc3D::create_three_point(SketchEntityId("arc.collinear"), local("a"),
                                             local("b"), local("c"));
  REQUIRE(arc);
  REQUIRE(sketch.value().add_arc(arc.value()));
  REQUIRE(document.value().add_sketch_3d(sketch.value()));
  CHECK(Sketch3DGeometryAdapter{}
            .convert(document.value(), Sketch3DId("sketch3d.invalid"))
            .has_error());
  CHECK(Sketch3DGeometryAdapter{}
            .convert(document.value(), Sketch3DId("sketch3d.missing"))
            .has_error());
}
