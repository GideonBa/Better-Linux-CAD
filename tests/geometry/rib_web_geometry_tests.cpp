#include "../support/rib_web_fixture.hpp"
#include "blcad/core/part_document_json.hpp"
#include "blcad/core/sketch_driven_intent.hpp"
#include "blcad/geometry/occt_shape_view.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/result_face_support.hpp"
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numbers>
#include <sstream>
using namespace blcad;
using namespace blcad::geometry;
namespace f = blcad::test::history_fixture;
namespace {
using blcad::test::rib_fixture::add;
using blcad::test::rib_fixture::fixture;
using blcad::test::rib_fixture::intent;
ShapeCache cache() {
  return f::checked(ShapeCache::create(ShapeCacheId("rib.test")));
}
void execute(const PartDocument& p, ShapeCache& c) {
  const auto r = GeometryRecomputeExecutor{}.execute_document(p, c);
  INFO((r ? "ok" : r.error().message()));
  REQUIRE(r);
}
double volume(const ShapeCache& c) {
  return RectangleExtrusionAdapter{}.summarize(*c.find_body_shape(BodyId("body.base"))).volume_mm3;
}
std::string bytes(const GeometryShape& s) {
  std::ostringstream stream;
  BRepTools::Write(*OcctShapeView::get(s), stream);
  return stream.str();
}
} // namespace
TEST_CASE("Rib adds the independently integrated straight symmetric asymmetric and drafted volume",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto r = intent();
  double expected = 64;
  SECTION("Symmetric") {}
  SECTION("First side") {
    r.side = RibThicknessSide::First;
  }
  SECTION("Second side") {
    r.side = RibThicknessSide::Second;
  }
  SECTION("Asymmetric") {
    r.side = RibThicknessSide::Asymmetric;
    r.second_thickness = ParameterId("rib.second");
    expected = 192;
  }
  SECTION("Draft") {
    r.draft = ParameterId("rib.draft");
    expected = 16 * (4 + 80 * std::tan(5 * std::numbers::pi / 180));
  }
  auto c = cache();
  execute(p, c);
  const auto original = *c.find_body_shape(BodyId("body.base"));
  const auto before = bytes(original);
  add(p, r);
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + expected).margin(1e-5));
  CHECK(
      RectangleExtrusionAdapter{}.summarize(*c.find_body_shape(BodyId("body.base"))).solid_count ==
      1);
  CHECK(bytes(original) == before);
  const auto text = f::checked(serialize_part_document_to_json(p));
  const auto opened = f::checked(deserialize_part_document_from_json(text));
  auto replay = cache();
  execute(opened, replay);
  CHECK(volume(replay) == Catch::Approx(volume(c)).margin(1e-7));
  CHECK(f::checked(serialize_part_document_to_json(opened)) == text);
}
TEST_CASE("Parallel Rib forms a checked curtain with Sketch normal thickness",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto r = intent();
  r.plane = RibExtrusionPlane::ParallelToSketch;
  r.direction = {0, 2.7, 0};
  add(p, r);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1752).margin(1e-6));
  BRepClass3d_SolidClassifier material(*OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
                                       gp_Pnt(0, 10, -.25), 1e-7);
  CHECK(material.State() == TopAbs_IN);
}
TEST_CASE("Curved Rib uses actual circular skeleton offsets and independent annular area",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto sketch = f::checked(Sketch::create(SketchId("arc.sketch"), "Arc", DatumPlaneId("datum.xy")));
  f::accept(sketch.add_entity(f::checked(ArcSegment::create_three_point(
      SketchEntityId("arc"), {4, 0}, {std::sqrt(8.), std::sqrt(8.)}, {0, 4}))));
  f::accept(p.add_sketch(sketch));
  auto r = intent();
  r.source = {sketch.id(), {SketchEntityId("arc")}, std::nullopt};
  add(p, r);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + 8 * std::numbers::pi).margin(1e-5));
}
TEST_CASE("Cross Web union has analytic overlap volume and explicitly rejects forbidden crossings",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture({f::checked(LineSegment::create(SketchEntityId("line"), {-8, 0}, {8, 0})),
                    f::checked(LineSegment::create(SketchEntityId("cross"), {0, -4}, {0, 4}))});
  auto r = intent();
  r.kind = RibWebKind::Web;
  r.source.curves.push_back(SketchEntityId("cross"));
  bool reject = false;
  SECTION("Union") {}
  SECTION("Reject") {
    r.crossings = RibCrossingPolicy::Reject;
    reject = true;
  }
  auto c = cache();
  execute(p, c);
  const auto original = bytes(*c.find_body_shape(BodyId("body.base")));
  add(p, r);
  const auto outcome = GeometryRecomputeExecutor{}.execute_document(p, c);
  if (reject) {
    CHECK_FALSE(outcome);
    CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == original);
  } else {
    INFO((outcome ? "ok" : outcome.error().message()));
    REQUIRE(outcome);
    CHECK(volume(c) == Catch::Approx(1692).margin(1e-6));
    const auto faces =
        f::checked(ResultFaceSupportGeometry::selectable_faces(p, c, BodyId("body.base")));
    CHECK(faces.size() >= 10);
  }
}
TEST_CASE(
    "Disconnected no-op branched and invalid-direction Rib candidates preserve the previous cache",
    "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto r = intent();
  SECTION("Disconnected") {
    auto sketch = f::checked(Sketch::create(SketchId("away"), "Away", DatumPlaneId("datum.xy")));
    f::accept(sketch.add_entity(
        f::checked(LineSegment::create(SketchEntityId("line"), {30, 0}, {40, 0}))));
    f::accept(p.add_sketch(sketch));
    r.source.sketch = sketch.id();
  }
  SECTION("No-op") {
    REQUIRE(p.set_parameter_value(ParameterId("rib.l"), f::checked(Quantity::length_mm(4))));
  }
  SECTION("Wrong direction") {
    r.direction = {1, 0, 1};
  }
  SECTION("Branched") {
    auto sketch =
        f::checked(Sketch::create(SketchId("branch"), "Branch", DatumPlaneId("datum.xy")));
    for (const auto& [id, end] : {std::pair{"a", Point2{4, 0}}, std::pair{"b", Point2{-4, 0}},
                                  std::pair{"c", Point2{0, 4}}})
      f::accept(
          sketch.add_entity(f::checked(LineSegment::create(SketchEntityId(id), {0, 0}, end))));
    f::accept(p.add_sketch(sketch));
    r.source = {
        sketch.id(), {SketchEntityId("a"), SketchEntityId("b"), SketchEntityId("c")}, std::nullopt};
  }
  auto c = cache();
  execute(p, c);
  const auto before = bytes(*c.find_body_shape(BodyId("body.base")));
  add(p, r);
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == before);
}
TEST_CASE("Connected Rib corners use mitered bands and closed profiles preserve inner material",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto r = intent();
  double expected = 48;
  SECTION("Open L skeleton") {
    auto sketch =
        f::checked(Sketch::create(SketchId("connected"), "Connected", DatumPlaneId("datum.xy")));
    f::accept(
        sketch.add_entity(f::checked(LineSegment::create(SketchEntityId("a"), {-8, 0}, {0, 0}))));
    f::accept(
        sketch.add_entity(f::checked(LineSegment::create(SketchEntityId("b"), {0, 0}, {0, 4}))));
    f::accept(p.add_sketch(sketch));
    r.source = {sketch.id(), {SketchEntityId("a"), SketchEntityId("b")}, std::nullopt};
  }
  SECTION("Closed rectangular band") {
    for (const auto& [id, value] : {std::pair{"band.w", 10.0}, {"band.h", 5.0}})
      f::accept(p.add_parameter(f::checked(
          Parameter::create_length(ParameterId(id), id, f::checked(Quantity::length_mm(value))))));
    auto sketch = f::checked(Sketch::create(SketchId("band"), "Band", DatumPlaneId("datum.xy")));
    f::accept(sketch.add_profile(f::checked(RectangleProfile::create(
        ProfileId("rectangle"), ParameterId("band.w"), ParameterId("band.h")))));
    f::accept(p.add_sketch(sketch));
    r.source = {sketch.id(), {}, ProfileId("rectangle")};
    expected = 120;
  }
  add(p, r);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + expected).margin(1e-5));
}
TEST_CASE("A Rib skeleton follows its explicit owned Sketch placement",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  f::accept(p.add_sketch_ownership(f::checked(SketchOwnership::create(
      SketchId("rib.sketch"), BodyId("body.base"), SketchAssociation::DrivesBody))));
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("place"), BodyId("body.base"), BodyTransformCoordinateSpace::World,
      std::nullopt, {30, 7, 2}, true, false))));
  add(p);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1664).margin(1e-6));
  BRepClass3d_SolidClassifier inside(*OcctShapeView::get(*c.find_body_shape(BodyId("body.base"))),
                                     gp_Pnt(30, 7, 12), 1e-7);
  CHECK(inside.State() == TopAbs_IN);
}
TEST_CASE("Rib terminates at an actual earlier placed Body derived from its target",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("tol"), "Tolerance", f::checked(Quantity::length_mm(1e-6))))));
  f::accept(p.add_body(f::checked(Body::create(BodyId("limit.body"), "Limit", BodyKind::Solid))));
  const auto top = f::checked(FaceReference::create_planar(
      PartFeatureInputRole::SurfaceSourceFace,
      f::checked(SemanticFaceReference::create(FeatureId("base"), SemanticFace::Top))));
  const auto source = f::checked(SurfaceReference::create_faces(BodyId("body.base"), {top}));
  const auto context = f::checked(FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("limit.body")));
  const SurfaceOffsetParameters sizes{ParameterId("rib.t"), ParameterId("tol"),
                                      SurfaceOffsetSide::AlongNormal,
                                      SurfaceOffsetApproximation::ExactAnalytic, std::nullopt};
  f::accept(p.add_surface_feature(f::checked(
      SurfaceOffsetFeature::create_thicken(FeatureId("limit"), "Limit", source, sizes, context))));
  f::accept(p.add_body_transform(f::checked(BodyTransform::create_translate(
      BodyTransformId("limit.place"), BodyId("limit.body"), BodyTransformCoordinateSpace::World,
      std::nullopt, {0, 0, 8}, false, false))));
  auto r = intent();
  r.extent = RibBodyExtent{BodyId("limit.body")};
  add(p, r);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1728).margin(1e-5));
  CHECK(
      RectangleExtrusionAdapter{}.summarize(*c.find_body_shape(BodyId("limit.body"))).volume_mm3 ==
      Catch::Approx(200).margin(1e-5));
  REQUIRE(p.set_parameter_value(ParameterId("rib.t"), f::checked(Quantity::length_mm(2))));
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1856).margin(1e-5));
}
TEST_CASE("Rib spline ribbons match independent source arclength integration",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto sketch = f::checked(Sketch::create(SketchId("spline"), "Spline", DatumPlaneId("datum.xy")));
  f::accept(sketch.add_entity(f::checked(SplineSegment::create_cubic_bezier(
      SketchEntityId("curve"), {-6, 0}, {-2, 1}, {2, -1}, {6, 0}))));
  f::accept(p.add_sketch(sketch));
  auto r = intent();
  r.source = {sketch.id(), {SketchEntityId("curve")}, std::nullopt};
  add(p, r);
  auto c = cache();
  execute(p, c);
  const auto speed = [](double t) {
    const double dy = 3 - 18 * t + 18 * t * t;
    return std::sqrt(144 + dy * dy);
  };
  const int n = 20000;
  double sum = speed(0) + speed(1);
  for (int i = 1; i < n; ++i)
    sum += (i % 2 ? 4 : 2) * speed(double(i) / n);
  const double length = sum / (3 * n);
  CHECK(volume(c) == Catch::Approx(1600 + 4 * length).margin(2e-4));
}
TEST_CASE("Closed circular Rib profiles retain their analytic annular band under draft",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  f::accept(p.add_parameter(f::checked(Parameter::create_length(
      ParameterId("band.d"), "Band diameter", f::checked(Quantity::length_mm(6))))));
  auto sketch =
      f::checked(Sketch::create(SketchId("circle"), "Circle band", DatumPlaneId("datum.xy")));
  f::accept(sketch.add_profile(
      f::checked(CircleProfile::create(ProfileId("circle"), ParameterId("band.d")))));
  f::accept(p.add_sketch(sketch));
  auto r = intent();
  r.source = {sketch.id(), {}, ProfileId("circle")};
  double integral = 4;
  SECTION("Constant thickness") {}
  SECTION("Drafted thickness") {
    r.draft = ParameterId("rib.draft");
    integral += 80 * std::tan(5 * std::numbers::pi / 180);
  }
  add(p, r);
  auto c = cache();
  execute(p, c);
  CHECK(volume(c) == Catch::Approx(1600 + 6 * std::numbers::pi * integral).margin(2e-4));
}
TEST_CASE("Rib rejects numerical collapse negative draft collapse and self-crossing chains",
          "[geometry][rib-web][parity-b181]") {
  auto p = fixture();
  auto r = intent();
  SECTION("Numerical thickness") {
    REQUIRE(p.set_parameter_value(ParameterId("rib.t"), f::checked(Quantity::length_mm(1e-7))));
  }
  SECTION("Draft collapse") {
    REQUIRE(p.set_parameter_value(ParameterId("rib.draft"), f::checked(Quantity::angle_deg(-5))));
    r.draft = ParameterId("rib.draft");
  }
  SECTION("Self crossing") {
    auto sketch =
        f::checked(Sketch::create(SketchId("crossed"), "Self crossed", DatumPlaneId("datum.xy")));
    const std::vector<Point2> points{{-4, -2}, {4, 2}, {-4, 2}, {4, -2}};
    std::vector<SketchEntityId> ids;
    for (int i = 0; i < 3; ++i) {
      ids.emplace_back(std::to_string(i));
      f::accept(
          sketch.add_entity(f::checked(LineSegment::create(ids.back(), points[i], points[i + 1]))));
    }
    f::accept(p.add_sketch(sketch));
    r.source = {sketch.id(), ids, std::nullopt};
  }
  auto c = cache();
  execute(p, c);
  const auto before = bytes(*c.find_body_shape(BodyId("body.base")));
  add(p, r);
  CHECK_FALSE(GeometryRecomputeExecutor{}.execute_document(p, c));
  CHECK(bytes(*c.find_body_shape(BodyId("body.base"))) == before);
}
