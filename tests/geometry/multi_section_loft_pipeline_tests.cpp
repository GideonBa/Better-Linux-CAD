#include "blcad/geometry/recompute_executor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace blcad;
using namespace blcad::geometry;

namespace {

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

Sketch rectangle_sketch(const std::string& id, const std::string& workplane,
                        const std::string& profile, const std::string& width,
                        const std::string& height) {
  auto sketch = Sketch::create(SketchId(id), id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  auto rectangle =
      RectangleProfile::create(ProfileId(profile), ParameterId(width), ParameterId(height));
  REQUIRE(rectangle);
  REQUIRE(sketch.value().add_profile(rectangle.value()));
  return sketch.value();
}

ProfileSectionReference rectangle_section(const std::string& sketch, const std::string& profile) {
  auto reference = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                                  PartFeatureInputRole::LoftSection);
  REQUIRE(reference);
  auto section = ProfileSectionReference::create_closed_region(reference.value());
  REQUIRE(section);
  return section.value();
}

FeatureBodyResultContext new_body_context(const char* body_id) {
  auto context = FeatureBodyResultContext::create(FeatureBodyOperationMode::NewBody, std::nullopt,
                                                  BodyId(body_id));
  REQUIRE(context);
  return context.value();
}

ShapeCache cache(const char* id) {
  auto value = ShapeCache::create(ShapeCacheId(id));
  REQUIRE(value);
  return value.value();
}

double volume(const ShapeCache& cache) {
  const GeometryShape* shape = cache.find_body_shape(BodyId("body.loft"));
  REQUIRE(shape != nullptr);
  return RectangleExtrusionAdapter{}.summarize(*shape).volume_mm3;
}

std::vector<ClosedProfileCurveSegment> polygon(std::vector<Point3> points) {
  std::vector<ClosedProfileCurveSegment> curves;
  curves.reserve(points.size());
  for (std::size_t index = 0U; index < points.size(); ++index)
    curves.push_back(
        {ClosedProfileCurveKind::Line, points[index], {}, points[(index + 1U) % points.size()]});
  return curves;
}

PartDocument multi_section_document(const std::vector<double>& widths,
                                    const std::vector<double>& heights) {
  REQUIRE(widths.size() == heights.size());
  REQUIRE(widths.size() >= 3U);
  auto created = PartDocument::create(DocumentId("part.multi-loft"), "Multi-section loft");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));

  std::vector<ProfileSectionReference> sections;
  sections.reserve(widths.size());
  for (std::size_t index = 0U; index < widths.size(); ++index) {
    const std::string suffix = std::to_string(index);
    const std::string workplane = index == 0U ? "datum.xy" : "plane." + suffix;
    const std::string sketch = "sketch." + suffix;
    const std::string profile = "profile." + suffix;
    const std::string width = "width." + suffix;
    const std::string height = "height." + suffix;
    if (index != 0U)
      REQUIRE(document.add_construction_plane(plane(workplane, 10.0 * index)));
    REQUIRE(document.add_parameter(length_parameter(width, widths[index])));
    REQUIRE(document.add_parameter(length_parameter(height, heights[index])));
    REQUIRE(document.add_sketch(rectangle_sketch(sketch, workplane, profile, width, height)));
    sections.push_back(rectangle_section(sketch, profile));
  }
  auto body = Body::create(BodyId("body.loft"), "Loft body", BodyKind::Solid);
  REQUIRE(body);
  REQUIRE(document.add_body(body.value()));
  auto loft = LoftFeature::create_loft(FeatureId("loft.multi"), "Multi-section loft",
                                       std::move(sections), new_body_context("body.loft"));
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  return document;
}

