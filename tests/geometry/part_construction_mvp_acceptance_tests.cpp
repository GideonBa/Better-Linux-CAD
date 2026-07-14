#include "blcad/core/part_document_json.hpp"
#include "blcad/geometry/body_result_inspector.hpp"
#include "blcad/geometry/recompute_executor.hpp"
#include "blcad/geometry/step_exporter.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <string>

using namespace blcad;
using namespace blcad::geometry;

namespace {

Parameter length_parameter(const std::string& id, double value_mm) {
  auto quantity = Quantity::length_mm(value_mm, id);
  REQUIRE(quantity);
  auto parameter = Parameter::create_length(ParameterId(id), id, quantity.value());
  REQUIRE(parameter);
  return parameter.value();
}

Body solid_body(const std::string& id) {
  auto body = Body::create(BodyId(id), id, BodyKind::Solid);
  REQUIRE(body);
  return body.value();
}

Feature new_body_extrude(const std::string& suffix) {
  auto feature = Feature::create_additive_extrude(
      FeatureId("feature." + suffix), "Extrude " + suffix,
      SketchId("sketch." + suffix), ParameterId(suffix + ".depth"));
  REQUIRE(feature);
  auto context = FeatureBodyResultContext::create(
      FeatureBodyOperationMode::NewBody, std::nullopt, BodyId("body." + suffix));
  REQUIRE(context);
  auto attached = feature.value().with_body_result_context(context.value());
  REQUIRE(attached);
  return attached.value();
}

PartDocument two_body_acceptance_document() {
  auto document = PartDocument::create(DocumentId("part.mvp6.acceptance"),
                                       "Part Construction MVP acceptance");
  REQUIRE(document);
  auto xy = DatumPlane::xy();
  REQUIRE(xy);
  REQUIRE(document.value().add_datum_plane(xy.value()));

  // Deliberately author B before A. Public Body and exchange order must still be BodyId order.
  for (const std::string suffix : {"b", "a"}) {
    const double scale = suffix == "a" ? 1.0 : 0.5;
    REQUIRE(document.value().add_parameter(length_parameter(suffix + ".width", 40.0 * scale)));
    REQUIRE(document.value().add_parameter(length_parameter(suffix + ".height", 30.0 * scale)));
    REQUIRE(document.value().add_parameter(length_parameter(suffix + ".depth", 5.0 * scale)));

    auto sketch = Sketch::create(SketchId("sketch." + suffix), "Sketch " + suffix,
                                 DatumPlaneId("datum.xy"));
    REQUIRE(sketch);
    auto rectangle = RectangleProfile::create(ProfileId("profile." + suffix),
                                              ParameterId(suffix + ".width"),
                                              ParameterId(suffix + ".height"));
    REQUIRE(rectangle);
    REQUIRE(sketch.value().add_profile(rectangle.value()));
    REQUIRE(document.value().add_sketch(sketch.value()));
    REQUIRE(document.value().add_body(solid_body("body." + suffix)));
    REQUIRE(document.value().add_feature(new_body_extrude(suffix)));
  }
  return document.value();
}

ShapeCache acceptance_cache() {
  auto cache = ShapeCache::create(ShapeCacheId("shape-cache.mvp6.acceptance"));
  REQUIRE(cache);
  return cache.value();
}

} // namespace

TEST_CASE("Part Construction MVP roundtrips recomputes incrementally and exports deterministically",
          "[integration][part-construction-mvp]") {
  const PartDocument authored = two_body_acceptance_document();
  const auto authored_json = serialize_part_document_to_json(authored);
  REQUIRE(authored_json);
  CHECK(authored_json.value().find("TopoDS") == std::string::npos);
  CHECK(authored_json.value().find("XDE") == std::string::npos);
  CHECK(authored_json.value().find("STEP") == std::string::npos);

  auto restored = deserialize_part_document_from_json(authored_json.value());
  REQUIRE(restored);
  const auto canonical_json = serialize_part_document_to_json(restored.value());
  REQUIRE(canonical_json);
  CHECK(canonical_json.value() == authored_json.value());

  ShapeCache cache = acceptance_cache();
  const GeometryRecomputeExecutor executor;
  const auto initial = executor.execute_document(restored.value(), cache);
  REQUIRE(initial);
  CHECK(initial.value().executed_feature_count == 2U);

  const auto inspection = BodyResultInspector{}.inspect(restored.value(), cache);
  REQUIRE(inspection);
  REQUIRE(inspection.value().bodies.size() == 2U);
  CHECK(inspection.value().bodies[0].body_id.value() == "body.a");
  CHECK(inspection.value().bodies[1].body_id.value() == "body.b");
  CHECK(inspection.value().solid_body_count == 2U);

  const GeometryShape* body_b_before = cache.find_body_shape(BodyId("body.b"));
  REQUIRE(body_b_before != nullptr);
  const double body_b_volume_before =
      RectangleExtrusionAdapter{}.summarize(*body_b_before).volume_mm3;

  auto wider = Quantity::length_mm(60.0, "a.width");
  REQUIRE(wider);
  REQUIRE(restored.value().set_parameter_value(ParameterId("a.width"), wider.value()));
  const auto plan = restored.value().create_recompute_plan();
  REQUIRE(plan);
  CHECK(plan.value().contains("feature.a"));
  CHECK_FALSE(plan.value().contains("feature.b"));
  const auto incremental = executor.execute_plan(restored.value(), plan.value(), cache);
  REQUIRE(incremental);

  const GeometryShape* body_a_after = cache.find_body_shape(BodyId("body.a"));
  const GeometryShape* body_b_after = cache.find_body_shape(BodyId("body.b"));
  REQUIRE(body_a_after != nullptr);
  REQUIRE(body_b_after != nullptr);
  CHECK(RectangleExtrusionAdapter{}.summarize(*body_a_after).volume_mm3 ==
        Catch::Approx(9000.0));
  CHECK(RectangleExtrusionAdapter{}.summarize(*body_b_after).volume_mm3 ==
        Catch::Approx(body_b_volume_before));

  const auto before_export = serialize_part_document_to_json(restored.value());
  REQUIRE(before_export);
  const auto path = std::filesystem::temp_directory_path() /
                    "blcad_part_construction_mvp_acceptance.step";
  std::filesystem::remove(path);
  const auto exported =
      StepExporter{}.write_part_step(restored.value(), cache, path.string());
  REQUIRE(exported);
  REQUIRE(exported.value().bodies.size() == 2U);
  CHECK(exported.value().bodies[0].body_id.value() == "body.a");
  CHECK(exported.value().bodies[1].body_id.value() == "body.b");
  CHECK(exported.value().exported_solid_body_count == 2U);
  CHECK(exported.value().exported_surface_body_count == 0U);
  CHECK(exported.value().written_bytes == std::filesystem::file_size(path));

  const auto after_export = serialize_part_document_to_json(restored.value());
  REQUIRE(after_export);
  CHECK(after_export.value() == before_export.value());
  std::filesystem::remove(path);
}
