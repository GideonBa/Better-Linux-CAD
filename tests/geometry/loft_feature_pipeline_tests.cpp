#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

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

Body body(const char* id, BodyKind kind = BodyKind::Solid) {
  auto value = Body::create(BodyId(id), id, kind);
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext context(FeatureBodyOperationMode mode, const char* body_id) {
  auto value = FeatureBodyResultContext::create(
      mode, mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{} : BodyId(body_id),
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId(body_id)}
                                                : std::optional<BodyId>{});
  REQUIRE(value);
  return value.value();
}

Sketch rectangle_sketch(const char* id, const char* workplane, const char* profile,
                        const char* width, const char* height) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  auto rectangle =
      RectangleProfile::create(ProfileId(profile), ParameterId(width), ParameterId(height));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  return sketch.value();
}

Sketch closed_line_sketch(const char* id, const char* workplane, const char* profile,
                          const char* prefix, double scale) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  std::vector<SketchEntityId> ids;
  for (const auto& [suffix, start, end] :
       std::vector<std::tuple<const char*, Point2, Point2>>{{"bottom", {-5.0, -3.0}, {5.0, -3.0}},
                                                            {"right", {5.0, -3.0}, {4.0, 3.0}},
                                                            {"top", {4.0, 3.0}, {-5.0, 3.0}},
                                                            {"left", {-5.0, 3.0}, {-5.0, -3.0}}}) {
    SketchEntityId entity_id(std::string(prefix) + "." + suffix);
    auto line = LineSegment::create(entity_id, {start.x * scale, start.y * scale},
                                    {end.x * scale, end.y * scale});
    REQUIRE(line);
    REQUIRE(sketch.value().add_entity(line.value()));
    ids.push_back(std::move(entity_id));
  }
  auto closed = ClosedProfile::create(ProfileId(profile), std::move(ids));
  REQUIRE(closed);
  REQUIRE(sketch.value().add_profile(closed.value()));
  return sketch.value();
}

ProfileSectionReference rectangle_section(const char* sketch, const char* profile,
                                          bool flip = false,
                                          std::optional<ParameterId> rotation = std::nullopt) {
  auto reference = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                                  PartFeatureInputRole::LoftSection);
  REQUIRE(reference);
  auto section = ProfileSectionReference::create_closed_region(reference.value(), std::nullopt,
                                                               flip, std::move(rotation));
  REQUIRE(section);
  return section.value();
}

ProfileSectionReference closed_line_section(const char* sketch, const char* profile,
                                            const char* alignment, bool flip = false,
                                            std::optional<ParameterId> rotation = std::nullopt) {
  auto reference = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                                  PartFeatureInputRole::LoftSection);
  REQUIRE(reference);
  auto section = ProfileSectionReference::create_closed_region(
      reference.value(), SketchEntityId(alignment), flip, std::move(rotation));
  REQUIRE(section);
  return section.value();
}

ConstructionPlane plane(const char* id, Point3 origin, Vector3 x_axis = {1.0, 0.0, 0.0},
                        Vector3 y_axis = {0.0, 1.0, 0.0}, Vector3 normal = {0.0, 0.0, 1.0}) {
  auto value = ConstructionPlane::create_explicit(ConstructionPlaneId(id), id, origin, x_axis,
                                                  y_axis, normal);
  REQUIRE(value);
  return value.value();
}

ShapeCache cache(const char* id = "cache.loft") {
  auto value = ShapeCache::create(ShapeCacheId(id));
  REQUIRE(value);
  return value.value();
}

const GeometryShape& body_shape(const ShapeCache& cache, const char* id) {
  const GeometryShape* shape = cache.find_body_shape(BodyId(id));
  REQUIRE(shape != nullptr);
  return *shape;
}

double volume(const ShapeCache& cache, const char* id) {
  return RectangleExtrusionAdapter{}.summarize(body_shape(cache, id)).volume_mm3;
}

PartDocument new_body_loft_document(Vector3 second_x = {1.0, 0.0, 0.0},
                                    Vector3 second_y = {0.0, 1.0, 0.0},
                                    Vector3 second_normal = {0.0, 0.0, 1.0},
                                    LoftContinuity continuity = LoftContinuity::C0) {
  auto created = PartDocument::create(DocumentId("part.loft.new"), "Loft new body");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_construction_plane(
      plane("plane.second", {0.0, 0.0, 20.0}, second_x, second_y, second_normal)));
  for (const auto& [id, value] : {std::pair{"first.width", 10.0},
                                  {"first.height", 8.0},
                                  {"second.width", 6.0},
                                  {"second.height", 4.0}})
    REQUIRE(document.add_parameter(length_parameter(id, value)));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.first", "datum.xy", "profile.first",
                                               "first.width", "first.height")));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.second", "plane.second", "profile.second",
                                               "second.width", "second.height")));
  REQUIRE(document.add_body(body("body.loft")));
  std::vector sections{rectangle_section("sketch.first", "profile.first"),
                       rectangle_section("sketch.second", "profile.second")};
  auto loft = LoftFeature::create_loft(FeatureId("loft.main"), "Loft", std::move(sections),
                                       context(FeatureBodyOperationMode::NewBody, "body.loft"),
                                       std::nullopt, {}, continuity);
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  return document;
}