void add_open_section(PartDocument& document, std::size_t index, double offset) {
  const std::string suffix = std::to_string(index);
  const std::string workplane = index == 0U ? "datum.xy" : "surface.plane." + suffix;
  if (index != 0U)
    REQUIRE(document.add_construction_plane(plane(workplane, 8.0 * index)));
  const std::string sketch_id = "surface.sketch." + suffix;
  const std::string line_id = "surface.line." + suffix;
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId(workplane));
  REQUIRE(sketch);
  auto line = LineSegment::create(SketchEntityId(line_id), {-5.0, offset}, {5.0, offset});
  REQUIRE(line);
  REQUIRE(sketch.value().add_entity(line.value()));
  REQUIRE(document.add_sketch(sketch.value()));
  auto segment = PathSegmentReference::create_planar(SketchId(sketch_id), SketchEntityId(line_id),
                                                     PlanarPathCurveKind::Line);
  REQUIRE(segment);
  const std::string path_id = "surface.path." + suffix;
  auto path = PathCurve::create(PathCurveId(path_id), path_id, {segment.value()}, PathClosure::Open,
                                PathOrientationRule::MinimumTwist);
  REQUIRE(path);
  REQUIRE(document.add_path_curve(path.value()));
}

} // namespace

TEST_CASE("Block 86 executes three ordered sections and recomputes an edited middle section",
          "[geometry][multi-section-loft]") {
  PartDocument document = multi_section_document({10.0, 3.0, 10.0}, {8.0, 2.0, 8.0});
  ShapeCache shape_cache = cache("cache.multi.edit");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const double narrow_middle_volume = volume(shape_cache);

  auto wider = Quantity::length_mm(15.0, "width.1");
  REQUIRE(wider);
  auto affected = document.set_parameter_value(ParameterId("width.1"), wider.value());
  REQUIRE(affected);
  CHECK(std::find(affected.value().begin(), affected.value().end(), "loft.multi") !=
        affected.value().end());
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const double wide_middle_volume = volume(shape_cache);
  CHECK(wide_middle_volume > narrow_middle_volume + 100.0);
}

TEST_CASE("Block 86 executes four sections deterministically", "[geometry][multi-section-loft]") {
  PartDocument document = multi_section_document({10.0, 4.0, 13.0, 7.0}, {8.0, 3.0, 9.0, 5.0});
  ShapeCache first = cache("cache.multi.four.first");
  ShapeCache second = cache("cache.multi.four.second");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, first));
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, second));
  CHECK(volume(first) == Catch::Approx(volume(second)).epsilon(1.0e-10));
}

TEST_CASE("Block 86 creates a four-section open LoftSurface", "[geometry][multi-section-loft]") {
  auto created = PartDocument::create(DocumentId("part.multi-surface"), "Multi surface");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  std::vector<ProfileSectionReference> sections;
  for (std::size_t index = 0U; index < 4U; ++index) {
    add_open_section(document, index, index == 1U ? 4.0 : static_cast<double>(index));
    auto section = ProfileSectionReference::create_open_path(
        PathCurveId("surface.path." + std::to_string(index)));
    REQUIRE(section);
    sections.push_back(section.value());
  }
  auto body = Body::create(BodyId("body.surface"), "Surface", BodyKind::Surface);
  REQUIRE(body);
  REQUIRE(document.add_body(body.value()));
  auto loft = LoftFeature::create_loft_surface(
      FeatureId("loft.surface"), "Surface", std::move(sections), new_body_context("body.surface"));
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  ShapeCache shape_cache = cache("cache.multi.surface");
  REQUIRE(GeometryRecomputeExecutor{}.execute_document(document, shape_cache));
  const GeometryShape* shape = shape_cache.find_body_shape(BodyId("body.surface"));
  REQUIRE(shape != nullptr);
  CHECK(RectangleExtrusionAdapter{}.summarize(*shape).solid_count == 0U);
}

TEST_CASE("Block 86 rejects incidental section correspondence", "[geometry][multi-section-loft]") {
  const auto square =
      polygon({{-5.0, -5.0, 0.0}, {5.0, -5.0, 0.0}, {5.0, 5.0, 0.0}, {-5.0, 5.0, 0.0}});
  const auto triangle = polygon({{-4.0, -4.0, 10.0}, {4.0, -4.0, 10.0}, {0.0, 4.0, 10.0}});
  const auto top =
      polygon({{-3.0, -3.0, 20.0}, {3.0, -3.0, 20.0}, {3.0, 3.0, 20.0}, {-3.0, 3.0, 20.0}});
  auto result =
      LoftAdapter{}.loft_closed_sections(FeatureId("loft.mismatch"), {square, triangle, top}, true);
  REQUIRE(result.has_error());
  CHECK(result.error().message().find("matching authored edge counts") != std::string::npos);
}
