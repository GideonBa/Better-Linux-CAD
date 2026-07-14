#include "blcad/core/part_document.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace blcad;

namespace {
SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point(const char* id, double x, double y, double z) {
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

SketchPointReference3D planar(const char* sketch_id, const char* line_id, bool start = true) {
  auto target = start ? SketchReferenceTarget::create_line_segment_start(SketchEntityId(line_id))
                      : SketchReferenceTarget::create_line_segment_end(SketchEntityId(line_id));
  REQUIRE(target);
  auto result =
      SketchPointReference3D::create_planar_sketch_point(SketchId(sketch_id), target.value());
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

Sketch make_planar_sketch(const char* id, const char* plane, const char* line) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(plane));
  REQUIRE(sketch);
  auto segment = LineSegment::create(SketchEntityId(line), {0.0, 0.0}, {5.0, 2.0});
  REQUIRE(segment);
  REQUIRE(sketch.value().add_entity(segment.value()));
  return sketch.value();
}

PartDocument referenced_document() {
  auto document = PartDocument::create(DocumentId("part.sketch3d-curves"), "3D curves");
  REQUIRE(document);
  REQUIRE(document.value().add_datum_plane(DatumPlane::xy().value()));
  auto xz = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.xz"), "XZ", {},
                                               {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, -1.0, 0.0});
  REQUIRE(xz);
  REQUIRE(document.value().add_construction_plane(xz.value()));
  auto yz = ConstructionPlane::create_explicit(ConstructionPlaneId("plane.yz"), "YZ", {},
                                               {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0});
  REQUIRE(yz);
  REQUIRE(document.value().add_construction_plane(yz.value()));
  REQUIRE(document.value().add_sketch(make_planar_sketch("sketch.xy", "datum.xy", "line.xy")));
  REQUIRE(document.value().add_sketch(make_planar_sketch("sketch.xz", "plane.xz", "line.xz")));
  REQUIRE(document.value().add_sketch(make_planar_sketch("sketch.yz", "plane.yz", "line.yz")));
  auto construction_point = ConstructionPoint::create_explicit(
      ConstructionPointId("point.construction"), "Point", {3.0, 4.0, 5.0});
  REQUIRE(construction_point);
  REQUIRE(document.value().add_construction_point(construction_point.value()));
  auto axis =
      DatumAxis::create_explicit(DatumAxisId("axis.helix"), "Helix axis", {}, {0.0, 0.0, 1.0});
  REQUIRE(axis);
  REQUIRE(document.value().add_datum_axis(axis.value()));
  REQUIRE(document.value().add_parameter(length_parameter("helix.radius", 4.0)));
  REQUIRE(document.value().add_parameter(length_parameter("helix.pitch", 2.0)));
  REQUIRE(document.value().add_parameter(count_parameter("helix.turns", 3.0)));
  return document.value();
}
} // namespace

TEST_CASE("Block 76 freezes three-point arcs and external point reference identity",
          "[core][sketch-3d-curves]") {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.curves"), "Curves");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point("point.local", 0.0, 0.0, 0.0)));
  auto arc = SketchArc3D::create_three_point(SketchEntityId("arc.main"), local("point.local"),
                                             construction("point.construction"),
                                             planar("sketch.xy", "line.xy", false));
  REQUIRE(arc);
  REQUIRE(sketch.value().add_arc(arc.value()));
  REQUIRE(sketch.value().find_arc(SketchEntityId("arc.main")) != nullptr);
  CHECK(sketch.value().find_arc(SketchEntityId("arc.main"))->intermediate().source() ==
        SketchPointReference3DSource::ConstructionPoint);
  CHECK(sketch.value().referenced_node_ids() ==
        std::vector<std::string>{"point.construction", "sketch.xy"});
  CHECK(sketch.value().remove_point(SketchEntityId("point.local")).has_error());

  auto document = referenced_document();
  REQUIRE(document.add_sketch_3d(sketch.value()));
  CHECK(document.dependency_graph().has_dependency("point.construction", "sketch3d.curves"));
  CHECK(document.dependency_graph().has_dependency("sketch.xy", "sketch3d.curves"));

  CHECK(SketchArc3D::create_three_point(SketchEntityId("arc.invalid"), local("point.local"),
                                        local("point.local"), construction("point.construction"))
            .has_error());
}

TEST_CASE("Block 76 freezes fit and control spline degree and continuity contracts",
          "[core][sketch-3d-curves]") {
  auto fit =
      SketchSpline3D::create(SketchEntityId("spline.fit"), SketchSpline3DRepresentation::FitPoints,
                             {planar("sketch.xy", "line.xy"), planar("sketch.xz", "line.xz"),
                              planar("sketch.yz", "line.yz")},
                             2U, SketchSpline3DContinuity::Tangent);
  REQUIRE(fit);
  CHECK(fit.value().representation() == SketchSpline3DRepresentation::FitPoints);
  CHECK(fit.value().degree() == 2U);
  CHECK(fit.value().continuity() == SketchSpline3DContinuity::Tangent);

  auto control = SketchSpline3D::create(SketchEntityId("spline.control"),
                                        SketchSpline3DRepresentation::ControlPoints,
                                        {local("p0"), local("p1"), local("p2"), local("p3")}, 3U,
                                        SketchSpline3DContinuity::Curvature);
  REQUIRE(control);
  CHECK(control.value().ordered_points().size() == 4U);

  CHECK(SketchSpline3D::create(SketchEntityId("spline.degree"),
                               SketchSpline3DRepresentation::FitPoints, {local("p0"), local("p1")},
                               0U, SketchSpline3DContinuity::Positional)
            .has_error());
  CHECK(SketchSpline3D::create(SketchEntityId("spline.continuity"),
                               SketchSpline3DRepresentation::FitPoints, {local("p0"), local("p1")},
                               1U, SketchSpline3DContinuity::Tangent)
            .has_error());
  CHECK(SketchSpline3D::create(
            SketchEntityId("spline.duplicate"), SketchSpline3DRepresentation::FitPoints,
            {local("p0"), local("p1"), local("p0")}, 2U, SketchSpline3DContinuity::Positional)
            .has_error());
}