PartDocument body_operation_document(FeatureBodyOperationMode mode) {
  auto created = PartDocument::create(DocumentId("part.loft.operation"), "Loft operation");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  const double start_z = mode == FeatureBodyOperationMode::Join ? 20.0 : 0.0;
  const double end_z = mode == FeatureBodyOperationMode::Join ? 30.0 : 20.0;
  REQUIRE(document.add_construction_plane(plane("plane.first", {0.0, 0.0, start_z})));
  REQUIRE(document.add_construction_plane(plane("plane.second", {0.0, 0.0, end_z})));
  for (const auto& [id, value] : {std::pair{"base.width", 20.0},
                                  {"base.height", 20.0},
                                  {"base.depth", 20.0},
                                  {"first.width", 6.0},
                                  {"first.height", 6.0},
                                  {"second.width", 8.0},
                                  {"second.height", 8.0}})
    REQUIRE(document.add_parameter(length_parameter(id, value)));
  REQUIRE(document.add_sketch(
      rectangle_sketch("sketch.base", "datum.xy", "profile.base", "base.width", "base.height")));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.first", "plane.first", "profile.first",
                                               "first.width", "first.height")));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.second", "plane.second", "profile.second",
                                               "second.width", "second.height")));
  REQUIRE(document.add_body(body("body.result")));
  auto base = Feature::create_additive_extrude(FeatureId("feature.base"), "Base",
                                               SketchId("sketch.base"), ParameterId("base.depth"));
  REQUIRE(base);
  auto attached = base.value().with_body_result_context(
      context(FeatureBodyOperationMode::NewBody, "body.result"));
  REQUIRE(attached);
  REQUIRE(document.add_feature(attached.value()));
  std::vector sections{rectangle_section("sketch.first", "profile.first"),
                       rectangle_section("sketch.second", "profile.second")};
  Result<LoftFeature> loft =
      mode == FeatureBodyOperationMode::Cut
          ? LoftFeature::create_loft_cut(FeatureId("loft.operation"), "Loft cut", sections,
                                         context(mode, "body.result"))
          : LoftFeature::create_loft(FeatureId("loft.operation"), "Loft", sections,
                                     context(mode, "body.result"));
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  return document;
}

