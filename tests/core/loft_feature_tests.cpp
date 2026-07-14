#include "blcad/core/part_document_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <tuple>

using namespace blcad;
using json = nlohmann::json;

namespace {

ProfileRegionReference profile(const char* sketch, const char* profile) {
  auto value = ProfileRegionReference::create(SketchId(sketch), ProfileId(profile),
                                              PartFeatureInputRole::LoftSection);
  REQUIRE(value);
  return value.value();
}

ProfileSectionReference section(const char* sketch, const char* profile,
                                std::optional<ParameterId> rotation = std::nullopt) {
  auto value = ProfileSectionReference::create_closed_region(
      ::profile(sketch, profile), std::nullopt, false, std::move(rotation));
  REQUIRE(value);
  return value.value();
}

FeatureBodyResultContext context(FeatureBodyOperationMode mode, const char* body) {
  auto value = FeatureBodyResultContext::create(
      mode, mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{} : BodyId(body),
      mode == FeatureBodyOperationMode::NewBody ? std::optional<BodyId>{BodyId(body)}
                                                : std::optional<BodyId>{});
  REQUIRE(value);
  return value.value();
}

Body body(const char* id, BodyKind kind = BodyKind::Solid) {
  auto value = Body::create(BodyId(id), id, kind);
  REQUIRE(value);
  return value.value();
}

Parameter angle(const char* id, double degrees) {
  auto quantity = Quantity::angle_deg(degrees, id);
  REQUIRE(quantity);
  auto value = Parameter::create_angle(ParameterId(id), id, quantity.value());
  REQUIRE(value);
  return value.value();
}

Sketch rectangle_sketch(const char* id, const char* profile_id) {
  auto value = Sketch::create(SketchId(id), id, DatumPlaneId("datum.xy"));
  REQUIRE(value);
  auto rectangle =
      RectangleProfile::create(ProfileId(profile_id), ParameterId("width"), ParameterId("height"));
  REQUIRE(rectangle);
  REQUIRE(value.value().add_profile(rectangle.value()));
  return value.value();
}

void add_path(PartDocument& document, const char* sketch_id, const char* entity_id,
              const char* path_id, double x) {
  auto sketch = Sketch::create(SketchId(sketch_id), sketch_id, DatumPlaneId("datum.xy"));
  REQUIRE(sketch);
  auto line = LineSegment::create(SketchEntityId(entity_id), {x, 0.0}, {x, 10.0});
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

PartDocument loft_document() {
  auto created = PartDocument::create(DocumentId("part.loft"), "Loft");
  REQUIRE(created);
  PartDocument document = created.value();
  REQUIRE(document.add_datum_plane(DatumPlane::xy().value()));
  auto width = Quantity::length_mm(10.0, "width");
  auto height = Quantity::length_mm(8.0, "height");
  REQUIRE(width);
  REQUIRE(height);
  REQUIRE(document.add_parameter(
      Parameter::create_length(ParameterId("width"), "width", width.value()).value()));
  REQUIRE(document.add_parameter(
      Parameter::create_length(ParameterId("height"), "height", height.value()).value()));
  REQUIRE(document.add_parameter(angle("rotation", 15.0)));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.first", "profile.first")));
  REQUIRE(document.add_sketch(rectangle_sketch("sketch.second", "profile.second")));
  add_path(document, "sketch.path", "line.path", "path.center", 20.0);
  add_path(document, "sketch.guide", "line.guide", "path.guide", 30.0);
  REQUIRE(document.add_body(body("body.loft")));
  return document;
}

} // namespace

TEST_CASE("Block 84 freezes ordered Loft feature intent", "[core][loft-feature]") {
  const std::vector sections{section("sketch.first", "profile.first"),
                             section("sketch.second", "profile.second")};
  auto loft = LoftFeature::create_loft(FeatureId("loft.main"), "Loft", sections,
                                       context(FeatureBodyOperationMode::NewBody, "body.loft"),
                                       PathCurveId("path.center"), {PathCurveId("path.guide")},
                                       LoftContinuity::G1);
  REQUIRE(loft);
  CHECK(loft.value().kind() == LoftFeatureKind::Loft);
  CHECK(loft.value().sections().size() == 2U);
  CHECK(loft.value().sections()[0].source_node_id() == "sketch.first");
  CHECK(loft.value().continuity() == LoftContinuity::G1);
  CHECK(to_string(LoftFeatureKind::LoftSurface) == "loft_surface");
  CHECK(to_string(LoftContinuity::G2) == "g2");
  auto cut = LoftFeature::create_loft_cut(FeatureId("loft.cut"), "Cut", sections,
                                          context(FeatureBodyOperationMode::Cut, "body.loft"));
  REQUIRE(cut);
  CHECK(cut.value().kind() == LoftFeatureKind::LoftCut);
  CHECK(LoftFeature::create_loft(FeatureId("bad"), "Bad", {sections.front()},
                                 context(FeatureBodyOperationMode::NewBody, "body.loft"))
            .has_error());
  CHECK(LoftFeature::create_loft_cut(FeatureId("bad.cut"), "Bad", sections,
                                     context(FeatureBodyOperationMode::NewBody, "body.loft"))
            .has_error());
}

TEST_CASE("Block 84 freezes closed-profile seam alignment identity", "[core][loft-feature]") {
  auto document = loft_document();
  auto aligned_sketch =
      Sketch::create(SketchId("sketch.aligned"), "Aligned", DatumPlaneId("datum.xy"));
  REQUIRE(aligned_sketch);
  for (const auto& [id, start, end] : std::vector<std::tuple<const char*, Point2, Point2>>{
           {"line.bottom", {0.0, 0.0}, {10.0, 0.0}},
           {"line.right", {10.0, 0.0}, {10.0, 8.0}},
           {"line.top", {10.0, 8.0}, {0.0, 8.0}},
           {"line.left", {0.0, 8.0}, {0.0, 0.0}}}) {
    auto line = LineSegment::create(SketchEntityId(id), start, end);
    REQUIRE(line);
    REQUIRE(aligned_sketch.value().add_entity(line.value()));
  }
  auto closed = ClosedProfile::create(ProfileId("profile.aligned"),
                                      {SketchEntityId("line.bottom"), SketchEntityId("line.right"),
                                       SketchEntityId("line.top"), SketchEntityId("line.left")});
  REQUIRE(closed);
  REQUIRE(aligned_sketch.value().add_profile(closed.value()));
  REQUIRE(document.add_sketch(aligned_sketch.value()));
  auto first = ProfileSectionReference::create_closed_region(
      profile("sketch.aligned", "profile.aligned"), SketchEntityId("line.bottom"));
  REQUIRE(first);
  auto valid = LoftFeature::create_loft(FeatureId("loft.aligned"), "Aligned",
                                        {first.value(), section("sketch.second", "profile.second")},
                                        context(FeatureBodyOperationMode::NewBody, "body.loft"));
  REQUIRE(valid);
  REQUIRE(document.add_loft_feature(valid.value()));

  auto invalid_section = ProfileSectionReference::create_closed_region(
      profile("sketch.aligned", "profile.aligned"), SketchEntityId("line.missing"));
  REQUIRE(invalid_section);
  auto invalid =
      LoftFeature::create_loft(FeatureId("loft.invalid-alignment"), "Invalid alignment",
                               {invalid_section.value(), section("sketch.first", "profile.first")},
                               context(FeatureBodyOperationMode::NewBody, "body.loft"));
  REQUIRE(invalid);
  CHECK(document.add_loft_feature(invalid.value()).has_error());
}

TEST_CASE("Block 84 validates dependencies and roundtrips strict Loft JSON",
          "[core][loft-feature][integration][part-construction-mvp]") {
  auto document = loft_document();
  std::vector sections{section("sketch.first", "profile.first", ParameterId("rotation")),
                       section("sketch.second", "profile.second")};
  auto loft = LoftFeature::create_loft(FeatureId("loft.main"), "Loft", std::move(sections),
                                       context(FeatureBodyOperationMode::NewBody, "body.loft"),
                                       PathCurveId("path.center"), {PathCurveId("path.guide")},
                                       LoftContinuity::G2);
  REQUIRE(loft);
  REQUIRE(document.add_loft_feature(loft.value()));
  CHECK(document.loft_feature_count() == 1U);
  CHECK(document.dependency_graph().has_dependency("sketch.first", "loft.main"));
  CHECK(document.dependency_graph().has_dependency("rotation", "loft.main"));
  CHECK(document.dependency_graph().has_dependency("path.center", "loft.main"));
  CHECK(document.dependency_graph().has_dependency("path.guide", "loft.main"));
  CHECK(document.remove_path_curve(PathCurveId("path.guide")).has_error());

  auto serialized = serialize_part_document_to_json(document);
  REQUIRE(serialized);
  json value = json::parse(serialized.value());
  REQUIRE(value["loft_features"].size() == 1U);
  CHECK(value["loft_features"][0]["type"] == "loft");
  CHECK(value["loft_features"][0]["continuity"] == "g2");
  CHECK(value["loft_features"][0]["sections"][0]["rotation_offset"] == "rotation");
  auto restored = deserialize_part_document_from_json(serialized.value());
  INFO((restored.has_error() ? restored.error().object_id() + ": " + restored.error().message()
                             : std::string("round-trip succeeded")));
  REQUIRE(restored);
  const LoftFeature* restored_loft = restored.value().find_loft_feature(FeatureId("loft.main"));
  REQUIRE(restored_loft != nullptr);
  CHECK(restored_loft->sections().size() == 2U);
  CHECK(std::get<ProfileRegionReference>(restored_loft->sections()[0].source()).profile() ==
        ProfileId("profile.first"));
  CHECK(std::get<ProfileRegionReference>(restored_loft->sections()[1].source()).profile() ==
        ProfileId("profile.second"));
  CHECK(restored_loft->path_curve() == PathCurveId("path.center"));
  CHECK(restored_loft->guide_curves() == std::vector<PathCurveId>{PathCurveId("path.guide")});

  json malformed = value;
  malformed["loft_features"][0]["sections"].erase(1);
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  malformed = value;
  malformed["loft_features"][0]["unexpected"] = true;
  CHECK(deserialize_part_document_from_json(malformed.dump()).has_error());
  value.erase("loft_features");
  auto historical = deserialize_part_document_from_json(value.dump());
  REQUIRE(historical);
  CHECK(historical.value().loft_feature_count() == 0U);
}

TEST_CASE("Block 84 accepts open ordered sections only for LoftSurface", "[core][loft-feature]") {
  auto document = loft_document();
  add_path(document, "sketch.open.first", "line.open.first", "path.open.first", 40.0);
  add_path(document, "sketch.open.second", "line.open.second", "path.open.second", 50.0);
  REQUIRE(document.add_body(body("body.surface", BodyKind::Surface)));
  auto first = ProfileSectionReference::create_open_path(PathCurveId("path.open.first"));
  auto second = ProfileSectionReference::create_open_path(PathCurveId("path.open.second"), true);
  REQUIRE(first);
  REQUIRE(second);
  std::vector sections{first.value(), second.value()};
  auto surface =
      LoftFeature::create_loft_surface(FeatureId("loft.surface"), "Surface", sections,
                                       context(FeatureBodyOperationMode::NewBody, "body.surface"));
  REQUIRE(surface);
  REQUIRE(document.add_loft_feature(surface.value()));
  CHECK(document.find_loft_feature(FeatureId("loft.surface")) != nullptr);
  auto wrong_body =
      LoftFeature::create_loft_surface(FeatureId("loft.wrong-body"), "Wrong body", sections,
                                       context(FeatureBodyOperationMode::NewBody, "body.loft"));
  REQUIRE(wrong_body);
  CHECK(document.add_loft_feature(wrong_body.value()).has_error());
  CHECK(LoftFeature::create_loft(FeatureId("bad.solid"), "Bad", sections,
                                 context(FeatureBodyOperationMode::NewBody, "body.loft"))
            .has_error());
}