TEST_CASE("Block 76 validates cross-plane spline dependencies transactionally",
          "[core][sketch-3d-curves]") {
  auto document = referenced_document();
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.guide"), "Guide");
  REQUIRE(sketch);
  auto spline = SketchSpline3D::create(
      SketchEntityId("spline.cross-plane"), SketchSpline3DRepresentation::FitPoints,
      {planar("sketch.xy", "line.xy"), planar("sketch.xz", "line.xz"),
       planar("sketch.yz", "line.yz")},
      2U, SketchSpline3DContinuity::Tangent);
  REQUIRE(spline);
  REQUIRE(sketch.value().add_spline(spline.value()));
  REQUIRE(document.add_sketch_3d(sketch.value()));
  CHECK(document.dependency_graph().has_dependency("sketch.xy", "sketch3d.guide"));
  CHECK(document.dependency_graph().has_dependency("sketch.xz", "sketch3d.guide"));
  CHECK(document.dependency_graph().has_dependency("sketch.yz", "sketch3d.guide"));

  auto invalid = Sketch3D::create(Sketch3DId("sketch3d.invalid"), "Invalid");
  REQUIRE(invalid);
  auto missing = SketchSpline3D::create(
      SketchEntityId("spline.missing"), SketchSpline3DRepresentation::FitPoints,
      {planar("sketch.xy", "line.xy"), planar("sketch.missing", "line.xz"),
       construction("point.construction")},
      2U, SketchSpline3DContinuity::Positional);
  REQUIRE(missing);
  REQUIRE(invalid.value().add_spline(missing.value()));
  CHECK(document.add_sketch_3d(invalid.value()).has_error());
  CHECK(document.find_sketch_3d(Sketch3DId("sketch3d.invalid")) == nullptr);
  CHECK_FALSE(document.dependency_graph().has_node("sketch3d.invalid"));
}

TEST_CASE("Block 76 freezes helix axis pitch turns and handedness dependencies",
          "[core][sketch-3d-curves]") {
  auto document = referenced_document();
  auto axis = SketchHelixAxis3D::create_datum_axis(DatumAxisId("axis.helix"));
  REQUIRE(axis);
  auto helix = SketchHelix3D::create(
      SketchEntityId("helix.main"), axis.value(), ParameterId("helix.radius"),
      ParameterId("helix.pitch"), ParameterId("helix.turns"), SketchHelix3DHandedness::LeftHanded);
  REQUIRE(helix);
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.helix"), "Helix");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_helix(helix.value()));
  REQUIRE(document.add_sketch_3d(sketch.value()));
  CHECK(document.find_sketch_3d(Sketch3DId("sketch3d.helix"))
            ->find_helix(SketchEntityId("helix.main"))
            ->handedness() == SketchHelix3DHandedness::LeftHanded);
  for (const char* dependency : {"axis.helix", "helix.radius", "helix.pitch", "helix.turns"})
    CHECK(document.dependency_graph().has_dependency(dependency, "sketch3d.helix"));

  document.mark_all_clean();
  auto pitch = Quantity::length_mm(3.0, "helix.pitch");
  REQUIRE(pitch);
  auto affected = document.set_parameter_value(ParameterId("helix.pitch"), pitch.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "sketch3d.helix") !=
        affected.value().end());
}

TEST_CASE("Block 76 guide-curve roles protect their owned curve sources",
          "[core][sketch-3d-curves]") {
  auto sketch = Sketch3D::create(Sketch3DId("sketch3d.guide-role"), "Guide role");
  REQUIRE(sketch);
  REQUIRE(sketch.value().add_point(point("p0", 0.0, 0.0, 0.0)));
  REQUIRE(sketch.value().add_point(point("p1", 1.0, 2.0, 3.0)));
  auto line =
      SketchLine3D::create(SketchEntityId("line.path"), SketchEntityId("p0"), SketchEntityId("p1"));
  REQUIRE(line);
  REQUIRE(sketch.value().add_line(line.value()));
  auto guide =
      SketchGuideCurve3D::create(SketchEntityId("guide.sweep"), SketchEntityId("line.path"),
                                 SketchGuideCurve3DRole::SweepPath, "Primary sweep path");
  REQUIRE(guide);
  REQUIRE(sketch.value().add_guide_curve(guide.value()));
  CHECK(sketch.value().find_guide_curve(SketchEntityId("guide.sweep"))->role() ==
        SketchGuideCurve3DRole::SweepPath);
  CHECK(sketch.value().remove_line(SketchEntityId("line.path")).has_error());
  REQUIRE(sketch.value().remove_guide_curve(SketchEntityId("guide.sweep")));
  REQUIRE(sketch.value().remove_line(SketchEntityId("line.path")));

  auto missing =
      SketchGuideCurve3D::create(SketchEntityId("guide.missing"), SketchEntityId("curve.missing"),
                                 SketchGuideCurve3DRole::LoftGuide);
  REQUIRE(missing);
  CHECK(sketch.value().add_guide_curve(missing.value()).has_error());
}