void add_open_section(PartDocument& document, const char* sketch_id, const char* workplane,
                      const char* entity_id, const char* path_id, double offset) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  auto line = LineSegment::create(SketchEntityId(entity_id), {-5.0, offset}, {5.0, offset});
  REQUIRE(line);
  REQUIRE(sketch.value().add_entity(line.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto segment = PathSegmentReference::create_planar(SketchId(sketch_id), SketchEntityId(entity_id),
                                                     PlanarPathCurveKind::Line);
  REQUIRE(segment);
  auto path = PathCurve::create(PathCurveId(path_id), path_id, {segment.value()}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
}

} // namespace

TEST_CASE("Block 85 lofts parallel non-parallel and arbitrary-plane sections",
          "[geometry][loft-feature]") {
  auto parallel = new_body_loft_document();
  auto tilted =
      new_body_loft_document({1.0, 0.0, 0.0}, {0.0, 0.7071067811865476, 0.7071067811865476},
                             {0.0, -0.7071067811865476, 0.7071067811865476});
  auto arbitrary =
      new_body_loft_document({0.7071067811865476, 0.7071067811865476, 0.0},
                             {-0.408248290463863, 0.408248290463863, 0.816496580927726},
                             {0.577350269189626, -0.577350269189626, 0.577350269189626});
  ShapeCache parallel_cache = cache("cache.parallel");
  ShapeCache tilted_cache = cache("cache.tilted");
  ShapeCache arbitrary_cache = cache("cache.arbitrary");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(parallel, parallel_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(tilted, tilted_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(arbitrary, arbitrary_cache));
  CHECK(volume(parallel_cache, "body.loft") > 700.0);
  CHECK(volume(tilted_cache, "body.loft") > 500.0);
  CHECK(volume(arbitrary_cache, "body.loft") > 300.0);
  auto bounds = BodyTransformAdapter{}.bounds(body_shape(arbitrary_cache, "body.loft"));
  REQUIRE(bounds);
  CHECK(bounds.value().maximum.z - bounds.value().minimum.z > 17.0);
}

TEST_CASE("Block 85 creates an open-section LoftSurface", "[geometry][loft-feature]") {
  auto created = PartDocument::create(DocumentId("part.loft.surface"), "Surface loft");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(document.add_construction_plane(plane("plane.second", {0.0, 0.0, 10.0})));
  add_open_section(document, "sketch.first", "datum.xy", "line.first", "path.first", 0.0);
  add_open_section(document, "sketch.second", "plane.second", "line.second", "path.second", 3.0);
  REQUIRE(document.add_body(body("body.surface", BodyKind::Surface)));
  auto first = ProfileSectionReference::create_open_path(PathCurveId("path.first"));
  auto second = ProfileSectionReference::create_open_path(PathCurveId("path.second"));
  REQUIRE(first);
  REQUIRE(second);
  auto loft = LoftFeature::create_loft_surface(
      FeatureId("loft.surface"), "Surface", {first.value(), second.value()},
      context(FeatureBodyOperationMode::NewBody, "body.surface"));
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  ShapeCache shape_cache = cache("cache.surface");
  auto summary = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(summary);
  CHECK(summary.value().executed_feature_count == 1U);
  CHECK(
      RectangleExtrusionAdapter{}.summarize(body_shape(shape_cache, "body.surface")).solid_count ==
      0U);
}

TEST_CASE("Block 85 applies Join LoftCut and Intersect transactionally",
          "[geometry][loft-feature]") {
  auto joined = body_operation_document(FeatureBodyOperationMode::Join);
  auto cut = body_operation_document(FeatureBodyOperationMode::Cut);
  auto intersected = body_operation_document(FeatureBodyOperationMode::Intersect);
  ShapeCache joined_cache = cache("cache.join");
  ShapeCache cut_cache = cache("cache.cut");
  ShapeCache intersected_cache = cache("cache.intersect");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(joined, joined_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(cut, cut_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(intersected, intersected_cache));
  CHECK(volume(joined_cache, "body.result") > 8000.0);
  CHECK(volume(cut_cache, "body.result") < 8000.0);
  CHECK(volume(cut_cache, "body.result") > 6500.0);
  CHECK(volume(intersected_cache, "body.result") > 500.0);
  CHECK(volume(intersected_cache, "body.result") < 1500.0);
}

TEST_CASE("Block 85 executes stable seam flip and rotation intent", "[geometry][loft-feature]") {
  auto controlled = PartDocument::create(DocumentId("part.loft.controls"), "Loft controls");
  REQUIRE(controlled);
  PartDocument model = controlled.value();
  REQUIRE(model.add_datum_plane(DatumPlane::xy().value()));
  REQUIRE(model.add_construction_plane(plane("plane.second", {0.0, 0.0, 20.0})));
  REQUIRE(model.add_parameter(angle_parameter("section.rotation", 20.0)));
  REQUIRE(model.add_sketch(
      closed_line_sketch("sketch.first", "datum.xy", "profile.first", "first", 1.0)));
  REQUIRE(model.add_sketch(
      closed_line_sketch("sketch.second", "plane.second", "profile.second", "second", 0.7)));
  REQUIRE(model.add_body(body("body.loft")));
  std::vector sections{closed_line_section("sketch.first", "profile.first", "first.bottom"),
                       closed_line_section("sketch.second", "profile.second", "second.bottom", true,
                                           ParameterId("section.rotation"))};
  auto loft = LoftFeature::create_loft(FeatureId("loft.controlled"), "Controlled", sections,
                                       context(FeatureBodyOperationMode::NewBody, "body.loft"));
  REQUIRE(loft);
  REQUIRE(model.add_loft_feature(loft.value()));
  ShapeCache first_cache = cache("cache.controls.first");
  ShapeCache second_cache = cache("cache.controls.second");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(model, first_cache));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(model, second_cache));
  CHECK(volume(first_cache, "body.loft") ==
        Catch::Approx(volume(second_cache, "body.loft")).epsilon(1.0e-10));
}

TEST_CASE("Block 87 rejects unsupported G2 continuity execution transactionally",
          "[geometry][loft-feature]") {
  auto document =
      new_body_loft_document({1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, LoftContinuity::G2);
  ShapeCache shape_cache = cache("cache.failure");
  auto result = GeometryRecomputeExecutor{}.execute_document(document, shape_cache);
  REQUIRE(result.has_error());
  CHECK(result.error().message().find("G2 loft continuity is unsupported") != std::string::npos);
  CHECK(shape_cache.feature_shape_count() == 0U);
  CHECK(shape_cache.body_shape_count() == 0U);
}
