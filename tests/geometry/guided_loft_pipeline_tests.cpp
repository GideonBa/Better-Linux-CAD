#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

SketchCoordinate3D coordinate(double value) {
  auto quantity = Quantity::linear_displacement_mm(value, "guided loft coordinate");
  REQUIRE(quantity);
  auto result = SketchCoordinate3D::create_explicit(quantity.value());
  REQUIRE(result);
  return result.value();
}

SketchPoint3D point3d(const std::string& id, Point3 point) {
  auto result = SketchPoint3D::create(SketchEntityId(id), coordinate(point.x), coordinate(point.y),
                                      coordinate(point.z));
  REQUIRE(result);
  return result.value();
}

Parameter length_parameter(const std::string& id, double value) {
  auto quantity = Quantity::length_mm(value, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

ConstructionPlane plane(const std::string& id, double z) {
  auto value =
      ConstructionPlane::create_explicit(ConstructionPlaneId(id), id, {0.0, 0.0, z},
                                         {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0});
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext new_body_context() {
  auto value = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                BodyId("body.loft"));
  REQUIRE(value);
  return value.value();
}

ShapeCache cache(const char* id) {
  auto value = ShapeCache::create(ShapeCacheId(id));
  REQUIRE(value);
  return value.value();
}

PathCurveId add_spatial_path(PartDocument& document, const std::string& id,
                             const std::vector<Point3>& points) {
  const Sketch3DId sketch_id("sketch3d." + id);
  const SketchEntityId polyline_id("polyline." + id);
  auto sketch = Sketch3D::create(sketch_id, id);
  REQUIRE(sketch);
  std::vector<SketchEntityId> point_ids;
  for (std::size_t index = 0U; index < points.size(); ++index) {
    const std::string point_id = id + ".point." + std::to_string(index);
    REQUIRE(sketch.value().add_point(point3d(point_id, points[index])));
    point_ids.emplace_back(point_id);
  }
  auto polyline = SketchPolyline3D::create(polyline_id, std::move(point_ids));
  REQUIRE(polyline);
  REQUIRE(sketch.value().add_polyline(polyline.value()));
  REQUIRE(document.add_sketch_3d(sketch.value()));
  auto segment = PathSegmentReference::create_sketch_3d(sketch_id, polyline_id,
                                                        Sketch3DPathCurveKind::Polyline);
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId(id), id, {segment.value()}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
  return PathCurveId(id);
}

void add_body(PartDocument& document) {
  auto body = Body::create(BodyId("body.loft"), "Loft", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.add_body(body.value()));
}

ProfileSectionReference rectangle_section(const std::string& sketch, const std::string& profile) {
  auto region = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                               PartFeatureInputRole::LoftSection);
  REQUIRE(region);
  auto section = ProfileSectionReference::create_closed_region(region.value());
  REQUIRE(section);
  return section.value();
}

PartDocument duct_document(bool controlled) {
  auto created = PartDocument::create(DocumentId(controlled ? "part.duct.guided" : "part.duct"),
                                      "Duct transition");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  std::vector<ProfileSectionReference> sections;
  for (std::size_t index = 0U; index < 3U; ++index) {
    const std::string suffix = std::to_string(index);
    const std::string workplane = index == 0U ? "datum.xy" : "plane." + suffix;
    if (index != 0U)
      REQUIRE(document.add_construction_plane(plane(workplane, 10.0 * index)));
    REQUIRE(document.add_parameter(length_parameter("width." + suffix, 8.0)));
    REQUIRE(document.add_parameter(length_parameter("height." + suffix, 6.0)));
    auto sketch = Sketch::create(SketchId("sketch." + suffix), "Section", DatumPlaneId(workplane));
    REQUIRE(sketch);
    auto rectangle =
        RectangleProfile::create(ProfileId("profile." + suffix), ParameterId("width." + suffix),
                                 ParameterId("height." + suffix));
    REQUIRE(rectangle);
    REQUIRE(sketch.value().add_profile(rectangle.value()));
    REQUIRE(document.add_sketch(sketch.value()));
    sections.push_back(rectangle_section("sketch." + suffix, "profile." + suffix));
  }
  std::optional<PathCurveId> center_path;
  if (controlled)
    center_path = add_spatial_path(
        document, "path.center",
        {{0.0, 0.0, 0.0}, {4.0, 0.0, 5.0}, {0.0, 0.0, 10.0}, {-4.0, 0.0, 15.0}, {0.0, 0.0, 20.0}});
  add_body(document);
  auto loft = LoftFeature::create_loft(FeatureId("loft.duct"), "Duct", std::move(sections),
                                       new_body_context(), center_path);
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  return document;
}

Sketch airfoil_sketch(const std::string& id, const std::string& workplane, double half_length,
                      double half_thickness) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  const std::vector<Point2> points{
      {-half_length, 0.0}, {0.0, -half_thickness}, {half_length, 0.0}, {0.0, half_thickness}};
  std::vector<SketchEntityId> lines;
  for (std::size_t index = 0U; index < points.size(); ++index) {
    const SketchEntityId line_id(id + ".edge." + std::to_string(index));
    auto line = LineSegment::create(line_id, points[index], points[(index + 1U) % points.size()]);
    REQUIRE(line);
    REQUIRE(sketch.value().add_entity(line.value()));
    lines.push_back(line_id);
  }
  auto profile = ClosedProfile::create(ProfileId(id + ".profile"), std::move(lines));
  REQUIRE(profile);
  REQUIRE(sketch.value().add_profile(profile.value()));
  return sketch.value();
}

ProfileSectionReference airfoil_section(const std::string& id) {
  auto region = ProfileRegionReference::create(SketchId(id), ProfileId(id + ".profile"),
                                               PartFeatureInputRole::LoftSection);
  REQUIRE(region);
  auto section =
      ProfileSectionReference::create_closed_region(region.value(), SketchEntityId(id + ".edge.0"));
  REQUIRE(section);
  return section.value();
}

PartDocument blade_document(bool guided, LoftContinuity continuity) {
  auto created = PartDocument::create(DocumentId(guided ? "part.blade.guided" : "part.blade"),
                                      "Blade root mid tip");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  std::vector<ProfileSectionReference> sections;
  for (std::size_t index = 0U; index < 3U; ++index) {
    const std::string suffix = std::to_string(index);
    const std::string workplane = index == 0U ? "datum.xy" : "blade.plane." + suffix;
    if (index != 0U)
      REQUIRE(document.add_construction_plane(plane(workplane, 10.0 * index)));
    const double half_length = index == 1U ? 3.0 : 5.0;
    const std::string sketch_id = "blade.section." + suffix;
    REQUIRE(document.add_sketch(
        airfoil_sketch(sketch_id, workplane, half_length, index == 1U ? 0.7 : 1.2)));
    sections.push_back(airfoil_section(sketch_id));
  }
  std::vector<PathCurveId> guides;
  if (guided) {
    guides.push_back(add_spatial_path(document, "guide.leading",
                                      {{-5.0, 0.0, 0.0},
                                       {-8.0, 0.0, 5.0},
                                       {-3.0, 0.0, 10.0},
                                       {-8.0, 0.0, 15.0},
                                       {-5.0, 0.0, 20.0}}));
    guides.push_back(add_spatial_path(
        document, "guide.trailing",
        {{5.0, 0.0, 0.0}, {8.0, 0.0, 5.0}, {3.0, 0.0, 10.0}, {8.0, 0.0, 15.0}, {5.0, 0.0, 20.0}}));
  }
  add_body(document);
  auto loft =
      LoftFeature::create_loft(FeatureId("loft.blade"), "Blade", std::move(sections),
                               new_body_context(), std::nullopt, std::move(guides), continuity);
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  return document;
}

GeometryShapeBounds bounds(const ShapeCache& shape_cache) {
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.loft"));
  REQUIRE(shape != nullptr);
  auto result = BodyTransformAdapter{}.bounds(*shape);
  REQUIRE(result);
  return result.value();
}

} // namespace

TEST_CASE("Block 87 center path controls a duct transition", "[geometry][guided-loft]") {
  PartDocument plain = duct_document(false);
  PartDocument controlled = duct_document(true);
  ShapeCache plain_cache = cache("cache.duct.plain");
  ShapeCache controlled_cache = cache("cache.duct.controlled");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(plain, plain_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(controlled, controlled_cache));
  CHECK(bounds(controlled_cache).maximum.x > bounds(plain_cache).maximum.x + 2.0);
  CHECK(bounds(controlled_cache).minimum.x < bounds(plain_cache).minimum.x - 2.0);
}

TEST_CASE("Block 87 leading and trailing guides control a G1 blade loft",
          "[geometry][guided-loft][integration][part-construction-mvp]") {
  PartDocument plain = blade_document(false, LoftContinuity::C0);
  PartDocument guided = blade_document(true, LoftContinuity::G1);
  ShapeCache plain_cache = cache("cache.blade.plain");
  ShapeCache guided_cache = cache("cache.blade.guided");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(plain, plain_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(guided, guided_cache));
  CHECK(bounds(guided_cache).maximum.x > bounds(plain_cache).maximum.x + 2.0);
  CHECK(bounds(guided_cache).minimum.x < bounds(plain_cache).minimum.x - 2.0);
}

TEST_CASE("Block 87 fails unsupported G2 closed transactionally",
          "[geometry][guided-loft][integration][part-construction-mvp]") {
  PartDocument document = blade_document(false, LoftContinuity::G2);
  ShapeCache shape_cache = cache("cache.blade.g2");
  auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(result.has_error());
  CHECK(result.error().message().find("G2 loft continuity is unsupported") != std::string::npos);
  CHECK(shape_cache.feature_shape_count() == 0U);
  CHECK(shape_cache.body_shape_count() == 0U);
}
